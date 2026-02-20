/**
 * @file WMLDebugLog.h
 * @brief Debug logging macros (gated by WML_ENABLE_DEBUG_LOGS)
 * 
 * Production builds should have WML_ENABLE_DEBUG_LOGS=0 to remove all debug output.
 * This reduces flash usage (~2-4KB) and improves performance.
 * 
 * Usage:
 *   WML_LOG("Message");           // Simple message
 *   WML_LOGF("Value: %d", val);   // Formatted message
 *   WML_LOG_PORTAL("Portal msg"); // Portal-prefixed message
 * 
 * @author Martin Lihs
 * @version 1.0.0
 */

#ifndef WML_DEBUG_LOG_H
#define WML_DEBUG_LOG_H

#include "WMLBuildConfig.h"

// Default: Debug logs enabled (set to 0 in production)
#ifndef WML_ENABLE_DEBUG_LOGS
#define WML_ENABLE_DEBUG_LOGS 1
#endif

// Log prefix (customizable)
#ifndef WML_LOG_PREFIX
#define WML_LOG_PREFIX "[WiFi] "
#endif

#ifndef WML_LOG_PREFIX_PORTAL
#define WML_LOG_PREFIX_PORTAL "[Portal] "
#endif

#if WML_ENABLE_DEBUG_LOGS

  // ==================== Debug Logging Enabled ====================
  
  // Simple log (String or const char*)
  #define WML_LOG(msg) do { \
      Serial.print(WML_LOG_PREFIX); \
      Serial.println(msg); \
  } while(0)
  
  // Formatted log (printf-style)
  #define WML_LOGF(fmt, ...) do { \
      Serial.print(WML_LOG_PREFIX); \
      Serial.printf(fmt "\n", ##__VA_ARGS__); \
  } while(0)
  
  // Portal-specific log
  #define WML_LOG_PORTAL(msg) do { \
      Serial.print(WML_LOG_PREFIX); \
      Serial.print(WML_LOG_PREFIX_PORTAL); \
      Serial.println(msg); \
  } while(0)
  
  // Portal-specific formatted log
  #define WML_LOGF_PORTAL(fmt, ...) do { \
      Serial.print(WML_LOG_PREFIX); \
      Serial.print(WML_LOG_PREFIX_PORTAL); \
      Serial.printf(fmt "\n", ##__VA_ARGS__); \
  } while(0)
  
  // Raw print (no prefix, no newline)
  #define WML_PRINT(x) Serial.print(x)
  #define WML_PRINTLN(x) Serial.println(x)
  #define WML_PRINTF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)

#else

  // ==================== Debug Logging Disabled ====================
  // All macros expand to nothing - compiler removes them completely
  
  #define WML_LOG(msg) ((void)0)
  #define WML_LOGF(fmt, ...) ((void)0)
  #define WML_LOG_PORTAL(msg) ((void)0)
  #define WML_LOGF_PORTAL(fmt, ...) ((void)0)
  #define WML_PRINT(x) ((void)0)
  #define WML_PRINTLN(x) ((void)0)
  #define WML_PRINTF(fmt, ...) ((void)0)

#endif // WML_ENABLE_DEBUG_LOGS

// ==================== Error Logging (Always Enabled) ====================
// Critical errors should always be visible, even in production

#define WML_ERROR(msg) do { \
    Serial.print("[WML ERROR] "); \
    Serial.println(msg); \
} while(0)

#define WML_ERRORF(fmt, ...) do { \
    Serial.print("[WML ERROR] "); \
    Serial.printf(fmt "\n", ##__VA_ARGS__); \
} while(0)

#endif // WML_DEBUG_LOG_H
