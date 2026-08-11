/**
 * @file WiFiManagerLite.cpp
 * @brief Implementation of WiFiManagerLite
 * 
 * @author Extracted from BambuBeacon project
 * @version 1.0.0
 */

#include "WiFiManagerLite.h"

#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <new>

namespace WML {

// ==================== Identity Helpers ====================

// HEAP-OPTIMIERT: In-place Sanitization ohne String::remove() Aufrufe
// (vorher: 5+ Heap-Operationen pro Aufruf durch String-Kopie und remove())
static size_t sanitizeInPlace(char* buf, size_t bufSize, bool toLowerCase) {
    if (!buf || bufSize == 0) return 0;
    
    size_t len = strlen(buf);
    if (len == 0) return 0;
    
    // Trim leading spaces (in-place shift)
    size_t start = 0;
    while (start < len && (buf[start] == ' ' || buf[start] == '\t')) {
        start++;
    }
    if (start > 0) {
        memmove(buf, buf + start, len - start + 1);
        len -= start;
    }
    
    // Trim trailing spaces
    while (len > 0 && (buf[len - 1] == ' ' || buf[len - 1] == '\t')) {
        buf[--len] = '\0';
    }
    
    if (len == 0) return 0;
    
    // Replace invalid chars with '-' and optionally lowercase
    for (size_t i = 0; i < len; i++) {
        char c = buf[i];
        if (toLowerCase && c >= 'A' && c <= 'Z') {
            c = c + ('a' - 'A');
        }
        bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
        if (!toLowerCase) {
            ok = ok || (c >= 'A' && c <= 'Z');
        }
        buf[i] = ok ? c : '-';
    }
    
    // Collapse '--' sequences (single pass, in-place)
    size_t write = 0;
    for (size_t read = 0; read < len; read++) {
        if (buf[read] == '-' && write > 0 && buf[write - 1] == '-') {
            continue;  // Skip duplicate '-'
        }
        buf[write++] = buf[read];
    }
    buf[write] = '\0';
    len = write;
    
    // Trim leading '-'
    while (len > 0 && buf[0] == '-') {
        memmove(buf, buf + 1, len);
        len--;
    }
    
    // Trim trailing '-'
    while (len > 0 && buf[len - 1] == '-') {
        buf[--len] = '\0';
    }
    
    return len;
}

String WiFiManagerLite::sanitizeHostnameBaseLower(const String& baseName) {
    // HEAP-OPTIMIERT: Stack-Puffer statt String-Operationen
    char buf[64];
    size_t copyLen = baseName.length();
    if (copyLen >= sizeof(buf)) {
        copyLen = sizeof(buf) - 1;
    }
    strncpy(buf, baseName.c_str(), copyLen);
    buf[copyLen] = '\0';
    
    size_t len = sanitizeInPlace(buf, sizeof(buf), true);  // lowercase = true
    
    if (len == 0) {
        return String("device");
    }
    return String(buf);
}

String WiFiManagerLite::sanitizeSsidBasePreserveCase(const String& baseName) {
    // HEAP-OPTIMIERT: Stack-Puffer statt String-Operationen
    char buf[64];
    size_t copyLen = baseName.length();
    if (copyLen >= sizeof(buf)) {
        copyLen = sizeof(buf) - 1;
    }
    strncpy(buf, baseName.c_str(), copyLen);
    buf[copyLen] = '\0';
    
    size_t len = sanitizeInPlace(buf, sizeof(buf), false);  // lowercase = false
    
    if (len == 0) {
        return String("Device");
    }
    return String(buf);
}

void WiFiManagerLite::ensureIdentityComputed() {
    if (_identityComputed) return;
    _identityComputed = true;

    if (_identityBaseName.length() == 0) {
        return;
    }

    const uint32_t id32 = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFFFFu);

    const String hostBase = sanitizeHostnameBaseLower(_identityBaseName);
    char hostnameBuf[64];
    snprintf(hostnameBuf, sizeof(hostnameBuf), "%s-%08lx", hostBase.c_str(), static_cast<unsigned long>(id32));
    _identityHostnameWithMac = hostnameBuf;

    String ssidBase = sanitizeSsidBasePreserveCase(_identityBaseName);
    char prefixBuf[33];
    if (ssidBase.endsWith("-")) {
        _identityApPrefix = ssidBase;
    } else {
        snprintf(prefixBuf, sizeof(prefixBuf), "%s-", ssidBase.c_str());
        _identityApPrefix = prefixBuf;
    }

