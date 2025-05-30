#include "gtest/gtest.h"
#include "utils/logging.h" // Adjust path as necessary
#include <sstream>
#include <fstream>
#include <regex>
#include <thread> // For testing thread ID later if needed, not for MT stress test here
#include <cstdio> // For std::remove

// Helper class to capture std::cout or std::cerr
class OutputInterceptor {
public:
    OutputInterceptor(std::ostream& stream_to_capture)
        : original_stream_(stream_to_capture), original_rdbuf_(stream_to_capture.rdbuf()) {
        stream_to_capture.rdbuf(captured_output_.rdbuf());
    }

    ~OutputInterceptor() {
        original_stream_.rdbuf(original_rdbuf_);
    }

    std::string GetCapturedOutput() const {
        return captured_output_.str();
    }

    void Clear() {
        captured_output_.str("");
        captured_output_.clear();
    }

private:
    std::ostream& original_stream_;
    std::streambuf* original_rdbuf_;
    std::stringstream captured_output_;

    OutputInterceptor(const OutputInterceptor&) = delete;
    OutputInterceptor& operator=(const OutputInterceptor&) = delete;
};


class LoggingTest : public ::testing::Test {
protected:
    Logger& logger_ = Logger::getInstance();
    LogLevel original_log_level_;
    bool original_console_output_enabled_;
    // To capture output for file logging, we'd need to know the file path or have a way to set it temporarily.

    // Store original std::cout and std::cerr buffers
    std::streambuf* original_cout_buf_ = nullptr;
    std::streambuf* original_cerr_buf_ = nullptr;
    std::stringstream captured_cout_;
    std::stringstream captured_cerr_;

    void SetUp() override {
        original_log_level_ = logger_.getLogLevel();
        // Assuming Logger has a way to get console output status, if not, we just control it.
        // original_console_output_enabled_ = logger_.isConsoleOutputEnabled(); // If it exists

        // Redirect cout and cerr
        original_cout_buf_ = std::cout.rdbuf();
        original_cerr_buf_ = std::cerr.rdbuf();
        std::cout.rdbuf(captured_cout_.rdbuf());
        std::cerr.rdbuf(captured_cerr_.rdbuf());
        logger_.setConsoleOutput(true); // Ensure console output is on for tests that capture it
        logger_.setOutputFile(""); // Ensure file output is off by default for most tests
    }

    void TearDown() override {
        logger_.setLogLevel(original_log_level_);
        // logger_.setConsoleOutput(original_console_output_enabled_); 
        logger_.setConsoleOutput(true); // Usually, want logs for other tests
        logger_.setOutputFile(""); // Make sure file logging is off

        // Restore cout and cerr
        std::cout.rdbuf(original_cout_buf_);
        std::cerr.rdbuf(original_cerr_buf_);
    }

    void ClearCaptured() {
        captured_cout_.str("");
        captured_cout_.clear();
        captured_cerr_.str("");
        captured_cerr_.clear();
    }

    std::string GetCapturedCout() { return captured_cout_.str(); }
    std::string GetCapturedCerr() { return captured_cerr_.str(); }
};

TEST_F(LoggingTest, SingletonInstance) {
    Logger& logger1 = Logger::getInstance();
    Logger& logger2 = Logger::getInstance();
    EXPECT_EQ(&logger1, &logger2);
}

TEST_F(LoggingTest, DefaultInitialState) {
    // Constructor of Logger sets default to INFO and console enabled.
    // This test might be tricky if other tests modify the global logger state before this one runs.
    // GTest usually creates a new test fixture object for each test.
    EXPECT_EQ(logger_.getLogLevel(), LogLevel::INFO); 
    // Cannot directly test console_output_enabled_ or output_file_path_ without getters,
    // but set them to known states in SetUp.
}

TEST_F(LoggingTest, SetAndGetLogLevel) {
    logger_.setLogLevel(LogLevel::DEBUG);
    EXPECT_EQ(logger_.getLogLevel(), LogLevel::DEBUG);
    logger_.setLogLevel(LogLevel::ERROR);
    EXPECT_EQ(logger_.getLogLevel(), LogLevel::ERROR);
}

TEST_F(LoggingTest, LogLevelFiltering) {
    const char* test_msg = "Test message";

    LogLevel levels_to_test[] = {LogLevel::ERROR, LogLevel::WARNING, LogLevel::INFO, LogLevel::DEBUG, LogLevel::TRACE};
    std::string level_strings[] = {"ERROR", "WARNING", "INFO", "DEBUG", "TRACE"};

    for (size_t i = 0; i < 5; ++i) { // Iterate through setting each level as current
        LogLevel current_set_level = levels_to_test[i];
        logger_.setLogLevel(current_set_level);
        SCOPED_TRACE("Current log level set to: " + std::string(level_strings[i]));

        for (size_t j = 0; j < 5; ++j) { // Iterate through logging at each level
            LogLevel message_level = levels_to_test[j];
            ClearCaptured();
            
            // Use macros to log
            if (message_level == LogLevel::ERROR) LOG_ERROR(test_msg);
            else if (message_level == LogLevel::WARNING) LOG_WARNING(test_msg);
            else if (message_level == LogLevel::INFO) LOG_INFO(test_msg);
            else if (message_level == LogLevel::DEBUG) LOG_DEBUG(test_msg);
            else if (message_level == LogLevel::TRACE) LOG_TRACE(test_msg);

            std::string output_cout = GetCapturedCout();
            std::string output_cerr = GetCapturedCerr();
            std::string combined_output = output_cout + output_cerr;

            if (message_level <= current_set_level) {
                EXPECT_FALSE(combined_output.empty()) << "Message at level " << level_strings[j] << " should be logged.";
                EXPECT_NE(combined_output.find(test_msg), std::string::npos);
                if (message_level == LogLevel::ERROR) {
                    EXPECT_FALSE(output_cerr.empty());
                    EXPECT_TRUE(output_cout.empty());
                } else {
                    EXPECT_FALSE(output_cout.empty());
                    EXPECT_TRUE(output_cerr.empty());
                }
            } else {
                EXPECT_TRUE(combined_output.empty()) << "Message at level " << level_strings[j] << " should NOT be logged.";
            }
        }
    }
}

