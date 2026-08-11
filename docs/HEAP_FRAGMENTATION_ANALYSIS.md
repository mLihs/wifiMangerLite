# Heap-Fragmentierung in wifiMangerLite – Analyse und Beweise

Diese Analyse durchsucht den **Quellcode der Library** (src/) nach Ursachen für Heap-Fragmentierung und belegt jede Stelle mit Datei und Zeile.

---

## 1. Zusammenfassung

**Gefundene Hauptursachen:**

| Kategorie | Schwere | Nachweis |
|-----------|---------|----------|
| Arduino-`String` in Config & überall | Hoch | WMLConfig.h, WiFiManagerLite, WMLCaptivePortal |
| `String`-Konkatenation (+ Operator) | Mittel | WiFiManagerLite.cpp, WMLCaptivePortal.cpp |
| Rückgabe/Kopie von `Config` (viele Strings) | Hoch | getEffectiveConfig(), getConfig(), IConfigProvider |
| WiFi-API liefert `String` (SSID, BSSID, toString()) | Mittel | Jeder Aufruf von WiFi.SSID(), .toString() etc. |
| Scan-Ergebnisse: String pro Netzwerk | Hoch | WMLCaptivePortal.cpp collectScanResults() |
| DynamicJsonDocument in Storage | Niedrig | WMLStorage.h – eine feste Allokation |

**Beweis:** Jede Stelle unten ist mit **Datei:Zeile** und Code-Zitat belegt.

---

## 2. Bewiesene Ursachen (mit Quellcode)

### 2.1 Config-Struktur nutzt durchgängig Arduino-`String`

**Datei:** `src/WMLConfig.h`

Die gesamte Konfiguration basiert auf Arduino-`String`. Jede Zuweisung oder Kopie kann den Heap vergrößern oder neue Blöcke erzeugen; Kopien von `Config` kopieren viele Strings gleichzeitig.

```cpp
// Zeilen 22–26, 38–41, 53–54, 91–95
struct WiFiCredentials {
    String ssid;
    String password;
    String bssid;
    // ...
};
struct StaticIPConfig { String ip; String gateway; String subnet; String dns; ... };
struct APConfig { String ssidPrefix; String password; ... };
struct Config {
    String deviceName;
    WiFiCredentials primary;   // 4 Strings
    WiFiCredentials secondary; // 4 Strings
    StaticIPConfig staticIP;   // 4 Strings
    APConfig ap;               // 2 Strings
    // ...
};
```

**Warum Fragmentierung:** Arduino-`String` verwaltet den Puffer auf dem Heap. Bei jedem `=`, `+`, `.remove()`, `.trim()` etc. können Reallokationen entstehen. Wenn `Config` per Wert zurückgegeben oder übergeben wird, werden alle diese Strings kopiert → viele Heap-Blöcke pro Aufruf.

---

### 2.2 Rückgabe von `Config` per Wert (mehrfach pro Loop)

**Datei:** `src/WiFiManagerLite.cpp`

- **Zeile 193–212:** `getEffectiveConfig()` gibt `Config` per Wert zurück und kann dabei Identität-Strings zuweisen:

```cpp
Config WiFiManagerLite::getEffectiveConfig() {
    Config cfg = _configProvider ? _configProvider->getConfig() : _config;
    // ...
    if (cfg.deviceName.length() == 0 || ...) {
        cfg.deviceName = _identityHostnameWithMac;  // String-Zuweisung
    }
    if (cfg.ap.ssidPrefix.length() == 0 || ...) {
        cfg.ap.ssidPrefix = _identityApPrefix;      // String-Zuweisung
    }
    return cfg;  // Kopie aller Config-Strings
}
```

- **Zeile 246, 259, 324, 361, 419, 454, 524, 696, 791:** `getEffectiveConfig()` wird in `begin()`, `loop()`, `startConnectAttempt()`, `processConnectAttempt()`, `handleConnected()`, `internalStartAP()`, `internalStopAP()` aufgerufen. Jeder Aufruf erzeugt eine komplette Kopie von `Config` inkl. aller Strings.

**Beweis:** Jeder `return cfg` bzw. jede Zuweisung `Config cfg = getEffectiveConfig()` kopiert mindestens 14+ String-Member → viele Heap-Allokationen bei häufigem Aufruf (z. B. in `loop()`).

---

### 2.3 String-Konkatenation und temporäre Strings (WiFiManagerLite)

**Datei:** `src/WiFiManagerLite.cpp`

