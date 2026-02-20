/**
 * @file WMLConfig.h
 * @brief Configuration structures for WiFiManagerLite
 * 
 * HEAP-OPTIMIERT: Verwendet char[] statt Arduino String für alle Konfigurationswerte.
 * Dies eliminiert Heap-Fragmentierung durch:
 * - Keine dynamischen String-Allokationen
 * - Fester Speicherverbrauch (~464 Bytes für Config)
 * - Keine Heap-Operationen bei Config-Kopien
 * 
 * BREAKING CHANGE ab v3.0: String-basierte API wurde durch char[]-basierte ersetzt.
 * 
 * @author Extracted from BambuBeacon project
 * @version 3.0.0
 */

#pragma once

#include <Arduino.h>
#include "WMLBuildConfig.h"

namespace WML {

// ==================== Heap-Optimierte Konfigurationsstrukturen ====================

/**
 * @brief WiFi credentials for a single network
 * 
 * Speicherverbrauch: ~120 Bytes (fest, keine Heap-Allokation)
 * 
 * WiFi-Spezifikationen:
 * - SSID: max 32 Zeichen
 * - WPA2 Passwort: max 64 Zeichen
 * - BSSID: 17 Zeichen ("XX:XX:XX:XX:XX:XX")
 */
struct WiFiCredentials {
    char ssid[33];          // Max SSID = 32 + null
    char password[65];      // Max WPA2 = 64 + null
    char bssid[18];         // "XX:XX:XX:XX:XX:XX" + null
    bool bssidLock;         // If true and bssid is set, connect only to that specific AP
    
    WiFiCredentials() : bssidLock(false) {
        ssid[0] = '\0';
        password[0] = '\0';
        bssid[0] = '\0';
    }
    
    bool isValid() const {
        return ssid[0] != '\0';
    }
    
    // Sichere Setter-Methoden
    void setSsid(const char* s) {
        if (s) {
            strncpy(ssid, s, sizeof(ssid) - 1);
            ssid[sizeof(ssid) - 1] = '\0';
        } else {
            ssid[0] = '\0';
        }
    }
    
    void setPassword(const char* p) {
        if (p) {
            strncpy(password, p, sizeof(password) - 1);
            password[sizeof(password) - 1] = '\0';
        } else {
            password[0] = '\0';
        }
    }
    
    void setBssid(const char* b) {
        if (b) {
            strncpy(bssid, b, sizeof(bssid) - 1);
            bssid[sizeof(bssid) - 1] = '\0';
        } else {
            bssid[0] = '\0';
        }
    }
    
    // Kompatibilitäts-Setter für Arduino String
    void setSsid(const String& s) { setSsid(s.c_str()); }
    void setPassword(const String& p) { setPassword(p.c_str()); }
    void setBssid(const String& b) { setBssid(b.c_str()); }
};

/**
 * @brief Static IP configuration
 * 
 * Speicherverbrauch: 64 Bytes (fest)
 */
struct StaticIPConfig {
    char ip[16];            // "xxx.xxx.xxx.xxx" + null
    char gateway[16];
    char subnet[16];
    char dns[16];
    
    StaticIPConfig() {
        ip[0] = '\0';
        gateway[0] = '\0';
        subnet[0] = '\0';
        dns[0] = '\0';
    }
    
    bool isValid() const {
        return ip[0] != '\0' && gateway[0] != '\0' && 
               subnet[0] != '\0' && dns[0] != '\0';
    }
    
    // Sichere Setter
    void setIp(const char* s) {
        if (s) { strncpy(ip, s, sizeof(ip) - 1); ip[sizeof(ip) - 1] = '\0'; }
        else { ip[0] = '\0'; }
    }
    void setGateway(const char* s) {
        if (s) { strncpy(gateway, s, sizeof(gateway) - 1); gateway[sizeof(gateway) - 1] = '\0'; }
        else { gateway[0] = '\0'; }
    }
    void setSubnet(const char* s) {
        if (s) { strncpy(subnet, s, sizeof(subnet) - 1); subnet[sizeof(subnet) - 1] = '\0'; }
        else { subnet[0] = '\0'; }
    }
    void setDns(const char* s) {
        if (s) { strncpy(dns, s, sizeof(dns) - 1); dns[sizeof(dns) - 1] = '\0'; }
        else { dns[0] = '\0'; }
    }
    
    // Kompatibilitäts-Setter
    void setIp(const String& s) { setIp(s.c_str()); }
    void setGateway(const String& s) { setGateway(s.c_str()); }
    void setSubnet(const String& s) { setSubnet(s.c_str()); }
    void setDns(const String& s) { setDns(s.c_str()); }
};

/**
 * @brief Access Point configuration
 * 
 * Speicherverbrauch: ~104 Bytes (fest)
 */
struct APConfig {
    char ssidPrefix[25];    // Max 24 (32 - 8 hex suffix) + null
    char password[65];      // Empty = open network
    IPAddress ip;
    IPAddress gateway;
    IPAddress subnet;
    
