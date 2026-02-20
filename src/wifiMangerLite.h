/**
 * @file wifiMangerLite.h
 * @brief Main include file for WiFiManagerLite library
 * 
 * A lightweight, modular WiFi connection manager for ESP32
 * with optional web-based captive portal and NVS storage.
 * 
 * Configure features via WMLBuildConfig.h or compiler flags:
 *   -DWML_ENABLE_CAPTIVE_PORTAL=0  (disable web UI)
 *   -DWML_ENABLE_STORAGE=0         (disable NVS storage)
 *   -DWML_ENABLE_DEBUG_LOGS=0      (disable debug output, saves ~2-4KB flash)
 * 
 * @author Extracted from BambuBeacon project
 * @version 2.2.0
 * @license MIT
 */

#pragma once

// Build configuration (must be first!)
#include "WMLBuildConfig.h"

// Debug logging macros (compile-time gated)
#include "WMLDebugLog.h"

// Core components (always included)
#include "WMLConfig.h"
#include "WiFiManagerLite.h"

// Legacy logger (deprecated - use WML_ENABLE_DEBUG_LOGS instead)
// Uncomment if you need backwards compatibility with custom ILogger implementations
// #include "WMLLogger.h"

// Persistent storage (optional, requires NVSConfigBus library)
#if WML_ENABLE_STORAGE
#include "WMLStorage.h"
#endif

// Captive Portal with Web UI (optional, requires ESPAsyncWebServer)
#if WML_ENABLE_CAPTIVE_PORTAL
#include "WMLCaptivePortal.h"
#endif

// ============================================================
// Convenience Type Aliases
// ============================================================

using WMLConfig = WML::Config;
using WMLCredentials = WML::WiFiCredentials;
using WMLStaticIP = WML::StaticIPConfig;
using WMLAPConfig = WML::APConfig;
using WMLTiming = WML::TimingConfig;

#if WML_ENABLE_CAPTIVE_PORTAL
using WMLCaptivePortal = WML::CaptivePortal;
#endif

#if WML_ENABLE_STORAGE
using WMLStorage = WML::Storage;
using WMLStorageProvider = WML::StorageProvider;
#endif