TEST_F(LoggingTest, LogMessageFormat) {
    logger_.setLogLevel(LogLevel::DEBUG);
    const char* test_message_content = "Testing format 123!";
    
    // Line number for EXPECT_NE below
    int line_for_log_macro = __LINE__; LOG_DEBUG(test_message_content); // Log occurs on this line
    
    std::string output = GetCapturedCout();
    // std::cout << "Actual log output for format test: " << output << std::endl; // Debug output removed

    // 1. Timestamp: [YYYY-MM-DD HH:MM:SS.mmm]
    EXPECT_TRUE(std::regex_search(output, std::regex("\\[\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}\\.\\d{3}\\]")));
    // 2. Log Level: [DEBUG]
    EXPECT_NE(output.find("[DEBUG]"), std::string::npos);
    // 3. File, Line, Function: [logging_test.cpp:XX (TestBody)]
    std::string file_line_func_regex_str = "\\[logging_test.cpp:" + std::to_string(line_for_log_macro) + " \\(.*TestBody.*\\)\\]";
    EXPECT_TRUE(std::regex_search(output, std::regex(file_line_func_regex_str)))
        << "Regex was: " << file_line_func_regex_str << "\nActual output segment was: " << output; 
    // 4. Message content
    EXPECT_NE(output.find(test_message_content), std::string::npos);
}

TEST_F(LoggingTest, SetConsoleOutput) {
    logger_.setLogLevel(LogLevel::INFO);
    logger_.setConsoleOutput(false);
    LOG_INFO("This should not appear on console.");
    EXPECT_TRUE(GetCapturedCout().empty());
    EXPECT_TRUE(GetCapturedCerr().empty());

    logger_.setConsoleOutput(true);
    LOG_INFO("This should appear on console.");
    EXPECT_FALSE(GetCapturedCout().empty());
}

TEST_F(LoggingTest, FileOutput) {
    std::string temp_log_file = "temp_test_log.txt";
    // Ensure file is clean before test, if it exists
    std::remove(temp_log_file.c_str()); 

    logger_.setLogLevel(LogLevel::INFO);
    logger_.setOutputFile(temp_log_file, false); // false = don't append, overwrite

    LOG_INFO("Message for file.");
    LOG_ERROR("Error for file.");

    // Close and reopen or flush to ensure content is written
    logger_.setOutputFile(""); // This closes the current file stream

    std::ifstream log_file(temp_log_file);
    std::string file_content((std::istreambuf_iterator<char>(log_file)), std::istreambuf_iterator<char>());
    log_file.close();
    std::remove(temp_log_file.c_str()); // Clean up

    EXPECT_NE(file_content.find("Message for file."), std::string::npos);
    EXPECT_NE(file_content.find("[INFO]"), std::string::npos);
    EXPECT_NE(file_content.find("Error for file."), std::string::npos);
    EXPECT_NE(file_content.find("[ERROR]"), std::string::npos);
}

TEST_F(LoggingTest, FileOutputAppend) {
    std::string temp_log_file = "temp_test_log_append.txt";
    std::remove(temp_log_file.c_str()); 

    logger_.setLogLevel(LogLevel::INFO);
    logger_.setOutputFile(temp_log_file, false); // Overwrite initially
    LOG_INFO("First message.");
    logger_.setOutputFile(""); // Close

    logger_.setOutputFile(temp_log_file, true); // True = append
    LOG_WARNING("Second message, appended.");
    logger_.setOutputFile(""); // Close

    std::ifstream log_file(temp_log_file);
    std::string file_content((std::istreambuf_iterator<char>(log_file)), std::istreambuf_iterator<char>());
    log_file.close();
    std::remove(temp_log_file.c_str());

    EXPECT_NE(file_content.find("First message."), std::string::npos);
    EXPECT_NE(file_content.find("Second message, appended."), std::string::npos);
}


TEST_F(LoggingTest, LogMessageContentWithStream) {
    logger_.setLogLevel(LogLevel::INFO);
    int value = 42;
    std::string text = "some text";
    LOG_INFO("Value: " << value << ", Text: " << text);
    
    std::string output = GetCapturedCout();
    EXPECT_NE(output.find("Value: 42"), std::string::npos);
    EXPECT_NE(output.find("Text: some text"), std::string::npos);
}

TEST_F(LoggingTest, LogLevelNone) {
    logger_.setLogLevel(LogLevel::NONE);
    LOG_ERROR("This error should not be logged.");
    LOG_INFO("This info should not be logged.");
    EXPECT_TRUE(GetCapturedCout().empty());
    EXPECT_TRUE(GetCapturedCerr().empty());
}

// int main(int argc, char **argv) {
//     ::testing::InitGoogleTest(&argc, argv);
//     return RUN_ALL_TESTS();
// }
