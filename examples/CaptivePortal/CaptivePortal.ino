/**
 * @file CaptivePortal.ino
 * @brief Complete WiFiManagerLite example with Captive Portal and NVS Storage
 * 
 * Features:
 * - WiFi connection with automatic AP fallback
 * - Beautiful web-based configuration interface (Homewind Design)
 * - Status page showing current settings
 * - Reset buttons (WiFi Reset & Factory Reset)
 * - Persistent settings using WML::Storage (NVSConfigBus + MessagePack)
 * - Network scanning and selection
 * 
 * Web Pages:
 * - /           → Status page (when connected) or WiFi Setup (in AP mode)
 * - /wml/setup  → WiFi configuration
 * - /status     → Current settings and reset options
 * 
 * Required libraries:
 * - ESPAsyncWebServer
 * - AsyncTCP
 * - ArduinoJson
 * - NVSUtilityLibrary
 */

#include <WiFi.h>
#include <wifiMangerLite.h>
#include <ESPAsyncWebServer.h>
#include "CaptivePortalPages.h"

// ==================== Global Objects ====================

AsyncWebServer server(80);
WML::WiFiManagerLite wifiMgr;
WML::CaptivePortal portal(server, wifiMgr);

// Storage using NVSConfigBus (MessagePack, no Strings)
WML::Storage storage("wificfg", "wifi");
WML::StorageProvider configProvider(storage);

// Note: Debug logging is now compile-time controlled via WML_ENABLE_DEBUG_LOGS
// Set to 0 in WMLBuildConfig.h to disable all debug output (saves ~2-4KB flash)

// Status page HTML moved to CaptivePortalPages.h (buildStatusPageHtml)

// ==================== Setup Custom Routes ====================

void setupCustomRoutes() {
    // Status page - main page when connected
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (wifiMgr.isAPMode()) {
            request->redirect("/wml/setup");
        } else {
            request->send(200, "text/html", buildStatusPageHtml(wifiMgr, configProvider.getConfig()));
        }
    });
    
    // Dedicated status page
    server.on("/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/html", buildStatusPageHtml(wifiMgr, configProvider.getConfig()));
    });
    
    // JSON API for status
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        WML::Config cfg = configProvider.getConfig();
        
        // Build JSON without String concatenation where possible
        DynamicJsonDocument doc(512);
        doc["connected"] = wifiMgr.isConnected();
        doc["apMode"] = wifiMgr.isAPMode();
        doc["ip"] = wifiMgr.isConnected() ? wifiMgr.getStationIP().toString() : wifiMgr.getAPIP().toString();
        doc["ssid"] = wifiMgr.isConnected() ? wifiMgr.getSSID() : "";
        doc["rssi"] = wifiMgr.isConnected() ? wifiMgr.getRSSI() : 0;
        doc["mac"] = WiFi.macAddress();
        doc["deviceName"] = cfg.deviceName;
        doc["freeHeap"] = ESP.getFreeHeap();
        
        char jsonBuffer[512];
        serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
        request->send(200, "application/json", jsonBuffer);
    });
}

// ==================== Setup ====================

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n========================================");
    Serial.println("  WiFiManagerLite - Captive Portal Demo");
    Serial.println("  with NVS Storage (MessagePack)");
    Serial.println("========================================\n");
    
    // Load configuration from NVS
    WML::Config config = configProvider.getConfig();
    // v3.0: char[] fields directly usable
    Serial.printf("Device: %s\n", config.deviceName);
    Serial.printf("SSID: %s\n", config.primary.ssid);
    
    // Setup WiFi Manager with StorageProvider
    wifiMgr.setConfigProvider(&configProvider);
    // Single-place naming:
    // - AP SSID: "Homewind-xxxxxxxx"
    // - mDNS: "homewind-xxxxxxxx.local"
    // - Portal branding: "Homewind" (no MAC)
    wifiMgr.setIdentityBaseName("Homewind");
    // Note: setLogger() is deprecated - debug output controlled by WML_ENABLE_DEBUG_LOGS
    
    wifiMgr.onEvent([](WML::Event event, const String& info) {
        switch (event) {
            case WML::Event::Connecting:
                Serial.printf("→ Connecting to: %s\n", info.c_str());
                break;
            case WML::Event::Connected:
                Serial.printf("✓ Connected! %s\n", info.c_str());
                Serial.printf("  Open http://%s for status page\n", wifiMgr.getStationIP().toString().c_str());
                break;
            case WML::Event::Disconnected:
                Serial.println("✗ Disconnected");
                break;
            case WML::Event::APStarted:
                Serial.printf("★ AP Started: %s\n", info.c_str());
                Serial.printf("  → Connect to AP and open http://%s\n", wifiMgr.getAPIP().toString().c_str());
                break;
            case WML::Event::APStopped:
                Serial.println("○ AP Stopped");
                break;
            default:
                break;
        }
    });
    
    // Start WiFi Manager
    wifiMgr.begin();
    
    // Setup Captive Portal
    // Note: setLogger() is deprecated - debug output controlled by WML_ENABLE_DEBUG_LOGS
    // portal.setDeviceName(...) is optional now; if not explicitly set,
    // the portal uses wifiMgr identity base name for branding.

    portal.setFirmwareVersion("1.1.0");
    
    // Callback: Provide current config to web UI
    portal.onConfigGet([]() {
        return configProvider.getConfig();
    });
    
    // Callback: Save new config from web UI
    portal.onConfigChange([](const WML::Config& newConfig) -> bool {
        Serial.println("\n>>> New configuration received");
        bool saved = configProvider.updateConfig(newConfig);
        if (saved) {
            Serial.println(">>> Configuration saved to NVS (MessagePack)");
        }
        return saved;
    });
    
    // Callback: Factory reset - clear storage
    portal.onFactoryReset([]() {
        Serial.println(">>> Factory reset - clearing NVS storage");
        configProvider.clearConfig();
    });
    
    // Callback: WiFi reset - also clear credentials (not just switch to AP mode)
    portal.onWiFiReset([]() {
        Serial.println(">>> WiFi reset - clearing WiFi credentials");
        configProvider.clearConfig();
    });
    
    // Setup custom routes BEFORE portal.begin()
    setupCustomRoutes();
    
    // Initialize portal routes
    portal.begin();
    
    // Start web server
    server.begin();
    Serial.println("Web server started on port 80");
    
    Serial.println("\n----------------------------------------");
    Serial.println("Available pages:");
    Serial.println("  /           - Status page (or WiFi setup in AP mode)");
    Serial.println("  /status     - Status page");
    Serial.println("  /wml/setup  - WiFi configuration");
    Serial.println("  /api/status - JSON status API");
    Serial.println("----------------------------------------\n");
}

// ==================== Loop ====================

void loop() {
    wifiMgr.loop();
    portal.loop();
    
    // Status output every 30 seconds
    static uint32_t lastStatus = 0;
    if (millis() - lastStatus > 30000) {
        lastStatus = millis();
        
        Serial.print("[Status] ");
        if (wifiMgr.isConnected()) {
            Serial.printf("Connected | IP: %s | RSSI: %d dBm | Heap: %d KB\n",
                wifiMgr.getStationIP().toString().c_str(),
                wifiMgr.getRSSI(),
                ESP.getFreeHeap() / 1024);
        } else if (wifiMgr.isAPMode()) {
            Serial.printf("AP Mode | IP: %s | Clients: %d\n",
                wifiMgr.getAPIP().toString().c_str(),
                wifiMgr.getAPClientCount());
        } else {
            Serial.printf("Connecting... (attempt %d)\n", wifiMgr.getRetryCount() + 1);
        }
    }
}
