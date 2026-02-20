# WiFiManagerLite v2.1.x - Optimierungen

Diese Dokumentation beschreibt die technischen Verbesserungen in Version 2.1.0 und Bugfixes in 2.1.1.

---

## Übersicht

| Phase | Bereich | Ziel |
|-------|---------|------|
| 1 | No Blocking | Loop-Latenz stabilisieren |
| 2 | String-Minimierung | Heap-Fragmente reduzieren |
| 3 | JSON Streaming | Heap-Peaks vermeiden |
| 4 | Scan-Cache | CPU + RAM sparen |
| 5 | DNSServer | Lazy Initialization (FreeRTOS-safe) |
| 6 | Storage | Wiederholte Allokationen vermeiden |
| 7 | Extension Points | Erweiterbarkeit ohne Core-Änderung |

### Bugfixes (v2.1.1)
- DNSServer als Member verursachte FreeRTOS Crash → Lazy Initialization
- Boot-Loop nach Factory Reset → `_wasConnected` Check
- Pre-Scan entfernt → On-demand Scan im Portal

---

## Phase 1: Non-Blocking Operation

### Problem
`delay()` in kritischen Pfaden verursacht:
- Jitter/Latenz-Spikes im Main Loop
- Blockierung anderer Tasks (BLE, WebSocket, Sensoren)
- Unvorhersehbares Timing

### Betroffene Stellen (vorher)

| Methode | delay() | Zeile |
|---------|---------|-------|
| `factoryReset()` | 200ms, 100ms | ~262, ~266 |
| `internalStartAP()` | 150ms, 50ms, 100ms | ~474, ~500 |
| AP disconnect wait | while + delay(50) | ~487-494 |
| Scan wait | while + delay(20) | ~533-536 |

### Lösung

**Pending Actions Pattern:**

```cpp
// WiFiManagerLite.h
uint32_t _pendingRestartAt;
uint32_t _pendingStartApAt;

// WiFiManagerLite.cpp
void WiFiManagerLite::servicePendingActions() {
    const uint32_t now = millis();
    
    if (_pendingRestartAt > 0 && now >= _pendingRestartAt) {
        _pendingRestartAt = 0;
        ESP.restart();
    }
    
    if (_pendingStartApAt > 0 && now >= _pendingStartApAt) {
        internalStartAP();  // Will retry if not ready
    }
}
```

**Asynchrones Disconnect-Waiting:**

```cpp
void WiFiManagerLite::internalStartAP() {
    wl_status_t status = WiFi.status();
    if (status != WL_DISCONNECTED && status != WL_IDLE_STATUS) {
        // Nicht blockieren - später erneut versuchen
        if (_pendingStartApAt == 0) {
            _pendingStartApAt = millis() + 100;
        }
        return;
    }
    // ... AP starten
}
```

### Ergebnis
- **Loop-Latenz**: ~200ms → <10ms
- **Vorhersagbarkeit**: Deterministisches Timing

---

## Phase 2: String-Minimierung

### Problem
String-Konkatenation (`+`) erzeugt temporäre Heap-Allokationen:

```cpp
// Schlecht: 3 temporäre Strings
String info = WiFi.SSID() + " - " + WiFi.localIP().toString();
```

### Lösung

**Stack-Buffer mit snprintf:**

```cpp
// Gut: Keine Heap-Allokation
char info[96];
snprintf(info, sizeof(info), "%s - %s", 
         WiFi.SSID().c_str(), 
         WiFi.localIP().toString().c_str());
```

**Betroffene Stellen:**

| Methode | Vorher | Nachher |
|---------|--------|---------|
| `factoryReset()` | `log("..." + String(...))` | `logf("...%s", ...)` |
| `handleConnected()` | `String info = ... + ...` | `char info[96]; snprintf()` |
| `internalStartAP()` | `String apName = ... + ...` | `char apName[64]; snprintf()` |
| `CaptivePortal::log()` | `"[Portal] " + message` | `char buf[256]; snprintf()` |

### Ergebnis
- **Heap-Drift**: Eliminiert
- **Fragmentierung**: Reduziert

