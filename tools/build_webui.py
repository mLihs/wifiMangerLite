#!/usr/bin/env python3
"""
WiFiManagerLite WebUI Build Pipeline

Compresses HTML, CSS, and JS files with GZIP and generates PROGMEM headers.
Supports build hash for cache-busting and minification:
  - JS: Terser (npx --yes terser), fallback to regex comment removal if unavailable
  - CSS: comment removal + whitespace minification (regex)
  - HTML: comment removal + whitespace collapse

MODULAR ARCHITECTURE:
  - common.css + common.js  → Base styles & logic (used by both variants)
  - advanced.css + advanced.js → Extensions for advanced UI
  
BUILD OUTPUT:
  - Basic:    common.css + common.js combined, basic_setup.html
  - Advanced: common + advanced combined, setup/status pages

Usage:
  python3 tools/build_webui.py

Output:
  src/generated/wml_basic_setup_html.h  - Basic setup page
  src/generated/wml_basic_style_css.h   - Basic styles (common only)
  src/generated/wml_basic_app_js.h      - Basic JS (common only)
  src/generated/wml_setup_html.h        - Advanced setup page
  src/generated/wml_status_html.h       - Advanced status page
  src/generated/wml_style_css.h         - Advanced styles (common + advanced)
  src/generated/wml_app_js.h            - Advanced JS (common + advanced)
  src/generated/wml_web_manifest.h      - Asset manifest
"""

import gzip
import os
import re
import sys
import hashlib
import subprocess
import tempfile
from pathlib import Path
from datetime import datetime

# Configuration
WEBUI_SRC_DIR = "webui_src"
OUTPUT_DIR = "src/generated"
HASH_PLACEHOLDER = "__WML_BUILD_HASH__"

# Files to combine
CSS_COMMON = "common.css"
CSS_ADVANCED = "advanced.css"
JS_COMMON = "common.js"
JS_ADVANCED = "advanced.js"

# HTML files
HTML_BASIC_SETUP = "basic_setup.html"
HTML_ADVANCED_SETUP = "advanced_setup.html"
HTML_ADVANCED_STATUS = "advanced_status.html"

# Response pages (used by both variants)
HTML_SUCCESS_RESTART = "success_restart.html"
HTML_SUCCESS = "success.html"
HTML_ERROR = "error.html"

MIME_TYPES = {
    '.html': 'text/html',
    '.css': 'text/css',
    '.js': 'application/javascript',
}

CACHE_SECONDS = {
    '.html': 300,
    '.css': 86400,
    '.js': 86400,
}


def get_mime_type(ext):
    return MIME_TYPES.get(ext, 'application/octet-stream')


def get_cache_seconds(ext):
    return CACHE_SECONDS.get(ext, 3600)


def minify_css(css):
    """Minify CSS by removing comments and whitespace."""
    css = re.sub(r'/\*[\s\S]*?\*/', '', css)  # Remove comments
    css = re.sub(r'\s*([{}:;,>+~])\s*', r'\1', css)  # Remove spaces around operators
    css = re.sub(r'\s+', ' ', css)  # Collapse whitespace
    css = re.sub(r':\s+', ':', css)  # Remove space after colon
    css = re.sub(r';}', '}', css)  # Remove trailing semicolons
    return css.strip()


def minify_js(js):
    """Minify JS by removing comments and normalizing whitespace (fallback when Terser unavailable)."""
    js = re.sub(r'(?<!:)//[^\n]*', '', js)  # Remove single-line comments
    js = re.sub(r'/\*[\s\S]*?\*/', '', js)  # Remove multi-line comments
    lines = [line.strip() for line in js.split('\n') if line.strip()]
    return '\n'.join(lines)


