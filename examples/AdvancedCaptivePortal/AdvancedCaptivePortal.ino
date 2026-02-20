/**
 * @file AdvancedCaptivePortal.ino
 * @brief Advanced WiFiManagerLite example with full-featured Captive Portal
 * 
 * This example demonstrates advanced features:
 * - Advanced Portal UI variant (full-featured web interface)
 * - Custom status page with system information
 * - Extended API endpoints (JSON, system info, network scan)
 * - Real-time status updates
 * - Uptime tracking
 * - Advanced event handling
 * - Custom routes and extensions
 * - Network statistics
 * 
 * Web Pages:
 * - /           → Status page (when connected) or WiFi Setup (in AP mode)
 * - /wml/setup  → Advanced WiFi configuration (dual SSID, static IP, BSSID lock)
 * - /status     → Extended status page with system info
 * - /api/status → JSON status API
 * - /api/system → System information API
 * - /api/scan   → Network scan API
 * 
 * Required libraries:
 * - ESPAsyncWebServer
 * - AsyncTCP
 * - ArduinoJson
 * - NVSUtilityLibrary
 * 
 * Enable Advanced Portal:
 * #define WML_PORTAL_VARIANT WML_PORTAL_ADVANCED
 */

// Enable Advanced Portal Variant
#define WML_PORTAL_VARIANT WML_PORTAL_ADVANCED

#include <WiFi.h>
#include <wifiMangerLite.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "AdvancedCaptivePortalPages.h"

// ==================== Global Objects ====================

AsyncWebServer server(80);
WML::WiFiManagerLite wifiMgr;
WML::CaptivePortal portal(server, wifiMgr);

// Storage using NVSConfigBus (MessagePack, no Strings)
WML::Storage storage("wificfg", "wifi");
WML::StorageProvider configProvider(storage);

// System tracking
uint32_t bootTime = 0;
uint32_t lastScanTime = 0;
int lastScanCount = 0;

// Helpers + HTML moved to AdvancedCaptivePortalPages.h (formatUptime, buildExtendedStatusPageHtml)

// ==================== Setup Custom Routes ====================

void setupCustomRoutes() {
    // Extended status page - main page when connected
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (wifiMgr.isAPMode()) {
            request->redirect("/wml/setup");
        } else {
            const uint32_t uptime = (millis() - bootTime) / 1000;
            request->send(200, "text/html", buildExtendedStatusPageHtml(wifiMgr, configProvider.getConfig(), uptime));
        }
    });
    
    // Dedicated status page
    server.on("/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        const uint32_t uptime = (millis() - bootTime) / 1000;
        request->send(200, "text/html", buildExtendedStatusPageHtml(wifiMgr, configProvider.getConfig(), uptime));
    });
    
    // Extended JSON API for status
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        WML::Config cfg = configProvider.getConfig();
        uint32_t uptime = (millis() - bootTime) / 1000;
        
        DynamicJsonDocument doc(1024);
        doc["connected"] = wifiMgr.isConnected();
        doc["apMode"] = wifiMgr.isAPMode();
        doc["ip"] = wifiMgr.isConnected() ? wifiMgr.getStationIP().toString() : wifiMgr.getAPIP().toString();
        doc["ssid"] = wifiMgr.isConnected() ? wifiMgr.getSSID() : "";
        doc["bssid"] = wifiMgr.isConnected() ? wifiMgr.getBSSID() : "";
        doc["rssi"] = wifiMgr.isConnected() ? wifiMgr.getRSSI() : 0;
        doc["mac"] = WiFi.macAddress();
        doc["deviceName"] = cfg.deviceName;
        doc["uptime"] = uptime;
        doc["uptimeFormatted"] = formatUptime(uptime);
        doc["freeHeap"] = ESP.getFreeHeap();
        doc["retryCount"] = wifiMgr.getRetryCount();
        
        char jsonBuffer[1024];
        serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
        request->send(200, "application/json", jsonBuffer);
    });
    
    // System information API
    server.on("/api/system", HTTP_GET, [](AsyncWebServerRequest* request) {
        uint32_t uptime = (millis() - bootTime) / 1000;
        
        DynamicJsonDocument doc(512);
        doc["chipModel"] = ESP.getChipModel();
        doc["chipRevision"] = ESP.getChipRevision();
        doc["cpuFreqMHz"] = ESP.getCpuFreqMHz();
        doc["freeHeap"] = ESP.getFreeHeap();
        doc["heapSize"] = ESP.getHeapSize();
        doc["uptime"] = uptime;
        doc["uptimeFormatted"] = formatUptime(uptime);
        doc["bootTime"] = bootTime;
        doc["firmwareVersion"] = "1.2.0";
        
        char jsonBuffer[512];
        serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
        request->send(200, "application/json", jsonBuffer);
    });
    
    // Network scan API
    server.on("/api/scan", HTTP_GET, [](AsyncWebServerRequest* request) {
        int n = WiFi.scanComplete();
        
        if (n == WIFI_SCAN_RUNNING) {
            request->send(202, "application/json", "{\"status\":\"scanning\"}");
            return;
        }
        
        if (n == WIFI_SCAN_FAILED || n < 0) {
            // Trigger new scan
            WiFi.scanNetworks(true, true);
            request->send(202, "application/json", "{\"status\":\"scanning\"}");
            return;
        }
        
        DynamicJsonDocument doc(2048);
        JsonArray networks = doc.createNestedArray("networks");
        
        for (int i = 0; i < n && i < 20; i++) {
            JsonObject net = networks.createNestedObject();
            net["ssid"] = WiFi.SSID(i);
            net["bssid"] = WiFi.BSSIDstr(i);
            net["rssi"] = WiFi.RSSI(i);
            net["channel"] = WiFi.channel(i);
            net["encrypted"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        }
        
        doc["count"] = n;
        doc["lastScan"] = lastScanTime;
        
        char jsonBuffer[2048];
        serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
        request->send(200, "application/json", jsonBuffer);
        
        WiFi.scanDelete();
    });
}

