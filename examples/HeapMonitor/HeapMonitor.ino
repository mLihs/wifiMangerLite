/**
 * @file HeapMonitor.ino
 * @brief WiFiManagerLite example with detailed heap fragmentation monitoring
 * 
 * Features:
 * - Complete WiFiManagerLite functionality (Captive Portal, NVS Storage)
 * - Detailed heap monitoring every 30 seconds
 * - Heap metrics: free, largest block, fragmentation %, drift, minFree
 * - Log format: [Heap] TICK 08:09:12: free=112032, largest=49140, frag=56.1%, drift=1204, minFree=45468
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

#ifdef ESP32
#include <esp_heap_caps.h>
#endif

// ==================== Global Objects ====================

AsyncWebServer server(80);
WML::WiFiManagerLite wifiMgr;
WML::CaptivePortal portal(server, wifiMgr);

// Storage using NVSConfigBus (MessagePack, no Strings)
WML::Storage storage("wificfg", "wifi");
WML::StorageProvider configProvider(storage);

// ==================== Heap Monitoring ====================

struct HeapMonitor {
    uint32_t lastFree = 0;
    uint32_t minFree = UINT32_MAX;
    uint32_t lastTickTime = 0;
    bool initialized = false;
    
    void init() {
        lastFree = ESP.getFreeHeap();
        minFree = lastFree;
        lastTickTime = millis();
        initialized = true;
    }
    
    void tick() {
        if (!initialized) {
            init();
            return;
        }
        
        uint32_t now = millis();
        if (now - lastTickTime < 30000) {
            return; // Not yet 30 seconds
        }
        lastTickTime = now;
        
        // Get current heap metrics
        uint32_t free = ESP.getFreeHeap();
        uint32_t largest = ESP.getMaxAllocHeap();
        
        // Calculate fragmentation percentage
        float frag = 0.0f;
        if (free > 0) {
            frag = ((float)(free - largest) / (float)free) * 100.0f;
        }
        
        // Calculate drift (change since last tick)
        int32_t drift = (int32_t)free - (int32_t)lastFree;
        
        // Update minimum
        if (free < minFree) {
            minFree = free;
        }
        
        // Format time (HH:MM:SS)
        uint32_t uptimeSeconds = now / 1000;
        uint8_t hours = (uptimeSeconds / 3600) % 24;
        uint8_t minutes = (uptimeSeconds / 60) % 60;
        uint8_t seconds = uptimeSeconds % 60;
        
        // Print heap metrics
        Serial.printf("[Heap] TICK %02d:%02d:%02d: free=%u, largest=%u, frag=%.1f%%, drift=%+d, minFree=%u\n",
            hours, minutes, seconds,
            free, largest, frag, drift, minFree);
        
        // Update for next tick
        lastFree = free;
    }
};

HeapMonitor heapMonitor;

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
    
    // JSON API for status (includes heap metrics)
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        WML::Config cfg = configProvider.getConfig();
        
        DynamicJsonDocument doc(512);
        doc["connected"] = wifiMgr.isConnected();
        doc["apMode"] = wifiMgr.isAPMode();
        doc["ip"] = wifiMgr.isConnected() ? wifiMgr.getStationIP().toString() : wifiMgr.getAPIP().toString();
        doc["ssid"] = wifiMgr.isConnected() ? wifiMgr.getSSID() : "";
        doc["rssi"] = wifiMgr.isConnected() ? wifiMgr.getRSSI() : 0;
        doc["mac"] = WiFi.macAddress();
        doc["deviceName"] = cfg.deviceName;
        doc["freeHeap"] = ESP.getFreeHeap();
        doc["largestBlock"] = ESP.getMaxAllocHeap();
        doc["minFreeHeap"] = heapMonitor.minFree;
        
        // Calculate fragmentation
        uint32_t free = ESP.getFreeHeap();
        uint32_t largest = ESP.getMaxAllocHeap();
        float frag = 0.0f;
        if (free > 0) {
            frag = ((float)(free - largest) / (float)free) * 100.0f;
        }
        doc["fragmentation"] = frag;
        
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
    Serial.println("  WiFiManagerLite - Heap Monitor Demo");
    Serial.println("  with NVS Storage (MessagePack)");
    Serial.println("========================================\n");
    
    // Initialize heap monitor
    heapMonitor.init();
    
    // Print initial heap state
    uint32_t free = ESP.getFreeHeap();
    uint32_t largest = ESP.getMaxAllocHeap();
    float frag = 0.0f;
    if (free > 0) {
        frag = ((float)(free - largest) / (float)free) * 100.0f;
    }
    Serial.printf("[Heap] INIT: free=%u, largest=%u, frag=%.1f%%\n", free, largest, frag);
    Serial.println();
    
    // Load configuration from NVS
    WML::Config config = configProvider.getConfig();
    // v3.0: char[] fields directly usable
    Serial.printf("Device: %s\n", config.deviceName);
    Serial.printf("SSID: %s\n", config.primary.ssid);
    
    // Setup WiFi Manager with StorageProvider
    wifiMgr.setConfigProvider(&configProvider);
    wifiMgr.setIdentityBaseName("Homewind");
    
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
    
    // Callback: WiFi reset - also clear credentials
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
    Serial.println("  /api/status - JSON status API (includes heap metrics)");
    Serial.println("----------------------------------------");
    Serial.println("\nHeap monitoring: Every 30 seconds");
    Serial.println("Format: [Heap] TICK HH:MM:SS: free=..., largest=..., frag=...%, drift=..., minFree=...\n");
}

// ==================== Loop ====================

void loop() {
    wifiMgr.loop();
    portal.loop();
    
    // Heap monitoring tick (every 30 seconds)
    heapMonitor.tick();
}