def minify_js_with_terser(js_content):
    """
    Minify JavaScript using Terser via npx (same as Homewind build_webui.py).

    Returns:
        Minified JavaScript string, or None if Terser is not available / fails.
    """
    try:
        with tempfile.NamedTemporaryFile(mode='w', suffix='.js', delete=False, encoding='utf-8') as tmp_in:
            tmp_in.write(js_content)
            tmp_in_path = tmp_in.name

        with tempfile.NamedTemporaryFile(mode='r', suffix='.js', delete=False, encoding='utf-8') as tmp_out:
            tmp_out_path = tmp_out.name

        result = subprocess.run(
            [
                'npx', '--yes', 'terser',
                tmp_in_path,
                '--ecma', '2020',
                '--compress',
                '--mangle',
                '--format', 'comments=false',
                '-o', tmp_out_path
            ],
            capture_output=True,
            text=True,
            timeout=30
        )

        os.unlink(tmp_in_path)

        if result.returncode == 0:
            with open(tmp_out_path, 'r', encoding='utf-8') as f:
                minified = f.read()
            os.unlink(tmp_out_path)
            return minified
        else:
            if os.path.exists(tmp_out_path):
                os.unlink(tmp_out_path)
            print(f"  Warning: Terser failed: {result.stderr}")
            return None
    except FileNotFoundError:
        print("  Warning: npx not found, JavaScript will use regex minification")
        return None
    except subprocess.TimeoutExpired:
        print("  Warning: Terser timed out, JavaScript will use regex minification")
        if 'tmp_in_path' in locals() and os.path.exists(tmp_in_path):
            os.unlink(tmp_in_path)
        if 'tmp_out_path' in locals() and os.path.exists(tmp_out_path):
            os.unlink(tmp_out_path)
        return None
    except Exception as e:
        print(f"  Warning: Terser error: {e}, JavaScript will use regex minification")
        if 'tmp_in_path' in locals() and os.path.exists(tmp_in_path):
            os.unlink(tmp_in_path)
        if 'tmp_out_path' in locals() and os.path.exists(tmp_out_path):
            os.unlink(tmp_out_path)
        return None


def minify_html(html):
    """Minify HTML by removing comments and collapsing whitespace."""
    html = re.sub(r'<!--(?!\[)[\s\S]*?-->', '', html)  # Remove comments
    html = re.sub(r'>\s+<', '><', html)  # Remove whitespace between tags
    html = re.sub(r'\s+', ' ', html)  # Collapse whitespace
    return html.strip()


def calculate_hash(data):
    """Calculate short MD5 hash for cache-busting."""
    if isinstance(data, str):
        data = data.encode('utf-8')
    return hashlib.md5(data).hexdigest()[:8]


def compress_gzip(data):
    """GZIP compress data with max compression."""
    if isinstance(data, str):
        data = data.encode('utf-8')
    return gzip.compress(data, compresslevel=9, mtime=0)


def bytes_to_c_array(data, var_name):
    """Convert bytes to C array."""
    lines = [f"const uint8_t {var_name}[] PROGMEM = {{"]
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        hex_bytes = ', '.join(f'0x{b:02x}' for b in chunk)
        lines.append(f"  {hex_bytes},")
    lines.append("};")
    return '\n'.join(lines)


def read_file(filepath):
    """Read file content as string."""
    with open(filepath, 'r', encoding='utf-8') as f:
        return f.read()


def combine_files(source_dir, filenames):
    """Combine multiple files into one string."""
    content = []
    for filename in filenames:
        filepath = source_dir / filename
        if filepath.exists():
            content.append(f"/* === {filename} === */\n")
            content.append(read_file(filepath))
            content.append("\n")
    return ''.join(content)


def process_content(content, ext, build_hash):
    """Process content: replace hash placeholder and minify. JS uses Terser if available, else regex fallback."""
    content = content.replace(HASH_PLACEHOLDER, build_hash)
    
    if ext == '.css':
        content = minify_css(content)
    elif ext == '.js':
        terser_result = minify_js_with_terser(content)
        content = terser_result if terser_result is not None else minify_js(content)
    elif ext == '.html':
        content = minify_html(content)
    
    return content


