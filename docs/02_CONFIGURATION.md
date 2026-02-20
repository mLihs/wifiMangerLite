# Configuration Reference

All configuration is done via `#define` macros at compile time. Define values BEFORE including the library header.

> **v3.0 Note:** The `WML::Config` structure now uses `char[]` arrays instead of `String` for zero heap fragmentation.  
> Use setter methods like `config.primary.setSsid("...")` instead of direct assignment.  
> See [API Reference](03_API_REFERENCE.md) for the new Config structure and [Migration Guide](MIGRATION_V2_TO_V3.md) for upgrade instructions.

## How to Configure

### Option 1: In your sketch (before include)

```cpp
#define WML_PORTAL_VARIANT WML_PORTAL_ADVANCED
#define WML_CONNECT_TIMEOUT_MS 10000
#define WML_AP_SSID_PREFIX "MyDevice-"
#include <wifiMangerLite.h>
```

### Option 2: PlatformIO build flags

```ini
build_flags = 
  -DWML_PORTAL_VARIANT=1
  -DWML_CONNECT_TIMEOUT_MS=10000
  -DWML_AP_SSID_PREFIX=\"MyDevice-\"
```

### Option 3: Arduino IDE build flags

Create `build_opt.h` in your sketch folder or use board-specific defines.

---

## Feature Gates

Enable/disable entire features to save flash and RAM.

| Define | Default | Description |
|--------|---------|-------------|
| `WML_ENABLE_CAPTIVE_PORTAL` | `1` | Web UI for configuration. Disable to save ~20KB |
| `WML_ENABLE_STORAGE` | `1` | NVS storage support. Disable if handling storage yourself |
| `WML_ENABLE_MDNS` | `1` | mDNS for `hostname.local` resolution |
| `WML_ENABLE_DUAL_SSID` | `1` | Primary + fallback SSID support |
| `WML_ENABLE_STATIC_IP` | `1` | Static IP configuration |
| `WML_ENABLE_BSSID_LOCK` | `1` | Lock to specific access point |
| `WML_ENABLE_DEBUG_LOGS` | `1` | Serial debug output. Disable for production |

### Example: Minimal Build

```cpp
#define WML_ENABLE_CAPTIVE_PORTAL 0  // No web UI
#define WML_ENABLE_STORAGE 0         // Handle storage yourself
#define WML_ENABLE_DUAL_SSID 0       // Single SSID only
#define WML_ENABLE_STATIC_IP 0       // DHCP only
#define WML_ENABLE_DEBUG_LOGS 0      // No debug output
#include <wifiMangerLite.h>
```

---

## Portal Variant

| Define | Value | Description |
|--------|-------|-------------|
| `WML_PORTAL_VARIANT` | `0` (default) | Basic UI (~2KB) - Network list, password, save |
| `WML_PORTAL_VARIANT` | `1` | Advanced UI (~6KB) - Full features |

Convenience aliases:
- `WML_PORTAL_BASIC` = `0`
- `WML_PORTAL_ADVANCED` = `1`

```cpp
#define WML_PORTAL_VARIANT WML_PORTAL_ADVANCED
```

---

## WiFi Connection Settings

| Define | Default | Description |
|--------|---------|-------------|
| `WML_CONNECT_TIMEOUT_MS` | `8000` | Timeout per connection attempt (ms) |
| `WML_RETRY_INTERVAL_MS` | `15000` | Delay between retry attempts (ms) |
| `WML_MAX_RETRIES_BEFORE_AP` | `2` | Failed attempts before starting AP |
| `WML_AP_AUTO_TIMEOUT_MS` | `0` | Auto-restart after AP inactivity (0=disabled) |

### Example: Aggressive Reconnection

```cpp
#define WML_CONNECT_TIMEOUT_MS 5000      // 5 second timeout
#define WML_RETRY_INTERVAL_MS 10000      // Retry every 10 seconds
#define WML_MAX_RETRIES_BEFORE_AP 2      // AP mode after 2 failures
```

---

## Access Point (AP) Settings

| Define | Default | Description |
|--------|---------|-------------|
| `WML_AP_SSID_PREFIX` | `"ESP-Setup-"` | AP SSID prefix (MAC suffix added) |
| `WML_AP_PASSWORD` | `""` | AP password (empty = open) |
| `WML_AP_CHANNEL` | `1` | WiFi channel (1-13) |
| `WML_AP_MAX_CONNECTIONS` | `4` | Max simultaneous AP clients |
| `WML_AP_HIDDEN` | `0` | Hidden SSID (0=visible, 1=hidden) |

### Example: Secured AP

```cpp
#define WML_AP_SSID_PREFIX "SmartDevice-"
#define WML_AP_PASSWORD "setup1234"
#define WML_AP_CHANNEL 6
```

---

## Web Server Settings

Only applies when `WML_ENABLE_CAPTIVE_PORTAL == 1`.

| Define | Default | Description |
|--------|---------|-------------|
| `WML_HTTP_PORT` | `80` | HTTP server port |
| `WML_WEB_AUTH_USER` | `""` | Basic auth username (empty = no auth) |
| `WML_WEB_AUTH_PASS` | `""` | Basic auth password |
| `WML_SCAN_CACHE_MS` | `10000` | Network scan cache duration |
| `WML_CLIENT_TIMEOUT_MS` | `30000` | Client activity timeout |
| `WML_RESTART_DELAY_MS` | `600` | Delay before restart after save |

### Example: Protected Web UI

```cpp
#define WML_WEB_AUTH_USER "admin"
#define WML_WEB_AUTH_PASS "secret123"
```

---

## NVS Storage Settings

Only applies when `WML_ENABLE_STORAGE == 1`.

| Define | Default | Description |
|--------|---------|-------------|
| `WML_NVS_NAMESPACE` | `"wml"` | NVS namespace for WiFi data |
| `WML_NVS_MODULE_ID` | `"cfg"` | Key within namespace |

### Example: Custom Namespace

```cpp
#define WML_NVS_NAMESPACE "myapp"
#define WML_NVS_MODULE_ID "wifi"
```

---

## Device Settings

| Define | Default | Description |
|--------|---------|-------------|
| `WML_DEFAULT_DEVICE_NAME` | `"ESP-Device"` | Default device name |
| `WML_FIRMWARE_VERSION` | `"1.0.0"` | Firmware version string |

---

## Debug Macros

When `WML_ENABLE_DEBUG_LOGS == 1`, these macros are available:

```cpp
WML_LOG("Message");           // Serial.println("[WML] Message")
WML_LOGF("Value: %d", 42);    // Serial.printf("[WML] Value: 42\n")
```

When disabled, these compile to nothing (zero overhead).

---

## Complete Configuration Example

```cpp
// Feature selection
#define WML_ENABLE_CAPTIVE_PORTAL 1
#define WML_ENABLE_STORAGE 1
#define WML_ENABLE_MDNS 1
#define WML_ENABLE_DEBUG_LOGS 0  // Disable for production

// Portal variant
#define WML_PORTAL_VARIANT WML_PORTAL_BASIC

// Connection behavior
#define WML_CONNECT_TIMEOUT_MS 8000
#define WML_RETRY_INTERVAL_MS 15000
#define WML_MAX_RETRIES_BEFORE_AP 3

// AP configuration
#define WML_AP_SSID_PREFIX "SmartWidget-"
#define WML_AP_PASSWORD ""
#define WML_AP_CHANNEL 1

// Device info
#define WML_DEFAULT_DEVICE_NAME "SmartWidget"
#define WML_FIRMWARE_VERSION "1.2.3"

#include <wifiMangerLite.h>
```
