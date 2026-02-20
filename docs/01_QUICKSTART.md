# Quickstart Guide

Get WiFiManagerLite running in 5 minutes.

> **v3.0 Note:** Config fields now use `char[]` instead of `String` for zero heap fragmentation.  
> Use setter methods like `setSsid()` instead of direct assignment.

## Prerequisites

- ESP32 board (ESP32, ESP32-S2, ESP32-S3, ESP32-C3)
- Arduino IDE 1.8+ or PlatformIO
- Required libraries:
  - `ArduinoJson` (v7+)
  - `ESPAsyncWebServer` (for Captive Portal)
  - `NVSUtilityLibrary` (for persistent storage)

## Installation

### Arduino IDE

1. Download the library as ZIP
2. Sketch → Include Library → Add .ZIP Library
3. Install dependencies via Library Manager

### PlatformIO

```ini
lib_deps = 
  bblanchon/ArduinoJson@^7.0.0
  ESPAsyncWebServer
  NVSUtilityLibrary
  wifiMangerLite
```

## Minimal Example (Basic Portal)

```cpp
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <wifiMangerLite.h>

AsyncWebServer server(80);
WML::WiFiManagerLite wifiManager;
WML::Storage storage("wml", "cfg");
WML::StorageProvider configProvider(storage);
WML::CaptivePortal portal(server, wifiManager);

void setup() {
    Serial.begin(115200);
    
    // Load saved config
    WML::Config config;
    if (storage.load(config)) {
        wifiManager.setConfig(config);
    }
    
    // Setup portal callbacks
    portal.onConfigGet([]() { return configProvider.getConfig(); });
    portal.onConfigChange([](const WML::Config& cfg) {
        return configProvider.updateConfig(cfg);
    });
    
    // Start
    wifiManager.begin();
    portal.begin();
    server.begin();
}

void loop() {
    wifiManager.loop();
    portal.loop();
}
```

## What Happens

1. **First boot:** No saved credentials → AP mode starts
2. **User connects to AP:** `ESP-Setup-XXXXXX` (open network)
3. **Captive portal appears:** WiFi selection page
4. **User selects network:** Enters password, clicks "Save & Connect"
5. **ESP restarts:** Connects to configured WiFi
6. **Next boot:** Credentials loaded from NVS → Auto-connect

## Portal Variants

### Basic (Default)
Simple UI: Network list, password input, save button.
- Size: ~2 KB
- Best for: Simple projects, memory-constrained devices

### Advanced
Full UI: Dual SSID, static IP, status page, reset buttons.
- Size: ~6 KB
- Enable with: `#define WML_PORTAL_VARIANT WML_PORTAL_ADVANCED`

## Next Steps

- [Configuration Options](02_CONFIGURATION.md) - Customize timeouts, AP settings
- [API Reference](03_API_REFERENCE.md) - All methods and callbacks
- [Examples](08_EXAMPLES.md) - More code examples
