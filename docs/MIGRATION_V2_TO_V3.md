# Migration Guide: WiFiManagerLite v2.x → v3.x

> **Target Audience**: AI Agents performing automated code migration

## Summary

Version 3.0 replaces all `String` members in the `Config` structure with fixed-size `char[]` arrays to eliminate heap fragmentation. This is a **breaking API change**.

## Quick Reference: Transformation Rules

### Rule 1: Direct String Assignment → Setter Method

```cpp
// BEFORE (v2.x)
config.deviceName = "MyDevice";
config.primary.ssid = "NetworkName";
config.primary.password = "secret123";
config.primary.bssid = "AA:BB:CC:DD:EE:FF";
config.secondary.ssid = "BackupNetwork";
config.staticIP.ip = "192.168.1.100";
config.staticIP.gateway = "192.168.1.1";
config.staticIP.subnet = "255.255.255.0";
config.staticIP.dns = "8.8.8.8";
config.ap.ssidPrefix = "ESP-Setup-";
config.ap.password = "appassword";

// AFTER (v3.x)
config.setDeviceName("MyDevice");
config.primary.setSsid("NetworkName");
config.primary.setPassword("secret123");
config.primary.setBssid("AA:BB:CC:DD:EE:FF");
config.secondary.setSsid("BackupNetwork");
config.staticIP.setIp("192.168.1.100");
config.staticIP.setGateway("192.168.1.1");
config.staticIP.setSubnet("255.255.255.0");
config.staticIP.setDns("8.8.8.8");
config.ap.setSsidPrefix("ESP-Setup-");
config.ap.setPassword("appassword");
```

### Rule 2: `.c_str()` Access → Direct Access

```cpp
// BEFORE (v2.x)
Serial.printf("Device: %s\n", config.deviceName.c_str());
Serial.printf("SSID: %s\n", config.primary.ssid.c_str());
someFunction(config.staticIP.ip.c_str());

// AFTER (v3.x) - char[] is already char*, no .c_str() needed
Serial.printf("Device: %s\n", config.deviceName);
Serial.printf("SSID: %s\n", config.primary.ssid);
someFunction(config.staticIP.ip);
```

### Rule 3: `.length() > 0` Check → `[0] != '\0'` Check

```cpp
// BEFORE (v2.x)
if (config.primary.ssid.length() > 0) { ... }
if (config.staticIP.ip.length() > 0) { ... }
config.secondary.ssid.length() > 0 ? config.secondary.ssid : "—"

// AFTER (v3.x)
if (config.primary.ssid[0] != '\0') { ... }
if (config.staticIP.ip[0] != '\0') { ... }
config.secondary.ssid[0] != '\0' ? config.secondary.ssid : "—"
```

### Rule 4: String Concatenation → Wrap with `String()`

```cpp
// BEFORE (v2.x) - String + String works
String html = "Title: " + config.deviceName + " - Status";

// AFTER (v3.x) - char[] needs String() wrapper for concatenation
String html = "Title: " + String(config.deviceName) + " - Status";
```

**Important**: This is only needed when concatenating with `+` operator. For `printf`-style functions, direct access works (see Rule 2).

### Rule 5: Empty String Comparison → Null-terminator Check

```cpp
// BEFORE (v2.x)
if (config.deviceName == "") { ... }
if (config.primary.ssid.isEmpty()) { ... }

// AFTER (v3.x)
if (config.deviceName[0] == '\0') { ... }
if (config.primary.ssid[0] == '\0') { ... }
```

---

## Search Patterns for Automated Migration

Use these regex patterns to find code that needs updating:

### Pattern 1: Direct Assignment to Config String Fields

```regex
\.(deviceName|ssid|password|bssid|ip|gateway|subnet|dns|ssidPrefix)\s*=\s*["']
```

**Action**: Replace with corresponding setter method.

### Pattern 2: `.c_str()` on Config Fields

```regex
(config|cfg|_config)\.(deviceName|primary|secondary|staticIP|ap)\.(ssid|password|bssid|ip|gateway|subnet|dns|ssidPrefix)?\.c_str\(\)
```

**Action**: Remove `.c_str()` - field is already `char*`.

### Pattern 3: `.length() > 0` Checks

```regex
(config|cfg)\.(deviceName|primary\.ssid|primary\.password|secondary\.ssid|staticIP\.ip|staticIP\.gateway)\.length\(\)\s*>\s*0
```

**Action**: Replace with `field[0] != '\0'`.

### Pattern 4: String Concatenation with Config Fields

```regex
["']\s*\+\s*(config|cfg)\.(deviceName|primary\.ssid)
```

or

```regex
(config|cfg)\.(deviceName|primary\.ssid)\s*\+\s*["']
```

**Action**: Wrap field with `String(field)`.

---

