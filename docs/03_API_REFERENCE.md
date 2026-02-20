# API Reference

Complete reference for all WiFiManagerLite classes and methods.

---

## Namespace

All classes are in the `WML` namespace:

```cpp
WML::WiFiManagerLite
WML::CaptivePortal
WML::Config
WML::Storage
WML::StorageProvider
```

Convenience aliases are provided:
```cpp
WMLConfig        // = WML::Config
WMLCredentials   // = WML::WiFiCredentials
WMLStaticIP      // = WML::StaticIPConfig
WMLAPConfig      // = WML::APConfig
WMLTiming        // = WML::TimingConfig
WMLCaptivePortal // = WML::CaptivePortal (if enabled)
WMLStorage       // = WML::Storage (if enabled)
```

---

## WML::Config

Configuration structure for WiFi settings.

> **v3.0 Breaking Change:** All `String` fields are now `char[]` arrays for zero heap fragmentation.  
> See [Migration Guide](MIGRATION_V2_TO_V3.md) for upgrade instructions.

### Fields

```cpp
struct Config {
    char deviceName[33];            // Device name (mDNS hostname)
    WiFiCredentials primary;        // Primary WiFi credentials
    WiFiCredentials secondary;      // Fallback WiFi credentials
    StaticIPConfig staticIP;        // Static IP configuration
    APConfig ap;                    // Access Point configuration
    TimingConfig timing;            // Connection timing settings
    bool enableMDNS = true;         // Enable mDNS
    uint16_t httpPort = 80;         // HTTP server port
    
    // Setter (safe copy with null-termination)
    void setDeviceName(const char* s);
    void setDeviceName(const String& s);  // Convenience overload
};
```

### WiFiCredentials

```cpp
struct WiFiCredentials {
    char ssid[33];          // Max 32 chars + null
    char password[65];      // Max 64 chars + null (WPA2)
    char bssid[18];         // "XX:XX:XX:XX:XX:XX" + null
    bool bssidLock = false; // Enable BSSID lock
    
    // Validity check
    bool isValid() const;   // Returns ssid[0] != '\0'
    
    // Safe setters (strncpy with null-termination)
    void setSsid(const char* s);
    void setSsid(const String& s);
    void setPassword(const char* p);
    void setPassword(const String& p);
    void setBssid(const char* b);
    void setBssid(const String& b);
};
```

**Usage:**
```cpp
// v3.0 - Use setter methods
config.primary.setSsid("MyNetwork");
config.primary.setPassword("secret123");

// Direct access for reading (char* compatible)
Serial.printf("SSID: %s\n", config.primary.ssid);

// Check validity
if (config.primary.isValid()) { /* has SSID */ }
```

### StaticIPConfig

```cpp
struct StaticIPConfig {
    char ip[16];        // "xxx.xxx.xxx.xxx" + null
    char gateway[16];
    char subnet[16];
    char dns[16];
    
    bool isValid() const;  // All fields non-empty
    
    void setIp(const char* s);
    void setIp(const String& s);
    void setGateway(const char* s);
    void setGateway(const String& s);
    void setSubnet(const char* s);
    void setSubnet(const String& s);
    void setDns(const char* s);
    void setDns(const String& s);
};
```

### APConfig

```cpp
struct APConfig {
    char ssidPrefix[25];    // Max 24 chars (32 - 8 hex suffix)
    char password[65];      // Empty = open network
    IPAddress ip;           // Default: 192.168.4.1
    IPAddress gateway;      // Default: 192.168.4.1
    IPAddress subnet;       // Default: 255.255.255.0
    
    void setSsidPrefix(const char* s);
    void setSsidPrefix(const String& s);
    void setPassword(const char* p);
    void setPassword(const String& p);
};
```

### TimingConfig

```cpp
struct TimingConfig {
    uint32_t connectTimeoutMs = 8000;   // Per-attempt timeout
    uint32_t retryIntervalMs = 15000;   // Between retries
    int maxTriesBeforeAp = 4;           // Failures before AP mode
};
```

### Memory Layout (v3.0)

| Structure | Size | Heap Usage |
|-----------|------|------------|
| WiFiCredentials | ~120 bytes | **0 bytes** |
| StaticIPConfig | ~64 bytes | **0 bytes** |
| APConfig | ~104 bytes | **0 bytes** |
| TimingConfig | ~20 bytes | 0 bytes |
| **Config (total)** | **~464 bytes** | **0 bytes** |

---

## WML::WiFiManagerLite

Core WiFi management class.

### Constructor

```cpp
WiFiManagerLite();
```

### Configuration Methods

```cpp
// Set configuration directly
void setConfig(const Config& config);

// Set dynamic configuration provider
void setConfigProvider(IConfigProvider* provider);

// Set logger for debug output
void setLogger(ILogger* logger);

// Register event callback
void onEvent(std::function<void(Event event, const String& message)> callback);
```

### Lifecycle Methods

```cpp
// Initialize WiFi (call in setup())
void begin();

// Process WiFi tasks (call in loop())
void loop();
```

### Control Methods

