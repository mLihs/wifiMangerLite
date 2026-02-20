# Storage Guide

Persistent configuration storage using ESP32 NVS (Non-Volatile Storage).

---

## Overview

WiFiManagerLite uses NVS to store WiFi credentials and device configuration. The storage module:

- Serializes config as **MessagePack** (binary, no String allocations)
- Uses **NVSConfigBus** library for modular NVS access
- Stores only WiFi-related data in its own namespace
- Does NOT affect other application data

---

## Quick Setup

```cpp
#include <wifiMangerLite.h>

// Create storage with namespace "wml" and module ID "cfg"
WML::Storage storage("wml", "cfg");

// Create provider for WiFiManagerLite integration
WML::StorageProvider configProvider(storage);

void setup() {
    // Load saved config
    WML::Config config;
    if (storage.load(config)) {
        Serial.println("Config loaded from NVS");
        wifiManager.setConfig(config);
    }
    
    // Or use provider for automatic loading
    wifiManager.setConfigProvider(&configProvider);
}
```

---

## Storage Architecture

### NVS Structure

```
NVS Flash
├── wml/                    ← Namespace (configurable)
│   └── cfg                 ← Module ID (configurable)
│       └── [MessagePack data]
├── other_app/              ← Your other app data (untouched)
│   └── settings
└── nvs/                    ← System data (untouched)
```

### Data Format (MessagePack)

```
{
  "dn": "device-name",      // Device name
  "s0": "primary-ssid",     // Primary SSID
  "p0": "primary-pass",     // Primary password
  "b0": "AA:BB:CC:DD:EE",   // Primary BSSID
  "bl": true,               // BSSID lock
  "s1": "fallback-ssid",    // Secondary SSID
  "p1": "fallback-pass",    // Secondary password
  "ip": "192.168.1.100",    // Static IP
  "gw": "192.168.1.1",      // Gateway
  "sn": "255.255.255.0",    // Subnet
  "ds": "8.8.8.8",          // DNS
  "ap": "ESP-Setup-",       // AP prefix
  "apw": "",                // AP password
  "mdns": true,             // Enable mDNS
  "hp": 80                  // HTTP port (if != 80)
}
```

Short keys reduce storage size. Timing config only stored if non-default.

---

## API Reference

### WML::Storage

```cpp
// Constructor
Storage(const char* nvsNamespace = "wml", const char* moduleId = "cfg");

// Load config from NVS
// Returns false if no config exists or read error
bool load(Config& config);

// Save config to NVS
// Returns false on write error
bool save(const Config& config);

// Delete stored config
// Returns false on error
bool clear();

// Check if config exists
bool exists();
```

### WML::StorageProvider

Implements `IConfigProvider` with caching:

```cpp
// Constructor
StorageProvider(Storage& storage);

// Get config (loads from NVS on first call, cached after)
Config getConfig();

// Check if config changed (resets flag after call)
bool hasChanged();

// Update config (saves to NVS immediately)
bool updateConfig(const Config& config);

// Force reload from NVS on next getConfig()
void invalidate();

// Clear stored config and reset to defaults
bool clearConfig();
```

---

## Usage Patterns

### Pattern 1: Direct Storage Access

```cpp
WML::Storage storage;
WML::Config config;

// Load
if (storage.load(config)) {
    // Use config
}

// Save
config.primary.ssid = "NewSSID";
storage.save(config);

// Clear
storage.clear();
```

### Pattern 2: With StorageProvider

```cpp
WML::Storage storage;
WML::StorageProvider provider(storage);

// Auto-load on first access
WML::Config config = provider.getConfig();

// Update (auto-save)
config.primary.ssid = "NewSSID";
provider.updateConfig(config);

// Check for changes
if (provider.hasChanged()) {
    // Config was modified
}
```

### Pattern 3: With Captive Portal

```cpp
WML::Storage storage;
WML::StorageProvider provider(storage);
WML::CaptivePortal portal(server, wifiManager);

// Portal reads config from provider
portal.onConfigGet([&]() {
    return provider.getConfig();
});

// Portal saves config through provider
portal.onConfigChange([&](const WML::Config& cfg) {
    return provider.updateConfig(cfg);
});

// Factory reset clears storage
portal.onFactoryReset([&]() {
    provider.clearConfig();
});
```

---

## Factory Reset Behavior

When `factoryReset()` is called:

1. **WiFi data cleared**: Only the `wml` namespace is affected
2. **Other data preserved**: Your app's NVS data is untouched
3. **ESP restarts** (optional): Clean state for reconnection

```cpp
// What gets cleared:
storage.clear();  // Only "wml/cfg" key

// What stays:
// - Other NVS namespaces
// - WiFi system settings (can persist)
// - Calibration data
// - User preferences in other namespaces
```

### Full NVS Erase (if needed)

If you need to erase ALL NVS data (not recommended):

```cpp
#include <nvs_flash.h>

nvs_flash_erase();  // Erases EVERYTHING
nvs_flash_init();   // Reinitialize
```

---

## Custom Namespace

Use different namespace to avoid conflicts:

```cpp
// For multi-device projects
WML::Storage wifiStorage("myapp_wifi", "cfg");

// Separate storage for different purposes
WML::Storage deviceStorage("myapp_device", "info");
```

---

## Error Handling

```cpp
WML::Config config;

if (!storage.load(config)) {
    Serial.println("No saved config or read error");
    // Use defaults
    config.deviceName = "NewDevice";
    config.primary.ssid = "";
}

if (!storage.save(config)) {
    Serial.println("Failed to save config!");
    // NVS might be full or corrupted
}
```

### Common Errors

| Issue | Cause | Solution |
|-------|-------|----------|
| Load fails | No config saved yet | Use defaults |
| Save fails | NVS full | Erase unused data |
| Data corrupted | Power loss during write | Clear and re-save |

---

## Memory Considerations

### Flash Wear

NVS uses wear leveling, but frequent writes should be avoided:

```cpp
// BAD: Save on every change
void onChange() {
    storage.save(config);  // Don't do this frequently
}

// GOOD: Batch changes
void onSaveButton() {
    storage.save(config);  // Save once when user confirms
}
```

### Buffer Size

Storage uses a 512-byte buffer for MessagePack serialization. This is sufficient for typical configs. If you add custom fields, monitor the size:

```cpp
// In WMLStorage.h
static const size_t kBufferSize = 512;  // Increase if needed
```

---

## Debugging

Enable storage debug output:

```cpp
#define WML_ENABLE_DEBUG_LOGS 1
#include <wifiMangerLite.h>
```

Check NVS usage:

```cpp
#include <nvs_flash.h>

nvs_stats_t stats;
nvs_get_stats(NULL, &stats);
Serial.printf("NVS: %d used, %d free, %d total entries\n",
    stats.used_entries, stats.free_entries, stats.total_entries);
```
