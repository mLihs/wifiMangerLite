# Captive Portal Guide

The Captive Portal provides a web-based interface for WiFi configuration.

---

## Overview

When the ESP32 cannot connect to a configured WiFi network, it starts an Access Point (AP) with a captive portal. Users connect to this AP and are automatically redirected to the configuration page.

---

## Portal Variants

### Basic (Default)

Minimal UI optimized for size (~2 KB).

**Features:**
- Available network list with signal strength
- Password input
- Save & Connect button

**Best for:**
- Memory-constrained projects
- Simple IoT devices
- Production deployments

**Screenshot:**
```
┌─────────────────────────┐
│    📶 WiFi Setup        │
├─────────────────────────┤
│ Available Networks      │
│ ┌─────────────────────┐ │
│ │ ████ MyWiFi    🔒   │ │ ← Click to select
│ │ ███░ Guest          │ │
│ │ ██░░ Neighbor  🔒   │ │
│ └─────────────────────┘ │
├─────────────────────────┤
│ WiFi Password           │
│ [••••••••••••••••]      │
├─────────────────────────┤
│ [  Save & Connect  ]    │
└─────────────────────────┘
```

### Advanced

Full-featured UI (~6 KB).

**Additional Features:**
- Device name configuration
- Fallback SSID support
- Static IP configuration
- BSSID lock option
- Status page with device info
- Reset WiFi button
- Factory Reset button

**Enable with:**
```cpp
#define WML_PORTAL_VARIANT WML_PORTAL_ADVANCED
```

---

## How Captive Portal Works

### Detection Flow

```
1. User connects to AP (e.g., "ESP-Setup-a1b2c3")
2. Device gets IP (192.168.4.x)
3. OS sends captive portal detection request:
   - Android: GET /generate_204
   - iOS: GET /hotspot-detect.html
   - Windows: GET /connecttest.txt
4. Library intercepts and redirects to /wml/setup
5. Captive portal notification appears on device
```

### Supported Platforms

| Platform | Detection Method | Status |
|----------|-----------------|--------|
| Android | `/generate_204` | ✅ Full support |
| iOS/macOS | `/hotspot-detect.html` | ✅ Full support |
| Windows | `/connecttest.txt` | ✅ Full support |
| Linux | Varies | ⚠️ Manual navigation may be needed |

---

## Web Endpoints

### All Variants

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Root redirect |
| `/wml/setup` | GET | Configuration page |
| `/wml/netlist` | GET | JSON network list |
| `/wml/config` | GET | JSON current config |
| `/wml/submit` | POST | Save configuration |
| `/wml/reset` | POST | Reset WiFi |
| `/wml/factoryreset` | POST | Factory reset |

### Advanced Only

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/wml/status` | GET | Status page |
| `/wml/status.json` | GET | JSON device status |
| `/wml/style.css` | GET | Stylesheet |
| `/wml/app.js` | GET | JavaScript |

---

## Portal UI vs Project UI (Custom Routes)

WiFiManagerLite’s Captive Portal UI is served under the **`/wml/*`** endpoints.
Your application can also serve its own pages (often `/`, `/status`, `/api/*`) using `server.on(...)`.

### Key Rules

- **Portal UI lives at `/wml/*`**: The library registers these routes inside `portal.begin()`.
- **Project UI is whatever you register on the web server** (e.g. `server.on("/", ...)`).
- **Order matters**:
  - If your sketch registers `/` or `/status` routes, those are your responsibility.
  - The portal does not try to “detect” your UI; it simply registers its own routes.

### Recommended Pattern

- Keep the portal under `/wml/*`.
- Use your own UI under `/` and other non-`/wml/*` routes.
- Register your custom routes **before** `portal.begin()` so your intentions are clear.

This approach keeps a stable entry point for configuration (`/wml/setup`) while allowing your project UI to evolve independently.

## API Responses

### GET /wml/netlist

**Behavior:**
- This endpoint **triggers an async WiFi scan on-demand** (if scan cache is expired).
- No scan is started just because AP mode started; scanning begins when a client actually loads the setup UI.

```json
{
  "networks": [
    {
      "ssid": "MyWiFi",
      "rssi": -45,
      "enc": true,
      "bssid": "AA:BB:CC:DD:EE:FF"
    },
    {
      "ssid": "Guest",
      "rssi": -67,
      "enc": false,
      "bssid": "11:22:33:44:55:66"
    }
  ],
  "scanning": false
}
```

### GET /wml/config

```json
{
  "deviceName": "ESP-Device",
  "ssid0": "MyWiFi",
  "pass0": "",
  "bssid0": "",
  "bssidLock": false,
  "ssid1": "",
  "pass1": "",
  "ip": "",
  "subnet": "",
  "gateway": "",
  "dns": ""
}
```

### GET /wml/status.json (Advanced)

```json
{
  "connected": true,
  "ssid": "MyWiFi",
  "ip": "192.168.1.100",
  "rssi": -52,
  "mac": "AA:BB:CC:DD:EE:FF",
  "hostname": "ESP-Device",
  "uptime": 3600,
  "heap": 230000,
  "version": "1.0.0",
  "apMode": false
}
```

### POST /wml/submit

**Request (form-urlencoded):**
```
devicename=MyDevice&ssid0=MyWiFi&password0=secret123&...
```

**Response:**
```json
{"success": true, "restart": true}
```

---

## Authentication

When connected to the main WiFi (not in AP mode), the portal can require authentication:

```cpp
portal.setAuthentication("admin", "password123");
```

In AP mode, authentication is always disabled to allow initial setup.

---

## Customization

### Device Name

```cpp
portal.setDeviceName("Smart Thermostat");
```

### Firmware Version

```cpp
portal.setFirmwareVersion("2.1.0");
```

### Disable Restart After Save

```cpp
portal.setRestartAfterSave(false);
```

---

## Callbacks

### Configuration Change

Called when user saves new configuration:

```cpp
portal.onConfigChange([](const WML::Config& config) {
    // Validate configuration
    if (config.primary.ssid.length() == 0) {
        return false;  // Reject invalid config
    }
    
    // Save to storage
    storage.save(config);
    
    return true;  // Accept and restart
});
```

### Configuration Get

Called when portal needs current configuration:

```cpp
portal.onConfigGet([]() {
    WML::Config config;
    storage.load(config);
    return config;
});
```

### Factory Reset

Called when user triggers factory reset:

```cpp
portal.onFactoryReset([]() {
    storage.clear();  // Clear WiFi config only
    // Optionally clear other app data
});
```

---

## Styling

The portal uses the **Homewind** design system (light theme) with CSS variables.

```css
:root {
  --background-app: #EEF2FA;
  --tile-bg: rgba(255,255,255,0.8);
  --text-main: #132237;
  --text-muted: #516682;
  --accent-pink: #F00F66;
}
```

To customize, modify the files in `webui_src/` (e.g. `common.css`, `common.js`, `advanced.css`, `advanced_setup.html`, `advanced_status.html`), then rebuild:

```bash
python3 tools/build_webui.py
```

---

## Troubleshooting

### Portal not appearing

1. Verify AP is active: Look for SSID in WiFi list
2. Check IP assignment: Should get 192.168.4.x
3. Try manual navigation: http://192.168.4.1

### Network scan empty

1. Wait for async scan to complete
2. Check Serial logs for scan status
3. Verify WiFi antenna connection

### Configuration not saving

1. Check callback return value (must return `true`)
2. Verify NVS has free space
3. Check Serial for error messages
