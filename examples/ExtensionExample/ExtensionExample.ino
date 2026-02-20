/**
 * @file ExtensionExample.ino
 * @brief Example showing how to extend CaptivePortal with custom modules
 * 
 * This demonstrates the Extension Points API:
 * - IStatusContributor: Add custom fields to /wml/status.json
 * - IRouteRegistrar: Add custom web endpoints
 * - IConfigSectionProvider: Add custom config fields
 * - ILoopHandler: Hook into the loop for periodic tasks
 */

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WiFiManagerLite.h>
#include <WMLCaptivePortal.h>
#include <WMLPortalExtensions.h>
#include <WMLStorage.h>

// ==================== Example Extension: Sensor Module ====================

/**
 * @brief Example sensor module that extends the CaptivePortal
 * 
 * This demonstrates all extension interfaces
 */
class SensorModule : public WML::IStatusContributor, 
                     public WML::IRouteRegistrar,
                     public WML::ILoopHandler {
public:
    SensorModule() : _lastReading(0), _sensorValue(0) {}
    
    // IStatusContributor: Add sensor data to status JSON
    void contribute(JsonDocument& doc) override {
        doc["sensorValue"] = _sensorValue;
        doc["sensorUnit"] = "°C";
        doc["sensorLastUpdate"] = _lastReading;
    }
    
    // IRouteRegistrar: Add custom API endpoint
    void registerRoutes(AsyncWebServer& server) override {
        server.on("/wml/ext/sensor", HTTP_GET, [this](AsyncWebServerRequest* request) {
            StaticJsonDocument<128> doc;
            doc["value"] = _sensorValue;
            doc["unit"] = "°C";
            doc["timestamp"] = _lastReading;
            
            AsyncResponseStream* response = request->beginResponseStream("application/json");
            serializeJson(doc, *response);
            request->send(response);
        });
        
        server.on("/wml/ext/sensor/calibrate", HTTP_POST, [this](AsyncWebServerRequest* request) {
            // Example calibration endpoint
            _sensorValue = 0;  // Reset
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Calibrated\"}");
        });
    }
    
    // ILoopHandler: Read sensor periodically
    void loop() override {
        if (millis() - _lastReading > 5000) {  // Every 5 seconds
            // Simulate sensor reading (replace with actual sensor code)
            _sensorValue = 20.0f + (random(0, 100) / 100.0f) * 5.0f;
            _lastReading = millis();
        }
    }
    
    float getSensorValue() const { return _sensorValue; }
    
private:
    uint32_t _lastReading;
    float _sensorValue;
};

// ==================== Main Application ====================

AsyncWebServer server(80);
WML::WiFiManagerLite wifiManager;
WML::CaptivePortal portal(server, wifiManager);
WML::Storage storage;
WML::StorageProvider storageProvider(storage);

// Note: Debug logging is now compile-time controlled via WML_ENABLE_DEBUG_LOGS
// Set to 0 in WMLBuildConfig.h to disable all debug output

// Extension modules
SensorModule sensorModule;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== WiFiManagerLite Extension Example ===\n");
    
    // Configure WiFi Manager
    wifiManager.setConfigProvider(&storageProvider);
    // Note: setLogger() is deprecated - debug output controlled by WML_ENABLE_DEBUG_LOGS
    wifiManager.setIdentityBaseName("ExtensionDemo");
    
    // Configure Portal
    // portal.setDeviceName(...) is optional now; if not explicitly set,
    // the portal uses wifiManager identity base name for branding.
    portal.setFirmwareVersion("1.0.0");
    
    portal.onConfigChange([](const WML::Config& config) {
        Serial.println("Config changed!");
        return storageProvider.updateConfig(config);
    });
    
    portal.onConfigGet([]() {
        return storageProvider.getConfig();
    });
    
    // ==================== Register Extensions ====================
    
    // Register sensor module as status contributor
    portal.addStatusContributor(&sensorModule);
    Serial.println("✓ Registered SensorModule as StatusContributor");
    
    // Register sensor module for custom routes
    portal.addRouteRegistrar(&sensorModule);
    Serial.println("✓ Registered SensorModule as RouteRegistrar");
    
    // Register sensor module for loop processing
    portal.addLoopHandler(&sensorModule);
    Serial.println("✓ Registered SensorModule as LoopHandler");
    
    // Start everything
    wifiManager.begin();
    portal.begin();
    server.begin();
    
    Serial.println("\n=== Setup Complete ===");
    Serial.println("Extension endpoints:");
    Serial.println("  GET  /wml/ext/sensor          - Get sensor data");
    Serial.println("  POST /wml/ext/sensor/calibrate - Calibrate sensor");
    Serial.println("  GET  /wml/status.json         - Includes sensor fields");
}

void loop() {
    wifiManager.loop();
    portal.loop();
    
    // Print sensor value every 10 seconds
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint > 10000) {
        Serial.printf("Sensor: %.2f°C\n", sensorModule.getSensorValue());
        lastPrint = millis();
    }
}
