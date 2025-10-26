// 高级日志系统 - 支持多种输出策略和日志等级控制
// 功能：
// 1. 支持 TRACE, DEBUG, INFO, WARN, ERROR, FATAL 六个日志等级
// 2. 支持多种输出策略：文件、控制台、UDP网络、TCP网络、Syslog、轮转文件、PostgreSQL数据库
// 3. 每种策略都支持独立的等级控制
// 4. 支持同时启用多种输出策略

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
#include <memory>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <syslog.h>
#include <pqxx/pqxx>

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
// 日志条目
// ============================================================
struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    LogLevel level;
    std::string message;

    LogEntry() = default;
    LogEntry(LogLevel l, const std::string& msg)
        : timestamp(std::chrono::system_clock::now()), level(l), message(msg) {}

    // 格式化为字符串
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

    // 检查此日志等级是否应该输出
    virtual bool should_output(LogLevel level) const = 0;

    // 输出日志
    virtual void output(const LogEntry& entry) = 0;

    // 获取策略名称
    virtual std::string name() const = 0;
};

// ============================================================
// 文件输出策略（支持轮转功能）
// ============================================================
class FileOutputStrategy : public OutputStrategy {
private:
    std::string filename_;
    LogLevel min_level_;
    mutable std::mutex file_mutex_;
    std::ofstream file_;
    size_t max_file_size_;
    size_t current_size_;
    int max_files_;
    bool rotating_;

public:
    // 构造函数 - 不带轮转功能（兼容原有接口）
    FileOutputStrategy(const std::string& filename, LogLevel min_level = LogLevel::TRACE)
        : filename_(filename), min_level_(min_level), max_file_size_(0), 
          current_size_(0), max_files_(0), rotating_(false) {
        file_.open(filename_, std::ios::app);
        if (!file_.is_open()) {
            std::cerr << "Failed to open log file: " << filename_ << std::endl;
        } else {
            // 获取当前文件大小
            file_.seekp(0, std::ios::end);
            current_size_ = file_.tellp();
        }
    }