// ==================== Setup ====================

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n==========================================");
    Serial.println("  WiFiManagerLite - Advanced Captive Portal");
    Serial.println("  Full-featured example with extended APIs");
    Serial.println("==========================================\n");
    
    bootTime = millis();
    
    // Load configuration from NVS
    WML::Config config = configProvider.getConfig();
    
    // Set defaults if no config exists
    if (!config.primary.isValid()) {
        Serial.println("No saved config, setting defaults...");
        // v3.0: Use setter methods for char[] fields
        config.setDeviceName("ESP-Advanced");
        config.primary.setSsid("YOUR_SSID");
        config.primary.setPassword("YOUR_PASSWORD");
        // AP SSID will be derived from identity base name (see below) unless explicitly set
        configProvider.updateConfig(config);
    }
    
    // v3.0: char[] fields directly usable
    Serial.printf("Device: %s\n", config.deviceName);
    Serial.printf("SSID: %s\n", config.primary.ssid);
    
    // Setup WiFi Manager with StorageProvider
    wifiMgr.setConfigProvider(&configProvider);
    // One-place naming: AP = BaseName+MAC, mDNS = basename+MAC.local, Portal branding = BaseName
    wifiMgr.setIdentityBaseName(config.deviceName);
    
    // Advanced event handling
    wifiMgr.onEvent([](WML::Event event, const String& info) {
        switch (event) {
            case WML::Event::Connecting:
                Serial.printf("→ Connecting to: %s\n", info.c_str());
                break;
            case WML::Event::Connected:
                Serial.printf("✓ Connected! %s\n", info.c_str());
                Serial.printf("  IP: %s | BSSID: %s | RSSI: %d dBm\n",
                    wifiMgr.getStationIP().toString().c_str(),
                    wifiMgr.getBSSID().c_str(),
                    wifiMgr.getRSSI());
                Serial.printf("  Open http://%s for status page\n", 
                    wifiMgr.getStationIP().toString().c_str());
                break;
            case WML::Event::Disconnected:
                Serial.println("✗ Disconnected");
                break;
            case WML::Event::ConnectionFailed:
                Serial.printf("✗ Connection failed (attempt %d)\n", 
                    wifiMgr.getRetryCount() + 1);
                break;
            case WML::Event::APStarted:
                Serial.printf("★ AP Started: %s\n", info.c_str());
                Serial.printf("  → Connect to AP and open http://%s\n", 
                    wifiMgr.getAPIP().toString().c_str());
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
    
    // Setup Advanced Captive Portal
    // portal.setDeviceName(...) is optional now; if not explicitly set,
    // the portal uses wifiMgr identity base name for branding.
    portal.setFirmwareVersion("1.2.0");
    
    // Callback: Provide current config to web UI
    portal.onConfigGet([]() {
        return configProvider.getConfig();
    });
    
    // Callback: Save new config from web UI
    portal.onConfigChange([](const WML::Config& newConfig) -> bool {
        Serial.println("\n>>> New configuration received");
        // v3.0: char[] fields directly usable
        Serial.printf("  Device: %s\n", newConfig.deviceName);
        Serial.printf("  Primary SSID: %s\n", newConfig.primary.ssid);
        Serial.printf("  BSSID Lock: %s\n", newConfig.primary.bssidLock ? "Yes" : "No");
        if (newConfig.secondary.ssid[0] != '\0') {
            Serial.printf("  Fallback SSID: %s\n", newConfig.secondary.ssid);
        }
        if (newConfig.staticIP.ip[0] != '\0') {
            Serial.printf("  Static IP: %s\n", newConfig.staticIP.ip);
        }
        
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
    
    // Setup custom routes BEFORE portal.begin()
    setupCustomRoutes();
    
    // Initialize portal routes (Advanced variant)
    portal.begin();
    
    // Start web server
    server.begin();
    Serial.println("Web server started on port 80");
    
    Serial.println("\n----------------------------------------");
    Serial.println("Available pages:");
    Serial.println("  /           - Status page (or WiFi setup in AP mode)");
    Serial.println("  /status     - Extended status page");
    Serial.println("  /wml/setup  - Advanced WiFi configuration");
    Serial.println("  /api/status - JSON status API");
    Serial.println("  /api/system - System information API");
    Serial.println("  /api/scan   - Network scan API");
    Serial.println("----------------------------------------\n");
}

// ==================== Loop ====================

void loop() {
    wifiMgr.loop();
    portal.loop();
    
    // Periodic network scan (every 5 minutes)
    static uint32_t lastAutoScan = 0;
    if (millis() - lastAutoScan > 300000) {
        lastAutoScan = millis();
        if (wifiMgr.isAPMode()) {
            Serial.println("Scanning networks...");
            WiFi.scanNetworks(true, true);
        }
    }
    
    // Update scan results
    int scanResult = WiFi.scanComplete();
    if (scanResult > 0) {
        lastScanTime = millis();
        lastScanCount = scanResult;
        Serial.printf("Network scan complete: %d networks found\n", scanResult);
        WiFi.scanDelete();
    }
    
    // Status output every 30 seconds
    static uint32_t lastStatus = 0;
    if (millis() - lastStatus > 30000) {
        lastStatus = millis();
        
        uint32_t uptime = (millis() - bootTime) / 1000;
        
        Serial.print("[Status] ");
        if (wifiMgr.isConnected()) {
            Serial.printf("Connected | IP: %s | RSSI: %d dBm | Uptime: %s | Heap: %d KB\n",
                wifiMgr.getStationIP().toString().c_str(),
                wifiMgr.getRSSI(),
                formatUptime(uptime).c_str(),
                ESP.getFreeHeap() / 1024);
        } else if (wifiMgr.isAPMode()) {
            Serial.printf("AP Mode | IP: %s | Clients: %d | Uptime: %s\n",
                wifiMgr.getAPIP().toString().c_str(),
                wifiMgr.getAPClientCount(),
                formatUptime(uptime).c_str());
        } else {
            Serial.printf("Connecting... (attempt %d) | Uptime: %s\n",
                wifiMgr.getRetryCount() + 1,
                formatUptime(uptime).c_str());
        }
    }
}
