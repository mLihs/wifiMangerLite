/**
 * @file WMLStorage.h
 * @brief Persistent storage for WiFiManagerLite configuration
 * 
 * Uses NVSConfigBus for efficient MessagePack-based storage.
 * No String allocations - uses binary buffers.
 * 
 * @author Martin Lihs
 * @version 1.0.0
 */

#pragma once

#include <Arduino.h>
#include <NVSConfigBus.h>
#include "WMLConfig.h"

namespace WML {

/**
 * @brief Persistent storage handler for WiFi configuration
 * 
 * Stores and loads WiFi configuration using NVSConfigBus.
 * Uses MessagePack format for efficient binary storage.
 */
class Storage {
public:
    /**
     * @brief Constructor
     * @param nvsNamespace NVS namespace (default: "wificfg")
     * @param moduleId Module ID within namespace (default: "wifi")
     */
    explicit Storage(const char* nvsNamespace = "wificfg", const char* moduleId = "wifi");
    
    /**
     * @brief Load configuration from NVS
     * @param config Reference to config structure to populate
     * @return true if loaded successfully, false if not found or error
     */
    bool load(Config& config);
    
    /**
     * @brief Save configuration to NVS
     * @param config Configuration to save
     * @return true if saved successfully
     */
    bool save(const Config& config);
    
    /**
     * @brief Clear stored configuration
     * @return true if cleared successfully
     */
    bool clear();
    
    /**
     * @brief Check if configuration exists in storage
     * @return true if config exists
     */
    bool exists();
    
private:
    NVSConfigBus _configBus;
    const char* _moduleId;
    
    // Buffer for MessagePack operations (avoids heap allocation per call)
    static const size_t kBufferSize = 1024;
    uint8_t _buffer[kBufferSize];
    
    // Reusable JSON document (avoids repeated heap allocations)
    // NVSConfigBus requires DynamicJsonDocument, so we keep one as member
    // and reuse it (clear() instead of new allocation each call)
    DynamicJsonDocument _doc;
    
    /**
     * @brief Convert Config to JSON document
     */
    void configToJson(const Config& config, JsonDocument& doc);
    
    /**
     * @brief Convert JSON document to Config
     */
    void jsonToConfig(const JsonDocument& doc, Config& config);
};

/**
 * @brief Integration helper - combines Storage with WiFiManagerLite
 * 
 * Provides a simple interface for loading/saving config automatically.
 */
class StorageProvider : public IConfigProvider {
public:
    /**
     * @brief Constructor
     * @param storage Reference to Storage instance
     */
    explicit StorageProvider(Storage& storage);
    
    /**
     * @brief Get current configuration (loads from storage if needed)
     */
    Config getConfig() override;
    
    /**
     * @brief Check if config has changed
     */
    bool hasChanged() override;
    
    /**
     * @brief Update and save configuration
     * @param config New configuration to save
     * @return true if saved successfully
     */
    bool updateConfig(const Config& config);
    
    /**
     * @brief Mark config as changed (forces reload on next getConfig)
     */
    void invalidate();
    
    /**
     * @brief Clear all stored configuration
     */
    bool clearConfig();
    
private:
    Storage& _storage;
    Config _cachedConfig;
    bool _loaded;
    bool _changed;
};

} // namespace WML