- **Zeile 103:**

```cpp
_identityHostnameWithMac = hostBase + "-" + String(suffix);
```

`hostBase` ist bereits `String`, `"-"` und `String(suffix)` erzeugen temporäre Strings; `operator+` kann weitere temporäre Puffer erzeugen. Mehrere kleine Allokationen pro Identity-Berechnung.

- **Zeile 191 (getIdentityNameWithMac):**

```cpp
return base + "-" + String(suffix);
```

Wird bei jedem Aufruf von `getIdentityNameWithMac()` ausgeführt (z. B. aus Portal/UI). Jeder Aufruf: mindestens 2–3 temporäre Strings.

- **Zeile 636–638 (handleConnected):**

```cpp
snprintf(info, sizeof(info), "%s - %s", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
// ...
emitEvent(Event::Connected, String(info));
```

- `WiFi.SSID()` und `WiFi.localIP().toString()` liefern jeweils eine **temporäre Arduino-String** (Heap). Zusätzlich wird `String(info)` für `emitEvent` angelegt → weitere Allokation bei jedem Verbindungsaufbau.

- **Zeile 701–703, 778–779 (internalStartAP):**

```cpp
Serial.printf("[AP] ap.ip = %s\n", cfg.ap.ip.toString().c_str());
// ...
WML_LOGF("AP started: %s (IP: %s)", apName, cfg.ap.ip.toString().c_str());
emitEvent(Event::APStarted, String(apName));
```

Jedes `.toString()` ist eine neue String-Allokation; `String(apName)` für das Event ebenfalls.

**Beweis:** Kompilierbarer Code; jeder Ausdruck mit `+`, `String(...)` oder `.toString()` erzeugt im Arduino-String-Modell Heap-Allokationen.

---

### 2.4 Sanitize-Funktionen: Kopie + in-place Änderungen

**Datei:** `src/WiFiManagerLite.cpp`

- **Zeile 21–52 (sanitizeHostnameBaseLower):**

```cpp
String WiFiManagerLite::sanitizeHostnameBaseLower(const String& baseName) {
    String s = baseName;  // Kopie → Heap
    s.trim();             // kann Reallokation auslösen
    // ...
    for (...) s.setCharAt(i, '-');
    while ((pos = s.indexOf("--")) >= 0) {
        s.remove(pos, 1);  // remove() kann bei Arduino-String reallocieren
    }
    while (s.length() > 0 && s[0] == '-') s.remove(0, 1);
    // ...
    return s;  // Kopie
}
```

- **Zeile 55–88:** `sanitizeSsidBasePreserveCase()` ist analog (weitere Kopie, `remove()`, `setCharAt()`).

**Beweis:** `String s = baseName` und `return s` sind Kopien; `remove()`/`trim()` in der Arduino-String-Implementierung können den Puffer neu allokieren → Fragmentierung bei jedem Aufruf (z. B. bei Identity-Berechnung oder Namensabfrage).

---

### 2.5 Getter geben String zurück (WiFi-API)

**Datei:** `src/WiFiManagerLite.cpp` und `src/WiFiManagerLite.h`

- **Zeile 337–345:**

```cpp
String WiFiManagerLite::getSSID() const {
    return WiFi.SSID();
}
String WiFiManagerLite::getBSSID() const {
    return WiFi.BSSIDstr();
}
```

`WiFi.SSID()` und `WiFi.BSSIDstr()` liefern auf ESP32 Arduino-`String` (heap-allokiert). Jeder Aufruf von `getSSID()`/`getBSSID()` erzeugt mindestens eine weitere String-Kopie, wenn der Aufrufer das Ergebnis speichert oder konkateniert.

**Beweis:** ESP32 Arduino Core – `WiFiClass::SSID()` und `BSSIDstr()` geben `String` zurück; Rückgabe per Wert verursacht Kopie.

---

### 2.6 Captive Portal: Status-JSON und WiFi.String

**Datei:** `src/WMLCaptivePortal.cpp`

- **Zeile 364–372 (handleStatusJson):**

```cpp
StaticJsonDocument<1024> doc;
doc["connected"] = _wifiManager.isConnected();
doc["ssid"] = WiFi.SSID();           // String von WiFi → ArduinoJson kann kopieren
doc["ip"] = WiFi.localIP().toString(); // toString() = neue String-Allokation
doc["rssi"] = WiFi.RSSI();
doc["mac"] = WiFi.macAddress();       // String
// ...
```