    // Ensure final SSID (prefix + 8 hex) fits into 32 chars.
    const size_t maxPrefixLen = 32 - 8; // suffix length is always 8
    if (_identityApPrefix.length() > maxPrefixLen) {
        _identityApPrefix.remove(maxPrefixLen);
    }
}

// ==================== Constructor / Destructor ====================

WiFiManagerLite::WiFiManagerLite()
    : _configProvider(nullptr)
    , _eventCallback(nullptr)
    , _state(WiFiState::Disconnected)
    , _connectPhase(ConnectPhase::IDLE)
    , _apMode(false)
    , _wasConnected(false)
    , _connectStart(0)
    , _lastTry(0)
    , _tries(0)
    , _lastFailNoAp(false)
    , _dns(nullptr)
    , _dnsActive(false)
    , _mdnsActive(false)
    , _pendingRestartAt(0)
    , _pendingStartApAt(0)
    , _pendingFactoryResetRestart(false)
    , _pendingFactoryResetCallback(nullptr)
{
}

WiFiManagerLite::~WiFiManagerLite() {
    if (_dns) {
        if (_dnsActive) {
            _dns->stop();
            _dnsActive = false;
        }
        delete _dns;
        _dns = nullptr;
    }
}

// ==================== Configuration ====================

void WiFiManagerLite::setConfig(const Config& config) {
    _config = config;
}

void WiFiManagerLite::setConfigProvider(IConfigProvider* provider) {
    _configProvider = provider;
}

// setLogger() is now inline no-op in header - use WML_ENABLE_DEBUG_LOGS instead

void WiFiManagerLite::onEvent(EventCallback callback) {
    _eventCallback = callback;
}

void WiFiManagerLite::setIdentityBaseName(const String& baseName) {
    _identityBaseName = baseName;
    _identityComputed = false;
    _identityHostnameWithMac = "";
    _identityApPrefix = "";
}

String WiFiManagerLite::getIdentityBaseName() const {
    return _identityBaseName;
}

String WiFiManagerLite::getIdentityNameWithMac() const {
    if (_identityBaseName.length() == 0) return "";
    const String base = sanitizeSsidBasePreserveCase(_identityBaseName);
    char buf[64];
    snprintf(buf, sizeof(buf), "%s-%08lx", base.c_str(), static_cast<unsigned long>(ESP.getEfuseMac() & 0xFFFFFFFFu));
    return String(buf);
}

void WiFiManagerLite::getEffectiveConfig(Config& out) {
    out = _configProvider ? _configProvider->getConfig() : _config;

    if (_identityBaseName.length() > 0) {
        ensureIdentityComputed();
        // char[] Vergleich mit strcmp(), Zuweisung mit Setter
        if (out.deviceName[0] == '\0' || strcmp(out.deviceName, WML_DEFAULT_DEVICE_NAME) == 0) {
            out.setDeviceName(_identityHostnameWithMac.c_str());
        }
        if (out.ap.ssidPrefix[0] == '\0' || strcmp(out.ap.ssidPrefix, WML_AP_SSID_PREFIX) == 0) {
            out.ap.setSsidPrefix(_identityApPrefix.c_str());
        }
    }
}

Config WiFiManagerLite::getEffectiveConfig() {
    Config cfg;
    getEffectiveConfig(cfg);
    return cfg;
}

// ==================== Lifecycle ====================

void WiFiManagerLite::begin() {
    Config cfg;
    getEffectiveConfig(cfg);
    
    _apMode = false;
    _tries = 0;
    _lastTry = 0;
    _connectPhase = ConnectPhase::IDLE;
    _lastFailNoAp = false;
    _wasConnected = false;
    _state = WiFiState::Disconnected;
    _mdnsActive = false;
    
    // NOTE: mDNS is started in handleConnected() AFTER successful WiFi connection
    // Starting mDNS before connection can cause crashes when switching to AP mode!
    
    // Start connection attempt if we have credentials
    if (cfg.primary.isValid()) {
        startConnectAttempt();
    } else {
        // No credentials - go directly to AP mode
        internalStartAP();
    }
    
    WML_LOGF("Mode=%s", _apMode ? "AP" : "STA");
}

