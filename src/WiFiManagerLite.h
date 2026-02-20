/**
 * @file WiFiManagerLite.h
 * @brief Lightweight WiFi connection manager for ESP32
 * 
 * Features:
 * - Automatic WiFi connection with retry logic
 * - Fallback Access Point mode
 * - Dual SSID support (primary + secondary)
 * - Static IP configuration
 * - BSSID lock for specific AP connection
 * - mDNS support
 * - Captive portal DNS
 * - Non-blocking operation
 * 
 * @author Extracted from BambuBeacon project
 * @version 1.0.0
 */

#pragma once

#include <Arduino.h>
#include "WMLConfig.h"
#include "WMLDebugLog.h"

// Forward declaration - DNSServer created lazily to avoid early initialization issues
class DNSServer;

namespace WML {

/**
 * @brief WiFi connection state
 */
enum class WiFiState : uint8_t {
    Disconnected,       ///< Not connected, not attempting
    Connecting,         ///< Connection attempt in progress
    Connected,          ///< Successfully connected to WiFi
    APMode,             ///< Running as Access Point
    APModeConnecting    ///< AP mode active, but also trying to connect
};

/**
 * @brief Event types for callbacks
 */
enum class Event : uint8_t {
    Connecting,         ///< Starting connection attempt
    Connected,          ///< Successfully connected
    Disconnected,       ///< Lost connection
    ConnectionFailed,   ///< Connection attempt failed
    APStarted,          ///< Access Point started
    APStopped,          ///< Access Point stopped
    APClientConnected,  ///< Client connected to AP
    APClientDisconnected ///< Client disconnected from AP
};

/**
 * @brief Event callback function type
 */
using EventCallback = std::function<void(Event event, const String& info)>;

/**
 * @brief Lightweight WiFi connection manager
 */
class WiFiManagerLite {
public:
    /**
     * @brief Constructor
     */
    WiFiManagerLite();
    
    /**
     * @brief Destructor
     */
    ~WiFiManagerLite();
    
    // ==================== Configuration ====================
    
    /**
     * @brief Set configuration directly
     * @param config Configuration structure
     */
    void setConfig(const Config& config);
    
    /**
     * @brief Set configuration provider for dynamic configuration
     * @param provider Pointer to configuration provider (not owned)
     */
    void setConfigProvider(IConfigProvider* provider);
    
    /**
     * @brief Set logger for debug output (DEPRECATED)
     * @param logger Ignored - use WML_ENABLE_DEBUG_LOGS instead
     * @deprecated Use compile-time WML_ENABLE_DEBUG_LOGS=0/1 in WMLBuildConfig.h
     */
    void setLogger(void* logger) { (void)logger; /* no-op, use WML_ENABLE_DEBUG_LOGS */ }
    
    /**
     * @brief Set event callback
     * @param callback Function to call on events
     */
    void onEvent(EventCallback callback);

    // ==================== Identity (One-place Naming) ====================
    /**
     * @brief Set a base name used to derive:
     * - AP SSID: "{base}-{mac8}" (overrides default WML_AP_SSID_PREFIX unless user configured cfg.ap.ssidPrefix)
     * - mDNS hostname: "{base}-{mac8}.local" (overrides default cfg.deviceName unless user configured cfg.deviceName)
     *
     * The Captive Portal can use the base name (without MAC) for UI branding.
     *
     * @param baseName Human-friendly base name (e.g. "Homewind")
     */
    void setIdentityBaseName(const String& baseName);

    /**
     * @brief Get the configured identity base name (UI branding name, without MAC).
     */
    String getIdentityBaseName() const;

    /**
     * @brief Get derived identity name with MAC suffix (used for AP SSID / mDNS hostname).
     * @return String like "Homewind-da4eb580" (hostname-safe may be lowercased)
     */
    String getIdentityNameWithMac() const;
    
    // ==================== Lifecycle ====================
    
    /**
     * @brief Initialize and start WiFi manager
     * 
     * Call this in setup() after setting configuration
     */
    void begin();
    
    /**
     * @brief Process WiFi manager tasks
     * 
     * Call this in loop() - must be called regularly
     */
    void loop();
    
    // ==================== Control ====================
    
    /**
     * @brief Force reconnection attempt
     */
    void reconnect();
    
    /**
     * @brief Start Access Point mode
     * @param keepTryingStation Keep attempting station connection
     */
    void startAP(bool keepTryingStation = true);
    
    /**
     * @brief Stop Access Point mode
     */
    void stopAP();
    
    /**
     * @brief Disconnect from WiFi
     */
    void disconnect();
    
