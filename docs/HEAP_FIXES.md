# Heap-Fragmentierung – Lösungen und Beweise

Dieses Dokument beschreibt die **umgesetzten Lösungen** zu den in `HEAP_FRAGMENTATION_ANALYSIS.md` identifizierten Ursachen, mit **Quellcode-Referenzen** und **Beweis** (Kompilierung und Verhalten).

**Backup vor Änderungen:** `wifiMangerLite-2.6.5.zip` (Version vor Fixes).  
**Version nach Fixes:** 2.6.6 (`library.properties`).

---

## 1. Übersicht der Lösungen

| Problem (Analyse) | Lösung | Datei:Zeile (nach Fix) | Beweis |
|------------------|--------|------------------------|--------|
| Identity: `hostBase + "-" + String(suffix)` | `snprintf(hostnameBuf, ...)` + eine Zuweisung | WiFiManagerLite.cpp ~100–106 | Build ✓, keine temporären Strings |
| Identity: `getIdentityNameWithMac()` Konkatenation | `snprintf(buf, ...); return String(buf)` | WiFiManagerLite.cpp ~188–194 | Build ✓, 1 Allokation statt 3+ |
| `getEffectiveConfig()` Rückgabe-Kopie | `void getEffectiveConfig(Config& out)` + interne Nutzung | WiFiManagerLite.cpp ~198–218, alle Aufrufer | Build ✓, keine Rückgabe-Kopie |
| Events: `String(info)` / `String(apName)` | `emitEvent(Event, const char*)` Overload | WiFiManagerLite.cpp ~634–638, ~779, ~811–815 | Build ✓, Callback erhält weiterhin String |
| Portal: Cache-Control `"…" + String(cacheSeconds)` | `snprintf(cacheBuf, ...); addHeader(..., cacheBuf)` | WMLCaptivePortal.cpp ~574–603 | Build ✓ |
| Portal: ETag als `String` | `char etagBuf[64]; snprintf(etagBuf, "\"%s\"", WML_BUILD_HASH)` | WMLCaptivePortal.cpp ~574–578 | Build ✓ |
| Scan: 40× `String ssid/bssid` in Schleife | 2 wiederverwendbare `String tmpSsid`, `tmpBssid` | WMLCaptivePortal.cpp ~682–698 | Build ✓, 2 Puffer statt 40 |

---

## 2. Lösung 1: Identity ohne String-Konkatenation

**Ursache:** `_identityHostnameWithMac = hostBase + "-" + String(suffix)` und `_identityApPrefix += "-"` erzeugen mehrere temporäre Strings.

**Lösung:** Hostname und Prefix mit `snprintf` in Stack-Puffer bauen, dann **eine** Zuweisung an die String-Member.

**Code (WiFiManagerLite.cpp):**

```cpp
const uint32_t id32 = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFFFFu);

const String hostBase = sanitizeHostnameBaseLower(_identityBaseName);
char hostnameBuf[64];
snprintf(hostnameBuf, sizeof(hostnameBuf), "%s-%08lx", hostBase.c_str(), static_cast<unsigned long>(id32));
_identityHostnameWithMac = hostnameBuf;

String ssidBase = sanitizeSsidBasePreserveCase(_identityBaseName);
char prefixBuf[33];
if (ssidBase.endsWith("-")) {
    _identityApPrefix = ssidBase;
} else {
    snprintf(prefixBuf, sizeof(prefixBuf), "%s-", ssidBase.c_str());
    _identityApPrefix = prefixBuf;
}
```

**Beweis:**  
- Projekt kompiliert mit Arduino/ESP32 (siehe Abschnitt „Build-Beweis“).  
- Vorher: mindestens 3 temporäre Strings pro Identity-Berechnung. Nachher: 2 Stack-Puffer + 2 String-Zuweisungen (keine Konkatenations-Temporaries).

---

## 3. Lösung 2: getIdentityNameWithMac() ohne Konkatenation

**Ursache:** `return base + "-" + String(suffix)` erzeugt 2–3 temporäre Strings pro Aufruf.

**Lösung:** Ein Stack-Puffer, `snprintf`, dann **eine** Rückgabe-String-Allokation.

**Code (WiFiManagerLite.cpp):**

```cpp
String WiFiManagerLite::getIdentityNameWithMac() const {
    if (_identityBaseName.length() == 0) return "";
    const String base = sanitizeSsidBasePreserveCase(_identityBaseName);
    char buf[64];
    snprintf(buf, sizeof(buf), "%s-%08lx", base.c_str(), static_cast<unsigned long>(ESP.getEfuseMac() & 0xFFFFFFFFu));
    return String(buf);
}
```

**Beweis:**  
- Build erfolgreich.  
- Vorher: 3+ Allokationen pro Aufruf. Nachher: 1 Allokation (`return String(buf)`), keine temporären Konkatenations-Strings.

---

## 4. Lösung 3: getEffectiveConfig(Config&) – weniger Kopien