    // 构造函数 - 带轮转功能
    FileOutputStrategy(const std::string& filename, LogLevel min_level, 
                      size_t max_file_size, int max_files)
        : filename_(filename), min_level_(min_level), max_file_size_(max_file_size), 
          current_size_(0), max_files_(max_files), rotating_(true) {
        file_.open(filename_, std::ios::app);
        if (!file_.is_open()) {
            std::cerr << "Failed to open log file: " << filename_ << std::endl;
        } else {
            // 获取当前文件大小
            file_.seekp(0, std::ios::end);
            current_size_ = file_.tellp();
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
        if (!file_.is_open()) {
            return;
        }

        std::string formatted_msg = entry.format() + "\n";
        size_t msg_size = formatted_msg.size();

        // 如果启用了轮转功能，检查是否需要轮转
        if (rotating_ && max_file_size_ > 0 && current_size_ + msg_size > max_file_size_) {
            rotate_files();
        }

        file_ << formatted_msg;
        file_.flush();
        current_size_ += msg_size;
    }

private:
    void rotate_files() {
        file_.close();

        // 重命名现有文件
        for (int i = max_files_ - 1; i > 0; --i) {
            std::string old_name = filename_ + "." + std::to_string(i);
            std::string new_name = filename_ + "." + std::to_string(i + 1);
            rename(old_name.c_str(), new_name.c_str());
        }

        // 重命名当前文件
        std::string first_backup = filename_ + ".1";
        rename(filename_.c_str(), first_backup.c_str());

        // 重新打开文件
        file_.open(filename_, std::ios::out | std::ios::trunc);
        if (!file_.is_open()) {
            std::cerr << "Failed to reopen log file: " << filename_ << std::endl;
        } else {
            current_size_ = 0;
        }
    }

public:
    std::string name() const override {
        return rotating_ ? "RotatingFileOutput" : "FileOutput";
    }

    // 设置最小日志等级
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
    ConsoleOutputStrategy(LogLevel min_level = LogLevel::WARN)
        : min_level_(min_level) {}

    bool should_output(LogLevel level) const override {
        return static_cast<int>(level) >= static_cast<int>(min_level_);
    }

    void output(const LogEntry& entry) override {
        if (!should_output(entry.level)) {
            return;
        }

        std::unique_lock<std::mutex> lock(console_mutex_);

        // 根据等级设置不同颜色
        std::string color;
        switch(entry.level) {
            case LogLevel::TRACE: color = "\033[36m"; break;  // 青色
            case LogLevel::DEBUG: color = "\033[34m"; break;  // 蓝色
            case LogLevel::INFO:  color = "\033[32m"; break;  // 绿色
            case LogLevel::WARN:  color = "\033[33m"; break;  // 黄色
            case LogLevel::ERROR: color = "\033[31m"; break;  // 红色
            case LogLevel::FATAL: color = "\033[35m"; break;  // 紫色
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
    UDPOutputStrategy(const std::string& host, int port, LogLevel min_level = LogLevel::ERROR)
        : host_(host), port_(port), min_level_(min_level), socket_fd_(-1) {

        // 创建UDP socket
        socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd_ < 0) {
            std::cerr << "Failed to create UDP socket" << std::endl;
            return;
        }

        // 配置服务器地址
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
        ssize_t sent = sendto(socket_fd_, msg.c_str(), msg.length(), 0,
                             (struct sockaddr*)&server_addr_, sizeof(server_addr_));
        if (sent < 0) {
            // 发送失败，静默处理，避免日志系统自身出错
            // std::cerr << "Failed to send UDP log" << std::endl;
        }
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
// TCP输出策略
// ============================================================
class TCPOutputStrategy : public OutputStrategy {
private:
    std::string host_;
    int port_;
    LogLevel min_level_;
    int socket_fd_;
    struct sockaddr_in server_addr_;
    mutable std::mutex tcp_mutex_;

public:
    TCPOutputStrategy(const std::string& host, int port, LogLevel min_level = LogLevel::WARN)
        : host_(host), port_(port), min_level_(min_level), socket_fd_(-1) {

        // 创建TCP socket
        socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd_ < 0) {
            std::cerr << "Failed to create TCP socket" << std::endl;
            return;
        }

        // 配置服务器地址
        std::memset(&server_addr_, 0, sizeof(server_addr_));
        server_addr_.sin_family = AF_INET;
        server_addr_.sin_port = htons(port_);

        if (inet_pton(AF_INET, host_.c_str(), &server_addr_.sin_addr) <= 0) {
            std::cerr << "Invalid TCP host: " << host_ << std::endl;
            close(socket_fd_);
            socket_fd_ = -1;
            return;
        }

        // 连接到服务器
        if (connect(socket_fd_, (struct sockaddr*)&server_addr_, sizeof(server_addr_)) < 0) {
            std::cerr << "Failed to connect to TCP server: " << host_ << ":" << port_ << std::endl;
            close(socket_fd_);
            socket_fd_ = -1;
        }
    }

    ~TCPOutputStrategy() {
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

        std::unique_lock<std::mutex> lock(tcp_mutex_);
        std::string msg = entry.format() + "\n";
        ssize_t sent = send(socket_fd_, msg.c_str(), msg.length(), 0);
        if (sent < 0) {
            // 发送失败，尝试重新连接
            reconnect();
        }
    }

private:
    void reconnect() {
        if (socket_fd_ >= 0) {
            close(socket_fd_);
        }
        
        socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd_ < 0) {
            return;
        }

        if (connect(socket_fd_, (struct sockaddr*)&server_addr_, sizeof(server_addr_)) < 0) {
            close(socket_fd_);
            socket_fd_ = -1;
        }
    }

public:
    std::string name() const override {
        return "TCPOutput";
    }

    void set_min_level(LogLevel level) {
        min_level_ = level;
    }

    bool is_valid() const {
        return socket_fd_ >= 0;
    }
};

// ============================================================
// Syslog输出策略
// ============================================================
class SyslogOutputStrategy : public OutputStrategy {
private:
    std::string ident_;
    LogLevel min_level_;
    bool opened_;

public:
    SyslogOutputStrategy(const std::string& ident, LogLevel min_level = LogLevel::INFO)
        : ident_(ident), min_level_(min_level), opened_(false) {
        openlog(ident_.c_str(), LOG_PID | LOG_CONS, LOG_USER);
        opened_ = true;
    }

    ~SyslogOutputStrategy() {
        if (opened_) {
            closelog();
        }
    }

    bool should_output(LogLevel level) const override {
        return static_cast<int>(level) >= static_cast<int>(min_level_);
    }

    void output(const LogEntry& entry) override {
        if (!should_output(entry.level) || !opened_) {
            return;
        }

        int syslog_level;
        switch(entry.level) {
            case LogLevel::TRACE: syslog_level = LOG_DEBUG; break;
            case LogLevel::DEBUG: syslog_level = LOG_DEBUG; break;
            case LogLevel::INFO:  syslog_level = LOG_INFO;  break;
            case LogLevel::WARN:  syslog_level = LOG_WARNING; break;
            case LogLevel::ERROR: syslog_level = LOG_ERR; break;
            case LogLevel::FATAL: syslog_level = LOG_CRIT; break;
            default: syslog_level = LOG_INFO;
        }

        syslog(syslog_level, "%s", entry.format().c_str());
    }

    std::string name() const override {
        return "SyslogOutput";
    }

    void set_min_level(LogLevel level) {
        min_level_ = level;
    }
};

// ============================================================
// PostgreSQL输出策略（支持分表）
// ============================================================
class PostgreSQLOutputStrategy : public OutputStrategy {
public:
    // 分表策略枚举
    enum class PartitionStrategy {
        NONE,        // 不分表
        DAILY,       // 按天分表
        MONTHLY,     // 按月分表
        YEARLY       // 按年分表
    };

private:
    std::string connection_string_;
    LogLevel min_level_;
    PartitionStrategy partition_strategy_;
    std::unique_ptr<pqxx::connection> conn_;
    std::unique_ptr<pqxx::work> txn_;
    mutable std::mutex db_mutex_;
    std::string current_table_name_;
    std::tm last_table_date_;

public:
    PostgreSQLOutputStrategy(const std::string& connection_string, LogLevel min_level = LogLevel::INFO, 
                            PartitionStrategy strategy = PartitionStrategy::NONE)
        : connection_string_(connection_string), min_level_(min_level), partition_strategy_(strategy) {
        try {
            conn_ = std::make_unique<pqxx::connection>(connection_string_);
            txn_ = std::make_unique<pqxx::work>(*conn_);
            
            // 初始化表名和日期
            auto now = std::chrono::system_clock::now();
            std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
            last_table_date_ = *std::localtime(&now_time_t);
            current_table_name_ = get_table_name(last_table_date_);
            
            // 创建当前需要的表
            create_table_if_not_exists(current_table_name_);
        } catch (const std::exception& e) {
            std::cerr << "Failed to connect to PostgreSQL: " << e.what() << std::endl;
        }
    }

    ~PostgreSQLOutputStrategy() {
        std::lock_guard<std::mutex> lock(db_mutex_);
        if (txn_) {
            try {
                txn_->commit();
            } catch (const std::exception& e) {
                std::cerr << "Failed to commit PostgreSQL transaction: " << e.what() << std::endl;
            }
        }
    }

    bool should_output(LogLevel level) const override {
        return static_cast<int>(level) >= static_cast<int>(min_level_);
    }

    void output(const LogEntry& entry) override {
        if (!should_output(entry.level) || !conn_ || !conn_->is_open()) {
            return;
        }

        std::lock_guard<std::mutex> lock(db_mutex_);
        try {
            // 检查是否需要切换表（基于分表策略）
            std::time_t entry_time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
            std::tm entry_time = *std::localtime(&entry_time_t);
            std::string table_name = get_table_name(entry_time);
            
            // 如果日期变化，需要创建新表并更新当前表名
            if (table_name != current_table_name_) {
                create_table_if_not_exists(table_name);
                current_table_name_ = table_name;
                last_table_date_ = entry_time;
            }

            // 将时间点转换为时间戳字符串
            std::ostringstream time_str;
            time_str << std::put_time(&entry_time, "%Y-%m-%d %H:%M:%S");

            // 插入日志到对应的表
            txn_->exec_params(
                "INSERT INTO " + current_table_name_ + " (timestamp, level, message) VALUES ($1, $2, $3)",
                time_str.str(),
                level_to_string(entry.level),
                entry.message
            );
            txn_->commit();
            txn_ = std::make_unique<pqxx::work>(*conn_);
        } catch (const std::exception& e) {
            std::cerr << "Failed to insert log into PostgreSQL: " << e.what() << std::endl;
            // 尝试重新连接
            reconnect();
        }
    }

private:
    // 根据分表策略获取表名
    std::string get_table_name(const std::tm& time) {
        std::ostringstream table_name;
        table_name << "logs";
        
        switch(partition_strategy_) {
            case PartitionStrategy::DAILY:
                table_name << "_" << std::put_time(&time, "%Y%m%d");
                break;
            case PartitionStrategy::MONTHLY:
                table_name << "_" << std::put_time(&time, "%Y%m");
                break;
            case PartitionStrategy::YEARLY:
                table_name << "_" << std::put_time(&time, "%Y");
                break;
            case PartitionStrategy::NONE:
            default:
                break;
        }
        
        return table_name.str();
    }

    // 创建表（如果不存在）
    void create_table_if_not_exists(const std::string& table_name) {
        try {
            txn_->exec("CREATE TABLE IF NOT EXISTS " + table_name + " ("
                      "id SERIAL PRIMARY KEY, "
                      "timestamp TIMESTAMP NOT NULL, "
                      "level VARCHAR(10) NOT NULL, "
                      "message TEXT NOT NULL)");
            txn_->commit();
            txn_ = std::make_unique<pqxx::work>(*conn_);
        } catch (const std::exception& e) {
            std::cerr << "Failed to create table " << table_name << ": " << e.what() << std::endl;
        }
    }

    void reconnect() {
        try {
            if (conn_) {
                conn_.reset();
            }
            conn_ = std::make_unique<pqxx::connection>(connection_string_);
            if (txn_) {
                txn_.reset();
            }
            txn_ = std::make_unique<pqxx::work>(*conn_);
        } catch (const std::exception& e) {
            std::cerr << "Failed to reconnect to PostgreSQL: " << e.what() << std::endl;
        }
    }

public:
    std::string name() const override {
        return "PostgreSQLOutput";
    }

    void set_min_level(LogLevel level) {
        min_level_ = level;
    }
    
    // 设置分表策略
    void set_partition_strategy(PartitionStrategy strategy) {
        partition_strategy_ = strategy;
    }
};

// ============================================================
// 高级日志系统（单例模式）
// ============================================================
class AdvancedLogger {
private:
    std::queue<LogEntry> queue_;
    std::thread worker_;
    std::atomic<bool> stop_requested_ = false;
    std::vector<std::shared_ptr<OutputStrategy>> strategies_;
    mutable std::mutex queue_mutex_;
    std::condition_variable cv_;

    AdvancedLogger() {
        worker_ = std::thread(&AdvancedLogger::worker_thread, this);
    }

    // 禁止拷贝构造和赋值
    AdvancedLogger(const AdvancedLogger&) = delete;
    AdvancedLogger& operator=(const AdvancedLogger&) = delete;

    // 工作线程函数
    void worker_thread() {
        LogEntry entry;
        while(true) {
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);

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

            // 在临界区外输出日志（所有策略）
            for(auto& strategy : strategies_) {
                strategy->output(entry);
            }
        }
    }

    // 添加输出策略
    void add_strategy(std::shared_ptr<OutputStrategy> strategy) {
        strategies_.push_back(strategy);
    }

public:
    // 获取单例实例
    static AdvancedLogger& instance() {
        static AdvancedLogger instance;
        return instance;
    }

    ~AdvancedLogger() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_requested_.store(true);
        }
        cv_.notify_one();

