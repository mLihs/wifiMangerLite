# wifiMangerLite – Auswertung Program Storage (34%)

Analyse des Quellcodes zur Ermittlung der Ursachen für den hohen Program-Speicherbedarf. Keine Schätzungen – nur nachweisbare Fakten aus dem Repository.

---

## 1. Zusammenfassung

| Kategorie | Nachweis im Code | Geschätzter Flash-Anteil |
|-----------|------------------|---------------------------|
| **Embedded Web-Assets (PROGMEM)** | Alle 10 generierten Header inkludiert, beide Varianten | **~18 KB** (exakt berechnet) |
| **Doppelte Portal-Varianten** | Basic + Advanced immer gelinkt | **~14,6 KB** einsparbar |
| **Captive Portal + Abhängigkeiten** | ESPAsyncWebServer, ArduinoJson | Sehr groß (extern) |
| **Eigener Lib-Code** | 4 .cpp, viele .h | Mittel |
| **Feature-Flags** | WMLBuildConfig.h | Reduzierbar |

---

## 2. Embedded Web-Assets (PROGMEM) – exakte Zahlen

Die generierten Header unter `src/generated/` werden **alle** über `wml_web_manifest.h` eingebunden. Dort steht:

```cpp
// wml_web_manifest.h (Zeilen 16–25)
#include "wml_basic_style_css.h"
#include "wml_basic_app_js.h"
#include "wml_basic_setup_html.h"
#include "wml_style_css.h"
#include "wml_app_js.h"
#include "wml_setup_html.h"
#include "wml_status_html.h"
#include "wml_success_restart_html.h"
#include "wml_success_html.h"
#include "wml_error_html.h"
```

Es gibt **kein** `#if WML_PORTAL_VARIANT` um diese Includes. Dadurch landen **Basic- und Advanced-Assets immer gemeinsam** im Programm.

### 2.1 Tatsächlich im Flash liegende Daten (aus `*_len` im Code)

| Asset | Konstante (_len) | Bytes (PROGMEM) |
|-------|-------------------|------------------|
| wml_basic_setup_html | wml_basic_setup_html_len | 729 |
| wml_basic_app_js | wml_basic_app_js_len | 1 630 |
| wml_basic_style_css | wml_basic_style_css_len | 3 608 |
| wml_setup_html | wml_setup_html_len | 1 163 |
| wml_app_js | wml_app_js_len | 2 285 |
| wml_style_css | wml_style_css_len | 4 452 |
| wml_status_html | wml_status_html_len | 757 |
| wml_success_restart_html | wml_success_restart_html_len | 891 |
| wml_success_html | wml_success_html_len | 821 |
| wml_error_html | wml_error_html_len | 822 |
| **Summe** | | **18 158** |

**→ Rund 18 KB Flash nur für die GZIP-Web-Assets**, unabhängig von `WML_PORTAL_VARIANT`.

### 2.2 Doppelte Varianten

- **Basic-Varianten** (nur Setup/CSS/JS): 729 + 1 630 + 3 608 = **5 967 Bytes**
- **Advanced-Varianten** (Setup/CSS/JS + Status): 1 163 + 2 285 + 4 452 + 757 = **8 657 Bytes**

Zur Laufzeit wird nur eine Variante genutzt (`getSetupPageData()` etc. wählen per `WML_PORTAL_VARIANT`), aber **beide** sind im Binary.  
**Einsparpotenzial bei variantenabhängiger Einbindung: ~14,6 KB** (die jeweils andere Variante weglassen).

---

## 3. Eigener Bibliotheks-Code

### 3.1 Übersicht Quellcode

| Datei | Zeilen (ca.) | Inhalt |
|-------|----------------|--------|
| WiFiManagerLite.cpp | ~580 | WiFi-Logik, AP, Verbindungszustand, mDNS, Identity |
| WMLCaptivePortal.cpp | ~470 | Routen, Handler, GZIP-Auslieferung, Scan-Cache, JSON-APIs |
| WMLStorage.cpp | ~205 | NVS/MessagePack, configToJson/jsonToConfig |
| WMLConfig.h | ~285 | Config-Strukturen, viele inline Setter |
| WMLCaptivePortal.h | ~275 | CaptivePortal-Klasse, Extension-Registry |
| WMLBuildConfig.h | ~220 | Feature-Gates, Defaults |
| WMLPortalExtensions.h | ~206 | Erweiterungs-Interfaces |
| WiFiManagerLite.h | ~330 | Öffentliche API, State, Callbacks |
| WMLStorage.h | – | Storage/StorageProvider |
| WMLDebugLog.h | – | WML_LOG/WML_LOGF etc. |

Hinzu kommen die **generierten Header** (~114 KB Quelltext, kompiliert **~18 KB** reine PROGMEM-Daten, siehe oben).

### 3.2 Was den Code groß macht

- **WiFiManagerLite.cpp**
  - Viele `Serial.println`/`Serial.printf` in `internalStartAP()` (z. B. Zeilen 438–468, 474–476, 479–481, 486–488, 494–496, 504–506, 511–513, 519–521, 527–529) – nur bei aktivem Debug; mit `WML_ENABLE_DEBUG_LOGS=0` wegoptimierbar.
  - String-/Puffer-Logik, State-Machine, mDNS, AP-Start – notwendiger Funktionsumfang.

