# WiFiManagerLite

A lightweight, modular WiFi connection manager for ESP32 with optional captive portal and NVS storage.

[![Version](https://img.shields.io/badge/version-3.0.1-blue)](CHANGELOG.md)
![Platform](https://img.shields.io/badge/platform-ESP32-green)
![License](https://img.shields.io/badge/license-MIT-lightgrey)

## Features

- 🔌 **Automatic WiFi connection** with retry logic
- 📶 **Captive Portal** for easy configuration (Basic or Advanced UI)
- 💾 **NVS Storage** for persistent credentials (MessagePack, no Strings)
- 🔄 **Dual SSID** support (primary + fallback)
- 🌐 **Static IP** configuration
- 🔒 **BSSID Lock** for specific AP binding
- 📡 **mDNS** support (hostname.local)
- ⚡ **Compile-time configuration** via `#define`
- 📦 **Modular design** - enable only what you need

## Quick Start

```cpp
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <wifiMangerLite.h>

AsyncWebServer server(80);
WML::WiFiManagerLite wifiManager;
WML::Storage storage;
WML::CaptivePortal portal(server, wifiManager);

void setup() {
    Serial.begin(115200);
    
    // Load saved config
    WML::Config config;
    if (storage.load(config)) {
        wifiManager.setConfig(config);
    }
    
    // Setup portal
    portal.onConfigGet([]() { 
        WML::Config cfg; 
        storage.load(cfg); 
        return cfg; 
    });
    portal.onConfigChange([](const WML::Config& cfg) {
        return storage.save(cfg);
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

## Portal Variants

| Variant | Size | Features |
|---------|------|----------|
| **Basic** (default) | ~2 KB | Network list, password, save |
| **Advanced** | ~6 KB | + Dual SSID, static IP, status page, resets |

Enable Advanced:
```cpp
#define WML_PORTAL_VARIANT WML_PORTAL_ADVANCED
#include <wifiMangerLite.h>
```

## Configuration

All options via `#define` before including the library:

```cpp
// Feature gates
#define WML_ENABLE_CAPTIVE_PORTAL 1  // Web UI
#define WML_ENABLE_STORAGE 1         // NVS storage
#define WML_ENABLE_MDNS 1            // mDNS support

// WiFi settings
#define WML_CONNECT_TIMEOUT_MS 8000
#define WML_MAX_RETRIES_BEFORE_AP 2
#define WML_RETRY_INTERVAL_MS 10000

// AP settings
#define WML_AP_SSID_PREFIX "MyDevice-"
#define WML_AP_PASSWORD ""

#include <wifiMangerLite.h>
```

## Documentation

Full documentation in the `docs/` folder:

| Document | Description |
|----------|-------------|
| [00_INDEX.md](docs/00_INDEX.md) | Documentation index |
| [01_QUICKSTART.md](docs/01_QUICKSTART.md) | Get started in 5 minutes |
| [02_CONFIGURATION.md](docs/02_CONFIGURATION.md) | All compile-time options |
| [03_API_REFERENCE.md](docs/03_API_REFERENCE.md) | Complete API documentation |
| [04_ARCHITECTURE.md](docs/04_ARCHITECTURE.md) | System design overview |
| [05_CAPTIVE_PORTAL.md](docs/05_CAPTIVE_PORTAL.md) | Web UI guide |
| [06_WEBUI_BUILD.md](docs/06_WEBUI_BUILD.md) | Customizing the Web UI |
| [07_STORAGE.md](docs/07_STORAGE.md) | NVS storage integration |
| [08_EXAMPLES.md](docs/08_EXAMPLES.md) | Code examples |

## Dependencies

- **Required:** ArduinoJson (v7+)
- **Optional:** ESPAsyncWebServer (for Captive Portal)
- **Required for Storage:** NVSUtilityLibrary

## Building Web UI

After modifying files in `webui_src/`:

```bash
python3 tools/build_webui.py
```

## License

MIT License – see [LICENSE](LICENSE) for details.

## Credits & Inspiration

This library was inspired by and extracted from the **[BambuBeacon](https://github.com/softwarecrash/BambuBeacon)** project by [@softwarecrash](https://github.com/softwarecrash) - a status light for BambuLab 3D printers.

The WiFi management, captive portal, and configuration patterns were generalized and enhanced into this reusable library.