        if (worker_.joinable()) {
            worker_.join();
        }
    }

    // 配置文件输出策略
    AdvancedLogger& with_file_output(const std::string& filename, LogLevel min_level = LogLevel::TRACE) {
        auto strategy = std::make_shared<FileOutputStrategy>(filename, min_level);
        add_strategy(strategy);
        return *this;
    }

    // 配置控制台输出策略
    AdvancedLogger& with_console_output(LogLevel min_level = LogLevel::WARN) {
        auto strategy = std::make_shared<ConsoleOutputStrategy>(min_level);
        add_strategy(strategy);
        return *this;
    }

    // 配置UDP输出策略
    AdvancedLogger& with_udp_output(const std::string& host, int port, LogLevel min_level = LogLevel::ERROR) {
        auto strategy = std::make_shared<UDPOutputStrategy>(host, port, min_level);
        if (strategy->is_valid()) {
            add_strategy(strategy);
        }
        return *this;
    }

    // 配置TCP输出策略
    AdvancedLogger& with_tcp_output(const std::string& host, int port, LogLevel min_level = LogLevel::WARN) {
        auto strategy = std::make_shared<TCPOutputStrategy>(host, port, min_level);
        if (strategy->is_valid()) {
            add_strategy(strategy);
        }
        return *this;
    }

    // 配置Syslog输出策略
    AdvancedLogger& with_syslog_output(const std::string& ident, LogLevel min_level = LogLevel::INFO) {
        auto strategy = std::make_shared<SyslogOutputStrategy>(ident, min_level);
        add_strategy(strategy);
        return *this;
    }

    // 配置轮转文件输出策略
    AdvancedLogger& with_rotating_file_output(const std::string& filename, LogLevel min_level = LogLevel::TRACE, 
                                             size_t max_file_size = 1024*1024, int max_files = 5) {
        auto strategy = std::make_shared<FileOutputStrategy>(filename, min_level, max_file_size, max_files);
        add_strategy(strategy);
        return *this;
    }

    // 配置PostgreSQL输出策略
    AdvancedLogger& with_postgresql_output(const std::string& connection_string, LogLevel min_level = LogLevel::INFO) {
        auto strategy = std::make_shared<PostgreSQLOutputStrategy>(connection_string, min_level);
        add_strategy(strategy);
        return *this;
    }
    
    // 配置PostgreSQL输出策略（带分表策略）
    AdvancedLogger& with_postgresql_output(const std::string& connection_string, 
                                          PostgreSQLOutputStrategy::PartitionStrategy strategy,
                                          LogLevel min_level = LogLevel::INFO) {
        auto strategy_ptr = std::make_shared<PostgreSQLOutputStrategy>(connection_string, min_level, strategy);
        add_strategy(strategy_ptr);
        return *this;
    }

