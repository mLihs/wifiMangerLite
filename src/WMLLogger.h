/**
 * @file WMLLogger.h
 * @brief Logger interface for WiFiManagerLite (DEPRECATED)
 * 
 * @deprecated This file is deprecated since v2.2.0.
 * Use WML_ENABLE_DEBUG_LOGS=0/1 in WMLBuildConfig.h instead.
 * The compile-time macro approach is more efficient (zero overhead when disabled).
 * 
 * See WMLDebugLog.h for the new logging system.
 * 
 * This file is kept for backwards compatibility but the classes are no longer
 * used by WiFiManagerLite internally.
 * 
 * @author Extracted from BambuBeacon project
 * @version 1.0.0 (deprecated)
 */

#pragma once

#include <Arduino.h>

// Deprecation warning
#if defined(__GNUC__) || defined(__clang__)
#warning "WMLLogger.h is deprecated. Use WML_ENABLE_DEBUG_LOGS in WMLBuildConfig.h instead."
#endif

namespace WML {

/**
 * @brief Abstract logger interface
 * 
 * Implement this interface to redirect logs to your preferred output
 */
class ILogger {
public:
    virtual ~ILogger() = default;
    
    /**
     * @brief Log a message
     * @param message The message to log
     */
    virtual void log(const String& message) = 0;
    
    /**
     * @brief Log a formatted message (printf-style)
     * @param format Format string
     * @param ... Variable arguments
     */
    virtual void logf(const char* format, ...) {
        char buffer[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        log(String(buffer));
    }
};

/**
 * @brief Serial logger implementation
 * 
 * Outputs logs to hardware Serial
 */
class SerialLogger : public ILogger {
public:
    explicit SerialLogger(const char* prefix = "[WiFi] ") 
        : _prefix(prefix) {}
    
    void log(const String& message) override {
        Serial.print(_prefix);
        Serial.println(message);
    }
    
    void logf(const char* format, ...) override {
        char buffer[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        Serial.print(_prefix);
        Serial.println(buffer);
    }
    
private:
    const char* _prefix;
};

/**
 * @brief Stream logger implementation
 * 
 * Outputs logs to any Stream object (Serial, SoftwareSerial, WebSerial, etc.)
 */
class StreamLogger : public ILogger {
public:
    explicit StreamLogger(Stream& stream, const char* prefix = "[WiFi] ") 
        : _stream(stream), _prefix(prefix) {}
    
    void log(const String& message) override {
        _stream.print(_prefix);
        _stream.println(message);
    }
    
    void logf(const char* format, ...) override {
        char buffer[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        _stream.print(_prefix);
        _stream.println(buffer);
    }
    
private:
    Stream& _stream;
    const char* _prefix;
};

/**
 * @brief Callback-based logger implementation
 * 
 * Calls a user-provided callback function for each log message
 */
class CallbackLogger : public ILogger {
public:
    using LogCallback = std::function<void(const String&)>;
    
    explicit CallbackLogger(LogCallback callback) 
        : _callback(callback) {}
    
    void log(const String& message) override {
        if (_callback) {
            _callback(message);
        }
    }
    
private:
    LogCallback _callback;
};

/**
 * @brief Null logger (discards all output)
 * 
 * Use when logging is not desired
 */
class NullLogger : public ILogger {
public:
    void log(const String& message) override {
        (void)message; // Suppress unused parameter warning
    }
};

/**
 * @brief Multi-output logger
 * 
 * Forwards logs to multiple loggers
 */
class MultiLogger : public ILogger {
public:
    void addLogger(ILogger* logger) {
        if (_count < kMaxLoggers && logger != nullptr) {
            _loggers[_count++] = logger;
        }
    }
    
    void log(const String& message) override {
        for (uint8_t i = 0; i < _count; i++) {
            if (_loggers[i]) {
                _loggers[i]->log(message);
            }
        }
    }
    
private:
    static const uint8_t kMaxLoggers = 4;
    ILogger* _loggers[kMaxLoggers] = {nullptr};
    uint8_t _count = 0;
};

} // namespace WML
