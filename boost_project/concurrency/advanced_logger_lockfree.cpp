// 高级日志系统 - Boost 无锁队列版本
// 支持日志等级、多策略输出，使用 Boost lockfree::queue
//
// 为什么使用 Boost 无锁队列？
// 1. 更高的并发性能（无锁，不需要互斥锁）
// 2. 更低的延迟（避免锁争用）
// 3. 更好的多核扩展性
//
// 关键技术点：
// - 使用原始指针包装 LogEntry（避免复杂拷贝）
// - 在栈上创建 LogEntry，然后通过队列传递指针
// - 消费线程负责释放内存

#include <boost/lockfree/queue.hpp>
#include <thread>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <atomic>
#include <mutex>
#include <memory>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

// ============================================================
// 日志等级定义
// ============================================================
enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    FATAL = 5
};

std::string level_to_string(LogLevel level) {
    switch(level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

// ============================================================
// 日志条目 - 使用指针传输
// ============================================================
struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    LogLevel level;
    std::string message;

    LogEntry() = default;
    LogEntry(LogLevel l, const std::string& msg)
        : timestamp(std::chrono::system_clock::now()), level(l), message(msg) {}

    std::string format() const {
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        auto tm = *std::localtime(&time_t);

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
            << " [" << level_to_string(level) << "] "
            << message;
        return oss.str();
    }
};

// ============================================================
// 输出策略基类
// ============================================================
class OutputStrategy {
public:
    virtual ~OutputStrategy() = default;
    virtual bool should_output(LogLevel level) const = 0;
    virtual void output(const LogEntry& entry) = 0;
    virtual std::string name() const = 0;
};

// ============================================================
// 文件输出策略
// ============================================================
class FileOutputStrategy : public OutputStrategy {
private:
    std::ofstream file_;
    LogLevel min_level_;
    mutable std::mutex file_mutex_;

public:
    FileOutputStrategy(const std::string& filename, LogLevel min_level = LogLevel::TRACE)
        : file_(filename, std::ios::app), min_level_(min_level) {
        if (!file_.is_open()) {
            std::cerr << "Failed to open log file: " << filename << std::endl;
        }
    }

    ~FileOutputStrategy() {
        if (file_.is_open()) {
            file_.close();
        }
    }

    bool should_output(LogLevel level) const override {
        return static_cast<int>(level) >= static_cast<int>(min_level_);
    }

    void output(const LogEntry& entry) override {
        if (!should_output(entry.level)) {
            return;
        }

        std::unique_lock<std::mutex> lock(file_mutex_);
        if (file_.is_open()) {
            file_ << entry.format() << std::endl;
            file_.flush();
        }
    }

    std::string name() const override {
        return "FileOutput";
    }

    void set_min_level(LogLevel level) {
        min_level_ = level;
    }
};

// ============================================================
// 控制台输出策略
// ============================================================
class ConsoleOutputStrategy : public OutputStrategy {
private:
    LogLevel min_level_;
    mutable std::mutex console_mutex_;

public:
    ConsoleOutputStrategy(LogLevel min_level = LogLevel::INFO)
        : min_level_(min_level) {}

    bool should_output(LogLevel level) const override {
        return static_cast<int>(level) >= static_cast<int>(min_level_);
    }

    void output(const LogEntry& entry) override {
        if (!should_output(entry.level)) {
            return;
        }

        std::unique_lock<std::mutex> lock(console_mutex_);

        std::string color;
        switch(entry.level) {
            case LogLevel::TRACE: color = "\033[36m"; break;
            case LogLevel::DEBUG: color = "\033[34m"; break;
            case LogLevel::INFO:  color = "\033[32m"; break;
            case LogLevel::WARN:  color = "\033[33m"; break;
            case LogLevel::ERROR: color = "\033[31m"; break;
            case LogLevel::FATAL: color = "\033[35m"; break;
            default: color = "\033[0m";
        }
        std::string reset = "\033[0m";

        std::cout << color << entry.format() << reset << std::endl;
    }

    std::string name() const override {
        return "ConsoleOutput";
    }

    void set_min_level(LogLevel level) {
        min_level_ = level;
    }
};

// ============================================================
// UDP输出策略
// ============================================================
class UDPOutputStrategy : public OutputStrategy {
private:
    std::string host_;
    int port_;
    LogLevel min_level_;
    int socket_fd_;
    struct sockaddr_in server_addr_;

public:
    UDPOutputStrategy(const std::string& host, int port, LogLevel min_level = LogLevel::WARN)
        : host_(host), port_(port), min_level_(min_level), socket_fd_(-1) {

        socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd_ < 0) {
            std::cerr << "Failed to create UDP socket" << std::endl;
            return;
        }

        std::memset(&server_addr_, 0, sizeof(server_addr_));
        server_addr_.sin_family = AF_INET;
        server_addr_.sin_port = htons(port_);

        if (inet_pton(AF_INET, host_.c_str(), &server_addr_.sin_addr) <= 0) {
            std::cerr << "Invalid UDP host: " << host_ << std::endl;
            close(socket_fd_);
            socket_fd_ = -1;
        }
    }

    ~UDPOutputStrategy() {
        if (socket_fd_ >= 0) {
            close(socket_fd_);
        }
    }

    bool should_output(LogLevel level) const override {
        return static_cast<int>(level) >= static_cast<int>(min_level_);
    }

    void output(const LogEntry& entry) override {
        if (!should_output(entry.level) || socket_fd_ < 0) {
            return;
        }

        std::string msg = entry.format();
        sendto(socket_fd_, msg.c_str(), msg.length(), 0,
               (struct sockaddr*)&server_addr_, sizeof(server_addr_));
    }

    std::string name() const override {
        return "UDPOutput";
    }

    void set_min_level(LogLevel level) {
        min_level_ = level;
    }

    bool is_valid() const {
        return socket_fd_ >= 0;
    }
};