private:
    void log(LogLevel level, const std::string& message) {
        if(stop_requested_.load()) {
            std::cerr << "Logger is shutting down\n";
            return;
        }

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_.emplace(level, message);
        }
        cv_.notify_one();
    }

public:
    template<typename... Args>
    void trace(const std::string& format, Args&&... args) {
        log(LogLevel::TRACE, format_string(format, std::forward<Args>(args)...));
    }
    
    template<typename... Args>
    void debug(const std::string& format, Args&&... args) {
        log(LogLevel::DEBUG, format_string(format, std::forward<Args>(args)...));
    }
    
    template<typename... Args>
    void info(const std::string& format, Args&&... args) {
        log(LogLevel::INFO, format_string(format, std::forward<Args>(args)...));
    }
    
    template<typename... Args>
    void warn(const std::string& format, Args&&... args) {
        log(LogLevel::WARN, format_string(format, std::forward<Args>(args)...));
    }
    
    template<typename... Args>
    void error(const std::string& format, Args&&... args) {
        log(LogLevel::ERROR, format_string(format, std::forward<Args>(args)...));
    }
    
    template<typename... Args>
    void fatal(const std::string& format, Args&&... args) {
        log(LogLevel::FATAL, format_string(format, std::forward<Args>(args)...));
    }