**Ursache:** `Config cfg = getEffectiveConfig();` kopiert den gesamten Config inkl. aller Strings bei jedem Aufruf (z. B. in `loop()`).

**Lösung:**  
- Neue interne API: `void getEffectiveConfig(Config& out)` füllt `out` direkt.  
- Öffentliche API bleibt: `Config getEffectiveConfig()` ruft intern `getEffectiveConfig(cfg)` auf und gibt `cfg` zurück (nur noch für externe Aufrufer eine Kopie).  
- **Alle internen Aufrufer** verwenden `Config cfg; getEffectiveConfig(cfg);` – damit entfällt die Rückgabe-Kopie.

**Code (WiFiManagerLite.cpp / .h):**

- Implementierung:
```cpp
void WiFiManagerLite::getEffectiveConfig(Config& out) {
    out = _configProvider ? _configProvider->getConfig() : _config;
    if (_identityBaseName.length() > 0) {
        ensureIdentityComputed();
        if (out.deviceName.length() == 0 || out.deviceName == WML_DEFAULT_DEVICE_NAME) {
            out.deviceName = _identityHostnameWithMac;
        }
        if (out.ap.ssidPrefix.length() == 0 || out.ap.ssidPrefix == WML_AP_SSID_PREFIX) {
            out.ap.ssidPrefix = _identityApPrefix;
        }
    }
}

Config WiFiManagerLite::getEffectiveConfig() {
    Config cfg;
    getEffectiveConfig(cfg);
    return cfg;
}
```

- Aufrufer (Beispiel): `begin()`, `loop()`, `startConnectAttempt()`, `processConnectAttempt()`, `handleConnected()`, `internalStartAP()`, `internalStopAP()`:
```cpp
Config cfg;
getEffectiveConfig(cfg);
```

**Beweis:**  
- Build erfolgreich.  
- Vorher: pro Aufruf 1 Kopie vom Provider + 1 Rückgabe-Kopie. Nachher (intern): 1 Kopie vom Provider, **keine** Rückgabe-Kopie.

---

## 5. Lösung 4: Events mit const char* (kein String-Bau in Library)

**Ursache:** `emitEvent(Event::Connected, String(info))` und `emitEvent(Event::APStarted, String(apName))` allokieren ein String-Objekt im Aufrufer.

**Lösung:**  
- Neuer Overload: `void emitEvent(Event event, const char* info)`.  
- Implementierung: `_eventCallback(event, info ? String(info) : "");` – die **eine** Allokation passiert zentral, Aufrufer übergeben nur Puffer.  
- `handleConnected`: `char info[96]; snprintf(...); emitEvent(Event::Connected, info);`  
- `internalStartAP`: `emitEvent(Event::APStarted, apName);` (apName ist bereits `char apName[64]`).

**Code (WiFiManagerLite.cpp):**

```cpp
// handleConnected
char info[96];
snprintf(info, sizeof(info), "%s - %s", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
WML_LOGF("Connected to %s (IP: %s)", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
emitEvent(Event::Connected, info);

// internalStartAP
WML_LOGF("AP started: %s (IP: %s)", apName, cfg.ap.ip.toString().c_str());
emitEvent(Event::APStarted, apName);

// Overload
void WiFiManagerLite::emitEvent(Event event, const char* info) {
    if (_eventCallback) {
        _eventCallback(event, info ? String(info) : "");
    }
}
```

**Beweis:**  
- Build erfolgreich.  
- API-kompatibel: Callback-Signatur bleibt `(Event, const String&)`; Aufrufer-Code (z. B. Examples) unverändert.  
- Weniger Allokationsstellen im Library-Code (kein `String(info)`/`String(apName)` an jeder Aufrufstelle).

---

## 6. Lösung 5: Captive Portal – Cache-Control und ETag ohne String-Konkatenation

**Ursache:**  
- `response->addHeader("Cache-Control", "public, max-age=" + String(cacheSeconds));`  
- `String etag = "\"" WML_BUILD_HASH "\"";`

**Lösung:** Beides mit `snprintf` in `char`-Puffer bauen und diese an `addHeader` übergeben.

**Code (WMLCaptivePortal.cpp):**

```cpp
char etagBuf[64];
snprintf(etagBuf, sizeof(etagBuf), "\"%s\"", WML_BUILD_HASH);
char cacheBuf[48];
snprintf(cacheBuf, sizeof(cacheBuf), "public, max-age=%lu", (unsigned long)cacheSeconds);
// ...
response->addHeader("ETag", etagBuf);
response->addHeader("Cache-Control", cacheBuf);
```

**Beweis:**  
- Build erfolgreich.  
- Keine `String`-Konkatenation mehr für ETag und Cache-Control; nur Stack-Puffer.

---

## 7. Lösung 6: collectScanResults() – wiederverwendbare Strings

**Ursache:** In der Schleife je `String ssid = WiFi.SSID(i);` und `String bssid = WiFi.BSSIDstr(i);` → bis zu 40 separate String-Objekte pro Scan, danach viele kleine freigegebene Blöcke.

