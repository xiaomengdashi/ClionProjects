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

// ��Logger�ඨ��ǰ���
enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

// ��������������������ת��Ϊ�ַ���
template<typename T>
std::string to_string_helper(T&& arg) {
    std::ostringstream oss;
    oss << std::forward<T>(arg);
    return oss.str();
}

// �̰߳�ȫ����־����
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

// Logger ��
class Logger {
public:
    Logger(const std::string& filename, LogLevel level = LogLevel::INFO)
    : log_file_(filename, std::ios::out | std::ios::app), exit_flag_(false),
    current_level_(level) {
        if (!log_file_.is_open()) {
            throw std::runtime_error("�޷�����־�ļ�");
        }
        worker_thread_ = std::thread(&Logger::processQueue, this);
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

    // ��־�ӿڣ�֧�ִ���ʽ�ַ�������־
    template<typename... Args>
    void log(LogLevel level, const std::string& format, Args&&... args) {
        if (level < current_level_) return; // ���Ե����ȼ�����־
        log_queue_.push(formatMessage(level, format, std::forward<Args>(args)...));
    }

    void setLevel(LogLevel new_level) {
        current_level_.store(new_level, std::memory_order_relaxed);
    }

private:
    LogQueue log_queue_;
    std::thread worker_thread_;
    std::ofstream log_file_;
    std::atomic<bool> exit_flag_;
    std::atomic<LogLevel> current_level_;

    // ��Logger�������˽�з���
    std::string getTimestamp() {
        auto now = std::chrono::system_clock::now();
        std::time_t time = std::chrono::system_clock::to_time_t(now);

        // �̰߳�ȫ��ʱ���ʽ������ƽ̨��
        struct tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &time);
#else
        localtime_r(&time, &tm_buf);
#endif

        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_buf);

        // ��Ӻ��루��ѡ��
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()
        ) % 1000;

        snprintf(timestamp + 19, sizeof(timestamp)-19, ".%03d", static_cast<int>(ms.count()));
        return timestamp;
    }

    void processQueue() {
        std::string msg;
        while (log_queue_.pop(msg)) {
            log_file_ << msg << std::endl;
        }
    }

    // �޸ĸ�ʽ������
    template<typename... Args>
    std::string formatMessage(LogLevel level, const std::string& format, Args&&... args) {
        std::string prefix = "[" + getTimestamp() + "] [" + levelToString(level) + "] ";
        return prefix + formatMessageImpl(format, std::forward<Args>(args)...);
    }

    // ����ת�ַ���
    std::string levelToString(LogLevel level) {
        static const std::string names[] = {"DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"};
        return names[static_cast<int>(level)];
    }

    // ʹ��ģ���۵���ʽ����־��Ϣ��֧�� "{}" ռλ��
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
                // û���㹻�Ĳ��������� "{}"
                oss << "{}";
            }
            pos = placeholder + 2; // ���� "{}"
            placeholder = format.find("{}", pos);
        }

        // ���ʣ����ַ���
        oss << format.substr(pos);

        // �������ʣ��Ĳ�������ԭ��ʽƴ��
        while (arg_index < arg_strings.size()) {
            oss << arg_strings[arg_index++];
        }

        return oss.str();
    }
};

// ʹ��ʾ��
int main() {
    try {
        Logger logger("log.txt");

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
        logger.log(LogLevel::INFO, "This message won't be recorded"); // ������
        logger.log(LogLevel::ERROR, "This message won't be recorded"); // ������

        // ģ��һЩ�ӳ���ȷ����̨�̴߳�������־
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    catch (const std::exception& ex) {
        std::cerr << "��־ϵͳ��ʼ��ʧ��: " << ex.what() << std::endl;
    }

    return 0;
}