private:
    // 格式化字符串函数
    template<typename... Args>
    std::string format_string(const std::string& format, Args&&... args) {
        // 如果没有参数，直接返回格式字符串
        if constexpr (sizeof...(args) == 0) {
            return format;
        } else {
            // 首先计算所需的缓冲区大小
            size_t size = snprintf(nullptr, 0, format.c_str(), args...);
            
            // 如果格式化失败，返回原始字符串
            if (size <= 0) {
                return format;
            }
            
            // 分配缓冲区并进行格式化
            std::string result(size, '\0');
            snprintf(&result[0], size + 1, format.c_str(), args...);
            
            return result;
        }
    }
};

// ============================================================
// 演示和测试
// ============================================================
int main() {
    auto& logger = AdvancedLogger::instance();

    // 使用链式配置
    logger.with_file_output("app.log", LogLevel::TRACE)
          .with_console_output(LogLevel::WARN)
          .with_udp_output("192.168.31.30", 8888, LogLevel::ERROR)
          .with_tcp_output("192.168.31.30", 9999, LogLevel::ERROR)
          .with_syslog_output("MyApp", LogLevel::INFO)
          .with_rotating_file_output("rotating.log", LogLevel::DEBUG, 1024, 3)
          .with_postgresql_output("host=localhost port=5432 dbname=logdb user=loguser password=logpass", 
                                PostgreSQLOutputStrategy::PartitionStrategy::DAILY, 
                                LogLevel::WARN);

    // 测试各种日志等级
    logger.trace("This is a TRACE message (file only)");
    logger.debug("This is a DEBUG message (file + rotating file)");
    logger.info("This is an INFO message (file + rotating file + syslog)");
    logger.warn("This is a WARN message (file + console + rotating file + tcp + syslog + postgresql)");
    logger.error("This is an ERROR message (file + console + UDP + rotating file + tcp + syslog + postgresql)");
    logger.fatal("This is a FATAL message (file + console + UDP + rotating file + tcp + syslog + postgresql)");
    
    // 测试带参数的日志
    int value = 42;
    std::string name = "test";
    logger.info("Value: %d, Name: %s", value, name.c_str());
    logger.warn("Warning with value: %d", value);
    logger.error("Error occurred: %s, code: %d", "Network error", 500);

    // 多线程并发写日志
    auto worker = [](int id) {
        auto& logger = AdvancedLogger::instance();
        for(int i = 0; i < 300; ++i) {
            LogLevel levels[] = {LogLevel::DEBUG, LogLevel::INFO, LogLevel::WARN, LogLevel::ERROR};
            LogLevel level = levels[i % 4];

            switch(level) {
                case LogLevel::DEBUG:
                    logger.debug("Thread %d: message %d", id, i);
                    break;
                case LogLevel::INFO:
                    logger.info("Thread %d: message %d", id, i);
                    break;
                case LogLevel::WARN:
                    logger.warn("Thread %d: message %d", id, i);
                    break;
                case LogLevel::ERROR:
                    logger.error("Thread %d: message %d", id, i);
                    break;
                default:
                    logger.debug("Thread %d: message %d", id, i);
                    break;
            }
        }
    };

    std::vector<std::thread> threads;
    for(int i = 0; i < 5; ++i) {
        threads.emplace_back(worker, i);
    }

    for(auto& t : threads) {
        t.join();
    }

    std::cout << "\n✓ All logs generated" << std::endl;
    std::cout << "Check app.log and rotating.log for complete output" << std::endl;

    return 0;
}
