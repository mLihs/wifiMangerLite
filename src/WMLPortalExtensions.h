/**
 * @file WMLPortalExtensions.h
 * @brief Extension interfaces for WiFiManagerLite Captive Portal
 * 
 * Allows external modules (sensors, controllers, etc.) to extend the portal
 * without modifying core CaptivePortal code.
 * 
 * ============================================================
 * EXTENSION CONTRACT - IMPORTANT!
 * ============================================================
 * 
 * All extension implementations MUST adhere to these rules:
 * 
 * 1. NON-BLOCKING: Extensions MUST NOT call delay() or block for more than 1ms
 * 2. NO WiFi MODIFICATION: Extensions MUST NOT modify WiFi state (connect/disconnect/scan)
 * 3. MEMORY SAFE: Extensions MUST NOT allocate large heap blocks (>512 bytes)
 * 4. EXCEPTION SAFE: Extensions MUST NOT throw exceptions
 * 5. THREAD SAFE: Extensions MAY be called from async web server context
 * 
 * Registry Limits:
 * - Maximum 8 extensions per type (kMaxExtensions)
 * - Registration returns false if limit reached
 * - Extensions are NOT owned by CaptivePortal (caller manages lifetime)
 * 
 * Lifecycle:
 * - Register extensions BEFORE calling portal.begin()
 * - Extensions remain registered until ESP restart
 * - No unregister mechanism (by design, keeps it simple)
 * 
 * ============================================================
 * 
 * @author Martin Lihs
 * @version 1.1.0
 */

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// Forward declarations
class AsyncWebServer;
class AsyncWebServerRequest;

namespace WML {

/**
 * @brief Interface for contributing additional status fields
 * 
 * Implement this to add custom fields to /wml/status.json
 * 
 * @warning MUST complete in <1ms, MUST NOT block or call delay()
 * @warning MUST NOT modify WiFi state or call WiFi.* functions
 * 
 * Example:
 * @code
 * class MyStatusContributor : public IStatusContributor {
 * public:
 *     void contribute(JsonDocument& doc) override {
 *         doc["myValue"] = _cachedValue;  // Use cached values!
 *         doc["myState"] = _state;
 *     }
 * private:
 *     float _cachedValue;  // Updated in loop(), not here
 * };
 * @endcode
 */
class IStatusContributor {
public:
    virtual ~IStatusContributor() = default;
    
    /**
     * @brief Add custom fields to the status JSON document
     * @param doc JSON document to add fields to
     */
    virtual void contribute(JsonDocument& doc) = 0;
};

/**
 * @brief Interface for registering custom web routes
 * 
 * Implement this to add custom endpoints to the web server
 * 
 * @note Called once during portal.begin() - safe to do setup here
 * @warning Route handlers MUST NOT block or call delay()
 * @warning Route handlers MUST NOT modify WiFi state
 * 
 * Example:
 * @code
 * class MyRouteRegistrar : public IRouteRegistrar {
 * public:
 *     void registerRoutes(AsyncWebServer& server) override {
 *         server.on("/wml/ext/mydata", HTTP_GET, [this](AsyncWebServerRequest* req) {
 *             // Use StaticJsonDocument for responses (no heap)
 *             StaticJsonDocument<128> doc;
 *             doc["value"] = _cachedValue;
 *             AsyncResponseStream* res = req->beginResponseStream("application/json");
 *             serializeJson(doc, *res);
 *             req->send(res);
 *         });
 *     }
 * };
 * @endcode
 */
class IRouteRegistrar {
public:
    virtual ~IRouteRegistrar() = default;
    
    /**
     * @brief Register custom routes on the web server
     * @param server AsyncWebServer instance to register routes on
     */
    virtual void registerRoutes(AsyncWebServer& server) = 0;
};

/**
 * @brief Interface for providing custom configuration sections
 * 
 * Implement this to add custom config fields to /wml/config
 * 
 * Example:
 * @code
 * class MyConfigProvider : public IConfigSectionProvider {
 * public:
 *     void contributeConfig(JsonDocument& doc) override {
 *         doc["myOption"] = _myOption;
 *     }
 *     
 *     bool applyConfig(const JsonDocument& doc) override {
 *         if (doc.containsKey("myOption")) {
 *             _myOption = doc["myOption"].as<int>();
 *             return true;
 *         }
 *         return false;
 *     }
 *     
 *     void getDefaults(JsonDocument& doc) override {
 *         doc["myOption"] = 100;  // default value
 *     }
 * };
 * @endcode
 */
class IConfigSectionProvider {
public:
    virtual ~IConfigSectionProvider() = default;
    
    /**
     * @brief Add custom config fields to the config JSON
     * @param doc JSON document to add fields to
     */
    virtual void contributeConfig(JsonDocument& doc) = 0;
    
    /**
     * @brief Apply configuration from submitted form data
     * @param doc JSON document with submitted values
     * @return true if any values were applied
     */
    virtual bool applyConfig(const JsonDocument& doc) = 0;
    
    /**
     * @brief Provide default values for custom config fields
     * @param doc JSON document to add defaults to
     */
    virtual void getDefaults(JsonDocument& doc) = 0;
};

/**
 * @brief Interface for loop-based processing in extensions
 * 
 * Implement this if your extension needs regular processing.
 * Called from CaptivePortal::loop() on every iteration.
 * 
 * @warning MUST complete quickly (<5ms typical, <50ms max)
 * @warning MUST NOT call delay() or block
 * @warning MUST NOT modify WiFi state directly
 * @note Use millis()-based timing for periodic tasks
 * 
 * Example:
 * @code
 * class MyLoopHandler : public ILoopHandler {
 * public:
 *     void loop() override {
 *         // Non-blocking: check time, update if needed
 *         if (millis() - _lastUpdate > 1000) {
 *             _cachedValue = analogRead(A0);  // Quick read
 *             _lastUpdate = millis();
 *         }
 *     }
 * private:
 *     uint32_t _lastUpdate = 0;
 *     int _cachedValue = 0;
 * };
 * @endcode
 */
class ILoopHandler {
public:
    virtual ~ILoopHandler() = default;
    
    /**
     * @brief Called regularly from CaptivePortal::loop()
     */
    virtual void loop() = 0;
};

} // namespace WML