    /**
     * @brief Reset WiFi manager and start AP mode
     * 
     * Clears current connection and forces AP mode.
     * Does NOT clear stored credentials (use resetConfig callback for that).
     */
    void reset();
    
    /**
     * @brief Factory reset - clear all and restart in AP mode
     * @param clearConfigCallback Optional callback to clear stored credentials
     * @param restart If true (default), restarts ESP after reset
     * 
     * Calls the callback (if set) to clear stored config,
     * then disconnects and starts AP mode.
     * If restart is true, the ESP will reboot for a clean state.
     */
    void factoryReset(std::function<void()> clearConfigCallback = nullptr, bool restart = true);
    
    /**
     * @brief Trigger a WiFi network scan
     * @param async If true, scan runs in background
     * @param showHidden Include hidden networks
     */
    void scanNetworks(bool async = true, bool showHidden = true);
    
    // ==================== Status ====================
    
    /**
     * @brief Check if connected to WiFi (station mode)
     * @return true if connected
     */
    bool isConnected() const;
    
    /**
     * @brief Check if Access Point is active
     * @return true if AP mode
     */
    bool isAPMode() const;
    
    /**
     * @brief Get current WiFi state
     * @return Current state
     */
    WiFiState getState() const;
    
    /**
     * @brief Get station IP address
     * @return IP address (0.0.0.0 if not connected)
     */
    IPAddress getStationIP() const;
    
    /**
     * @brief Get Access Point IP address
     * @return AP IP address
     */
    IPAddress getAPIP() const;
    
    /**
     * @brief Get current SSID
     * @return SSID string (empty if not connected)
     */
    String getSSID() const;
    
    /**
     * @brief Get current BSSID
     * @return BSSID string
     */
    String getBSSID() const;
    
    /**
     * @brief Get WiFi signal strength
     * @return RSSI in dBm
     */
    int8_t getRSSI() const;
    
    /**
     * @brief Get number of clients connected to AP
     * @return Client count
     */
    uint8_t getAPClientCount() const;
    
    /**
     * @brief Set/update mDNS hostname
     * @param hostname The hostname (without .local suffix)
     * @param httpPort HTTP port for service advertisement (default: 80)
     * @return true if mDNS started successfully
     * 
     * Can be called anytime to change the hostname.
     * Example: setHostname("mydevice") → accessible as mydevice.local
     */
    bool setHostname(const char* hostname, uint16_t httpPort = 80);
    
    /**
     * @brief Get connection attempt count
     * @return Number of failed attempts since last success
     */
    uint8_t getRetryCount() const;
    
private:
    // ==================== Internal Types ====================
    
    enum class ConnectPhase : uint8_t {
        IDLE,
        PRIMARY,
        SECONDARY
    };
    
    enum class AttemptResult : uint8_t {
        InProgress,
        Connected,
        Failed
    };
    
    // ==================== Internal Methods ====================
    
    void startConnectAttempt();
    AttemptResult processConnectAttempt();
    void handleConnected();
    void handleDisconnected();
    void internalStartAP();
    void internalStopAP();
    void emitEvent(Event event, const String& info = "");
    void emitEvent(Event event, const char* info);
    void getEffectiveConfig(Config& out);
    Config getEffectiveConfig();
    static bool parseBssid(const char* str, uint8_t out[6]);
    void ensureIdentityComputed();
    static String sanitizeHostnameBaseLower(const String& baseName);
    static String sanitizeSsidBasePreserveCase(const String& baseName);
    
    // ==================== Member Variables ====================
    
    // Configuration
    Config _config;
    IConfigProvider* _configProvider;
    EventCallback _eventCallback;

    // Identity (optional)
    String _identityBaseName;
    String _identityHostnameWithMac; // hostname-safe (lowercase)
    String _identityApPrefix;        // SSID prefix (preserve case, includes trailing '-')
    bool _identityComputed = false;
    
    // State
    WiFiState _state;
    ConnectPhase _connectPhase;
    bool _apMode;
    bool _wasConnected;
    
    // Timing
    uint32_t _connectStart;
    uint32_t _lastTry;
    uint8_t _tries;
    bool _lastFailNoAp;
    
    // DNS Server for captive portal (lazy initialization to avoid FreeRTOS issues)
    DNSServer* _dns;
    bool _dnsActive;
    
    // mDNS state tracking (to safely call MDNS.end())
    bool _mdnsActive;
    
    // Pending actions (non-blocking state machine)
    uint32_t _pendingRestartAt;
    uint32_t _pendingStartApAt;
    bool _pendingFactoryResetRestart;
    std::function<void()> _pendingFactoryResetCallback;
    
    void servicePendingActions();
};

} // namespace WML