def compress_content(content):
    """Compress content and return best option."""
    if isinstance(content, str):
        raw_data = content.encode('utf-8')
    else:
        raw_data = content
    
    compressed_data = compress_gzip(raw_data)
    use_compressed = len(compressed_data) < len(raw_data)
    final_data = compressed_data if use_compressed else raw_data
    
    return {
        'raw_size': len(raw_data),
        'compressed_size': len(compressed_data),
        'final_data': final_data,
        'gzipped': use_compressed,
        'etag': calculate_hash(final_data)
    }


def generate_header(data, var_name, filename):
    """Generate C++ header file for asset."""
    header = f"""/**
 * @file {var_name}.h
 * @brief Auto-generated WebUI asset: {filename}
 * @warning DO NOT EDIT - Generated by build_webui.py
 * @date {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
 */

#ifndef WML_WEBUI_{var_name.upper()}_H
#define WML_WEBUI_{var_name.upper()}_H

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

{bytes_to_c_array(data['final_data'], f"{var_name}_data")}

const size_t {var_name}_len = {len(data['final_data'])};

#endif // WML_WEBUI_{var_name.upper()}_H
"""
    return header


def generate_manifest(assets, build_hash):
    """Generate asset manifest header."""
    includes = []
    for name in assets:
        includes.append(f'#include "{name}.h"')
    
    # Generate asset entries for advanced variant
    advanced_entries = []
    path_map = {
        'wml_setup_html': '/wml/setup',
        'wml_status_html': '/wml/status',
        'wml_style_css': '/wml/style.css',
        'wml_app_js': '/wml/app.js'
    }
    
    for name, info in assets.items():
        if name.startswith('wml_basic'):
            continue  # Skip basic assets in advanced manifest
        
        web_path = path_map.get(name, f'/wml/{name}')
        entry = f"""  {{
    .path = "{web_path}",
    .data = {name}_data,
    .len = {name}_len,
    .gzipped = {str(info['gzipped']).lower()},
    .mimeType = "{info['mime_type']}",
    .etag = "{info['etag']}",
    .cacheSeconds = {info['cache_seconds']}
  }}"""
        advanced_entries.append(entry)
    
    manifest = f"""/**
 * @file wml_web_manifest.h
 * @brief Auto-generated WebUI asset manifest
 * @warning DO NOT EDIT - Generated by build_webui.py
 * @date {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
 */

#ifndef WML_WEB_MANIFEST_H
#define WML_WEB_MANIFEST_H

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>
#include "../WMLBuildConfig.h"

{chr(10).join(includes)}

namespace WML {{

/**
 * @brief WebUI build hash for cache-busting
 */
#define WML_BUILD_HASH "{build_hash}"

/**
 * @struct WebAsset
 * @brief Single web asset entry
 */
struct WebAsset {{
  const char* path;
  const uint8_t* data;
  size_t len;
  bool gzipped;
  const char* mimeType;
  const char* etag;
  uint32_t cacheSeconds;
}};

#if WML_PORTAL_VARIANT == WML_PORTAL_ADVANCED

/**
 * @brief Advanced variant assets
 */
static const WebAsset WEB_ASSETS[] PROGMEM = {{
{(',' + chr(10)).join(advanced_entries)}
}};

static const size_t WEB_ASSETS_COUNT = {len(advanced_entries)};

#endif // WML_PORTAL_ADVANCED

/**
 * @brief Get setup page data based on variant
 */
inline const uint8_t* getSetupPageData() {{
#if WML_PORTAL_VARIANT == WML_PORTAL_BASIC
  return wml_basic_setup_html_data;
#else
  return wml_setup_html_data;
#endif
}}

/**
 * @brief Get setup page length based on variant
 */
inline size_t getSetupPageLen() {{
#if WML_PORTAL_VARIANT == WML_PORTAL_BASIC
  return wml_basic_setup_html_len;
#else
  return wml_setup_html_len;
#endif
}}

/**
 * @brief Get style.css data based on variant
 */
inline const uint8_t* getStyleData() {{
#if WML_PORTAL_VARIANT == WML_PORTAL_BASIC
  return wml_basic_style_css_data;
#else
  return wml_style_css_data;
#endif
}}

/**
 * @brief Get style.css length based on variant
 */
inline size_t getStyleLen() {{
#if WML_PORTAL_VARIANT == WML_PORTAL_BASIC
  return wml_basic_style_css_len;
#else
  return wml_style_css_len;
#endif
}}

/**
 * @brief Get app.js data based on variant
 */
inline const uint8_t* getAppJsData() {{
#if WML_PORTAL_VARIANT == WML_PORTAL_BASIC
  return wml_basic_app_js_data;
#else
  return wml_app_js_data;
#endif
}}

/**
 * @brief Get app.js length based on variant
 */
inline size_t getAppJsLen() {{
#if WML_PORTAL_VARIANT == WML_PORTAL_BASIC
  return wml_basic_app_js_len;
#else
  return wml_app_js_len;
#endif
}}

/**
 * @brief Check if variant has status page
 */
inline bool hasStatusPage() {{
#if WML_PORTAL_VARIANT == WML_PORTAL_ADVANCED
  return true;
#else
  return false;
#endif
}}

// ==================== Response Pages (used by both variants) ====================

/**
 * @brief Get success page (with restart) data
 */
inline const uint8_t* getSuccessRestartData() {{
  return wml_success_restart_html_data;
}}

inline size_t getSuccessRestartLen() {{
  return wml_success_restart_html_len;
}}

/**
 * @brief Get success page (no restart) data
 */
inline const uint8_t* getSuccessData() {{
  return wml_success_html_data;
}}

inline size_t getSuccessLen() {{
  return wml_success_html_len;
}}

/**
 * @brief Get error page data
 */
inline const uint8_t* getErrorData() {{
  return wml_error_html_data;
}}

inline size_t getErrorLen() {{
  return wml_error_html_len;
}}

}} // namespace WML

#endif // WML_WEB_MANIFEST_H
"""
    return manifest


