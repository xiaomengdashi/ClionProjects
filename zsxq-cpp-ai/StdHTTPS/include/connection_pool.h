/**
 * @file connection_pool.h
 * @brief HTTP连接池管理器头文件
 */

#ifndef STDHTTPS_CONNECTION_POOL_H
#define STDHTTPS_CONNECTION_POOL_H

#include "ssl_handler.h"
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <functional>
#include <atomic>

namespace stdhttps {

// 前向声明
class HttpConnection;

/**
 * @brief 连接状态枚举
 */
enum class ConnectionState {
    CONNECTING,     // 连接中
    CONNECTED,      // 已连接
    IDLE,          // 空闲状态
    BUSY,          // 忙碌状态
    CLOSING,       // 关闭中
    CLOSED,        // 已关闭
    ERROR          // 错误状态
};

/**
 * @brief 连接统计信息
 */
struct ConnectionStats {
    size_t total_connections;       // 总连接数
    size_t active_connections;      // 活跃连接数
    size_t idle_connections;        // 空闲连接数
    size_t failed_connections;      // 失败连接数
    size_t total_requests;          // 总请求数
    size_t successful_requests;     // 成功请求数
    size_t failed_requests;         // 失败请求数
    
    ConnectionStats() 
        : total_connections(0), active_connections(0), idle_connections(0)
        , failed_connections(0), total_requests(0), successful_requests(0)
        , failed_requests(0) {}
};

/**
 * @brief 连接池配置
 */
struct ConnectionPoolConfig {
    size_t max_connections_per_host;    // 每个主机的最大连接数
    size_t max_total_connections;       // 总的最大连接数
    std::chrono::seconds connection_timeout;     // 连接超时时间
    std::chrono::seconds keep_alive_timeout;     // keep-alive超时时间
    std::chrono::seconds request_timeout;        // 请求超时时间
    bool enable_ssl;                    // 是否启用SSL
    SSLConfig ssl_config;               // SSL配置
    int max_retries;                    // 最大重试次数
    bool enable_pipeline;               // 是否启用HTTP管道化
    size_t max_pipeline_requests;       // 最大管道化请求数
    
    ConnectionPoolConfig()
        : max_connections_per_host(8)
        , max_total_connections(100)
        , connection_timeout(30)
        , keep_alive_timeout(60)
        , request_timeout(30)
        , enable_ssl(false)
        , max_retries(3)
        , enable_pipeline(false)
        , max_pipeline_requests(10) {}
};

/**
 * @brief HTTP连接类
 * @details 封装单个HTTP连接，支持HTTP/HTTPS协议
 */
class HttpConnection {
public:
    /**
     * @brief 数据回调函数类型
     */
    using DataCallback = std::function<void(const std::string&)>;
    
    /**
     * @brief 构造函数
     * @param host 主机地址
     * @param port 端口号
     * @param use_ssl 是否使用SSL
     */
    HttpConnection(const std::string& host, int port, bool use_ssl = false);
    
    /**
     * @brief 析构函数
     */
    ~HttpConnection();
    
    /**
     * @brief 禁用拷贝构造
     */
    HttpConnection(const HttpConnection&) = delete;
    HttpConnection& operator=(const HttpConnection&) = delete;

    /**
     * @brief 连接到服务器
     * @param timeout 连接超时时间
     * @return 是否成功
     */
    bool connect(std::chrono::seconds timeout = std::chrono::seconds(30));
    
    /**
     * @brief 发送数据
     * @param data 数据内容
     * @return 是否成功
     */
    bool send(const std::string& data);
    
    /**
     * @brief 接收数据
     * @param buffer 接收缓冲区
     * @param size 缓冲区大小
     * @param timeout 接收超时时间
     * @return 实际接收的字节数，-1表示错误
     */
    int receive(char* buffer, size_t size, std::chrono::seconds timeout = std::chrono::seconds(30));
    
    /**
     * @brief 关闭连接
     */
    void close();
    
