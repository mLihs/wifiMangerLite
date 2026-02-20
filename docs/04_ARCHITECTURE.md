# Architecture Overview

Understanding the design and module structure of WiFiManagerLite.

---

## Design Principles

1. **Modular**: Each component is independent and optional
2. **Non-monolithic**: Clean separation of concerns
3. **Compile-time configuration**: Features enabled/disabled via `#define`
4. **Zero-overhead abstraction**: Disabled features don't consume resources
5. **Event-driven**: Non-blocking, callback-based architecture

---

## Module Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                      User Application                        │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐     │
│  │   Config    │    │   Logger    │    │   Storage   │     │
│  │ (WMLConfig) │    │ (ILogger)   │    │(WMLStorage) │     │
│  └──────┬──────┘    └──────┬──────┘    └──────┬──────┘     │
│         │                  │                  │             │
│         ▼                  ▼                  ▼             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │              WiFiManagerLite (Core)                  │   │
│  │  • Connection management                             │   │
│  │  • Retry logic                                       │   │
│  │  • AP mode fallback                                  │   │
│  │  • Event callbacks                                   │   │
│  └─────────────────────────┬───────────────────────────┘   │
│                            │                                │
│                            ▼                                │
│  ┌─────────────────────────────────────────────────────┐   │
│  │              CaptivePortal (Optional)                │   │
│  │  • Web UI (Basic/Advanced)                          │   │
│  │  • Network scanning                                  │   │
│  │  • Configuration forms                               │   │
│  │  • Reset endpoints                                   │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    ESP32 Hardware Layer                      │
│  • WiFi (STA + AP)  • NVS Flash  • mDNS  • HTTP Server      │
└─────────────────────────────────────────────────────────────┘
```

---

## File Structure

```
wifiMangerLite/
├── src/
│   ├── wifiMangerLite.h      # Main include (entry point)
│   ├── WMLBuildConfig.h      # Compile-time configuration
│   ├── WMLConfig.h           # Data structures
│   ├── WMLLogger.h           # Logging interfaces
│   ├── WiFiManagerLite.h/cpp # Core WiFi management
│   ├── WMLCaptivePortal.h/cpp# Web UI (optional)
│   ├── WMLStorage.h/cpp      # NVS persistence (optional)
│   └── generated/            # Auto-generated web assets
│       ├── wml_basic_setup_html.h
│       ├── wml_setup_html.h
│       ├── wml_status_html.h
│       ├── wml_style_css.h
│       ├── wml_app_js.h
│       └── wml_web_manifest.h
├── webui_src/                # Web UI source files
│   ├── basic/
│   │   └── setup.html        # Basic variant (self-contained)
│   ├── setup.html            # Advanced setup page
│   ├── status.html           # Advanced status page
│   ├── style.css             # Advanced stylesheet
│   └── app.js                # Advanced JavaScript
├── tools/
│   └── build_webui.py        # Web asset build script
├── examples/
│   ├── BasicUsage/
│   ├── AdvancedUsage/
│   └── CaptivePortal/
├── docs/                     # Documentation
├── library.properties
└── keywords.txt
```

---

## State Machine

### WiFi Connection States

```
                    ┌──────────────┐
                    │ Disconnected │◄────────────────┐
                    └──────┬───────┘                 │
                           │ begin() / reconnect()  │
                           ▼                        │
                    ┌──────────────┐                │
              ┌────►│  Connecting  │────────────────┤
              │     └──────┬───────┘   timeout/fail │
              │            │                        │
              │            │ success                │
              │            ▼                        │
              │     ┌──────────────┐                │
              │     │  Connected   │────────────────┤
              │     └──────────────┘   disconnect   │
              │                                     │
              │     max retries exceeded            │
              │            │                        │
              │            ▼                        │
              │     ┌──────────────┐                │
              └─────│   AP Mode    │────────────────┘
                    └──────────────┘   config saved
```

### Connection Flow

```
1. begin()
   │
   ├─► Has credentials? ─► No ──► Start AP Mode
   │         │
   │         ▼ Yes
   │   Connect to Primary SSID
   │         │
   │         ├─► Success ──► Connected ✓
   │         │
   │         ▼ Fail
   │   Has Fallback SSID?
   │         │
   │         ├─► Yes ──► Try Fallback
   │         │              │
   │         │              ├─► Success ──► Connected ✓
   │         │              │
   │         ▼              ▼ Fail
   │   Retry count < max?
   │         │
   │         ├─► Yes ──► Wait retry interval ──► Retry
   │         │
   │         ▼ No
   │   Start AP Mode
   │
   └─► loop() monitors connection, triggers reconnect on loss
```

---

## Memory Layout

### Flash Usage (Approximate)

| Component | Size | Condition |
|-----------|------|-----------|
| Core (WiFiManagerLite) | ~8 KB | Always |
| CaptivePortal | ~12 KB | `WML_ENABLE_CAPTIVE_PORTAL=1` |
| Basic Web UI | ~2 KB | `WML_PORTAL_VARIANT=0` |
| Advanced Web UI | ~6 KB | `WML_PORTAL_VARIANT=1` |
| Storage | ~4 KB | `WML_ENABLE_STORAGE=1` |
| **Total (Basic)** | ~24 KB | |
| **Total (Advanced)** | ~30 KB | |

### RAM Usage

| Component | Static | Dynamic (peak) |
|-----------|--------|----------------|
| Config struct | ~200 bytes | - |
| WiFiManagerLite | ~100 bytes | ~500 bytes (scan) |
| CaptivePortal | ~150 bytes | ~2 KB (JSON) |
| Storage | ~50 bytes | ~1 KB (buffer) |

---

## Thread Safety

- **Single-threaded design**: All methods should be called from the main loop
- **Async operations**: Network scans run in background, results collected in `loop()`. Scans are started on-demand by the captive portal (via `GET /wml/netlist`).
- **No mutexes required**: ESP32 Arduino runs on single core by default
- **Interrupt-safe**: Callbacks may be called from WiFi event handlers

---

## Dependencies

### Required

| Library | Version | Purpose |
|---------|---------|---------|
| Arduino ESP32 | 2.x+ | Platform |
| ArduinoJson | 7.x+ | JSON serialization |

### Optional

| Library | Version | Purpose | Required for |
|---------|---------|---------|--------------|
| ESPAsyncWebServer | 1.x+ | HTTP server | Captive Portal |
| NVSConfigBus | 1.x+ | NVS abstraction | Storage |

---

## Extensibility Points

### Custom Logger

```cpp
class MyLogger : public WML::ILogger {
public:
    void log(const String& msg) override {
        // Send to cloud, SD card, etc.
    }
};
```

### Custom Config Provider

```cpp
class CloudConfigProvider : public WML::IConfigProvider {
public:
    WML::Config getConfig() override {
        // Fetch from cloud
    }
    bool hasChanged() override {
        // Check for remote updates
    }
};
```

### Event Handling

```cpp
wifiManager.onEvent([](WML::Event event, const String& msg) {
    switch (event) {
        case WML::Event::Connected:
            // Start other services
            break;
        case WML::Event::Disconnected:
            // Handle offline mode
            break;
    }
});
```