```cpp
// Force reconnection attempt
void reconnect();

// Disconnect WiFi and start AP mode
void reset();

// Clear config and start AP mode
// clearCallback: Called to clear stored credentials
// restart: If true, ESP restarts after reset (default: true)
void factoryReset(std::function<void()> clearCallback = nullptr, bool restart = true);

// Manually start AP mode
void startAP();

// Stop AP mode
void stopAP();

// Disconnect from current network
void disconnect();

// Scan for networks
// async: Run in background
// showHidden: Include hidden networks
int scanNetworks(bool async = false, bool showHidden = false);
```

### Status Methods

```cpp
// Check if connected to WiFi
bool isConnected() const;

// Check if in AP mode
bool isAPMode() const;

// Get current state
WiFiState getState() const;

// Get station IP address
IPAddress getStationIP() const;

// Get AP IP address
IPAddress getAPIP() const;

// Get connected SSID
String getSSID() const;

// Get connected BSSID
String getBSSID() const;

// Get signal strength (dBm)
int getRSSI() const;

// Get number of AP clients
int getAPClientCount() const;

// Get current retry count
int getRetryCount() const;
```

### WiFiState Enum

```cpp
enum class WiFiState {
    Disconnected,       // Not connected
    Connecting,         // Connection in progress
    Connected,          // Successfully connected
    APMode,             // Access Point active
    APModeConnecting,   // AP mode, connection attempt ongoing
    ConnectionFailed    // Connection failed
};
```

### Event Enum

```cpp
enum class Event {
    Disconnected,           // Lost connection
    Connecting,             // Starting connection
    Connected,              // Successfully connected
    ConnectionFailed,       // Connection attempt failed
    APStarted,              // AP mode started
    APStopped,              // AP mode stopped
    APClientConnected,      // Client connected to AP
    APClientDisconnected    // Client disconnected from AP
};
```

---

## WML::CaptivePortal

Web-based configuration portal.

### Constructor

```cpp
CaptivePortal(AsyncWebServer& server, WiFiManagerLite& wifiManager);
```

### Configuration Methods

```cpp
// Called when user saves new config
// Return true to accept, false to reject
void onConfigChange(std::function<bool(const Config&)> callback);

// Called to get current config for display
void onConfigGet(std::function<Config()> callback);

// Called on factory reset to clear stored data
void onFactoryReset(std::function<void()> callback);

// Set debug logger
void setLogger(ILogger* logger);

// Set HTTP Basic authentication (empty = no auth)
void setAuthentication(const String& username, const String& password);

// Set device name shown in UI
void setDeviceName(const String& name);

// Set firmware version shown in footer
void setFirmwareVersion(const String& version);

// Set whether to restart after config save (default: true)
void setRestartAfterSave(bool restart);
```

### Lifecycle Methods

```cpp
// Initialize routes (call after setting callbacks)
void begin();

// Process portal tasks (call in loop())
void loop();
```

### Status Methods

```cpp
// Check if clients are actively using portal
bool hasActiveClients() const;
```

### Web Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Redirect to setup or status |
| `/wml/setup` | GET | WiFi configuration page |
| `/wml/status` | GET | Status page (Advanced only) |
| `/wml/style.css` | GET | Stylesheet (Advanced only) |
| `/wml/app.js` | GET | JavaScript (Advanced only) |
| `/wml/netlist` | GET | JSON: Available networks |
| `/wml/config` | GET | JSON: Current configuration |
| `/wml/submit` | POST | Save new configuration |
| `/wml/reset` | POST | Reset WiFi (AP mode) |
| `/wml/factoryreset` | POST | Factory reset |
| `/wml/status.json` | GET | JSON: Device status (Advanced only) |

---

## WML::Storage

Persistent storage using NVS (Non-Volatile Storage).

### Constructor

```cpp
Storage(const char* nvsNamespace = "wml", const char* moduleId = "cfg");
```

### Methods

```cpp
// Load config from NVS
// Returns true if loaded successfully
bool load(Config& config);

// Save config to NVS
// Returns true if saved successfully
bool save(const Config& config);

// Clear stored config
bool clear();

// Check if config exists in NVS
bool exists();
```

---

## WML::StorageProvider

Implements `IConfigProvider` interface with caching.

### Constructor

```cpp
StorageProvider(Storage& storage);
```

### Methods

```cpp
// Get config (cached after first load)
Config getConfig();

// Check if config changed since last check
bool hasChanged();

// Update config (saves to NVS)
bool updateConfig(const Config& config);

// Force reload from NVS on next getConfig()
void invalidate();

// Clear stored config
bool clearConfig();
```

---

## WML::ILogger Interface

Interface for custom logging.

```cpp
class ILogger {
public:
    virtual void log(const String& message) = 0;
};
```

### Built-in Loggers

```cpp
// Log to Serial
WML::SerialLogger serialLogger;

// Log to any Stream
WML::StreamLogger streamLogger(Serial);

// Log via callback
WML::CallbackLogger callbackLogger([](const String& msg) {
    // Custom handling
});

// Discard all logs
WML::NullLogger nullLogger;

// Log to multiple destinations
WML::MultiLogger multiLogger;
multiLogger.addLogger(&serialLogger);
multiLogger.addLogger(&callbackLogger);
```

---

## WML::IConfigProvider Interface

Interface for dynamic configuration.

```cpp
class IConfigProvider {
public:
    virtual Config getConfig() = 0;
    virtual bool hasChanged() = 0;
};
```

Use `StorageProvider` for NVS-backed implementation, or implement your own.
