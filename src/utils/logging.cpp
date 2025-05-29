#include "utils/logging.h"
#include <stdexcept> // For std::runtime_error if file cannot be opened

// --- Logger Singleton Initialization ---
// Default initialization (can be configured later)
Logger::Logger()
    : current_level_(LogLevel::INFO), // Default log level
      console_output_enabled_(true) { // Default to console output
    // std::cout << "Logger initialized. Default level: INFO, Console: enabled." << std::endl;
}

Logger::~Logger() {
    // std::cout << "Logger shutting down." << std::endl;
    if (output_file_stream_.is_open()) {
        output_file_stream_.flush();
        output_file_stream_.close();
    }
}

Logger& Logger::getInstance() {
    // Thread-safe singleton initialization (Magic Statics, C++11 and later)
    static Logger instance;
    return instance;
}

// --- Configuration Methods ---
void Logger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    current_level_ = level;
    // log(LogLevel::INFO, "Log level set to " + levelToString(level)); // Log this change?
}

LogLevel Logger::getLogLevel() const {
    // No lock needed for const access to atomic or pre-set values if current_level_ were atomic.
    // However, if setLogLevel can be called concurrently, current_level_ read should be safe.
    // For this simple logger, current_level_ is protected by log_mutex_ during write.
    // A direct read might be racy if not also locked, but usually getLogLevel is called often
    // and setLogLevel rarely. For simplicity, not locking read here.
    // If high contention on setLogLevel/getLogLevel, current_level_ could be std::atomic.
    return current_level_;
}

void Logger::setConsoleOutput(bool enabled) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    console_output_enabled_ = enabled;
}

void Logger::setOutputFile(const std::string& file_path, bool append) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    if (output_file_stream_.is_open()) {
        output_file_stream_.flush();
        output_file_stream_.close();
    }
    output_file_path_ = file_path;
    if (!file_path.empty()) {
        std::ios_base::openmode mode = std::ios_base::out;
        if (append) {
            mode |= std::ios_base::app;
        }
        output_file_stream_.open(file_path, mode);
        if (!output_file_stream_.is_open()) {
            // Could throw, or log to console if possible
            std::cerr << "Error: Logger failed to open output file: " << file_path << std::endl;
            // Fallback: disable file logging or throw std::runtime_error("Failed to open log file: " + file_path);
            output_file_path_.clear();
        }
    }
}

// --- Helper Methods ---
std::string Logger::levelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::NONE:    return "NONE";
        case LogLevel::ERROR:   return "ERROR";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::TRACE:   return "TRACE";
        default:                return "UNKNOWN";
    }
}

std::string Logger::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::stringstream ss;
    // std::put_time is from <iomanip>
    // Note: std::put_time might not be fully thread-safe with some old library versions for the static buffer in std::localtime.
    // Using a C++20 chrono-based formatter would be better if available.
    // For this skeleton, assuming typical C++11/14/17 environment where it's usually fine.
    tm timeinfo;
#ifdef _WIN32
    localtime_s(&timeinfo, &now_c); // Windows specific
#else
    localtime_r(&now_c, &timeinfo); // POSIX specific
#endif
    ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

std::string Logger::formatMessage(LogLevel level, const std::string& message,
                                  const char* file, int line, const char* function) const {
    std::stringstream formatted_message;
    formatted_message << "[" << getCurrentTimestamp() << "] ";
    formatted_message << "[" << levelToString(level) << "] ";
    
    // Get thread ID
    // formatted_message << "[TID:" << std::this_thread::get_id() << "] ";

    if (file && line > 0) {
        std::string filename = file;
        // Get only the filename from the path
        size_t last_slash = filename.find_last_of("/\\");
        if (last_slash != std::string::npos) {
            filename = filename.substr(last_slash + 1);
        }
        formatted_message << "[" << filename << ":" << line;
        if (function) {
            formatted_message << " (" << function << ")";
        }
        formatted_message << "] ";
    } else if (function) {
         formatted_message << "[" << function << "] ";
    }

    formatted_message << message;
    return formatted_message.str();
}

// --- Core Logging Method ---
void Logger::log(LogLevel level, const std::string& message,
                 const char* file, int line, const char* function) {
    // Check log level (already done by macros, but good for direct calls)
    if (level > current_level_ || level == LogLevel::NONE) {
        return;
    }

    std::string final_message = formatMessage(level, message, file, line, function);

    std::lock_guard<std::mutex> lock(log_mutex_); // Ensure thread safety
    if (console_output_enabled_) {
        // Output to std::cerr for ERROR, std::cout for others
        (level == LogLevel::ERROR ? std::cerr : std::cout) << final_message << std::endl;
    }
    if (output_file_stream_.is_open()) {
        output_file_stream_ << final_message << std::endl;
        // output_file_stream_.flush(); // Optional: flush after every message (performance impact)
    }
}

// --- Convenience Logging Methods (using the main log method) ---
void Logger::error(const std::string& message, const char* file, int line, const char* function) {
    log(LogLevel::ERROR, message, file, line, function);
}
void Logger::warning(const std::string& message, const char* file, int line, const char* function) {
    log(LogLevel::WARNING, message, file, line, function);
}
void Logger::info(const std::string& message, const char* file, int line, const char* function) {
    log(LogLevel::INFO, message, file, line, function);
}
void Logger::debug(const std::string& message, const char* file, int line, const char* function) {
    log(LogLevel::DEBUG, message, file, line, function);
}
void Logger::trace(const std::string& message, const char* file, int line, const char* function) {
    log(LogLevel::TRACE, message, file, line, function);
}

/*
// --- LogMessage RAII Class Implementation (if used) ---
// LogMessage::LogMessage(Logger* logger, LogLevel msg_level, const char* file_path, int line_num, const char* func_name)
//     : owner_logger_(logger), level_(msg_level), file_(file_path), line_(line_num), function_(func_name), is_active_(true) {
//     // Check if this message should be logged based on current global log level
//     if (!owner_logger_ || level_ > owner_logger_->getLogLevel() || level_ == LogLevel::NONE) {
//         is_active_ = false;
//     }
// }

// LogMessage::~LogMessage() {
//     if (is_active_ && owner_logger_) {
//         owner_logger_->log(level_, stream_.str(), file_, line_, function_);
//     }
// }
*/