def build_webui():
    print("=" * 55)
    print("  WiFiManagerLite WebUI Build Pipeline (Modular)")
    print("=" * 55)
    print()
    
    script_dir = Path(__file__).parent.absolute()
    project_root = script_dir.parent
    
    source_dir = project_root / WEBUI_SRC_DIR
    output_dir = project_root / OUTPUT_DIR
    
    if not source_dir.is_dir():
        print(f"❌ Error: Source directory not found: {source_dir}")
        return False
    
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Collect all source content for build hash
    print("Calculating build hash...")
    all_files = [
        CSS_COMMON, CSS_ADVANCED, JS_COMMON, JS_ADVANCED,
        HTML_BASIC_SETUP, HTML_ADVANCED_SETUP, HTML_ADVANCED_STATUS,
        HTML_SUCCESS_RESTART, HTML_SUCCESS, HTML_ERROR
    ]
    all_content = ''
    for f in all_files:
        filepath = source_dir / f
        if filepath.exists():
            all_content += read_file(filepath)
    
    build_hash = calculate_hash(all_content)
    print(f"  Build hash: {build_hash}")
    print()
    
    assets = {}
    total_raw = 0
    total_final = 0
    
    # ==================== BASIC VARIANT ====================
    print("[BASIC] Processing...")
    
    # Basic CSS (common only)
    basic_css = read_file(source_dir / CSS_COMMON)
    basic_css = process_content(basic_css, '.css', build_hash)
    data = compress_content(basic_css)
    data['mime_type'] = get_mime_type('.css')
    data['cache_seconds'] = get_cache_seconds('.css')
    assets['wml_basic_style_css'] = data
    total_raw += data['raw_size']
    total_final += len(data['final_data'])
    compression = (1 - len(data['final_data']) / data['raw_size']) * 100
    print(f"  ✓ basic/style.css (common): {data['raw_size']:,} → {len(data['final_data']):,} bytes ({compression:.1f}%)")
    
    # Basic JS (common only) – Terser used if available, else regex minification
    print("  Attempting to minify JS with Terser...")
    basic_js = read_file(source_dir / JS_COMMON)
    basic_js = process_content(basic_js, '.js', build_hash)
    data = compress_content(basic_js)
    data['mime_type'] = get_mime_type('.js')
    data['cache_seconds'] = get_cache_seconds('.js')
    assets['wml_basic_app_js'] = data
    total_raw += data['raw_size']
    total_final += len(data['final_data'])
    compression = (1 - len(data['final_data']) / data['raw_size']) * 100
    print(f"  ✓ basic/app.js (common): {data['raw_size']:,} → {len(data['final_data']):,} bytes ({compression:.1f}%)")
    
    # Basic HTML
    basic_html = read_file(source_dir / HTML_BASIC_SETUP)
    basic_html = process_content(basic_html, '.html', build_hash)
    data = compress_content(basic_html)
    data['mime_type'] = get_mime_type('.html')
    data['cache_seconds'] = get_cache_seconds('.html')
    assets['wml_basic_setup_html'] = data
    total_raw += data['raw_size']
    total_final += len(data['final_data'])
    compression = (1 - len(data['final_data']) / data['raw_size']) * 100
    print(f"  ✓ basic_setup.html: {data['raw_size']:,} → {len(data['final_data']):,} bytes ({compression:.1f}%)")
    
    print()
    
    # ==================== ADVANCED VARIANT ====================
    print("[ADVANCED] Processing...")
    
    # Advanced CSS (common + advanced combined)
    advanced_css = combine_files(source_dir, [CSS_COMMON, CSS_ADVANCED])
    advanced_css = process_content(advanced_css, '.css', build_hash)
    data = compress_content(advanced_css)
    data['mime_type'] = get_mime_type('.css')
    data['cache_seconds'] = get_cache_seconds('.css')
    assets['wml_style_css'] = data
    total_raw += data['raw_size']
    total_final += len(data['final_data'])
    compression = (1 - len(data['final_data']) / data['raw_size']) * 100
    print(f"  ✓ style.css (common+advanced): {data['raw_size']:,} → {len(data['final_data']):,} bytes ({compression:.1f}%)")
    
    # Advanced JS (common + advanced combined)
    advanced_js = combine_files(source_dir, [JS_COMMON, JS_ADVANCED])
    advanced_js = process_content(advanced_js, '.js', build_hash)
    data = compress_content(advanced_js)
    data['mime_type'] = get_mime_type('.js')
    data['cache_seconds'] = get_cache_seconds('.js')
    assets['wml_app_js'] = data
    total_raw += data['raw_size']
    total_final += len(data['final_data'])
    compression = (1 - len(data['final_data']) / data['raw_size']) * 100
    print(f"  ✓ app.js (common+advanced): {data['raw_size']:,} → {len(data['final_data']):,} bytes ({compression:.1f}%)")
    
    # Advanced Setup HTML
    adv_setup_html = read_file(source_dir / HTML_ADVANCED_SETUP)
    adv_setup_html = process_content(adv_setup_html, '.html', build_hash)
    data = compress_content(adv_setup_html)
    data['mime_type'] = get_mime_type('.html')
    data['cache_seconds'] = get_cache_seconds('.html')
    assets['wml_setup_html'] = data
    total_raw += data['raw_size']
    total_final += len(data['final_data'])
    compression = (1 - len(data['final_data']) / data['raw_size']) * 100
    print(f"  ✓ advanced_setup.html: {data['raw_size']:,} → {len(data['final_data']):,} bytes ({compression:.1f}%)")
    
    # Advanced Status HTML
    adv_status_html = read_file(source_dir / HTML_ADVANCED_STATUS)
    adv_status_html = process_content(adv_status_html, '.html', build_hash)
    data = compress_content(adv_status_html)
    data['mime_type'] = get_mime_type('.html')
    data['cache_seconds'] = get_cache_seconds('.html')
    assets['wml_status_html'] = data
    total_raw += data['raw_size']
    total_final += len(data['final_data'])
    compression = (1 - len(data['final_data']) / data['raw_size']) * 100
    print(f"  ✓ advanced_status.html: {data['raw_size']:,} → {len(data['final_data']):,} bytes ({compression:.1f}%)")
    
    print()
    
    # ==================== RESPONSE PAGES (shared) ====================
    print("[RESPONSE] Processing...")
    
    # Success with restart
    success_restart_html = read_file(source_dir / HTML_SUCCESS_RESTART)
    success_restart_html = process_content(success_restart_html, '.html', build_hash)
    data = compress_content(success_restart_html)
    data['mime_type'] = get_mime_type('.html')
    data['cache_seconds'] = 0  # No cache for response pages
    assets['wml_success_restart_html'] = data
    total_raw += data['raw_size']
    total_final += len(data['final_data'])
    compression = (1 - len(data['final_data']) / data['raw_size']) * 100
    print(f"  ✓ success_restart.html: {data['raw_size']:,} → {len(data['final_data']):,} bytes ({compression:.1f}%)")
    
    # Success without restart
    success_html = read_file(source_dir / HTML_SUCCESS)
    success_html = process_content(success_html, '.html', build_hash)
    data = compress_content(success_html)
    data['mime_type'] = get_mime_type('.html')
    data['cache_seconds'] = 0
    assets['wml_success_html'] = data
    total_raw += data['raw_size']
    total_final += len(data['final_data'])
    compression = (1 - len(data['final_data']) / data['raw_size']) * 100
    print(f"  ✓ success.html: {data['raw_size']:,} → {len(data['final_data']):,} bytes ({compression:.1f}%)")
    
    # Error page
    error_html = read_file(source_dir / HTML_ERROR)
    error_html = process_content(error_html, '.html', build_hash)
    data = compress_content(error_html)
    data['mime_type'] = get_mime_type('.html')
    data['cache_seconds'] = 0
    assets['wml_error_html'] = data
    total_raw += data['raw_size']
    total_final += len(data['final_data'])
    compression = (1 - len(data['final_data']) / data['raw_size']) * 100
    print(f"  ✓ error.html: {data['raw_size']:,} → {len(data['final_data']):,} bytes ({compression:.1f}%)")
    
    print()
    print("Generating headers...")
    
    # Generate individual headers
    for name, info in assets.items():
        header = generate_header(info, name, name.split('_')[-1])
        header_file = output_dir / f"{name}.h"
        with open(header_file, 'w', encoding='utf-8') as f:
            f.write(header)
        print(f"  ✓ {header_file.name}")
    
    # Generate manifest
    manifest = generate_manifest(assets, build_hash)
    manifest_file = output_dir / "wml_web_manifest.h"
    with open(manifest_file, 'w', encoding='utf-8') as f:
        f.write(manifest)
    print(f"  ✓ {manifest_file.name}")
    
    # Summary
    print()
    print("=" * 55)
    print("  Build Summary")
    print("=" * 55)
    print(f"  Basic variant:    3 assets")
    print(f"  Advanced variant: 4 assets")
    print(f"  Response pages:   3 assets (shared)")
    print(f"  Total size (raw):    {total_raw:,} bytes")
    print(f"  Total size (final):  {total_final:,} bytes")
    print(f"  Compression:         {(1 - total_final / total_raw) * 100:.1f}%")
    print(f"  Build hash:          {build_hash}")
    print()
    print("✅ Build complete!")
    print()
    print("Usage in WMLBuildConfig.h:")
    print("  #define WML_PORTAL_VARIANT WML_PORTAL_BASIC    // ~3KB")
    print("  #define WML_PORTAL_VARIANT WML_PORTAL_ADVANCED // ~6KB")
    print()
    
    return True


if __name__ == '__main__':
    success = build_webui()
    sys.exit(0 if success else 1)
