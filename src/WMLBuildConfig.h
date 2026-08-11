/**
 * @file WMLBuildConfig.h
 * @brief Compile-time configuration and feature gates for WiFiManagerLite
 * 
 * This file contains all compile-time switches for optional subsystems.
 * Override defaults by defining values BEFORE including this header
 * or via compiler flags: -DWML_ENABLE_CAPTIVE_PORTAL=0
 * 
 * @author WiFiManagerLite
 * @version 1.0.0
 */

#ifndef WML_BUILD_CONFIG_H
#define WML_BUILD_CONFIG_H

// ============================================================
// Feature Gates (compile-time only, no runtime overhead)
// ============================================================

// Captive Portal with Web UI (requires ESPAsyncWebServer)
// Set to 0 to disable and save ~20KB flash
#ifndef WML_ENABLE_CAPTIVE_PORTAL
#define WML_ENABLE_CAPTIVE_PORTAL 1
#endif

// Captive Portal UI Variant
// 0 = Basic (simple: network list, password, save - minimal size)
// 1 = Advanced (full: dual SSID, static IP, BSSID lock, status page, reset buttons)
#ifndef WML_PORTAL_VARIANT
#define WML_PORTAL_VARIANT 0
#endif

// Convenience aliases
#define WML_PORTAL_BASIC    0
#define WML_PORTAL_ADVANCED 1

// NVS Storage support (requires NVSConfigBus library)
// Set to 0 if you handle storage yourself
#ifndef WML_ENABLE_STORAGE
#define WML_ENABLE_STORAGE 1
#endif

// mDNS support (for hostname.local resolution)
#ifndef WML_ENABLE_MDNS
#define WML_ENABLE_MDNS 1
#endif

// Dual SSID support (primary + fallback network)
// Set to 0 if you only need single SSID to save RAM
#ifndef WML_ENABLE_DUAL_SSID
#define WML_ENABLE_DUAL_SSID 1
#endif

// Static IP configuration support
#ifndef WML_ENABLE_STATIC_IP
#define WML_ENABLE_STATIC_IP 1
#endif

// BSSID lock support (connect to specific access point)
#ifndef WML_ENABLE_BSSID_LOCK
#define WML_ENABLE_BSSID_LOCK 1
#endif

// Debug logging (disable for production to save flash)
#ifndef WML_ENABLE_DEBUG_LOGS
#define WML_ENABLE_DEBUG_LOGS 1
#endif

// ============================================================
// WiFi Connection Settings
// ============================================================

// Connection timeout (milliseconds)
// Time to wait for WiFi connection before giving up
#ifndef WML_CONNECT_TIMEOUT_MS
#define WML_CONNECT_TIMEOUT_MS 8000
#endif

// Retry interval (milliseconds)
// Time between reconnection attempts
#ifndef WML_RETRY_INTERVAL_MS
#define WML_RETRY_INTERVAL_MS 10000
#endif

// Max retries before AP mode
// Number of failed attempts before starting AP mode
#ifndef WML_MAX_RETRIES_BEFORE_AP
#define WML_MAX_RETRIES_BEFORE_AP 2
#endif

// AP mode auto-timeout (milliseconds, 0 = disabled)
// Auto-restart after this time in AP mode without client activity
#ifndef WML_AP_AUTO_TIMEOUT_MS
#define WML_AP_AUTO_TIMEOUT_MS 0
#endif

// ============================================================
// Access Point (AP) Mode Settings
// ============================================================

// Default AP SSID prefix (MAC suffix will be appended)
// Example: "ESP-Setup-" → "ESP-Setup-a1b2c3d4"
#ifndef WML_AP_SSID_PREFIX
#define WML_AP_SSID_PREFIX "ESP-Setup-"
#endif

// Append MAC suffix to AP SSID (1 = "Prefix-a1b2c3d4", 0 = "Prefix" only).
// Disable only for single-device products; a trailing '-' is trimmed.
#ifndef WML_AP_APPEND_MAC
#define WML_AP_APPEND_MAC 1
#endif

