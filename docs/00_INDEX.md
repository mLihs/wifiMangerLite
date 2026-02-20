# WiFiManagerLite Documentation

> A lightweight, modular WiFi connection manager for ESP32 with optional captive portal and NVS storage.

## ⚠️ v3.0 Breaking Change

Version 3.0 replaces all `String` fields in `Config` with fixed-size `char[]` arrays for **zero heap fragmentation**.

**If upgrading from v2.x:** See [Migration Guide](MIGRATION_V2_TO_V3.md)

```cpp
// v2.x (OLD)
config.primary.ssid = "MyNetwork";

// v3.0 (NEW)
config.primary.setSsid("MyNetwork");
```

## Documentation Index

| Document | Description |
|----------|-------------|
| [01_QUICKSTART.md](01_QUICKSTART.md) | Get started in 5 minutes |
| [02_CONFIGURATION.md](02_CONFIGURATION.md) | All compile-time options (`#define`) |
| [03_API_REFERENCE.md](03_API_REFERENCE.md) | Complete API documentation |
| [04_ARCHITECTURE.md](04_ARCHITECTURE.md) | System design and module overview |
| [05_CAPTIVE_PORTAL.md](05_CAPTIVE_PORTAL.md) | Web UI variants and customization |
| [06_WEBUI_BUILD.md](06_WEBUI_BUILD.md) | Building and customizing the Web UI |
| [07_STORAGE.md](07_STORAGE.md) | NVS storage integration |
| [08_EXAMPLES.md](08_EXAMPLES.md) | Code examples and use cases |
| [09_OPTIMIZATION.md](09_OPTIMIZATION.md) | Performance optimizations & extension points |
| **[MIGRATION_V2_TO_V3.md](MIGRATION_V2_TO_V3.md)** | **v2→v3 Migration Guide (for AI agents)** |
| [HEAP_ANALYSIS_V2.md](HEAP_ANALYSIS_V2.md) | Heap fragmentation analysis & test results |
| [HEAP_FRAGMENTATION_ANALYSIS.md](HEAP_FRAGMENTATION_ANALYSIS.md) | Original heap fragmentation analysis |
| [HEAP_FIXES.md](HEAP_FIXES.md) | Heap fragmentation fixes history |

## Quick Links

### For Humans
- **New to the library?** → Start with [Quickstart](01_QUICKSTART.md)
- **Upgrading from v2.x?** → See [Migration Guide](MIGRATION_V2_TO_V3.md)
- **Need to customize?** → See [Configuration](02_CONFIGURATION.md)
- **Building a product?** → Check [Architecture](04_ARCHITECTURE.md)
- **Adding custom modules?** → Read [Optimization & Extensions](09_OPTIMIZATION.md)

### For AI Agents
- **Migration:** [MIGRATION_V2_TO_V3.md](MIGRATION_V2_TO_V3.md) contains regex patterns and transformation rules
- **Code generation context:** This library uses namespace `WML::` for all classes
- **Main include:** `#include <wifiMangerLite.h>` (note: intentional typo preserved for compatibility)
- **Key classes:** `WiFiManagerLite`, `CaptivePortal`, `Storage`, `StorageProvider`
- **Config fields:** All string fields are `char[]` arrays - use setter methods like `setSsid()`
- **Extension interfaces:** `IStatusContributor`, `IRouteRegistrar`, `IConfigSectionProvider`, `ILoopHandler`
- **Configuration:** All options are compile-time `#define` in `WMLBuildConfig.h`

## Version

- **Library Version:** 3.0.1
- **Documentation Updated:** 2026-01-29
- **UI Design:** Homewind Design System (Light Theme)
- **JavaScript:** ES5-compatible, XMLHttpRequest (Captive Portal optimized)
- **Changelog:** [CHANGELOG.md](../CHANGELOG.md)

## Credits & Inspiration

This library was inspired by and extracted from the **[BambuBeacon](https://github.com/softwarecrash/BambuBeacon)** project by [@softwarecrash](https://github.com/softwarecrash) - a status light for BambuLab 3D printers.

## License

MIT License - See LICENSE file in repository root.
