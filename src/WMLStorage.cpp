/**
 * @file WMLStorage.cpp
 * @brief Implementation of persistent storage for WiFiManagerLite
 * 
 * @author Martin Lihs
 * @version 1.0.0
 */

#include "WMLStorage.h"

namespace WML {

// ==================== Storage Implementation ====================

Storage::Storage(const char* nvsNamespace, const char* moduleId)
    : _configBus(nvsNamespace)
    , _moduleId(moduleId)
    , _doc(kBufferSize)
{
}

bool Storage::load(Config& config) {
    // Reuse member document (no heap allocation)
    _doc.clear();
    
    // Try MessagePack first, then JSON fallback
    if (!_configBus.loadModuleConfigMsgPack(_moduleId, _doc, _buffer, kBufferSize)) {
        // Try regular load (handles JSON fallback)
        if (!_configBus.loadModuleConfig(_moduleId, _doc)) {
            return false;
        }
    }
    
    jsonToConfig(_doc, config);
    return true;
}

bool Storage::save(const Config& config) {
    // Reuse member document (no heap allocation)
    _doc.clear();
    configToJson(config, _doc);
    
    // Save using MessagePack (no Strings)
    return _configBus.saveModuleConfigMsgPack(_moduleId, _doc, _buffer, kBufferSize);
}

bool Storage::clear() {
    return _configBus.clearModuleConfig(_moduleId);
}

bool Storage::exists() {
    Config tmp;
    return load(tmp);
}

void Storage::configToJson(const Config& config, JsonDocument& doc) {
    doc.clear();
    
    // Device (char[] direkt verwendbar mit ArduinoJson)
    doc["dn"] = config.deviceName;
    
    // Primary credentials
    doc["s0"] = config.primary.ssid;
    doc["p0"] = config.primary.password;
    doc["b0"] = config.primary.bssid;
    doc["bl"] = config.primary.bssidLock;
    
    // Secondary credentials
    doc["s1"] = config.secondary.ssid;
    doc["p1"] = config.secondary.password;
    
    // Static IP
    doc["ip"] = config.staticIP.ip;
    doc["gw"] = config.staticIP.gateway;
    doc["sn"] = config.staticIP.subnet;
    doc["ds"] = config.staticIP.dns;
    
    // AP Config
    doc["ap"] = config.ap.ssidPrefix;
    doc["apw"] = config.ap.password;
    
    // Timing (only non-default values)
    if (config.timing.connectTimeoutMs != WML_CONNECT_TIMEOUT_MS) {
        doc["tco"] = config.timing.connectTimeoutMs;
    }
    if (config.timing.retryIntervalMs != WML_RETRY_INTERVAL_MS) {
        doc["tri"] = config.timing.retryIntervalMs;
    }
    if (config.timing.maxTriesBeforeAp != WML_MAX_RETRIES_BEFORE_AP) {
        doc["tma"] = config.timing.maxTriesBeforeAp;
    }
    
    // Options
    doc["mdns"] = config.enableMDNS;
    if (config.httpPort != WML_HTTP_PORT) {
        doc["hp"] = config.httpPort;
    }
}

void Storage::jsonToConfig(const JsonDocument& doc, Config& config) {
    // Reset to defaults first
    config = Config();
    
    // Device (char[] mit Setter-Methoden)
    if (doc["dn"].is<const char*>()) {
        config.setDeviceName(doc["dn"].as<const char*>());
    }
    
    // Primary credentials
    if (doc["s0"].is<const char*>()) {
        config.primary.setSsid(doc["s0"].as<const char*>());
    }
    if (doc["p0"].is<const char*>()) {
        config.primary.setPassword(doc["p0"].as<const char*>());
    }
    if (doc["b0"].is<const char*>()) {
        config.primary.setBssid(doc["b0"].as<const char*>());
    }
    config.primary.bssidLock = doc["bl"] | false;
    
    // Secondary credentials
    if (doc["s1"].is<const char*>()) {
        config.secondary.setSsid(doc["s1"].as<const char*>());
    }
    if (doc["p1"].is<const char*>()) {
        config.secondary.setPassword(doc["p1"].as<const char*>());
    }
    
    // Static IP
    if (doc["ip"].is<const char*>()) {
        config.staticIP.setIp(doc["ip"].as<const char*>());
    }
    if (doc["gw"].is<const char*>()) {
        config.staticIP.setGateway(doc["gw"].as<const char*>());
    }
    if (doc["sn"].is<const char*>()) {
        config.staticIP.setSubnet(doc["sn"].as<const char*>());
    }
    if (doc["ds"].is<const char*>()) {
        config.staticIP.setDns(doc["ds"].as<const char*>());
    }
    
    // AP Config
    if (doc["ap"].is<const char*>()) {
        config.ap.setSsidPrefix(doc["ap"].as<const char*>());
    }
    if (doc["apw"].is<const char*>()) {
        config.ap.setPassword(doc["apw"].as<const char*>());
    }
    
    // Timing
    config.timing.connectTimeoutMs = doc["tco"] | static_cast<uint32_t>(WML_CONNECT_TIMEOUT_MS);
    config.timing.retryIntervalMs = doc["tri"] | static_cast<uint32_t>(WML_RETRY_INTERVAL_MS);
    config.timing.maxTriesBeforeAp = doc["tma"] | static_cast<uint8_t>(WML_MAX_RETRIES_BEFORE_AP);
    
    // Options
    config.enableMDNS = doc["mdns"] | (WML_ENABLE_MDNS != 0);
    config.httpPort = doc["hp"] | static_cast<uint16_t>(WML_HTTP_PORT);
}

// ==================== StorageProvider Implementation ====================

StorageProvider::StorageProvider(Storage& storage)
    : _storage(storage)
    , _loaded(false)
    , _changed(false)
{
}

Config StorageProvider::getConfig() {
    if (!_loaded) {
        if (_storage.load(_cachedConfig)) {
            _loaded = true;
        }
        // If load fails, _cachedConfig has defaults
    }
    return _cachedConfig;
}

bool StorageProvider::hasChanged() {
    bool changed = _changed;
    _changed = false;
    return changed;
}

bool StorageProvider::updateConfig(const Config& config) {
    _cachedConfig = config;
    _loaded = true;
    _changed = true;
    return _storage.save(config);
}

void StorageProvider::invalidate() {
    _loaded = false;
    _changed = true;
}

bool StorageProvider::clearConfig() {
    _cachedConfig = Config();
    _loaded = false;
    _changed = true;
    return _storage.clear();
}

} // namespace WML
