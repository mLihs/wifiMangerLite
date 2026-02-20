# Examples

Code examples for common use cases.

> **v3.0 Note:** All examples use the new setter methods (`setSsid()`, `setPassword()`, etc.)  
> For migration from v2.x, see [Migration Guide](MIGRATION_V2_TO_V3.md)

---

## Example 1: Minimal Setup (Basic Portal)

Simplest possible implementation.

```cpp
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <wifiMangerLite.h>

AsyncWebServer server(80);
WML::WiFiManagerLite wifiManager;
WML::Storage storage;
WML::StorageProvider configProvider(storage);
WML::CaptivePortal portal(server, wifiManager);

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== WiFiManagerLite Basic Example ===\n");
    
    // Load saved configuration
    WML::Config config;
    if (storage.load(config)) {
        Serial.println("Loaded saved config");
        wifiManager.setConfig(config);
    }
    
    // Setup portal callbacks
    portal.onConfigGet([&]() { return configProvider.getConfig(); });
    portal.onConfigChange([&](const WML::Config& cfg) {
        return configProvider.updateConfig(cfg);
    });
    portal.onFactoryReset([&]() { storage.clear(); });
    
    // Start everything
    wifiManager.begin();
    portal.begin();
    server.begin();
    
    Serial.println("Ready!");
}

void loop() {
    wifiManager.loop();
    portal.loop();
}
```

---

## Example 2: Advanced Portal with Status

Full-featured setup with status display and logging.

```cpp
#define WML_PORTAL_VARIANT WML_PORTAL_ADVANCED
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <wifiMangerLite.h>

AsyncWebServer server(80);
WML::WiFiManagerLite wifiManager;
WML::Storage storage;
WML::StorageProvider configProvider(storage);
WML::CaptivePortal portal(server, wifiManager);
WML::SerialLogger logger;

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== WiFiManagerLite Advanced Example ===\n");
    
    // Enable logging
    wifiManager.setLogger(&logger);
    portal.setLogger(&logger);
    
    // Load configuration
    WML::Config config;
    if (storage.load(config)) {
        wifiManager.setConfig(config);
    }

    // One-place naming (recommended):
    // - AP SSID: "{BaseName}-{mac8}"
    // - mDNS: "{basename}-{mac8}.local"
    // - Portal branding: "{BaseName}" (no MAC)
    wifiManager.setIdentityBaseName(config.deviceName);
    
    // Setup event handling
    wifiManager.onEvent([](WML::Event event, const String& msg) {
        switch (event) {
            case WML::Event::Connected:
                Serial.println("✓ Connected to WiFi");
                Serial.print("  IP: ");
                Serial.println(WiFi.localIP());
                break;
            case WML::Event::Disconnected:
                Serial.println("✗ Disconnected from WiFi");
                break;
            case WML::Event::APStarted:
                Serial.println("★ AP Mode started");
                Serial.print("  Connect to: ");
                Serial.println(msg);
                break;
        }
    });
    
    // Setup portal
    // portal.setDeviceName(...) is optional. If not explicitly set, the portal
    // uses wifiManager identity base name for UI branding.
    portal.setFirmwareVersion("1.0.0");
    portal.onConfigGet([&]() { return configProvider.getConfig(); });
    portal.onConfigChange([&](const WML::Config& cfg) {
        Serial.println("Config changed, saving...");
        return configProvider.updateConfig(cfg);
    });
    portal.onFactoryReset([&]() {
        Serial.println("Factory reset!");
        storage.clear();
    });
    
    // Start
    wifiManager.begin();
    portal.begin();
    server.begin();
    
    Serial.println("Setup complete!");
}

void loop() {
    wifiManager.loop();
    portal.loop();
    
    // Your application code here
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint > 30000) {
        lastPrint = millis();
        if (wifiManager.isConnected()) {
            Serial.printf("Status: Connected, RSSI: %d dBm, Heap: %d KB\n",
                wifiManager.getRSSI(), ESP.getFreeHeap() / 1024);
        }
    }
}
```

---

## Example 3: Without Storage (Hardcoded Credentials)

For development or fixed installations.

```cpp
#define WML_ENABLE_STORAGE 0
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <wifiMangerLite.h>

AsyncWebServer server(80);
WML::WiFiManagerLite wifiManager;
WML::CaptivePortal portal(server, wifiManager);

void setup() {
    Serial.begin(115200);
    
    // Hardcoded configuration (v3.0: use setter methods)
    WML::Config config;
    config.setDeviceName("TestDevice");
    config.primary.setSsid("MyWiFi");
    config.primary.setPassword("MyPassword");
    
    wifiManager.setConfig(config);
    wifiManager.setIdentityBaseName(config.deviceName);
    
    // Portal for fallback only
    portal.onConfigGet([&config]() { return config; });
    portal.onConfigChange([&config](const WML::Config& cfg) {
        config = cfg;  // Only in RAM, lost on restart
        return true;
    });
    
    wifiManager.begin();
    portal.begin();
    server.begin();
}

void loop() {
    wifiManager.loop();
    portal.loop();
}
```

---

## Example 4: Without Captive Portal

Minimal footprint, programmatic setup only.

