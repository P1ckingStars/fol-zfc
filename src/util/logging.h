#pragma once

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#include "src/util/error.h"

namespace logic::util {

enum class LogLevel { DEBUG = 0, WARNING = 1, ERROR = 2, FATAL = 3 };

inline const char* level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR:   return "ERROR";
        case LogLevel::FATAL:   return "FATAL";
    }
    return "UNKNOWN";
}

// Global minimum log level - messages below this are ignored
inline LogLevel& min_log_level() {
    static LogLevel level = LogLevel::DEBUG;
    return level;
}

inline void set_log_level(LogLevel level) {
    min_log_level() = level;
}

// Log message with source location
inline void log_message(LogLevel level, const char* file, int line,
                        const std::string& message) {
    if (level < min_log_level()) return;

    std::ostream& out = (level >= LogLevel::ERROR) ? std::cerr : std::cout;
    out << "[" << level_to_string(level) << "] "
        << filename_from_path(file) << ":" << line
        << " - " << message << std::endl;

    if (level == LogLevel::FATAL) {
        std::abort();
    }
}

// Stream-style logger for building messages
class LogStream {
public:
    LogStream(LogLevel level, const char* file, int line)
        : level_(level), file_(file), line_(line), enabled_(level >= min_log_level()) {}

    ~LogStream() {
        if (enabled_) {
            log_message(level_, file_, line_, ss_.str());
        }
    }

    template<typename T>
    LogStream& operator<<(const T& value) {
        if (enabled_) {
            ss_ << value;
        }
        return *this;
    }

private:
    LogLevel level_;
    const char* file_;
    int line_;
    bool enabled_;
    std::ostringstream ss_;
};

}  // namespace logic::util

// Logging macros - capture file and line automatically
#define LOG_DEBUG   ::logic::util::LogStream(::logic::util::LogLevel::DEBUG, __FILE__, __LINE__)
#define LOG_WARNING ::logic::util::LogStream(::logic::util::LogLevel::WARNING, __FILE__, __LINE__)
#define LOG_ERROR   ::logic::util::LogStream(::logic::util::LogLevel::ERROR, __FILE__, __LINE__)
#define LOG_FATAL   ::logic::util::LogStream(::logic::util::LogLevel::FATAL, __FILE__, __LINE__)

// Conditional logging
#define LOG_IF(level, condition) \
    if (!(condition)) {} else LOG_##level

// Check macro - logs FATAL if condition is false
#define CHECK(condition) \
    LOG_IF(FATAL, !(condition)) << "Check failed: " #condition " "