// ============================================================
// 无锁日志系统 - 使用 Boost lockfree
// ============================================================
class LockFreeAdvancedLogger {
private:
    // 使用指针来存储 LogEntry，避免复杂对象拷贝
    // 容量设为 32768（2^15，必须是2的幂次方）
    typedef boost::lockfree::queue<LogEntry*, boost::lockfree::capacity<32768>> LogQueue;
    LogQueue queue_;

    std::thread worker_;
    std::atomic<bool> stop_requested_ = false;
    std::vector<std::shared_ptr<OutputStrategy>> strategies_;
    std::atomic<int> pending_count_ = 0;  // 待处理日志计数

public:
    LockFreeAdvancedLogger() {
        // 启动后台工作线程
        worker_ = std::thread([this]() {
            LogEntry* entry = nullptr;
            int spin_count = 0;
            const int SPIN_THRESHOLD = 1000;

            while(true) {
                // 尝试从无锁队列弹出日志
                if(queue_.pop(entry)) {
                    // 处理日志
                    for(auto& strategy : strategies_) {
                        strategy->output(*entry);
                    }

                    // 释放内存
                    delete entry;
                    pending_count_--;
                    spin_count = 0;
                } else {
                    // 检查是否应该停止
                    if(stop_requested_.load() && pending_count_.load() == 0) {
                        break;
                    }

                    // 自适应自旋：先自旋，然后休眠
                    if(spin_count++ < SPIN_THRESHOLD) {
                        std::this_thread::yield();
                    } else {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        spin_count = 0;
                    }
                }
            }
        });
    }