## Complete Field Reference

### Config Structure

| Field Path | Type (v2.x) | Type (v3.x) | Setter Method |
|------------|-------------|-------------|---------------|
| `deviceName` | `String` | `char[33]` | `setDeviceName(const char*)` |

### WiFiCredentials Structure (primary, secondary)

| Field Path | Type (v2.x) | Type (v3.x) | Setter Method |
|------------|-------------|-------------|---------------|
| `.ssid` | `String` | `char[33]` | `setSsid(const char*)` |
| `.password` | `String` | `char[65]` | `setPassword(const char*)` |
| `.bssid` | `String` | `char[18]` | `setBssid(const char*)` |

### StaticIPConfig Structure

| Field Path | Type (v2.x) | Type (v3.x) | Setter Method |
|------------|-------------|-------------|---------------|
| `.ip` | `String` | `char[16]` | `setIp(const char*)` |
| `.gateway` | `String` | `char[16]` | `setGateway(const char*)` |
| `.subnet` | `String` | `char[16]` | `setSubnet(const char*)` |
| `.dns` | `String` | `char[16]` | `setDns(const char*)` |

### APConfig Structure

| Field Path | Type (v2.x) | Type (v3.x) | Setter Method |
|------------|-------------|-------------|---------------|
| `.ssidPrefix` | `String` | `char[25]` | `setSsidPrefix(const char*)` |
| `.password` | `String` | `char[65]` | `setPassword(const char*)` |

---

## Migration Checklist

For each file using `WML::Config`:

- [ ] Find all direct string assignments (`field = "value"`) → Replace with setter
- [ ] Find all `.c_str()` calls on config fields → Remove `.c_str()`
- [ ] Find all `.length() > 0` checks → Replace with `[0] != '\0'`
- [ ] Find all `.isEmpty()` checks → Replace with `[0] == '\0'`
- [ ] Find all string concatenations with config fields → Wrap with `String()`
- [ ] Verify compilation succeeds
- [ ] Test functionality

---

## Common Migration Errors

### Error 1: `incompatible types in assignment of 'const char [N]' to 'char [M]'`

**Cause**: Direct assignment to char[] field  
**Solution**: Use setter method

```cpp
// Wrong
config.deviceName = "MyDevice";

// Correct
config.setDeviceName("MyDevice");
```

### Error 2: `invalid operands of types 'const char [N]' and 'char [M]' to binary 'operator+'`

**Cause**: String concatenation with char[] without wrapper  
**Solution**: Wrap char[] in String()

```cpp
// Wrong
String html = "Device: " + config.deviceName;

// Correct
String html = "Device: " + String(config.deviceName);
```

### Error 3: `'class char [N]' has no member named 'c_str'`

**Cause**: Calling .c_str() on char[] (not needed)  
**Solution**: Remove .c_str()

```cpp
// Wrong
Serial.printf("%s", config.deviceName.c_str());

// Correct
Serial.printf("%s", config.deviceName);
```

### Error 4: `'class char [N]' has no member named 'length'`

**Cause**: Calling .length() on char[]  
**Solution**: Use strlen() or [0] check

```cpp
// Wrong
if (config.deviceName.length() > 0)

// Correct
if (config.deviceName[0] != '\0')
// or
if (strlen(config.deviceName) > 0)
```

---

## Setter Method Signatures

All setters accept both `const char*` and `const String&`:

```cpp
// In WiFiCredentials:
void setSsid(const char* s);
void setSsid(const String& s);  // calls setSsid(s.c_str())
void setPassword(const char* s);
void setPassword(const String& s);
void setBssid(const char* s);
void setBssid(const String& s);

// In StaticIPConfig:
void setIp(const char* s);
void setIp(const String& s);
void setGateway(const char* s);
void setGateway(const String& s);
void setSubnet(const char* s);
void setSubnet(const String& s);
void setDns(const char* s);
void setDns(const String& s);

// In APConfig:
void setSsidPrefix(const char* s);
void setSsidPrefix(const String& s);
void setPassword(const char* s);
void setPassword(const String& s);

// In Config:
void setDeviceName(const char* s);
void setDeviceName(const String& s);
```

---

## Version Information

- **Source Version**: 2.6.x (String-based Config)
- **Target Version**: 3.0.0 (char[]-based Config)
- **Breaking Change**: Yes
- **Semantic Versioning**: Major version increment required

---

## Benefits of Migration

| Metric | v2.x | v3.x | Improvement |
|--------|------|------|-------------|
| Heap Fragmentation | ~20.6% | ~18.8% | -1.8 pp |
| Largest Free Block | 180 KB | 184 KB | +4 KB |
| Config Memory | ~320 B heap | ~296 B stack | Zero heap |
| String Allocations | Multiple | Zero | 100% reduction |