    /**
     * @brief 获取连接状态
     */
    ConnectionState get_state() const { return state_; }
    
    /**
     * @brief 是否可复用
     */
    bool is_reusable() const;
    
    /**
     * @brief 是否已过期
     */
    bool is_expired() const;
    
    /**
     * @brief 获取主机信息
     */
    const std::string& get_host() const { return host_; }
    int get_port() const { return port_; }
    bool is_ssl() const { return use_ssl_; }
    
    /**
     * @brief 获取连接唯一标识
     */
    std::string get_connection_key() const;
    
    /**
     * @brief 设置SSL上下文
     */
    void set_ssl_context(SSL_CTX* ssl_ctx);
    
    /**
     * @brief 更新最后使用时间
     */
    void touch();
    
    /**
     * @brief 设置keep-alive状态
     */
    void set_keep_alive(bool keep_alive) { keep_alive_ = keep_alive; }
    
    /**
     * @brief 获取错误信息
     */
    const std::string& get_error() const { return error_message_; }

private:
    bool create_socket();
    bool connect_socket();
    void cleanup();
    void set_error(const std::string& message);
    
private:
    std::string host_;              // 主机地址
    int port_;                      // 端口号
    bool use_ssl_;                  // 是否使用SSL
    int socket_fd_;                 // socket文件描述符
    ConnectionState state_;         // 连接状态
    bool keep_alive_;              // 是否keep-alive
    std::chrono::steady_clock::time_point last_used_; // 最后使用时间
    std::chrono::steady_clock::time_point created_at_; // 创建时间
    std::string error_message_;     // 错误信息
    
    // SSL相关
    std::unique_ptr<SSLHandler> ssl_handler_;
    SSL_CTX* ssl_ctx_;
    
    mutable std::mutex mutex_;      // 线程安全锁
};

/**
 * @brief 连接池管理器
 * @details 管理HTTP连接的创建、复用和清理
 */
class ConnectionPool {
public:
    /**
     * @brief 构造函数
     * @param config 连接池配置
     */
    explicit ConnectionPool(const ConnectionPoolConfig& config = ConnectionPoolConfig());
    
    /**
     * @brief 析构函数
     */
    ~ConnectionPool();
    
    /**
     * @brief 禁用拷贝构造
     */
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;

    /**
     * @brief 获取连接
     * @param host 主机地址
     * @param port 端口号
     * @param use_ssl 是否使用SSL
     * @param timeout 获取超时时间
     * @return 连接对象，nullptr表示获取失败
     */
    std::shared_ptr<HttpConnection> get_connection(const std::string& host, 
                                                  int port, 
                                                  bool use_ssl = false,
                                                  std::chrono::seconds timeout = std::chrono::seconds(30));
    
    /**
     * @brief 归还连接
     * @param connection 连接对象
     * @param reusable 是否可复用
     */
    void return_connection(std::shared_ptr<HttpConnection> connection, bool reusable = true);
    
    /**
     * @brief 关闭指定主机的所有连接
     * @param host 主机地址
     * @param port 端口号
     * @param use_ssl 是否SSL连接
     */
    void close_connections(const std::string& host, int port, bool use_ssl);
    
    /**
     * @brief 关闭所有连接
     */
    void close_all_connections();
    
    /**
     * @brief 清理过期连接
     */
    void cleanup_expired_connections();
    
    /**
     * @brief 获取连接统计信息
     */
    ConnectionStats get_stats() const;
    
    /**
     * @brief 设置SSL上下文管理器
     * @param ssl_context SSL上下文管理器
     */
    void set_ssl_context_manager(std::shared_ptr<SSLContextManager> ssl_context);
    
    /**
     * @brief 启动连接池
     */
    void start();
    
    /**
     * @brief 停止连接池
     */
    void stop();
    