    ~LockFreeAdvancedLogger() {
        // 标记停止
        stop_requested_.store(true);

        // 等待后台线程完成
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    // 添加输出策略
    void add_strategy(std::shared_ptr<OutputStrategy> strategy) {
        strategies_.push_back(strategy);
        std::cout << "Added strategy: " << strategy->name() << std::endl;
    }

    // 记录日志
    void log(LogLevel level, const std::string& message) {
        if(stop_requested_.load()) {
            std::cerr << "Logger is shutting down\n";
            return;
        }

        // 在堆上创建日志条目
        LogEntry* entry = new LogEntry(level, message);

        // 尝试入队，使用自适应重试
        int retry_count = 0;
        const int max_retries = 10000;

        while(!queue_.push(entry)) {
            if(retry_count++ >= max_retries) {
                std::cerr << "Warning: Queue full, dropping log\n";
                delete entry;
                return;
            }

            if(retry_count < 100) {
                std::this_thread::yield();
            } else if(retry_count < 1000) {
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }

        pending_count_++;
    }

    // 便捷方法
    void trace(const std::string& msg) { log(LogLevel::TRACE, msg); }
    void debug(const std::string& msg) { log(LogLevel::DEBUG, msg); }
    void info(const std::string& msg) { log(LogLevel::INFO, msg); }
    void warn(const std::string& msg) { log(LogLevel::WARN, msg); }
    void error(const std::string& msg) { log(LogLevel::ERROR, msg); }
    void fatal(const std::string& msg) { log(LogLevel::FATAL, msg); }

    size_t strategy_count() const {
        return strategies_.size();
    }

    // 等待所有日志处理完成
    void wait_for_completion() {
        while(pending_count_.load() > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
};

// ============================================================
// 演示和测试
// ============================================================
int main() {
    std::cout << "=== Advanced Logger System (Boost Lockfree Edition) ===" << std::endl;
    std::cout << "Features:" << std::endl;
    std::cout << "  1. Log Levels: TRACE, DEBUG, INFO, WARN, ERROR, FATAL" << std::endl;
    std::cout << "  2. Output Strategies: File, Console (with colors), UDP" << std::endl;
    std::cout << "  3. Lockfree queue for high performance" << std::endl;
    std::cout << "  4. Multiple strategies can run simultaneously" << std::endl;
    std::cout << std::endl;

    LockFreeAdvancedLogger logger;

    // 配置输出策略
    auto file_strategy = std::make_shared<FileOutputStrategy>("app_lockfree.log", LogLevel::TRACE);
    logger.add_strategy(file_strategy);

    auto console_strategy = std::make_shared<ConsoleOutputStrategy>(LogLevel::INFO);
    logger.add_strategy(console_strategy);

    auto udp_strategy = std::make_shared<UDPOutputStrategy>("127.0.0.1", 8888, LogLevel::WARN);
    if (udp_strategy->is_valid()) {
        logger.add_strategy(udp_strategy);
        std::cout << "\nUDP strategy configured" << std::endl;
    }

    std::cout << "\n=== Logging Test ===" << std::endl;
    std::cout << "Total strategies: " << logger.strategy_count() << std::endl;
    std::cout << "---" << std::endl;

    // 测试各种日志等级
    std::cout << "\nGenerating logs with different levels:" << std::endl;
    logger.trace("This is a TRACE message (file only)");
    logger.debug("This is a DEBUG message (file only)");
    logger.info("This is an INFO message (file + console)");
    logger.warn("This is a WARN message (file + console + UDP)");
    logger.error("This is an ERROR message (file + console + UDP)");
    logger.fatal("This is a FATAL message (file + console + UDP)");

    std::cout << "\nMulti-threaded stress test..." << std::endl;

    // 多线程并发写日志
    auto worker = [&logger](int id) {
        for(int i = 0; i < 500; ++i) {
            LogLevel levels[] = {LogLevel::DEBUG, LogLevel::INFO, LogLevel::WARN};
            LogLevel level = levels[i % 3];

            std::ostringstream oss;
            oss << "Thread " << id << " message " << i;

            logger.log(level, oss.str());
        }
    };

    std::vector<std::thread> threads;
    for(int i = 0; i < 5; ++i) {
        threads.emplace_back(worker, i);
    }

    for(auto& t : threads) {
        t.join();
    }

    // 等待所有日志处理完成
    std::cout << "Waiting for all logs to be processed..." << std::endl;
    logger.wait_for_completion();

    std::cout << "\n✓ All logs generated and processed" << std::endl;
    std::cout << "Check app_lockfree.log for complete output" << std::endl;

    return 0;
}