`WiFi.SSID()`, `WiFi.localIP().toString()`, `WiFi.macAddress()` liefern jeweils Arduino-`String`. ArduinoJson übernimmt bei Zuweisung von `String` oft den Inhalt (kann je nach Version intern kopieren). Allein diese drei Zeilen erzeugen mehrere Heap-Strings pro Status-Request.

**Beweis:** Code wie oben; ESP32-API-Dokumentation und ArduinoJson-Verhalten für `String`.

---

### 2.7 Captive Portal: Cache-Control-Header mit String-Konkatenation

**Datei:** `src/WMLCaptivePortal.cpp`

- **Zeile 593 und 603:**

```cpp
response->addHeader("Cache-Control", "public, max-age=" + String(cacheSeconds));
```

Pro Asset-Request (HTML, CSS, JS) wird `String(cacheSeconds)` erzeugt und mit `"public, max-age="` konkateniert → mindestens eine temporäre String-Allokation pro Response.

**Beweis:** Direkt im Code; `operator+` mit String-Literal und `String(...)` erzeugt temporäre Strings.

---

### 2.8 Captive Portal: Scan-Ergebnisse – String pro Netzwerk

**Datei:** `src/WMLCaptivePortal.cpp`

- **Zeile 682–696 (collectScanResults):**

```cpp
for (uint8_t i = 0; i < maxNets; i++) {
    String ssid = WiFi.SSID(i);      // Heap-Allokation pro Netzwerk
    strncpy(_scanCache[i].ssid, ssid.c_str(), ...);
    String bssid = WiFi.BSSIDstr(i); // weitere Heap-Allokation
    strncpy(_scanCache[i].bssid, bssid.c_str(), ...);
    // ...
}
```

Bei z. B. 20 Netzen: 20× `WiFi.SSID(i)` und 20× `WiFi.BSSIDstr(i)` = 40 String-Allokationen pro Scan-Auswertung. Die Strings werden sofort in feste `char`-Arrays kopiert und gehen dann aus dem Scope → viele kleine freigegebene Blöcke → typische Fragmentierung.

**Beweis:** Schleife und Zuweisungen sind im Code; ESP32 `WiFi.SSID(int i)` / `BSSIDstr(int i)` geben `String` zurück.

---

### 2.9 Captive Portal: Request-Header und getParam als String

**Datei:** `src/WMLCaptivePortal.cpp`

- **Zeile 384–385, 391, 394:**

```cpp
String encoding = request->header("Accept-Encoding");
// ...
String etag = "\"" WML_BUILD_HASH "\"";
String clientEtag = request->header("If-None-Match");
```

- **Zeile 416–419 (handleSubmitConfig):**

```cpp
auto getParam = [request](const char* name) -> String {
    if (!request->hasParam(name, true)) return "";
    return request->getParam(name, true)->value();  // value() oft String
};
// ...
newConfig.deviceName = getParam("devicename");
newConfig.primary.ssid = getParam("ssid0");
newConfig.primary.password = getParam("password0");
// ... viele weitere getParam()-Aufrufe
```

Jeder `header()`- und `getParam()`-Aufruf kann ein `String` zurückgeben bzw. zuweisen. Pro Form-Submit: viele String-Zuweisungen in `newConfig` und bei Vergleich (z. B. `getParam("bssidLock") == "1"`).

**Beweis:** Typische AsyncWebServer-API: `getParam()->value()` und `header()` liefern oft `String`; Zuweisung in Config-Strings wie oben.

---

### 2.10 Storage: Config-Kopie und DynamicJsonDocument

**Datei:** `src/WMLStorage.h`, `src/WMLStorage.cpp`

- **WMLStorage.h Zeile 71:**  
  `DynamicJsonDocument _doc` (mit Größe z. B. kBufferSize) → eine feste Heap-Allokation pro Storage-Objekt. Keine laufende Fragmentierung, aber Heap-Nutzung.

- **WMLStorage.cpp Zeile 168–176 (StorageProvider::getConfig):**

```cpp
Config StorageProvider::getConfig() {
    if (!_loaded) {
        if (_storage.load(_cachedConfig)) { ... }
    }
    return _cachedConfig;  // Kopie des gesamten Config inkl. aller Strings
}
```

Jeder Aufruf von `getConfig()` (z. B. von WiFiManagerLite oder Portal) gibt eine vollständige Kopie von `Config` zurück → wieder viele String-Kopien.

- **WMLStorage.cpp Zeile 52–54 (exists):**

