/**
 * @file AdvancedUsage.ino
 * @brief Advanced example for WiFiManagerLite with full NVS Storage
 * 
 * Demonstrates:
 * - NVS Storage with MessagePack (no Strings)
 * - Static IP configuration
 * - BSSID lock
 * - Custom timing parameters
 * - Multi-logger setup
 * - Serial command interface
 * 
 * Required libraries:
 * - ArduinoJson
 * - NVSUtilityLibrary
 */

#include <WiFi.h>
#include <wifiMangerLite.h>

// ==================== Global Objects ====================

WML::WiFiManagerLite wifiMgr;

// Multiple loggers
WML::SerialLogger serialLogger("[WiFi] ");
WML::MultiLogger multiLogger;

// Storage using NVSConfigBus (MessagePack, no Strings)
WML::Storage storage("wificfg", "wifi");
WML::StorageProvider configProvider(storage);

// ==================== Helper Functions ====================

void printStatus() {
    Serial.println("\n--- WiFi Status ---");
    Serial.print("State: ");
    
    switch (wifiMgr.getState()) {
        case WML::WiFiState::Disconnected:
            Serial.println("Disconnected");
            break;
        case WML::WiFiState::Connecting:
            Serial.println("Connecting");
            break;
        case WML::WiFiState::Connected:
            Serial.println("Connected");
            break;
        case WML::WiFiState::APMode:
            Serial.println("AP Mode");
            break;
        case WML::WiFiState::APModeConnecting:
            Serial.println("AP Mode + Connecting");
            break;
    }
    
    if (wifiMgr.isConnected()) {
        Serial.printf("SSID: %s\n", wifiMgr.getSSID().c_str());
        Serial.printf("BSSID: %s\n", wifiMgr.getBSSID().c_str());
        Serial.printf("IP: %s\n", wifiMgr.getStationIP().toString().c_str());
        Serial.printf("RSSI: %d dBm\n", wifiMgr.getRSSI());
    }
    
    if (wifiMgr.isAPMode()) {
        Serial.printf("AP IP: %s\n", wifiMgr.getAPIP().toString().c_str());
        Serial.printf("AP Clients: %d\n", wifiMgr.getAPClientCount());
    }
    
    Serial.printf("Retry count: %d\n", wifiMgr.getRetryCount());
    Serial.printf("Free Heap: %d KB\n", ESP.getFreeHeap() / 1024);
    Serial.println("-------------------\n");
}

void printConfig() {
    WML::Config cfg = configProvider.getConfig();
    
    // v3.0: char[] fields directly usable as char*
    Serial.println("\n--- Stored Config ---");
    Serial.printf("Device Name: %s\n", cfg.deviceName);
    Serial.printf("Primary SSID: %s\n", cfg.primary.ssid);
    Serial.printf("Primary BSSID: %s\n", cfg.primary.bssid);
    Serial.printf("BSSID Lock: %s\n", cfg.primary.bssidLock ? "Yes" : "No");
    Serial.printf("Secondary SSID: %s\n", cfg.secondary.ssid);
    Serial.printf("Static IP: %s\n", cfg.staticIP.ip[0] != '\0' ? cfg.staticIP.ip : "DHCP");
    Serial.printf("Gateway: %s\n", cfg.staticIP.gateway);
    Serial.printf("Subnet: %s\n", cfg.staticIP.subnet);
    Serial.printf("DNS: %s\n", cfg.staticIP.dns);
    Serial.printf("AP Prefix: %s\n", cfg.ap.ssidPrefix);
    Serial.printf("Connect Timeout: %lu ms\n", cfg.timing.connectTimeoutMs);
    Serial.printf("Retry Interval: %lu ms\n", cfg.timing.retryIntervalMs);
    Serial.printf("Max Tries: %d\n", cfg.timing.maxTriesBeforeAp);
    Serial.printf("mDNS: %s\n", cfg.enableMDNS ? "Enabled" : "Disabled");
    Serial.println("---------------------\n");
}

void printScanResults() {
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_FAILED) {
        Serial.println("Scan failed!");
        return;
    }
    if (n == WIFI_SCAN_RUNNING) {
        Serial.println("Scan still running...");
        return;
    }
    
    Serial.printf("\nFound %d networks:\n", n);
    for (int i = 0; i < n; i++) {
        Serial.printf("  %d: %s (%d dBm) %s [%s]\n",
            i + 1,
            WiFi.SSID(i).c_str(),
            WiFi.RSSI(i),
            WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "Open" : "Secured",
            WiFi.BSSIDstr(i).c_str());
    }
    Serial.println();
    
    WiFi.scanDelete();
}

void printHelp() {
    Serial.println("\n--- Commands ---");
    Serial.println("  r - Force reconnect");
    Serial.println("  a - Start AP mode");
    Serial.println("  s - Stop AP mode");
    Serial.println("  i - Show status info");
    Serial.println("  c - Show stored config");
    Serial.println("  n - Scan networks");
    Serial.println("  f - Factory reset");
    Serial.println("  h - Show this help");
    Serial.println("----------------\n");
}