    /**
     * @brief 是否正在运行
     */
    bool is_running() const { return running_; }

private:
    std::string make_connection_key(const std::string& host, int port, bool use_ssl) const;
    void cleanup_worker();
    void remove_expired_connections();
    std::shared_ptr<HttpConnection> create_connection(const std::string& host, int port, bool use_ssl);
    
private:
    ConnectionPoolConfig config_;   // 连接池配置
    
    // 连接管理
    std::unordered_map<std::string, std::queue<std::shared_ptr<HttpConnection>>> idle_connections_;
    std::unordered_map<std::string, std::vector<std::shared_ptr<HttpConnection>>> active_connections_;
    
    // 线程同步
    mutable std::mutex connections_mutex_;
    std::condition_variable connections_condition_;
    
    // 清理线程
    std::atomic<bool> running_;
    std::thread cleanup_thread_;
    
    // SSL支持
    std::shared_ptr<SSLContextManager> ssl_context_manager_;
    
    // 统计信息
    mutable std::mutex stats_mutex_;
    ConnectionStats stats_;
};

/**
 * @brief 连接池工厂类
 * @details 提供创建和管理连接池实例的便捷方法
 */
class ConnectionPoolFactory {
public:
    /**
     * @brief 创建HTTP连接池
     * @param max_connections_per_host 每个主机的最大连接数
     * @param max_total_connections 总的最大连接数
     * @return 连接池实例
     */
    static std::unique_ptr<ConnectionPool> create_http_pool(size_t max_connections_per_host = 8,
                                                           size_t max_total_connections = 100);
    
    /**
     * @brief 创建HTTPS连接池
     * @param ssl_config SSL配置
     * @param max_connections_per_host 每个主机的最大连接数
     * @param max_total_connections 总的最大连接数
     * @return 连接池实例
     */
    static std::unique_ptr<ConnectionPool> create_https_pool(const SSLConfig& ssl_config,
                                                            size_t max_connections_per_host = 8,
                                                            size_t max_total_connections = 100);
    
    /**
     * @brief 创建通用连接池
     * @param config 连接池配置
     * @return 连接池实例
     */
    static std::unique_ptr<ConnectionPool> create_pool(const ConnectionPoolConfig& config);

private:
    ConnectionPoolFactory() = delete; // 工具类，不允许实例化
};

/**
 * @brief 连接包装器
 * @details RAII风格的连接管理，自动归还连接到池中
 */
class ConnectionWrapper {
public:
    /**
     * @brief 构造函数
     * @param connection 连接对象
     * @param pool 连接池
     */
    ConnectionWrapper(std::shared_ptr<HttpConnection> connection, 
                     std::shared_ptr<ConnectionPool> pool);
    
    /**
     * @brief 析构函数
     */
    ~ConnectionWrapper();
    
    /**
     * @brief 禁用拷贝构造
     */
    ConnectionWrapper(const ConnectionWrapper&) = delete;
    ConnectionWrapper& operator=(const ConnectionWrapper&) = delete;
    
    /**
     * @brief 移动构造函数
     */
    ConnectionWrapper(ConnectionWrapper&& other) noexcept;
    ConnectionWrapper& operator=(ConnectionWrapper&& other) noexcept;

    /**
     * @brief 获取连接对象
     */
    HttpConnection* get() const { return connection_.get(); }
    
    /**
     * @brief 连接对象操作符
     */
    HttpConnection* operator->() const { return connection_.get(); }
    
    /**
     * @brief 连接对象解引用操作符
     */
    HttpConnection& operator*() const { return *connection_; }
    
    /**
     * @brief 释放连接（不归还到池中）
     */
    void release();
    
    /**
     * @brief 设置连接为不可复用
     */
    void set_not_reusable() { reusable_ = false; }

private:
    std::shared_ptr<HttpConnection> connection_;
    std::shared_ptr<ConnectionPool> pool_;
    bool reusable_;
    bool released_;
};

} // namespace stdhttps

#endif // STDHTTPS_CONNECTION_POOL_H