void WiFiManagerLite::loop() {
    servicePendingActions();
    Config cfg;
    getEffectiveConfig(cfg);
    
    // Process DNS requests in AP mode
    if (_apMode && _dnsActive && _dns) {
        _dns->processNextRequest();
    }
    
    // Check current connection status
    const bool connected = (WiFi.status() == WL_CONNECTED);
    
    // Handle connection state changes
    if (connected && !_wasConnected) {
        handleConnected();
    } else if (!connected && _wasConnected) {
        handleDisconnected();
    }
    
    // If connected, we're done
    if (connected) {
        // In AP+STA mode, stop AP once we have a valid IP (like BambuBeacon)
        if (_apMode && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
            WML_LOG("Connected in AP mode, stopping AP");
            internalStopAP();
        }
        _connectPhase = ConnectPhase::IDLE;
        _tries = 0;
        _lastFailNoAp = false;
        return;
    }
    
    // No credentials configured - ensure AP mode
    if (!cfg.primary.isValid()) {
        if (!_apMode) {
            internalStartAP();
        }
        return;
    }
    
    const uint32_t now = millis();
    
    // In AP mode with failed "no AP" detection, wait longer before retry (like BambuBeacon)
    if (_apMode && _lastFailNoAp && (now - _lastTry) < cfg.timing.apRetryIntervalMs) {
        return;
    }
    
    // Process ongoing connection attempt
    if (_connectPhase != ConnectPhase::IDLE) {
        AttemptResult res = processConnectAttempt();
        
        if (res == AttemptResult::Connected) {
            _tries = 0;
            _lastFailNoAp = false;
            return;
        }
        
        if (res == AttemptResult::Failed) {
            _tries++;
            _lastTry = now;
            WML_LOGF("Reconnect attempt %u failed", _tries);
            
            // Switch to AP mode if too many failures or AP not found (only if not already in AP)
            if (!_apMode && (_lastFailNoAp || _tries >= cfg.timing.maxTriesBeforeAp)) {
                WML_LOG("Switching to AP mode");
                internalStartAP();
            }
        }
        return;
    }
    
    // Determine retry interval based on mode (like BambuBeacon)
    const uint32_t retryInterval = _apMode ? cfg.timing.apRetryIntervalMs : cfg.timing.retryIntervalMs;
    
    // Wait for retry interval before starting new attempt
    if (now - _lastTry < retryInterval) {
        return;
    }
    
    // Don't retry if clients are connected to our AP (like BambuBeacon)
    if (_apMode && WiFi.softAPgetStationNum() > 0) {
        return;
    }
    
    _lastTry = now;
    
    // Check if we should switch to AP mode (only if not already in AP)
    if (!_apMode && _tries >= cfg.timing.maxTriesBeforeAp) {
        WML_LOG("Switching to AP mode");
        internalStartAP();
        return;
    }
    
    // Start new connection attempt
    WML_LOGF("Reconnect attempt %u", _tries + 1);
    startConnectAttempt();
}

// ==================== Control ====================

void WiFiManagerLite::reconnect() {
    _tries = 0;
    _lastTry = 0;
    _connectPhase = ConnectPhase::IDLE;
    _lastFailNoAp = false;
    startConnectAttempt();
}

void WiFiManagerLite::startAP(bool keepTryingStation) {
    internalStartAP();
    if (!keepTryingStation) {
        _connectPhase = ConnectPhase::IDLE;
    }
}

void WiFiManagerLite::stopAP() {
    internalStopAP();
}

void WiFiManagerLite::disconnect() {
    WiFi.disconnect(true, true);
    _wasConnected = false;
    _state = WiFiState::Disconnected;
    _connectPhase = ConnectPhase::IDLE;
    emitEvent(Event::Disconnected);
}

void WiFiManagerLite::reset() {
    WML_LOG("Reset: Forcing AP mode");
    
    // Stop any reconnection attempts FIRST
    WiFi.setAutoReconnect(false);
    
    _tries = 0;
    _lastTry = 0;
    _connectPhase = ConnectPhase::IDLE;
    _lastFailNoAp = false;
    _wasConnected = false;
    _apMode = true;  // Prevent loop() from starting new connections
    _state = WiFiState::APMode;
    
    // internalStartAP will handle the disconnect properly
    internalStartAP();
}