// ==================== Setup ====================

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n==========================================");
    Serial.println("  WiFiManagerLite Advanced Example");
    Serial.println("  with NVS Storage (MessagePack)");
    Serial.println("==========================================\n");
    
    // Load configuration from NVS
    WML::Config config = configProvider.getConfig();
    
    // If no saved config, set defaults
    if (!config.primary.isValid()) {
        Serial.println("No saved config, setting defaults...");
        
        // v3.0: Use setter methods for char[] fields
        config.setDeviceName("ESP-Advanced");
        config.primary.setSsid("YOUR_SSID");
        config.primary.setPassword("YOUR_PASSWORD");
        
        // Optional: Lock to specific BSSID
        // config.primary.setBssid("AA:BB:CC:DD:EE:FF");
        // config.primary.bssidLock = true;
        
        // Optional: Static IP
        // config.staticIP.setIp("192.168.1.100");
        // config.staticIP.setGateway("192.168.1.1");
        // config.staticIP.setSubnet("255.255.255.0");
        // config.staticIP.setDns("8.8.8.8");
        
        // Custom timing
        config.timing.connectTimeoutMs = 10000;
        config.timing.maxTriesBeforeAp = 3;
        config.timing.retryIntervalMs = 20000;
        
        // AP SSID + mDNS hostname can be derived from identity base name
        // (set below via wifiMgr.setIdentityBaseName) unless you explicitly set cfg.ap.ssidPrefix
        
        // Save to NVS (MessagePack)
        configProvider.updateConfig(config);
        Serial.println("Default config saved to NVS");
    }
    
    // v3.0: char[] fields directly usable
    Serial.printf("Device: %s\n", config.deviceName);
    Serial.printf("SSID: %s\n", config.primary.ssid);
    
    // Setup multi-logger
    multiLogger.addLogger(&serialLogger);
    
    // Configure WiFi manager with StorageProvider
    wifiMgr.setConfigProvider(&configProvider);
    wifiMgr.setLogger(&multiLogger);
    wifiMgr.setIdentityBaseName(config.deviceName);
    
    // Event handling
    wifiMgr.onEvent([](WML::Event event, const String& info) {
        switch (event) {
            case WML::Event::Connecting:
                Serial.printf("→ Connecting to: %s\n", info.c_str());
                break;
                
            case WML::Event::Connected:
                Serial.printf("✓ Connected: %s\n", info.c_str());
                Serial.printf("  BSSID: %s\n", wifiMgr.getBSSID().c_str());
                break;
                
            case WML::Event::Disconnected:
                Serial.println("✗ Disconnected from WiFi");
                break;
                
            case WML::Event::ConnectionFailed:
                Serial.printf("✗ Connection failed (attempt %d)\n", 
                    wifiMgr.getRetryCount());
                break;
                
            case WML::Event::APStarted:
                Serial.printf("★ Access Point started: %s\n", info.c_str());
                Serial.printf("  Configure at: http://%s\n", 
                    wifiMgr.getAPIP().toString().c_str());
                break;
                
            case WML::Event::APStopped:
                Serial.println("○ Access Point stopped");
                break;
                
            default:
                break;
        }
    });
    
    // Start WiFi manager
    wifiMgr.begin();
    
    printHelp();
}

// ==================== Loop ====================

void loop() {
    wifiMgr.loop();
    
    // Handle serial commands
    if (Serial.available()) {
        char cmd = Serial.read();
        
        switch (cmd) {
            case 'r':
                Serial.println("\nForcing reconnect...");
                wifiMgr.reconnect();
                break;
                
            case 'a':
                Serial.println("\nStarting AP mode...");
                wifiMgr.startAP(true);
                break;
                
            case 's':
                Serial.println("\nStopping AP mode...");
                wifiMgr.stopAP();
                break;
                
            case 'i':
                printStatus();
                break;
                
            case 'c':
                printConfig();
                break;
                
            case 'n':
                Serial.println("\nScanning networks...");
                wifiMgr.scanNetworks(false, true);
                delay(3000);  // Wait for sync scan
                printScanResults();
                break;
                
            case 'f':
                Serial.println("\n⚠️ Factory Reset - clearing all config...");
                wifiMgr.factoryReset([&]() {
                    configProvider.clearConfig();
                    Serial.println("Config cleared from NVS");
                });
                break;
                
            case 'h':
            case '?':
                printHelp();
                break;
        }
    }
    
    // Status output every 30 seconds
    static uint32_t lastStatus = 0;
    if (millis() - lastStatus > 30000) {
        lastStatus = millis();
        
        Serial.print("[Status] ");
        if (wifiMgr.isConnected()) {
            Serial.printf("Connected to %s | IP: %s | RSSI: %d dBm\n",
                wifiMgr.getSSID().c_str(),
                wifiMgr.getStationIP().toString().c_str(),
                wifiMgr.getRSSI());
        } else if (wifiMgr.isAPMode()) {
            Serial.printf("AP Mode | IP: %s | Clients: %d\n",
                wifiMgr.getAPIP().toString().c_str(),
                wifiMgr.getAPClientCount());
        } else {
            Serial.printf("Connecting... (attempt %d)\n", wifiMgr.getRetryCount() + 1);
        }
    }
}