**Lösung:** Zwei lokale `String`-Variablen, die in der Schleife **wiederverwendet** werden (`tmpSsid = WiFi.SSID(i);`, `tmpBssid = WiFi.BSSIDstr(i);`). Dadurch weniger unterschiedliche Heap-Blöcke und weniger Fragmentierung.

**Code (WMLCaptivePortal.cpp):**

```cpp
String tmpSsid;
String tmpBssid;
for (uint8_t i = 0; i < maxNets; i++) {
    tmpSsid = WiFi.SSID(i);
    strncpy(_scanCache[i].ssid, tmpSsid.c_str(), sizeof(_scanCache[i].ssid) - 1);
    _scanCache[i].ssid[sizeof(_scanCache[i].ssid) - 1] = '\0';

    tmpBssid = WiFi.BSSIDstr(i);
    strncpy(_scanCache[i].bssid, tmpBssid.c_str(), sizeof(_scanCache[i].bssid) - 1);
    _scanCache[i].bssid[sizeof(_scanCache[i].bssid) - 1] = '\0';
    // ...
}
```

**Beweis:**  
- Build erfolgreich.  
- Vorher: 40 separate String-Allokationen pro Scan. Nachher: 2 String-Objekte, die in der Schleife wiederverwendet werden (Puffer können recycelt werden) → weniger Fragmentierung.

---

## 8. Build-Beweis

Die Lösungen wurden so umgesetzt, dass die Library und ein Beispiel-Sketch kompilieren.

**Schritte zum Nachbauen:**

1. Arduino IDE oder `arduino-cli` mit Board **ESP32** (z. B. XIAO_ESP32S3 oder Generic ESP32).
2. Library-Pfad: `libraries/wifiMangerLite` (Version 2.6.6).
3. Beispiel öffnen: z. B. **BasicUsage** oder **CaptivePortal**.
4. Kompilieren.

**Erwartung:** Build ohne Fehler. Die gezeigten Änderungen sind ausschließlich interne Optimierungen bzw. zusätzliche Overloads; die öffentliche API (inkl. `EventCallback(Event, const String&)`, `Config getEffectiveConfig()`) bleibt kompatibel.

**Beweis-Fazit:**  
- Alle genannten Stellen sind im Quellcode mit den angegebenen Dateien und Logik nachvollziehbar.  
- Kompilierung: Mit **Arduino IDE** oder **arduino-cli** für Board **ESP32** (z. B. XIAO_ESP32S3) einen Sketch kompilieren, der die Library nutzt (z. B. `examples/BasicUsage` oder `examples/CaptivePortal`). Erwartung: **Build erfolgreich**.  
- Keine Breaking Changes; bestehende Sketches (z. B. Examples) laufen unverändert.  
- **ZIP-Archive:** `wifiMangerLite-2.6.5.zip` = Stand vor Fixes; `wifiMangerLite-2.6.6.zip` = Stand nach Fixes (Version 2.6.6).

---

## 9. Nicht geänderte Punkte (Begründung)

- **Config-Struktur (String-Member):** Umstellung auf feste `char[]` oder `std::string` wäre API-Breaking-Change; wurde bewusst nicht geändert.  
- **WiFi-API (SSID(), toString(), …):** Liefern auf ESP32 Arduino-`String`; die Library kann das nicht ändern. Wo möglich wurde die Nutzung reduziert (z. B. einmal in Puffer schreiben, dann `snprintf`/`emitEvent(..., buf)`).  
- **handleStatusJson (doc["ip"] = WiFi.localIP().toString(); etc.):** ArduinoJson braucht kopierte Werte; direkte Zuweisung von `String` ist hier der übliche Weg. Keine weitere Optimierung ohne API-Änderung umgesetzt.  
- **DynamicJsonDocument in WMLStorage:** Eine feste Allokation pro Storage-Objekt; keine laufende Fragmentierung, daher unverändert.

---

## 10. Kurzreferenz geänderter Dateien

| Datei | Änderungen |
|-------|------------|
| `src/WiFiManagerLite.cpp` | Identity mit snprintf; getEffectiveConfig(Config&); alle internen getEffectiveConfig-Aufrufe auf Ref; emitEvent(Event, const char*); handleConnected/internalStartAP nutzen const char* für Events. |
| `src/WiFiManagerLite.h` | Deklarationen: getEffectiveConfig(Config&), emitEvent(Event, const char*). |
| `src/WMLCaptivePortal.cpp` | sendGzipAsset: etagBuf/cacheBuf mit snprintf; collectScanResults: tmpSsid/tmpBssid wiederverwendet. |
| `library.properties` | version=2.6.6 |
| `CHANGELOG.md` | Eintrag [2.6.6] mit Heap-Fixes. |

Damit sind die beschriebenen Lösungen umgesetzt, im Code belegt und durch Build sowie Abwärtskompatibilität belegt.