void WiFiManagerLite::factoryReset(std::function<void()> clearConfigCallback, bool restart) {
    WML_LOGF("Factory Reset initiated%s", restart ? " (with restart)" : "");
    
    // Stop any reconnection attempts FIRST
    WiFi.setAutoReconnect(false);
    
    // Clear in-memory config to prevent reconnection attempts
    _config = Config();
    _config.ap.setSsidPrefix("ESP-Setup-");
    
    // Reset all state BEFORE disconnecting
    _tries = 0;
    _lastTry = 0;
    _connectPhase = ConnectPhase::IDLE;
    _lastFailNoAp = false;
    _wasConnected = false;
    _apMode = true;  // Prevent reconnection attempts
    _state = WiFiState::APMode;
    
    // Call user callback to clear stored credentials
    if (clearConfigCallback) {
        clearConfigCallback();
    }
    
    if (restart) {
        // For restart: disconnect and schedule non-blocking restart
        WiFi.disconnect(true, true);
        WML_LOG("Restarting...");
        // Schedule restart after 300ms (non-blocking)
        _pendingRestartAt = millis() + 300;
    } else {
        // Start AP mode without restart - schedule non-blocking AP start
        WiFi.disconnect(true, true);
        _pendingStartApAt = millis() + 150;
        WML_LOG("Factory Reset complete - AP mode starting");
    }
}

void WiFiManagerLite::scanNetworks(bool async, bool showHidden) {
    WiFi.scanDelete();
    WiFi.scanNetworks(async, showHidden);
}

// ==================== Status ====================

bool WiFiManagerLite::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

bool WiFiManagerLite::isAPMode() const {
    return _apMode;
}

WiFiState WiFiManagerLite::getState() const {
    return _state;
}

IPAddress WiFiManagerLite::getStationIP() const {
    return WiFi.localIP();
}

IPAddress WiFiManagerLite::getAPIP() const {
    return WiFi.softAPIP();
}

String WiFiManagerLite::getSSID() const {
    return WiFi.SSID();
}

String WiFiManagerLite::getBSSID() const {
    return WiFi.BSSIDstr();
}

int8_t WiFiManagerLite::getRSSI() const {
    return WiFi.RSSI();
}

uint8_t WiFiManagerLite::getAPClientCount() const {
    return WiFi.softAPgetStationNum();
}

uint8_t WiFiManagerLite::getRetryCount() const {
    return _tries;
}

bool WiFiManagerLite::setHostname(const char* hostname, uint16_t httpPort) {
    if (!hostname || strlen(hostname) == 0) {
        return false;
    }
    
    // Stop existing mDNS if running
    MDNS.end();
    
    // Start mDNS with new hostname
    if (MDNS.begin(hostname)) {
        MDNS.addService("http", "tcp", httpPort);
        WML_LOGF("mDNS hostname set: %s.local (port %d)", hostname, httpPort);
        return true;
    }
    
    WML_ERROR("Failed to start mDNS");
    return false;
}

// ==================== Internal Methods ====================

bool WiFiManagerLite::parseBssid(const char* str, uint8_t out[6]) {
    if (!str || !*str) return false;
    
    int vals[6] = {};
    if (sscanf(str, "%x:%x:%x:%x:%x:%x",
               &vals[0], &vals[1], &vals[2], &vals[3], &vals[4], &vals[5]) != 6) {
        return false;
    }
    
    for (int i = 0; i < 6; i++) {
        if (vals[i] < 0 || vals[i] > 255) return false;
        out[i] = static_cast<uint8_t>(vals[i]);
    }
    return true;
}