// Default AP password (empty = open network)
#ifndef WML_AP_PASSWORD
#define WML_AP_PASSWORD ""
#endif

// AP channel (1-13)
#ifndef WML_AP_CHANNEL
#define WML_AP_CHANNEL 1
#endif

// AP max connections
#ifndef WML_AP_MAX_CONNECTIONS
#define WML_AP_MAX_CONNECTIONS 4
#endif

// AP hidden SSID (0 = visible, 1 = hidden)
#ifndef WML_AP_HIDDEN
#define WML_AP_HIDDEN 0
#endif

// ============================================================
// Web Server Settings (only if WML_ENABLE_CAPTIVE_PORTAL == 1)
// ============================================================

// Default HTTP port
#ifndef WML_HTTP_PORT
#define WML_HTTP_PORT 80
#endif

// Web UI authentication (empty = no auth)
#ifndef WML_WEB_AUTH_USER
#define WML_WEB_AUTH_USER ""
#endif

#ifndef WML_WEB_AUTH_PASS
#define WML_WEB_AUTH_PASS ""
#endif

// Network scan cache duration (milliseconds)
#ifndef WML_SCAN_CACHE_MS
#define WML_SCAN_CACHE_MS 10000
#endif

// Client activity timeout (milliseconds)
#ifndef WML_CLIENT_TIMEOUT_MS
#define WML_CLIENT_TIMEOUT_MS 30000
#endif

// Restart delay after config save (milliseconds)
#ifndef WML_RESTART_DELAY_MS
#define WML_RESTART_DELAY_MS 600
#endif

// ============================================================
// NVS Storage Settings (only if WML_ENABLE_STORAGE == 1)
// ============================================================

// NVS namespace for WiFiManagerLite data
#ifndef WML_NVS_NAMESPACE
#define WML_NVS_NAMESPACE "wml"
#endif

// NVS module ID (key within namespace)
#ifndef WML_NVS_MODULE_ID
#define WML_NVS_MODULE_ID "cfg"
#endif

// ============================================================
// Default Device Settings
// ============================================================

// Default device name (shown in UI and used for mDNS)
#ifndef WML_DEFAULT_DEVICE_NAME
#define WML_DEFAULT_DEVICE_NAME "ESP-Device"
#endif

// Default firmware version string
#ifndef WML_FIRMWARE_VERSION
#define WML_FIRMWARE_VERSION "2.6.5"
#endif

// ============================================================
// Platform Checks
// ============================================================

#ifndef ESP32
#warning "WiFiManagerLite is optimized for ESP32. Other platforms may have limited functionality."
#endif

// ============================================================
// Feature Dependency Checks
// ============================================================

#if WML_ENABLE_CAPTIVE_PORTAL && !WML_ENABLE_MDNS
#warning "Captive Portal works best with mDNS enabled (WML_ENABLE_MDNS=1)"
#endif

// ============================================================
// Debug Macros - See WMLDebugLog.h for implementation
// ============================================================
// Note: WMLDebugLog.h provides the actual macro implementations
// based on WML_ENABLE_DEBUG_LOGS setting above.
// 
// Available macros (when WML_ENABLE_DEBUG_LOGS=1):
//   WML_LOG(msg)              - Simple log with prefix
//   WML_LOGF(fmt, ...)        - Formatted log (printf-style)
//   WML_LOG_PORTAL(msg)       - Portal-specific log
//   WML_LOGF_PORTAL(fmt, ...) - Portal-specific formatted log
//   WML_ERROR(msg)            - Error log (always enabled)
//   WML_ERRORF(fmt, ...)      - Error formatted (always enabled)
//
// When WML_ENABLE_DEBUG_LOGS=0, all debug macros expand to nothing
// and are completely removed by the compiler (saves ~2-4KB flash).

#endif // WML_BUILD_CONFIG_H