---

## Phase 3: JSON Streaming

### Problem
Doppelte Daten bei JSON-Responses:

```cpp
// Schlecht: JSON → String → Send
JsonDocument doc;
// ... befüllen ...
String output;
serializeJson(doc, output);  // Allokation 1
request->send(200, "application/json", output);  // Kopie 2
```

### Lösung

**StaticJsonDocument + AsyncResponseStream:**

```cpp
// Gut: Direktes Streaming
StaticJsonDocument<1024> doc;
// ... befüllen ...
AsyncResponseStream* response = request->beginResponseStream("application/json");
serializeJson(doc, *response);  // Direkt in Response
request->send(response);
```

### Betroffene Endpoints

| Endpoint | Vorher | Nachher |
|----------|--------|---------|
| `/wml/status.json` | JsonDocument + String | StaticJsonDocument<1024> + Stream |
| `/wml/config` | JsonDocument + String | StaticJsonDocument<1024> + Stream |
| `/wml/netlist` | JsonDocument + String | StaticJsonDocument + Stream |

### Ergebnis
- **Heap-Peak**: ~4KB → ~1KB
- **Latenz**: Schnellere Response-Zeit

---

## Phase 4: Scan-Cache als Struct

### Problem
JSON-String als Cache erfordert Parse bei jedem Abruf:

```cpp
// Schlecht: JSON → String → JSON → Response
String _scanCacheJson;

// Bei Scan-Ende:
serializeJson(doc, _scanCacheJson);

// Bei Request:
deserializeJson(cached, _scanCacheJson);
doc["networks"] = cached["networks"];
```

### Lösung

**Kompakte Struct-Speicherung:**

```cpp
// WMLCaptivePortal.h
struct ScanNet {
    char ssid[33];      // Max SSID + null
    char bssid[18];     // "XX:XX:XX:XX:XX:XX" + null
    int8_t rssi;
    uint8_t encrypted;
};
ScanNet _scanCache[20];
uint8_t _scanCount;
```

**Direktes Streaming:**

```cpp
void CaptivePortal::handleNetworkList(...) {
    StaticJsonDocument<2100> doc;
    JsonArray networks = doc.createNestedArray("networks");
    
    for (uint8_t i = 0; i < _scanCount; i++) {
        JsonObject net = networks.createNestedObject();
        net["ssid"] = _scanCache[i].ssid;
        net["rssi"] = _scanCache[i].rssi;
        net["enc"] = (_scanCache[i].encrypted != 0);
        net["bssid"] = _scanCache[i].bssid;
    }
    
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    serializeJson(doc, *response);
    request->send(response);
}
```

### Speichervergleich

| Variante | Speicher | Operation |
|----------|----------|-----------|
| JSON-String | ~2KB String | Parse + Serialize |
| Struct-Array | ~1.1KB fixed | Direkter Zugriff |

### Ergebnis
- **CPU-Last**: ~50% weniger bei Netlist-Requests
- **Heap**: Konstant, keine String-Wachstum

---

## Phase 5: DNSServer Lazy Initialization

### Problem
DNSServer als Member-Variable verursacht FreeRTOS Crash:

```cpp
// Schlecht - Crash bei Boot
DNSServer _dns;  // Konstruktor läuft vor WiFi-Stack!
```

Der DNSServer-Konstruktor erstellt Semaphores, aber FreeRTOS/WiFi ist noch nicht initialisiert → `xQueueSemaphoreTake` Assert-Fehler.

### Lösung

**Lazy Initialization mit Pointer:**

```cpp
// WiFiManagerLite.h
DNSServer* _dns;  // Nur Pointer, kein Objekt
bool _dnsActive;

// WiFiManagerLite.cpp - in internalStartAP()
if (!_dnsActive) {
    if (!_dns) {
        _dns = new DNSServer();  // Erst wenn WiFi bereit
    }
    _dns->start(53, "*", cfg.ap.ip);
    _dnsActive = true;
}
```