```cpp
#define WML_ENABLE_CAPTIVE_PORTAL 0
#include <WiFi.h>
#include <wifiMangerLite.h>

WML::WiFiManagerLite wifiManager;
WML::Storage storage;

void setup() {
    Serial.begin(115200);
    
    WML::Config config;
    if (storage.load(config)) {
        wifiManager.setConfig(config);
    } else {
        // First boot - set defaults (v3.0: use setter methods)
        config.setDeviceName("Sensor-001");
        config.primary.setSsid("MyWiFi");
        config.primary.setPassword("MyPassword");
        storage.save(config);
        wifiManager.setConfig(config);
    }
    
    wifiManager.onEvent([](WML::Event event, const String& msg) {
        if (event == WML::Event::Connected) {
            Serial.println("Connected!");
        } else if (event == WML::Event::APStarted) {
            Serial.println("AP Mode - no portal, configure via Serial");
        }
    });
    
    wifiManager.begin();
}

void loop() {
    wifiManager.loop();
    
    // Handle Serial commands for configuration
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        handleSerialCommand(cmd);
    }
}

void handleSerialCommand(const String& cmd) {
    if (cmd.startsWith("ssid=")) {
        WML::Config config;
        storage.load(config);
        // v3.0: use setter method
        config.primary.setSsid(cmd.substring(5).c_str());
        storage.save(config);
        Serial.println("SSID updated, restart to apply");
    }
    // Add more commands as needed
}
```

---

## Example 5: Custom Logger

Send logs to cloud or SD card.

```cpp
#include <wifiMangerLite.h>

class CloudLogger : public WML::ILogger {
public:
    void log(const String& message) override {
        Serial.println(message);  // Also to Serial
        
        // Send to cloud (example)
        if (WiFi.isConnected()) {
            // sendToCloud(message);
        }
        
        // Or write to SD card
        // logFile.println(message);
    }
};

CloudLogger cloudLogger;

void setup() {
    wifiManager.setLogger(&cloudLogger);
    portal.setLogger(&cloudLogger);
    // ...
}
```

---

## Example 6: Multiple Loggers

Log to Serial AND callback.

```cpp
#include <wifiMangerLite.h>

WML::SerialLogger serialLogger;
WML::CallbackLogger callbackLogger([](const String& msg) {
    // Update display, send notification, etc.
    updateStatusDisplay(msg);
});

WML::MultiLogger multiLogger;

void setup() {
    multiLogger.addLogger(&serialLogger);
    multiLogger.addLogger(&callbackLogger);
    
    wifiManager.setLogger(&multiLogger);
    // ...
}
```

---

## Example 7: Static IP Configuration

```cpp
WML::Config config;

// v3.0: use setter methods for all string fields
config.setDeviceName("StaticDevice");
config.primary.setSsid("MyWiFi");
config.primary.setPassword("MyPassword");

// Static IP settings
config.staticIP.setIp("192.168.1.100");
config.staticIP.setGateway("192.168.1.1");
config.staticIP.setSubnet("255.255.255.0");
config.staticIP.setDns("8.8.8.8");

wifiManager.setConfig(config);
wifiManager.begin();
```

---

## Example 8: Dual SSID with BSSID Lock

```cpp
WML::Config config;
config.setDeviceName("DualSSID");

// Primary network (home) - v3.0: use setter methods
config.primary.setSsid("HomeWiFi");
config.primary.setPassword("HomePassword");
config.primary.setBssid("AA:BB:CC:DD:EE:FF");  // Specific AP
config.primary.bssidLock = true;               // Lock to this AP

// Fallback network (mobile hotspot)
config.secondary.setSsid("iPhone");
config.secondary.setPassword("HotspotPassword");

wifiManager.setConfig(config);
wifiManager.begin();
```

---

## Example 9: Protected Web UI

```cpp
WML::CaptivePortal portal(server, wifiManager);

// Require login when connected to main WiFi
// (AP mode is always open for initial setup)
portal.setAuthentication("admin", "secret123");

// Disable restart after save (manual control)
portal.setRestartAfterSave(false);

portal.onConfigChange([](const WML::Config& cfg) {
    // Validate - v3.0: check char[] with [0] != '\0' or strlen()
    if (strlen(cfg.primary.ssid) < 2) {
        return false;  // Reject
    }
    
    // Save
    storage.save(cfg);
    
    // Manual restart after delay
    delay(1000);
    ESP.restart();
    
    return true;
});
```

---

## Example 10: Integration with Other Libraries

```cpp
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <wifiMangerLite.h>
#include <PubSubClient.h>  // MQTT

AsyncWebServer server(80);
WML::WiFiManagerLite wifiManager;
WML::CaptivePortal portal(server, wifiManager);
WiFiClient espClient;
PubSubClient mqtt(espClient);

void setup() {
    // WiFiManagerLite setup
    wifiManager.onEvent([](WML::Event event, const String& msg) {
        if (event == WML::Event::Connected) {
            // Start MQTT when WiFi connects
            mqtt.setServer("mqtt.example.com", 1883);
            mqtt.connect("ESP32Client");
        }
    });
    
    wifiManager.begin();
    portal.begin();
    server.begin();
}

void loop() {
    wifiManager.loop();
    portal.loop();
    
    // Only process MQTT when connected
    if (wifiManager.isConnected()) {
        mqtt.loop();
    }
}
```