    APConfig() 
        : ip(192, 168, 4, 1)
        , gateway(192, 168, 4, 1)
        , subnet(255, 255, 255, 0) 
    {
        strncpy(ssidPrefix, WML_AP_SSID_PREFIX, sizeof(ssidPrefix) - 1);
        ssidPrefix[sizeof(ssidPrefix) - 1] = '\0';
        
        const char* defaultPw = WML_AP_PASSWORD;
        if (defaultPw && defaultPw[0]) {
            strncpy(password, defaultPw, sizeof(password) - 1);
            password[sizeof(password) - 1] = '\0';
        } else {
            password[0] = '\0';
        }
    }
    
    // Sichere Setter
    void setSsidPrefix(const char* s) {
        if (s) { strncpy(ssidPrefix, s, sizeof(ssidPrefix) - 1); ssidPrefix[sizeof(ssidPrefix) - 1] = '\0'; }
        else { ssidPrefix[0] = '\0'; }
    }
    void setPassword(const char* p) {
        if (p) { strncpy(password, p, sizeof(password) - 1); password[sizeof(password) - 1] = '\0'; }
        else { password[0] = '\0'; }
    }
    
    // Kompatibilitäts-Setter
    void setSsidPrefix(const String& s) { setSsidPrefix(s.c_str()); }
    void setPassword(const String& p) { setPassword(p.c_str()); }
};

/**
 * @brief Timing configuration for connection attempts
 * 
 * Speicherverbrauch: ~20 Bytes (keine Strings)
 */
struct TimingConfig {
    uint32_t connectTimeoutMs;      // Max time to wait for connection
    uint32_t fastFailNoApMs;        // Time before fast-failing if AP not found
    uint32_t retryIntervalMs;       // Retry interval in STA mode
    uint32_t apRetryIntervalMs;     // Retry interval when in AP mode
    uint8_t  maxTriesBeforeAp;      // Number of failed attempts before switching to AP
    
    TimingConfig()
        : connectTimeoutMs(WML_CONNECT_TIMEOUT_MS)
        , fastFailNoApMs(2500)
        , retryIntervalMs(WML_RETRY_INTERVAL_MS)
        , apRetryIntervalMs(WML_AP_AUTO_TIMEOUT_MS > 0 ? WML_AP_AUTO_TIMEOUT_MS : 300000)
        , maxTriesBeforeAp(WML_MAX_RETRIES_BEFORE_AP) {}
};

/**
 * @brief Complete WiFi Manager configuration
 * 
 * Gesamter Speicherverbrauch: ~464 Bytes (fest, keine Heap-Allokation)
 * 
 * Heap-Allokationen pro Config-Kopie: 0 (vorher: 15 String-Kopien)
 */
struct Config {
    char deviceName[33];             // Hostname for mDNS (max 32 + null)
    WiFiCredentials primary;         // Primary WiFi credentials (~120 Bytes)
    WiFiCredentials secondary;       // Secondary/fallback WiFi credentials (~120 Bytes)
    StaticIPConfig staticIP;         // Optional static IP configuration (64 Bytes)
    APConfig ap;                     // Access Point configuration (~104 Bytes)
    TimingConfig timing;             // Connection timing parameters (~20 Bytes)
    bool enableMDNS;                 // Enable mDNS responder
    uint16_t httpPort;               // Port for mDNS HTTP service announcement
    
    Config() 
        : enableMDNS(WML_ENABLE_MDNS != 0)
        , httpPort(WML_HTTP_PORT) 
    {
        strncpy(deviceName, WML_DEFAULT_DEVICE_NAME, sizeof(deviceName) - 1);
        deviceName[sizeof(deviceName) - 1] = '\0';
    }
    
    // Sichere Setter
    void setDeviceName(const char* s) {
        if (s) { strncpy(deviceName, s, sizeof(deviceName) - 1); deviceName[sizeof(deviceName) - 1] = '\0'; }
        else { deviceName[0] = '\0'; }
    }
    
    // Kompatibilitäts-Setter
    void setDeviceName(const String& s) { setDeviceName(s.c_str()); }
};

/**
 * @brief Abstract interface for providing configuration
 * 
 * Implement this interface to provide dynamic configuration
 * from your settings backend (e.g., Preferences, EEPROM)
 */
class IConfigProvider {
public:
    virtual ~IConfigProvider() = default;
    
    /**
     * @brief Get the current configuration
     * @return Current Config structure
     * 
     * Hinweis: Rückgabe per Wert ist jetzt günstig (~464 Bytes memcpy, keine Heap-Ops)
     */
    virtual Config getConfig() = 0;
    
    /**
     * @brief Check if configuration has changed since last call
     * @return true if configuration changed
     */
    virtual bool hasChanged() { return false; }
};

/**
 * @brief Simple static configuration provider
 * 
 * Use this when your configuration doesn't change at runtime
 */
class StaticConfigProvider : public IConfigProvider {
public:
    explicit StaticConfigProvider(const Config& config) : _config(config) {}
    
    Config getConfig() override { return _config; }
    
    void setConfig(const Config& config) { 
        _config = config; 
        _changed = true;
    }
    
    bool hasChanged() override {
        bool changed = _changed;
        _changed = false;
        return changed;
    }
    
private:
    Config _config;
    bool _changed = false;
};

} // namespace WML
