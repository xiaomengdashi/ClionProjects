//
// Created by Kolane on 2025/3/12.
//

#include <queue>
#include <string>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <fstream>
#include <atomic>
#include <sstream>
#include <vector>
#include <stdexcept>
#include <iostream>

// 在Logger类定义前添加
enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

// 辅助函数，将单个参数转换为字符串
template<typename T>
std::string to_string_helper(T&& arg) {
    std::ostringstream oss;
    oss << std::forward<T>(arg);
    return oss.str();
}

// 线程安全的日志队列
class LogQueue {
public:
    void push(const std::string& msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(msg);
        cond_var_.notify_one();
    }

    bool pop(std::string& msg) {
        std::unique_lock<std::mutex> lock(mutex_);
        while (queue_.empty() && !is_shutdown_) {
            cond_var_.wait(lock);
        }
        if (is_shutdown_ && queue_.empty()) {
            return false;
        }
        msg = queue_.front();
        queue_.pop();
        return true;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        is_shutdown_ = true;
        cond_var_.notify_all();
    }

private:
    std::queue<std::string> queue_;
    std::mutex mutex_;
    std::condition_variable cond_var_;
    bool is_shutdown_ = false;
};

// Logger 类
class Logger {
public:
    Logger(const std::string& filename, LogLevel level = LogLevel::INFO, 
          size_t max_size = 10*1024*1024)
    : filename_(filename), max_file_size_(max_size),
      current_file_size_(0), 
      log_file_(filename, std::ios::out | std::ios::app), exit_flag_(false),
    current_level_(level) {
        if (!log_file_.is_open()) {
            throw std::runtime_error("无法打开日志文件");
        }
        worker_thread_ = std::thread(&Logger::processQueue, this);
        initFileSize();
    }

    ~Logger() {
        log_queue_.shutdown();
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
        if (log_file_.is_open()) {
            log_file_.close();
        }
    }

    // 日志接口：支持带格式字符串的日志
    template<typename... Args>
    void log(LogLevel level, const std::string& format, Args&&... args) {
        if (level < current_level_) return; // 忽略低优先级的日志
        log_queue_.push(formatMessage(level, format, std::forward<Args>(args)...));
    }

    void setLevel(LogLevel new_level) {
        current_level_.store(new_level, std::memory_order_relaxed);
    }

private:
    void initFileSize() {
        std::ifstream in(filename_, std::ifstream::ate | std::ifstream::binary);
        if (in.is_open()) {
            current_file_size_ = in.tellg();
        }
    }

    void processQueue() {
        std::string msg;
        while (log_queue_.pop(msg)) {
            if (needRotate(msg.size())) {
                rotateFile();
            }
            
            log_file_ << msg << std::endl;
            std::cout << msg << std::endl;
            current_file_size_ += msg.size() + 1;

            log_file_.flush();  
        }
    }

    bool needRotate(size_t new_msg_size) const {
        return (current_file_size_ + new_msg_size + 1) > max_file_size_;
    }

    void rotateFile() {
        if (log_file_.is_open()) {
            log_file_.close();
        }

        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&t);
        
        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", &tm);
        std::string new_name = filename_ + "_" + buffer + ".log";

        std::rename(filename_.c_str(), new_name.c_str());

        log_file_.open(filename_, std::ios::out | std::ios::trunc);
        if (!log_file_.is_open()) {
            throw std::runtime_error("无法重新打开日志文件");
        }
        
        current_file_size_ = 0;
    }

private:
    LogQueue log_queue_;
    std::thread worker_thread_;
    std::ofstream log_file_;
    std::atomic<bool> exit_flag_;
    std::atomic<LogLevel> current_level_;
    std::string filename_;
    size_t max_file_size_;
    size_t current_file_size_;

    // 在Logger类内添加私有方法
    std::string getTimestamp() {
        auto now = std::chrono::system_clock::now();
        std::time_t time = std::chrono::system_clock::to_time_t(now);

        // 线程安全的时间格式化（跨平台）
        struct tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &time);
#else
        localtime_r(&time, &tm_buf);
#endif

        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_buf);

        // 添加毫秒（可选）
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()
        ) % 1000;

        snprintf(timestamp + 19, sizeof(timestamp)-19, ".%03d", static_cast<int>(ms.count()));
        return timestamp;
    }

    // 修改格式化方法
    template<typename... Args>
    std::string formatMessage(LogLevel level, const std::string& format, Args&&... args) {
        std::string prefix = "[" + getTimestamp() + "] [" + levelToString(level) + "] ";
        return prefix + formatMessageImpl(format, std::forward<Args>(args)...);
    }

    // 级别转字符串
    std::string levelToString(LogLevel level) {
        static const std::string names[] = {"DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"};
        return names[static_cast<int>(level)];
    }

    // 使用模板折叠格式化日志消息，支持 "{}" 占位符
    template<typename... Args>
    std::string formatMessageImpl(const std::string& format, Args&&... args) {
        std::vector<std::string> arg_strings = { to_string_helper(std::forward<Args>(args))... };
        std::ostringstream oss;
        size_t arg_index = 0;
        size_t pos = 0;
        size_t placeholder = format.find("{}", pos);

        while (placeholder != std::string::npos) {
            oss << format.substr(pos, placeholder - pos);
            if (arg_index < arg_strings.size()) {
                oss << arg_strings[arg_index++];
            } else {
                // 没有足够的参数，保留 "{}"
                oss << "{}";
            }
            pos = placeholder + 2; // 跳过 "{}"
            placeholder = format.find("{}", pos);
        }

        // 添加剩余的字符串
        oss << format.substr(pos);

        // 如果还有剩余的参数，按原方式拼接
        while (arg_index < arg_strings.size()) {
            oss << arg_strings[arg_index++];
        }

        return oss.str();
    }
};

// 使用示例
int main() {
    try {
        Logger logger("log.txt", LogLevel::DEBUG, 10*1024*1024);

        logger.log(LogLevel::INFO, "Starting application.");

        int user_id = 42;
        std::string action = "login";
        double duration = 3.5;
        std::string world = "World";

        logger.log(LogLevel::DEBUG, "User {} performed {} in {} seconds.", user_id, action, duration);
        logger.log(LogLevel::INFO,"Hello {}", world);
        logger.log(LogLevel::WARNING,"This is a message without placeholders.");
        logger.log(LogLevel::ERROR,"Multiple placeholders: {}, {}, {}.", 1, 2, 3);

        logger.setLevel(LogLevel::WARNING);
        logger.log(LogLevel::INFO, "This message won't be recorded"); // 被过滤
        logger.log(LogLevel::ERROR, "This message won't be recorded"); // 被过滤

        // 模拟一些延迟以确保后台线程处理完日志
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    catch (const std::exception& ex) {
        std::cerr << "日志系统初始化失败: " << ex.what() << std::endl;
    }

    return 0;
}