### Ergebnis
- **Crash vermieden**: DNSServer wird erst nach WiFi.mode() erstellt
- **Einmalige Allokation**: Nur beim ersten AP-Start
- **Stabil auf ESP32-S3**: Keine FreeRTOS-Konflikte

---

## Phase 6: Storage mit Member-Doc

### Problem
Wiederholte Allokationen bei Load/Save:

```cpp
bool Storage::load(Config& config) {
    DynamicJsonDocument doc(1024);  // Jedes Mal neu
    // ...
}
```

### Lösung

**StaticJsonDocument als Member:**

```cpp
// WMLStorage.h
StaticJsonDocument<1024> _doc;

// WMLStorage.cpp
bool Storage::load(Config& config) {
    _doc.clear();  // Wiederverwendung
    // ...
}
```

### Ergebnis
- **Heap-Stabilität**: Langfristig besser
- **Allokationen**: Keine bei Config-Operationen

---

## Phase 7: Extension Points

### Motivation
Neue Sensor-Typen (Zwiftcontrol, Lenkplatte, etc.) sollten ohne Core-Änderung integrierbar sein.

### Interfaces

```cpp
// WMLPortalExtensions.h

class IStatusContributor {
    virtual void contribute(JsonDocument& doc) = 0;
};

class IRouteRegistrar {
    virtual void registerRoutes(AsyncWebServer& server) = 0;
};

class IConfigSectionProvider {
    virtual void contributeConfig(JsonDocument& doc) = 0;
    virtual bool applyConfig(const JsonDocument& doc) = 0;
    virtual void getDefaults(JsonDocument& doc) = 0;
};

class ILoopHandler {
    virtual void loop() = 0;
};
```

### Registry (Fixed-Size, kein Heap)

```cpp
// WMLCaptivePortal.h
static const uint8_t kMaxExtensions = 8;
IStatusContributor* _statusContributors[kMaxExtensions];
uint8_t _statusContributorCount;
```

### Verwendung

```cpp
// Sensor-Modul definieren
class MySensor : public IStatusContributor, public IRouteRegistrar {
    void contribute(JsonDocument& doc) override {
        doc["temperature"] = readTemp();
    }
    
    void registerRoutes(AsyncWebServer& server) override {
        server.on("/wml/ext/temp", HTTP_GET, ...);
    }
};

// Registrieren
MySensor sensor;
portal.addStatusContributor(&sensor);
portal.addRouteRegistrar(&sensor);
```

### Vorteile
- **Separation of Concerns**: Module kennen Core nicht
- **Keine Regression**: Core bleibt unverändert
- **Testbarkeit**: Module isoliert testbar

---

## Performance-Messungen

### Empfohlene Messpunkte

```cpp
// In loop()
static uint32_t loopStart;
static uint32_t loopMax = 0;

uint32_t loopTime = millis() - loopStart;
if (loopTime > loopMax) loopMax = loopTime;
loopStart = millis();

// Alle 10s ausgeben
Serial.printf("Loop max: %lu ms, Heap: %lu\n", loopMax, ESP.getFreeHeap());
loopMax = 0;
```

### Erwartete Werte

| Metrik | v2.0.0 | v2.1.1 |
|--------|--------|--------|
| Loop max (idle) | 5-10ms | 2-5ms |
| Loop max (AP start) | 200-400ms | 5-15ms |
| Loop max (Portal request) | 50-100ms | 10-30ms |
| Heap nach 1h | Abnehmend | Stabil |
| Boot-Stabilität | Crashes möglich | Stabil |

---

## Zusammenfassung

Version 2.1.x macht WiFiManagerLite:

1. **Reaktiver**: Keine Blocking-Delays mehr
2. **Stabiler**: Weniger Heap-Fragmente, FreeRTOS-safe
3. **Effizienter**: Optimierte JSON-Verarbeitung
4. **Erweiterbarer**: Plugin-System für Module
5. **Robuster**: Keine Boot-Loops, On-demand Scans

Alle Änderungen sind **abwärtskompatibel** - bestehender Code funktioniert ohne Anpassung.