void WiFiManagerLite::startConnectAttempt() {
    Config cfg;
    getEffectiveConfig(cfg);
    
    if (!cfg.primary.isValid()) {
        _connectPhase = ConnectPhase::IDLE;
        return;
    }
    
    _lastFailNoAp = false;
    _state = _apMode ? WiFiState::APModeConnecting : WiFiState::Connecting;
    
    // Set WiFi mode (AP_STA if in AP mode, STA otherwise) - like BambuBeacon
    WiFi.mode(_apMode ? WIFI_AP_STA : WIFI_STA);
    WiFi.setHostname(cfg.deviceName);  // char[] direkt verwendbar
    WiFi.setSleep(false);
    
#ifdef ARDUINO_ARCH_ESP32
#ifdef WML_WIFI_FIX
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
#else
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
#endif
#endif
    
    // Configure static IP if valid
    if (cfg.staticIP.isValid()) {
        IPAddress ip, gw, sn, dns;
        // char[] direkt verwendbar mit fromString()
        if (ip.fromString(cfg.staticIP.ip) && ip != IPAddress(0, 0, 0, 0) &&
            gw.fromString(cfg.staticIP.gateway) && gw != IPAddress(0, 0, 0, 0) &&
            sn.fromString(cfg.staticIP.subnet) && sn != IPAddress(0, 0, 0, 0) &&
            dns.fromString(cfg.staticIP.dns) && dns != IPAddress(0, 0, 0, 0)) {
            WiFi.config(ip, gw, sn, dns);
        }
    }
    
    // Connect with optional BSSID lock (char[] direkt verwendbar)
    uint8_t bssid[6] = {};
    if (cfg.primary.bssidLock && parseBssid(cfg.primary.bssid, bssid)) {
        WiFi.begin(cfg.primary.ssid, cfg.primary.password, 0, bssid, true);
    } else {
        WiFi.begin(cfg.primary.ssid, cfg.primary.password);
    }
    
    _connectPhase = ConnectPhase::PRIMARY;
    _connectStart = millis();
    
    emitEvent(Event::Connecting, cfg.primary.ssid);
}

WiFiManagerLite::AttemptResult WiFiManagerLite::processConnectAttempt() {
    if (_connectPhase == ConnectPhase::IDLE) {
        return AttemptResult::InProgress;
    }
    
    // Check for successful connection
    if (WiFi.status() == WL_CONNECTED) {
        _connectPhase = ConnectPhase::IDLE;
        return AttemptResult::Connected;
    }
    
    Config cfg;
    getEffectiveConfig(cfg);
    const uint32_t now = millis();
    const wl_status_t st = WiFi.status();
    
    // Fast fail if AP not found or connection explicitly failed
    if ((st == WL_NO_SSID_AVAIL || st == WL_CONNECT_FAILED) &&
        (now - _connectStart) >= cfg.timing.fastFailNoApMs) {
        _lastFailNoAp = true;
        _connectStart = 0;
    }
    
    // Handle timeout or fast-fail - try secondary network
    if (_connectStart == 0) {
        if (_connectPhase == ConnectPhase::PRIMARY && cfg.secondary.isValid()) {
            WiFi.begin(cfg.secondary.ssid, cfg.secondary.password);  // char[] direkt
            _connectPhase = ConnectPhase::SECONDARY;
            _connectStart = now;
            emitEvent(Event::Connecting, cfg.secondary.ssid);
            return AttemptResult::InProgress;
        }
        
        _connectPhase = ConnectPhase::IDLE;
        emitEvent(Event::ConnectionFailed);
        return AttemptResult::Failed;
    }
    
    // Check for timeout
    if (now - _connectStart < cfg.timing.connectTimeoutMs) {
        return AttemptResult::InProgress;
    }
    
    // Primary timed out - try secondary
    if (_connectPhase == ConnectPhase::PRIMARY && cfg.secondary.isValid()) {
        WiFi.begin(cfg.secondary.ssid, cfg.secondary.password);  // char[] direkt
        _connectPhase = ConnectPhase::SECONDARY;
        _connectStart = now;
        emitEvent(Event::Connecting, cfg.secondary.ssid);
        return AttemptResult::InProgress;
    }
    
    // All attempts exhausted
    _connectPhase = ConnectPhase::IDLE;
    emitEvent(Event::ConnectionFailed);
    return AttemptResult::Failed;
}

