/**
 * @file BasicUsage.ino
 * @brief Basic example for WiFiManagerLite with NVS Storage
 * 
 * Demonstrates simple WiFi connection with AP fallback.
 * Uses WML::Storage for persistent configuration.
 * If credentials are invalid, device starts in AP mode for configuration.
 * 
 * Required libraries:
 * - ArduinoJson
 * - NVSUtilityLibrary
 */

#include <WiFi.h>
#include <wifiMangerLite.h>

// ==================== Global Objects ====================

WML::WiFiManagerLite wifiMgr;

// Storage using NVSConfigBus (MessagePack, no Strings)
WML::Storage storage("wificfg", "wifi");
WML::StorageProvider configProvider(storage);

// Note: Logging is now compile-time controlled via WML_ENABLE_DEBUG_LOGS
// Set to 0 in WMLBuildConfig.h to disable all debug output (saves ~2-4KB flash)

// ==================== Setup ====================

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\nWiFiManagerLite Basic Example");
    Serial.println("==============================\n");
    
    // Load configuration from NVS (or use defaults)
    WML::Config config = configProvider.getConfig();
    
    // If no config saved, set initial values
    if (!config.primary.isValid()) {
        Serial.println("No saved config, setting defaults...");
        
        // v3.0: Use setter methods for char[] fields
        config.setDeviceName("ESP-Basic");
        config.primary.setSsid("YOUR_SSID");
        config.primary.setPassword("YOUR_PASSWORD");
        
        // Optional: Secondary/backup network
        // config.secondary.setSsid("BACKUP_SSID");
        // config.secondary.setPassword("BACKUP_PASSWORD");
        
        // AP SSID + mDNS hostname will be derived from the identity base name
        // (set below via wifiMgr.setIdentityBaseName)
        
        // Save to NVS
        configProvider.updateConfig(config);
        Serial.println("Default config saved to NVS");
    }
    
    // v3.0: char[] fields are directly usable as char*
    Serial.printf("Device: %s\n", config.deviceName);
    Serial.printf("SSID: %s\n", config.primary.ssid);
    
    // Setup WiFi Manager with StorageProvider
    wifiMgr.setConfigProvider(&configProvider);
    // Note: setLogger() is deprecated - debug output is controlled by WML_ENABLE_DEBUG_LOGS
    wifiMgr.setIdentityBaseName(config.deviceName);
    
    // Optional: Handle events
    wifiMgr.onEvent([](WML::Event event, const String& info) {
        switch (event) {
            case WML::Event::Connected:
                Serial.printf("✓ Connected: %s\n", info.c_str());
                break;
            case WML::Event::Disconnected:
                Serial.println("✗ Disconnected");
                break;
            case WML::Event::APStarted:
                Serial.printf("★ AP Started: %s\n", info.c_str());
                break;
            case WML::Event::ConnectionFailed:
                Serial.println("✗ Connection failed");
                break;
            default:
                break;
        }
    });
    
    // Start WiFi manager
    wifiMgr.begin();
}

// ==================== Loop ====================

void loop() {
    // Must call loop() regularly
    wifiMgr.loop();
    
    // Status output every 10 seconds
    static uint32_t lastStatus = 0;
    if (millis() - lastStatus > 10000) {
        lastStatus = millis();
        
        if (wifiMgr.isConnected()) {
            Serial.printf("Connected - IP: %s, RSSI: %d dBm\n",
                wifiMgr.getStationIP().toString().c_str(),
                wifiMgr.getRSSI());
        } else if (wifiMgr.isAPMode()) {
            Serial.printf("AP Mode - IP: %s, Clients: %d\n",
                wifiMgr.getAPIP().toString().c_str(),
                wifiMgr.getAPClientCount());
        } else {
            Serial.println("Connecting...");
        }
    }
    
    // Example: Serial commands
    if (Serial.available()) {
        char cmd = Serial.read();
        
        switch (cmd) {
            case 'r':  // Reset WiFi
                Serial.println("Resetting WiFi...");
                wifiMgr.reset();
                break;
                
            case 'f':  // Factory reset
                Serial.println("Factory reset...");
                wifiMgr.factoryReset([&]() {
                    configProvider.clearConfig();
                });
                break;
                
            case 's':  // Save new SSID (example)
                Serial.println("Enter new SSID:");
                // In real code, read from Serial...
                break;
                
            case 'i':  // Info
                {
                    WML::Config cfg = configProvider.getConfig();
                    Serial.println("\n--- Current Config ---");
                    // v3.0: char[] fields directly usable
                    Serial.printf("Device: %s\n", cfg.deviceName);
                    Serial.printf("SSID: %s\n", cfg.primary.ssid);
                    Serial.printf("Static IP: %s\n", cfg.staticIP.ip[0] != '\0' ? cfg.staticIP.ip : "DHCP");
                    Serial.println("----------------------\n");
                }
                break;
        }
    }
}
