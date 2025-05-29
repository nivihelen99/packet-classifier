#ifndef LOGGING_H
#define LOGGING_H

#include <string>
#include <fstream> // For file logging
#include <sstream> // For formatting messages
#include <iostream> // For console logging
#include <mutex>    // For thread-safe logging
#include <chrono>   // For timestamps
#include <iomanip>  // For std::put_time
#include <memory>   // For std::shared_ptr, std::unique_ptr
#include <thread>   // For std::this_thread::get_id

// Define log levels
enum class LogLevel {
    NONE = 0,  // No logging
    ERROR,     // Critical errors
    WARNING,   // Warnings, potential issues
    INFO,      // General informational messages
    DEBUG,     // Detailed debugging information
    TRACE      // Very detailed tracing, function calls, etc.
};

// Forward declaration for LogMessage class if used for RAII-style logging
class LogMessage;

class Logger {
public:
    // Singleton access (optional, but common for global logger)
    static Logger& getInstance();

    // Disable copy/move for singleton or global utility
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    // Configuration methods
    void setLogLevel(LogLevel level);
    LogLevel getLogLevel() const;

    // Enable/disable console logging
    void setConsoleOutput(bool enabled);
    // Set file logging (if path is empty, file logging is disabled)
    void setOutputFile(const std::string& file_path, bool append = true);

    // Core logging method
    void log(LogLevel level, const std::string& message,
             const char* file = nullptr, int line = -1, const char* function = nullptr);

    // --- Convenience logging methods ---
    // Basic string logging
    void error(const std::string& message, const char* file = nullptr, int line = -1, const char* function = nullptr);
    void warning(const std::string& message, const char* file = nullptr, int line = -1, const char* function = nullptr);
    void info(const std::string& message, const char* file = nullptr, int line = -1, const char* function = nullptr);
    void debug(const std::string& message, const char* file = nullptr, int line = -1, const char* function = nullptr);
    void trace(const std::string& message, const char* file = nullptr, int line = -1, const char* function = nullptr);

    // RAII-style logging (returns a temporary object that logs on destruction)
    // Example: LOG_INFO() << "This is info message with value " << 42;
    // LogMessage raiiLog(LogLevel level, const char* file = nullptr, int line = -1, const char* function = nullptr);


protected: // Protected for potential subclassing if needed, private otherwise
    Logger(); // Constructor for singleton
    ~Logger();

private:
    LogLevel current_level_;
    bool console_output_enabled_;
    std::ofstream output_file_stream_;
    std::string output_file_path_;
    std::mutex log_mutex_; // Ensures thread safety for writing to file/console

    std::string formatMessage(LogLevel level, const std::string& message,
                              const char* file, int line, const char* function) const;
    std::string levelToString(LogLevel level) const;
    std::string getCurrentTimestamp() const;
};

// --- Macro-based logging for convenience (recommended) ---
// These macros automatically capture __FILE__, __LINE__, __FUNCTION__

// Helper to decide if to log based on current level
#define SHOULD_LOG(level) (level <= Logger::getInstance().getLogLevel() && level != LogLevel::NONE)

// Core logging macro
#define LOG_MSG(level, message) \
    do { \
        if (SHOULD_LOG(level)) { \
            std::stringstream ss_macro_internal_logger; \
            ss_macro_internal_logger << message; \
            Logger::getInstance().log(level, ss_macro_internal_logger.str(), __FILE__, __LINE__, __func__); \
        } \
    } while(0)

// Convenience macros for each level
#define LOG_ERROR(message)   LOG_MSG(LogLevel::ERROR, message)
#define LOG_WARNING(message) LOG_MSG(LogLevel::WARNING, message)
#define LOG_INFO(message)    LOG_MSG(LogLevel::INFO, message)
#define LOG_DEBUG(message)   LOG_MSG(LogLevel::DEBUG, message)
#define LOG_TRACE(message)   LOG_MSG(LogLevel::TRACE, message)

/*
// RAII Style Logging Macros (requires LogMessage class implementation)
// #define LOG_RAII(level) \
//    if (!SHOULD_LOG(level)) {} \
//    else Logger::getInstance().raiiLog(level, __FILE__, __LINE__, __func__)

// #define LOG_ERROR_RAII()   LOG_RAII(LogLevel::ERROR)
// #define LOG_WARNING_RAII() LOG_RAII(LogLevel::WARNING)
// #define LOG_INFO_RAII()    LOG_RAII(LogLevel::INFO)
// #define LOG_DEBUG_RAII()   LOG_RAII(LogLevel::DEBUG)
// #define LOG_TRACE_RAII()   LOG_RAII(LogLevel::TRACE)
*/

// Placeholder for LogMessage if using RAII style stream logging
// class LogMessage {
// public:
//     LogMessage(Logger* logger, LogLevel level, const char* file, int line, const char* function);
//     ~LogMessage(); // This is where the actual logging happens
//     template <typename T>
//     LogMessage& operator<<(const T& value) {
//         if (is_active_) stream_ << value;
//         return *this;
//     }
// private:
//     Logger* owner_logger_;
//     LogLevel level_;
//     const char* file_;
//     int line_;
//     const char* function_;
//     std::stringstream stream_;
//     bool is_active_;
// };

#endif // LOGGING_H