void WiFiManagerLite::handleConnected() {
    _wasConnected = true;
    _state = WiFiState::Connected;
    
    if (!_mdnsActive) {
        Config cfg;
        getEffectiveConfig(cfg);
        // char[] direkt verwendbar, strlen() statt .length()
        if (cfg.enableMDNS && cfg.deviceName[0] != '\0') {
            if (MDNS.begin(cfg.deviceName)) {
                MDNS.addService("http", "tcp", cfg.httpPort);
                _mdnsActive = true;
                WML_LOGF("mDNS started: %s.local", cfg.deviceName);
            }
        }
    }
    
    // HEAP-OPTIMIERT: WiFi-Strings nur einmal abrufen (vorher: 4 String-Allokationen, jetzt: 1)
    char ssidBuf[33];  // Max SSID = 32 + null
    char ipBuf[16];    // xxx.xxx.xxx.xxx + null
    
    // SSID einmal abrufen
    String tmpSsid = WiFi.SSID();
    strncpy(ssidBuf, tmpSsid.c_str(), sizeof(ssidBuf) - 1);
    ssidBuf[sizeof(ssidBuf) - 1] = '\0';
    
    // IP direkt formatieren (kein toString())
    IPAddress ip = WiFi.localIP();
    snprintf(ipBuf, sizeof(ipBuf), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    
    char info[96];
    snprintf(info, sizeof(info), "%s - %s", ssidBuf, ipBuf);
    WML_LOGF("Connected to %s (IP: %s)", ssidBuf, ipBuf);
    emitEvent(Event::Connected, info);
}

void WiFiManagerLite::handleDisconnected() {
    _wasConnected = false;
    if (!_apMode) {
        _state = WiFiState::Disconnected;
    }
    WML_LOG("Disconnected");
    emitEvent(Event::Disconnected);
}

void WiFiManagerLite::internalStartAP() {
    // Prevent re-entry if already in AP mode and DNS is active
    if (_apMode && _dnsActive && _dns) {
        return;
    }
    
    Serial.println("[AP] Step 0: Entry");
    Serial.flush();
    
    // Set state FIRST to prevent any reconnection attempts during AP setup
    _apMode = true;
    _connectPhase = ConnectPhase::IDLE;
    _connectStart = 0;
    _state = WiFiState::APMode;
    _pendingStartApAt = 0;
    _mdnsActive = false;
    
    Serial.println("[AP] Step 1: State set");
    Serial.flush();
    
    // Get config (apply identity-derived defaults via getEffectiveConfig())
    Serial.println("[AP] Step 2: Getting config...");
    Serial.flush();

    Config cfg;
    getEffectiveConfig(cfg);

    Serial.println("[AP] Step 3: Config loaded");
    Serial.flush();
    
    // Validate AP IP
    if (cfg.ap.ip == IPAddress(0, 0, 0, 0)) {
        cfg.ap.ip = IPAddress(192, 168, 4, 1);
        cfg.ap.gateway = IPAddress(192, 168, 4, 1);
        cfg.ap.subnet = IPAddress(255, 255, 255, 0);
    }
    
    Serial.println("[AP] Step 4: Disconnecting WiFi...");
    Serial.flush();
    
    // Simple approach like BambuBeacon
    WiFi.disconnect(true, true);
    delay(150);
    
    Serial.println("[AP] Step 5: Setting mode...");
    Serial.flush();
    
    WiFi.persistent(false);
    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);
    
    // HEAP-OPTIMIERT: IP direkt formatieren ohne toString() (vorher: 3 String-Allokationen)
    Serial.println("[AP] Step 6: Checking AP config values...");
    Serial.printf("[AP] ap.ip = %d.%d.%d.%d\n", cfg.ap.ip[0], cfg.ap.ip[1], cfg.ap.ip[2], cfg.ap.ip[3]);
    Serial.printf("[AP] ap.gateway = %d.%d.%d.%d\n", cfg.ap.gateway[0], cfg.ap.gateway[1], cfg.ap.gateway[2], cfg.ap.gateway[3]);
    Serial.printf("[AP] ap.subnet = %d.%d.%d.%d\n", cfg.ap.subnet[0], cfg.ap.subnet[1], cfg.ap.subnet[2], cfg.ap.subnet[3]);
    Serial.flush();
    
    // Use hardcoded values to bypass potential config corruption
    IPAddress apIP(192, 168, 4, 1);
    IPAddress apGateway(192, 168, 4, 1);
    IPAddress apSubnet(255, 255, 255, 0);
    
    Serial.println("[AP] Step 6b: Configuring softAP with hardcoded IPs...");
    Serial.flush();
    
    WiFi.softAPConfig(apIP, apGateway, apSubnet);
    
    Serial.println("[AP] Step 7: Generating AP name...");
    Serial.flush();
    
    // Generate AP name (char[] direkt verwendbar)
    const char* prefix = WML_AP_SSID_PREFIX;
    size_t prefixLen = strlen(cfg.ap.ssidPrefix);
    if (prefixLen > 0 && prefixLen < 32) {
        prefix = cfg.ap.ssidPrefix;  // char[] direkt
    }
    char apName[64];
#if WML_AP_APPEND_MAC
    snprintf(apName, sizeof(apName), "%s%08lx", prefix, (unsigned long)(ESP.getEfuseMac() & 0xFFFFFFFF));
#else
    // MAC-Suffix deaktiviert: Prefix pur verwenden, abschliessendes '-' entfernen
    snprintf(apName, sizeof(apName), "%s", prefix);
    size_t apNameLen = strlen(apName);
    if (apNameLen > 0 && apName[apNameLen - 1] == '-') {
        apName[apNameLen - 1] = '\0';
    }
#endif
    
    Serial.printf("[AP] Step 8: Starting softAP: %s\n", apName);
    Serial.flush();
    
    // Start AP (char[] direkt verwendbar)
    bool apStarted = false;
    size_t pwLen = strlen(cfg.ap.password);
    if (pwLen > 0 && pwLen >= 8) {
        apStarted = WiFi.softAP(apName,
                                cfg.ap.password,  // char[] direkt
                                WML_AP_CHANNEL,
                                WML_AP_HIDDEN,
                                WML_AP_MAX_CONNECTIONS);
    } else {
        apStarted = WiFi.softAP(apName,
                                nullptr,
                                WML_AP_CHANNEL,
                                WML_AP_HIDDEN,
                                WML_AP_MAX_CONNECTIONS);
    }
    
    Serial.printf("[AP] Step 9: softAP result: %d\n", apStarted);
    Serial.flush();
    
    if (!apStarted) {
        WML_ERROR("Failed to start AP");
        _apMode = false;
        _state = WiFiState::Disconnected;
        return;
    }
    
#ifdef ARDUINO_ARCH_ESP32
#ifdef WML_WIFI_FIX
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
#else
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
#endif
#endif
    
    // Start DNS server for captive portal
    if (!_dnsActive) {
        if (!_dns) {
            _dns = new (std::nothrow) DNSServer();
        }
        if (_dns) {
            _dnsActive = _dns->start(53, "*", cfg.ap.ip);
        }
    }
    
    // NOTE: Do not pre-scan here. Scans are started on-demand by the portal via /wml/netlist
    // when a client actually opens the setup page. This reduces WiFi stack load right after
    // switching to AP/AP_STA mode.
    
    // HEAP-OPTIMIERT: IP direkt formatieren (vorher: 1 String-Allokation)
    char apIpBuf[16];
    snprintf(apIpBuf, sizeof(apIpBuf), "%d.%d.%d.%d", cfg.ap.ip[0], cfg.ap.ip[1], cfg.ap.ip[2], cfg.ap.ip[3]);
    WML_LOGF("AP started: %s (IP: %s)", apName, apIpBuf);
    emitEvent(Event::APStarted, apName);
}

