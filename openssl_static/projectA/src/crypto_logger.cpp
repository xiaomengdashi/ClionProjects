#include "crypto_logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace CryptoUtils {

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::setLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    current_level_ = level;
}

void Logger::setOutputFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    file_stream_ = std::make_unique<std::ofstream>(filename, std::ios::app);
}

void Logger::enableConsoleOutput(bool enable) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    console_output_ = enable;
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::WARNING, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < current_level_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(log_mutex_);
    writeLog(level, message);
}

std::string Logger::levelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

std::string Logger::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

void Logger::writeLog(LogLevel level, const std::string& message) {
    std::string log_line = "[" + getCurrentTimestamp() + "] [" + 
                          levelToString(level) + "] " + message;
    
    if (console_output_) {
        std::cout << log_line << std::endl;
    }
    
    if (file_stream_ && file_stream_->is_open()) {
        *file_stream_ << log_line << std::endl;
        file_stream_->flush();
    }
}

} // namespace CryptoUtils