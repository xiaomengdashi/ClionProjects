#ifndef CRYPTO_LOGGER_H
#define CRYPTO_LOGGER_H

#include <string>
#include <fstream>
#include <memory>
#include <mutex>

namespace CryptoUtils {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3
};

/**
 * 简单的日志系统
 */
class Logger {
public:
    static Logger& getInstance();
    
    void setLevel(LogLevel level);
    void setOutputFile(const std::string& filename);
    void enableConsoleOutput(bool enable);
    
    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    
    void log(LogLevel level, const std::string& message);
    
private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    LogLevel current_level_ = LogLevel::INFO;
    bool console_output_ = true;
    std::unique_ptr<std::ofstream> file_stream_;
    std::mutex log_mutex_;
    
    std::string levelToString(LogLevel level) const;
    std::string getCurrentTimestamp() const;
    void writeLog(LogLevel level, const std::string& message);
};

// 便利宏
#define CRYPTO_LOG_DEBUG(msg) CryptoUtils::Logger::getInstance().debug(msg)
#define CRYPTO_LOG_INFO(msg) CryptoUtils::Logger::getInstance().info(msg)
#define CRYPTO_LOG_WARNING(msg) CryptoUtils::Logger::getInstance().warning(msg)
#define CRYPTO_LOG_ERROR(msg) CryptoUtils::Logger::getInstance().error(msg)

} // namespace CryptoUtils

#endif // CRYPTO_LOGGER_H