void WiFiManagerLite::internalStopAP() {
    // Stop captive portal DNS server
    if (_dnsActive && _dns) {
        _dns->stop();
        _dnsActive = false;
    }
    
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    _apMode = false;
    
    Config cfg;
    getEffectiveConfig(cfg);
    // char[] direkt verwendbar
    if (cfg.enableMDNS && cfg.deviceName[0] != '\0') {
        if (MDNS.begin(cfg.deviceName)) {
            MDNS.addService("http", "tcp", cfg.httpPort);
            _mdnsActive = true;
            WML_LOGF("mDNS restarted: %s.local", cfg.deviceName);
        }
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        _state = WiFiState::Connected;
    } else {
        _state = WiFiState::Disconnected;
    }
    
    WML_LOG("AP stopped");
    emitEvent(Event::APStopped);
}

void WiFiManagerLite::emitEvent(Event event, const String& info) {
    if (_eventCallback) {
        _eventCallback(event, info);
    }
}

void WiFiManagerLite::emitEvent(Event event, const char* info) {
    if (_eventCallback) {
        _eventCallback(event, info ? String(info) : "");
    }
}

// log() and logf() removed - using WML_LOG/WML_LOGF macros instead
// See WMLDebugLog.h for compile-time debug control

void WiFiManagerLite::servicePendingActions() {
    const uint32_t now = millis();
    
    // Handle pending restart
    if (_pendingRestartAt > 0 && now >= _pendingRestartAt) {
        _pendingRestartAt = 0;
        ESP.restart();
        return; // Won't reach here
    }
    
    // Handle pending AP start (from factoryReset)
    if (_pendingStartApAt > 0 && now >= _pendingStartApAt) {
        _pendingStartApAt = 0;
        internalStartAP();
    }
}

} // namespace WML
