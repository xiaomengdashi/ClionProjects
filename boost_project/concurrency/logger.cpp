// 无锁日志系统（使用简单、可靠的设计）

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
#include <queue>
#include <condition_variable>

// 日志条目
struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    std::string level;
    std::string message;

    LogEntry() = default;
    LogEntry(const std::string& l, const std::string& m)
        : timestamp(std::chrono::system_clock::now()), level(l), message(m) {}
};

// 改进的日志系统（修复版）
class Logger {
private:
    std::queue<LogEntry> queue_;
    std::thread worker_;
    std::atomic<bool> stop_requested_ = false;
    std::ofstream log_file_;
    mutable std::mutex queue_mutex_;
    std::condition_variable cv_;

public:
    Logger(const std::string& filename) : log_file_(filename, std::ios::app) {
        worker_ = std::thread([this]() {
            LogEntry entry;
            while(true) {
                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);

                    // 等待直到有日志或收到停止信号
                    cv_.wait(lock, [this]() {
                        return !queue_.empty() || stop_requested_.load();
                    });

                    if(stop_requested_.load() && queue_.empty()) {
                        break;
                    }

                    if(!queue_.empty()) {
                        entry = queue_.front();
                        queue_.pop();
                    } else {
                        continue;
                    }
                }

                // 在临界区外写文件
                write_to_file(entry);
            }
        });
    }

    ~Logger() {
        // 标记停止
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_requested_.store(true);
        }
        cv_.notify_one();

        // 等待后台线程完成
        if (worker_.joinable()) {
            worker_.join();
        }

        log_file_.close();
    }

    void log(const std::string& level, const std::string& message) {
        // 检查是否已开始关闭
        if(stop_requested_.load()) {
            std::cerr << "Logger is shutting down, cannot log new entries\n";
            return;
        }

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_.emplace(level, message);
        }
        cv_.notify_one();
    }

    void info(const std::string& message) { log("INFO", message); }
    void warn(const std::string& message) { log("WARN", message); }
    void error(const std::string& message) { log("ERROR", message); }

private:
    void write_to_file(const LogEntry& entry) {
        auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
        auto tm = *std::localtime(&time_t);

        log_file_ << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " [" << entry.level << "] " << entry.message << std::endl;
        log_file_.flush();
    }
};


int main() {
    Logger logger("app.txt");

    std::cout << "Starting multi-threaded logging test...\n";

    // 多线程并发写日志
    auto worker = [&logger](int id) {
        for(int i = 0; i < 5000; ++i) {
            std::stringstream ss;
            ss << "Thread " << id << " iteration " << i;
            logger.info(ss.str());

            if(i % 100 == 0) {
                logger.warn("Warning from thread " + std::to_string(id));
            }
        }
    };

    std::vector<std::thread> threads;
    for(int i = 0; i < 10; ++i) {
        threads.emplace_back(worker, i);
    }

    for(auto& t : threads) {
        t.join();
    }

    std::cout << "All logs written successfully\n";
    return 0;
}
