# Web UI Build System

How to customize and build the web interface.

---

## Overview

The Web UI is built from source files in `webui_src/` and compiled into GZIP-compressed C headers in `src/generated/`. This provides:

- **Minification**: CSS/JS comments and whitespace removed
- **GZIP compression**: ~60% size reduction
- **Cache busting**: Build hash in URLs prevents stale cache
- **ETag support**: Efficient 304 Not Modified responses

---

## Directory Structure

```
wifiMangerLite/
├── webui_src/                  # Source files (edit these)
│   ├── basic_setup.html        # Basic setup page
│   ├── advanced_setup.html     # Advanced setup page
│   ├── advanced_status.html    # Advanced status page
│   ├── common.css              # Shared stylesheet (Homewind)
│   ├── advanced.css            # Advanced-only stylesheet
│   ├── common.js               # Shared JavaScript
│   └── advanced.js             # Advanced-only JavaScript
├── tools/
│   └── build_webui.py          # Build script
└── src/generated/              # Generated headers (don't edit)
    ├── wml_basic_setup_html.h
    ├── wml_setup_html.h
    ├── wml_status_html.h
    ├── wml_style_css.h
    ├── wml_app_js.h
    └── wml_web_manifest.h
```

---

## Building

### Prerequisites

- Python 3.6+
- No external dependencies

### Build Command

```bash
cd /path/to/wifiMangerLite
python3 tools/build_webui.py
```

### Output

```
==================================================
  WiFiManagerLite WebUI Build Pipeline
==================================================

Calculating build hash...
  Build hash: 3d74cea7

[BASIC] Processing...
  ✓ basic/setup.html: 5,068 → 2,094 bytes (gzip, 58.7%)
[ADVANCED] Processing...
  ✓ setup.html: 2,482 → 1,014 bytes (gzip, 59.1%)
  ✓ status.html: 2,490 → 893 bytes (gzip, 64.1%)
  ✓ style.css: 4,198 → 1,461 bytes (gzip, 65.2%)
  ✓ app.js: 7,366 → 2,710 bytes (gzip, 63.2%)

Generating headers...
  ✓ wml_basic_setup_html.h
  ✓ wml_setup_html.h
  ✓ wml_status_html.h
  ✓ wml_style_css.h
  ✓ wml_app_js.h
  ✓ wml_web_manifest.h

==================================================
  Build Summary
==================================================
  Basic assets:    1
  Advanced assets: 4
  Total size (raw): 21,604 bytes
  Total size (final): 8,172 bytes
  Compression: 62.2%
  Build hash: 3d74cea7

✅ Build complete!
```

---

## Cache Busting

The build script calculates a hash from all source files. This hash is:

1. Embedded in HTML as URL parameter:
   ```html
   <link rel="stylesheet" href="/wml/style.css?v=3d74cea7">
   <script src="/wml/app.js?v=3d74cea7"></script>
   ```

2. Used as ETag for HTTP caching:
   ```
   ETag: "3d74cea7"
   ```

3. Available as macro:
   ```cpp
   #define WML_BUILD_HASH "3d74cea7"
   ```

When any source file changes, the hash changes, forcing browsers to fetch new versions.

---

## Customization

### Basic Variant

Edit `webui_src/basic_setup.html`.

**Key sections:**

```html
<link rel="stylesheet" href="/wml/style.css?v=__WML_BUILD_HASH__"/>
<script src="/wml/app.js?v=__WML_BUILD_HASH__"></script>
```

### Advanced Variant

Edit separate files for better organization:

| File | Purpose |
|------|---------|
| `advanced_setup.html` | WiFi configuration page structure |
| `advanced_status.html` | Device status page structure |
| `common.css` | Shared styling (Homewind design system) |
| `advanced.css` | Advanced-only styling |
| `common.js` | Shared JavaScript logic |
| `advanced.js` | Advanced-only JavaScript |

**CSS Variables (common.css):**

```css
:root {
  --background-app: #EEF2FA;
  --tile-bg: rgba(255,255,255,0.8);
  --text-main: #132237;
  --text-muted: #516682;
  --accent-pink: #F00F66;
}
```

### Placeholder

Use `__WML_BUILD_HASH__` in HTML files for cache busting:

```html
<link rel="stylesheet" href="/wml/style.css?v=__WML_BUILD_HASH__">
```

The build script replaces this with the actual hash.

---

## Generated Header Format

Each asset becomes a C header with:

```cpp
// wml_style_css.h
const uint8_t wml_style_css_data[] PROGMEM = {
  0x1f, 0x8b, 0x08, 0x00, ...  // GZIP compressed data
};

const size_t wml_style_css_len = 1461;
```

The manifest provides lookup functions:

```cpp
// wml_web_manifest.h
namespace WML {
  #define WML_BUILD_HASH "3d74cea7"
  
  inline const uint8_t* getSetupPageData() {
    #if WML_PORTAL_VARIANT == WML_PORTAL_BASIC
      return wml_basic_setup_html_data;
    #else
      return wml_setup_html_data;
    #endif
  }
  
  inline size_t getSetupPageLen() { ... }
  inline bool hasStatusPage() { ... }
  inline bool hasSeparateAssets() { ... }
}
```

---

## Adding New Assets

1. Add file to `webui_src/` (or `webui_src/basic/` for basic variant)

2. Update `VARIANTS` in `build_webui.py`:
   ```python
   VARIANTS = {
       "basic": {
           "files": ["basic/setup.html"],
           "prefix": "wml_basic"
       },
       "advanced": {
           "files": ["setup.html", "status.html", "style.css", "app.js", "new_file.html"],
           "prefix": "wml"
       }
   }
   ```

3. Add web endpoint in `WMLCaptivePortal.cpp`:
   ```cpp
   _server.on("/wml/newpage", HTTP_GET, [this](AsyncWebServerRequest* request) {
       sendGzipAsset(request, wml_new_file_html_data, wml_new_file_html_len, "text/html", 300);
   });
   ```

4. Rebuild: `python3 tools/build_webui.py`

---

## Minification Rules

### CSS
- Remove comments (`/* ... */`)
- Remove whitespace around `{}:;,>+~`
- Collapse multiple spaces
- Remove trailing semicolons

### JavaScript
- Remove single-line comments (`// ...`)
- Remove multi-line comments (`/* ... */`)
- Remove empty lines
- Preserve newlines (safer than full minification)

### HTML
- Remove comments (`<!-- ... -->`)
- Collapse whitespace between tags
- Preserve content within `<script>` and `<style>`

---

## Troubleshooting

### Build script fails

```
❌ Error: Source directory not found
```
→ Run from library root, not from `tools/`

### Changes not reflected

1. Verify build completed successfully
2. Check browser cache (hard refresh: Ctrl+Shift+R)
3. Verify build hash changed in output

### GZIP not working

Browser must send `Accept-Encoding: gzip`. Most browsers do this automatically. The library checks the header and serves compressed content.

### File too large

For files >100KB, consider:
- Splitting into multiple files
- More aggressive minification
- External CDN for large assets (if device has internet)