```cpp
bool Storage::exists() {
    Config tmp;
    return load(tmp);  // tmp wird mit allen Strings aus JSON gefüllt
}
```

`jsonToConfig()` weist jeder Config-String-Zeile etwas wie `config.xxx = doc["x"].as<const char*>()` zu; Arduino-String erzeugt dabei pro Zuweisung einen Heap-Puffer.

**Beweis:** Code wie zitiert; `Config` ist per Wert; `load()`/`jsonToConfig()` füllen String-Member aus JSON.

---

### 2.11 String-Member in Klassen

**Datei:** `src/WiFiManagerLite.h`, `src/WMLCaptivePortal.h`

- **WiFiManagerLite.h Zeile 298–301:**  
  `String _identityBaseName`, `_identityHostnameWithMac`, `_identityApPrefix`  
  Jede Zuweisung/Änderung (z. B. in `ensureIdentityComputed()`, `setIdentityBaseName()`) kann Heap verändern.

- **WMLCaptivePortal.h Zeile 231–235:**  
  `String _authUser`, `_authPass`, `_deviceName`, `_firmwareVersion`  
  Setter wie `setAuthentication(..., String)`, `setDeviceName(String)` etc. verursachen String-Zuweisungen.

**Beweis:** Member-Deklarationen in den genannten Headern.

---

## 3. Was die Library bereits heap-schonend macht

- **StaticJsonDocument** in Portal-Handlern (z. B. handleStatusJson, handleNetworkList, handleNetworkConfig) mit fester Größe → keine dynamische JSON-Allokation pro Request.
- **Response-Streaming** mit `beginResponseStream()` und `serializeJson(doc, *response)` statt großem String-Buffer.
- **Scan-Cache** als festes Struct-Array (`ScanNet _scanCache[kMaxScanNets]`) mit `char ssid[33]`/`char bssid[18]` – nur die Befüllung über `WiFi.SSID(i)`/`BSSIDstr(i)` erzeugt die oben beschriebenen temporären Strings.
- **snprintf** für Verbindungs-Info in `handleConnected()` (Zeile 634–636) statt reiner String-Konkatenation – der Flaschenhals dort sind die Aufrufe `WiFi.SSID().c_str()` und `.toString().c_str()` sowie `String(info)`.
- **DNSServer** einmal mit `new (std::nothrow)` angelegt und im Destruktor mit `delete` freigegeben – keine Fragmentierung durch viele kleine Allokationen.

---

## 4. Warum „nichts gefunden“ nicht zutrifft

Die Library enthält **mehrere klar belegte** Quellen für Heap-Fragmentierung:

1. **Durchgängige Nutzung von Arduino-`String`** in Config und in vielen Codepfaden (Identity, Events, Portal, Storage).
2. **Häufige Kopien von `Config`** durch `getEffectiveConfig()` und `getConfig()` (inkl. StorageProvider).
3. **WiFi-API** liefert überall `String` (SSID, BSSID, MAC, IP.toString()); die Library kann das nicht ändern, nutzt diese APIs aber an vielen Stellen.
4. **Konkatenation** mit `+` und `String(...)` in Identity, Events und Headern.
5. **Scan-Schleife** mit 40 String-Allokationen pro Scan in `collectScanResults()`.

Alle genannten Stellen sind im **Quellcode der Library** (src/) lokalisierbar und mit Zeilennummer und Code-Zitat belegt.

---

## 5. Empfohlene nächste Schritte (kurz)

- **Config:** Wo möglich feste `char[]`-Längen oder `std::string` mit reserviertem Speicher; oder Config per Referenz übergeben/ zurückgeben statt per Wert.
- **Identity/Events:** Puffer (z. B. `char buf[64]`) und `snprintf` statt `base + "-" + String(suffix)`; Event-Callback mit `const char*` + Länge statt `const String&`.
- **Portal:** Bei Scan-Befüllung direkt in `_scanCache[i]` mit `WiFi.SSID(i).toCharArray(...)` oder eigenem Buffer arbeiten, um temporäre `String`-Variablen in der Schleife zu vermeiden.
- **Header:** `char buf[32]; snprintf(buf, sizeof(buf), "public, max-age=%lu", (unsigned long)cacheSeconds); addHeader("Cache-Control", buf);` statt String-Konkatenation.

Damit lassen sich die identifizierten Ursachen schrittweise entschärfen, ohne das Verhalten der API nach außen grundlegend zu ändern.