- **WMLCaptivePortal.cpp**
  - Verwendet **ESPAsyncWebServer** (lambdas, viele `on()`-Routen), **ArduinoJson** (StaticJsonDocument 1024, 64+kMaxScanNets*100).
  - GZIP-Auslieferung, Auth, viele Handler – typisch für ein vollwertiges Web-UI.

- **WMLStorage.cpp**
  - Nutzt **NVSUtilityLibrary** und **ArduinoJson** (MessagePack/JSON).  
  - `library.properties`: `depends=ArduinoJson,NVSUtilityLibrary`.

Die **34 % Program Storage** kommen nicht nur von diesen wenigen Dateien, sondern stark von den **externen Abhängigkeiten** (siehe Abschnitt 5).

---

## 4. Feature-Gates (WMLBuildConfig.h)

Relevante Schalter und Kommentare im Code:

| Define | Default | Kommentar im Code |
|--------|---------|-------------------|
| WML_ENABLE_CAPTIVE_PORTAL | 1 | „Set to 0 to disable and save **~20KB** flash“ (WMLBuildConfig.h Z.21) |
| WML_PORTAL_VARIANT | 0 (Basic) | Nur Laufzeit-Auswahl; beide Varianten trotzdem gelinkt (siehe 2.) |
| WML_ENABLE_DEBUG_LOGS | 1 | „saves **~2–4KB** flash“ (WMLBuildConfig.h Z.219, WMLDebugLog.h) |
| WML_ENABLE_STORAGE | 1 | Ohne Storage: WMLStorage + Teile NVS/Json weg |

Die ~20 KB beim Abschalten des Captive Portals passen zur Kombination aus **eigenem Portal-Code + Web-Assets (~18 KB) + weniger genutzter ESPAsyncWebServer/ArduinoJson** (weil Portal-Routen und -Handler wegfallen).

---

## 5. Abhängigkeiten (treiben 34 % mit)

- **Im Code verwendet, nicht in library.properties:**
  - **ESPAsyncWebServer** (WMLCaptivePortal.cpp) – typisch sehr großer Flash-Verbrauch.
  - **DNSServer**, **ESPmDNS**, **WiFi** (ESP32-Core).

- **In library.properties:**
  - **ArduinoJson**
  - **NVSUtilityLibrary** (NVSConfigBus)

Die **34 %** beziehen sich auf das **gesamte Programm** (Sketch + alle Libs). Ein großer Teil davon kommt von:

1. **ESPAsyncWebServer** (Captive Portal),
2. **ArduinoJson** (Portal + Storage),
3. **eigenem Code + ~18 KB Web-Assets**,
4. **NVSUtilityLibrary**.

Ohne einen konkreten Build (Linker-Map oder „Sketchgröße mit/ohne Lib“) kann man die 34 % nicht exakt aufteilen; die Aussage „34 % program storage“ bezieht sich auf den Gesamt-Firmware-Footprint, in den wifiMangerLite und ihre Abhängigkeiten stark reinspielen.

---

## 6. Konkrete Befunde (nur aus dem Repo)

1. **18 158 Bytes PROGMEM** für Web-Assets – aus den `*_len`-Konstanten in `src/generated/*.h` berechnet.
2. **Beide Portal-Varianten (Basic + Advanced)** werden immer gelinkt, weil alle generierten Header in `wml_web_manifest.h` ohne Varianten-`#if` inkludiert werden.
3. **Reduzierbar ohne Feature-Änderung:**
   - Nur die benötigte Portal-Variante einbinden → **~14,6 KB** weniger Flash möglich.
   - `WML_ENABLE_DEBUG_LOGS=0` → laut Code **~2–4 KB** weniger.
   - `WML_ENABLE_CAPTIVE_PORTAL=0` → laut Code **~20 KB** weniger (Portal + Nutzung von ESPAsyncWebServer/ArduinoJson reduziert).
4. **Viele Serial-Ausgaben** in `internalStartAP()` – nur bei Debug; mit `WML_ENABLE_DEBUG_LOGS=0` sollten sie wegfallen.

---

## 7. Empfehlungen zur Verringerung des Speicherbedarfs

1. **Variantenabhängige Includes**  
   In `wml_web_manifest.h` (oder im Build-Skript) nur die Header der gewählten `WML_PORTAL_VARIANT` inkludieren, damit nur eine Variante (Basic **oder** Advanced) im Binary landet.  
   → **~14,6 KB** weniger Flash.

2. **Captive Portal optional lassen**  
   `WML_ENABLE_CAPTIVE_PORTAL=0` nutzen, wenn kein Web-Setup nötig ist.  
   → Laut Kommentar **~20 KB** und weniger Nutzung von ESPAsyncWebServer/ArduinoJson.

3. **Debug-Logs für Release aus**  
   `WML_ENABLE_DEBUG_LOGS=0` für Produktion setzen.  
   → **~2–4 KB** laut Dokumentation.

4. **Messung**  
   Mit/ohne wifiMangerLite, mit/ohne Captive Portal, mit Basic vs. Advanced bauen und die Sketch-Größe (oder Linker-Map) vergleichen – dann sind die 34 % und die Wirkung der Optionen exakt bezifferbar.

---

*Stand: Auswertung ausschließlich auf Basis des Quellcodes in wifiMangerLite; keine Annahmen über Board-, Core- oder Toolchain-Version.*
