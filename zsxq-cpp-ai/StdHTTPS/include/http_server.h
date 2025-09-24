/**
 * @file http_server.h
 * @brief HTTP服务器头文件
 */

#ifndef STDHTTPS_HTTP_SERVER_H
#define STDHTTPS_HTTP_SERVER_H

#include "http_message.h"
#include "ssl_handler.h"
#include "connection_pool.h"
#include <functional>
#include <thread>
#include <vector>
#include <memory>
#include <atomic>
#include <queue>
#include <condition_variable>
#include <unordered_map>
#include <mutex>

namespace stdhttps {

// 前向声明
class HttpServerConnection;
class HttpServer;

/**
 * @brief HTTP请求处理器函数类型
 * @param request HTTP请求对象
 * @param response HTTP响应对象
 */
using RequestHandler = std::function<void(const HttpRequest& request, HttpResponse& response)>;

/**
 * @brief HTTP服务器配置
 */
struct HttpServerConfig {
    std::string bind_address;           // 绑定地址
    int port;                          // 端口号
    size_t worker_threads;             // 工作线程数
    size_t max_connections;            // 最大连接数
    size_t max_request_size;           // 最大请求大小
    std::chrono::seconds keep_alive_timeout;    // keep-alive超时时间
    std::chrono::seconds request_timeout;       // 请求超时时间
    bool enable_ssl;                   // 是否启用SSL
    SSLConfig ssl_config;              // SSL配置
    bool enable_chunked;               // 是否支持chunked编码
    size_t default_chunk_size;         // 默认chunk大小
    
    HttpServerConfig()
        : bind_address("0.0.0.0")
        , port(8080)
        , worker_threads(4)
        , max_connections(1000)
        , max_request_size(1024 * 1024)    // 1MB
        , keep_alive_timeout(60)
        , request_timeout(30)
        , enable_ssl(false)
        , enable_chunked(true)
        , default_chunk_size(8192) {}
};

/**
 * @brief HTTP服务器统计信息
 */
struct HttpServerStats {
    std::atomic<size_t> total_connections{0};      // 总连接数
    std::atomic<size_t> active_connections{0};     // 当前活跃连接数
    std::atomic<size_t> total_requests{0};         // 总请求数
    std::atomic<size_t> successful_requests{0};    // 成功请求数
    std::atomic<size_t> failed_requests{0};        // 失败请求数
    std::atomic<size_t> bytes_received{0};         // 接收字节数
    std::atomic<size_t> bytes_sent{0};             // 发送字节数
    std::chrono::steady_clock::time_point start_time; // 启动时间
    
    HttpServerStats() {
        start_time = std::chrono::steady_clock::now();
    }
    
    // Copy constructor for atomic members
    HttpServerStats(const HttpServerStats& other) 
        : total_connections(other.total_connections.load())
        , active_connections(other.active_connections.load())
        , total_requests(other.total_requests.load())
        , successful_requests(other.successful_requests.load())
        , failed_requests(other.failed_requests.load())
        , bytes_received(other.bytes_received.load())
        , bytes_sent(other.bytes_sent.load())
        , start_time(other.start_time) {
    }
};

/**
 * @brief HTTP路由管理器
 * @details 管理URL路由和请求处理器的映射
 */
class HttpRouter {
public:
    /**
     * @brief 路由匹配函数类型
     * @param method HTTP方法
     * @param path URL路径
     * @return 是否匹配
     */
    using RouteMatcher = std::function<bool(HttpMethod method, const std::string& path)>;

    /**
     * @brief 添加路由
     * @param method HTTP方法
     * @param path URL路径（支持简单的通配符）
     * @param handler 请求处理器
     */
    void add_route(HttpMethod method, const std::string& path, RequestHandler handler);
    
    /**
     * @brief 添加路由（字符串方法）
     * @param method HTTP方法字符串
     * @param path URL路径
     * @param handler 请求处理器
     */
    void add_route(const std::string& method, const std::string& path, RequestHandler handler);
    
    /**
     * @brief 添加GET路由
     * @param path URL路径
     * @param handler 请求处理器
     */
    void get(const std::string& path, RequestHandler handler);
    
    /**
     * @brief 添加POST路由
     * @param path URL路径
     * @param handler 请求处理器
     */
    void post(const std::string& path, RequestHandler handler);
    
    /**
     * @brief 添加PUT路由
     * @param path URL路径
     * @param handler 请求处理器
     */
    void put(const std::string& path, RequestHandler handler);
    
    /**
     * @brief 添加DELETE路由
     * @param path URL路径
     * @param handler 请求处理器
     */
    void del(const std::string& path, RequestHandler handler);
    
    /**
     * @brief 设置默认处理器
     * @param handler 默认请求处理器
     */
    void set_default_handler(RequestHandler handler);
    
    /**
     * @brief 路由请求
     * @param request HTTP请求
     * @param response HTTP响应
     * @return 是否找到匹配的路由
     */
    bool route_request(const HttpRequest& request, HttpResponse& response);

private:
    struct Route {
        HttpMethod method;
        std::string path;
        RequestHandler handler;
        RouteMatcher matcher;
        
        Route(HttpMethod m, const std::string& p, RequestHandler h)
            : method(m), path(p), handler(h) {
            matcher = create_matcher(p);
        }
    };
    
    static RouteMatcher create_matcher(const std::string& path);
    static bool match_path(const std::string& pattern, const std::string& path);
    
private:
    std::vector<Route> routes_;
    RequestHandler default_handler_;
    mutable std::mutex routes_mutex_;
};

/**
 * @brief HTTP服务器连接类
 * @details 处理单个客户端连接的HTTP通信
 */
class HttpServerConnection {
public:
    /**
     * @brief 构造函数
     * @param socket_fd 客户端socket
     * @param server 服务器实例
     */
    HttpServerConnection(int socket_fd, HttpServer* server);
    
    /**
     * @brief 析构函数
     */
    ~HttpServerConnection();
    
    /**
     * @brief 禁用拷贝构造
     */
    HttpServerConnection(const HttpServerConnection&) = delete;
    HttpServerConnection& operator=(const HttpServerConnection&) = delete;

    /**
     * @brief 处理连接
     */
    void handle_connection();
    
    /**
     * @brief 关闭连接
     */
    void close();
    
    /**
     * @brief 获取客户端地址
     */
    std::string get_client_address() const { return client_address_; }
    
    /**
     * @brief 是否活跃
     */
    bool is_active() const { return active_; }

private:
    bool setup_ssl();
    bool read_request(HttpRequest& request);
    bool send_response(const HttpResponse& response);
    void handle_request(const HttpRequest& request, HttpResponse& response);
    void send_error_response(int status_code, const std::string& message = "");
    
private:
    int socket_fd_;                     // 客户端socket
    HttpServer* server_;                // 服务器实例
    std::string client_address_;        // 客户端地址
    std::atomic<bool> active_;          // 是否活跃
    
    // SSL相关
    std::unique_ptr<SSLHandler> ssl_handler_;
    
    // 缓冲区
    std::string read_buffer_;
    static constexpr size_t BUFFER_SIZE = 8192;
};

/**
 * @brief HTTP服务器类
 * @details 多线程HTTP/HTTPS服务器实现
 */
class HttpServer {
public:
    /**
     * @brief 构造函数
     * @param config 服务器配置
     */
    explicit HttpServer(const HttpServerConfig& config = HttpServerConfig());
    
    /**
     * @brief 析构函数
     */
    ~HttpServer();
    
    /**
     * @brief 禁用拷贝构造
     */
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    /**
     * @brief 启动服务器
     * @return 是否成功启动
     */
    bool start();
    
    /**
     * @brief 停止服务器
     */
    void stop();
    
    /**
     * @brief 是否正在运行
     */
    bool is_running() const { return running_; }
    
    /**
     * @brief 等待服务器停止
     */
    void wait_for_shutdown();

    // 路由管理
    /**
     * @brief 获取路由管理器
     */
    HttpRouter& router() { return router_; }
    
    /**
     * @brief 添加路由
     */
    void add_route(HttpMethod method, const std::string& path, RequestHandler handler);
    void add_route(const std::string& method, const std::string& path, RequestHandler handler);
    void get(const std::string& path, RequestHandler handler);
    void post(const std::string& path, RequestHandler handler);
    void put(const std::string& path, RequestHandler handler);
    void del(const std::string& path, RequestHandler handler);
    
    /**
     * @brief 设置默认请求处理器
     */
    void set_default_handler(RequestHandler handler);

    // 中间件支持
    /**
     * @brief 中间件函数类型
     * @param request HTTP请求
     * @param response HTTP响应
     * @param next 调用下一个中间件的函数
     */
    using Middleware = std::function<void(const HttpRequest& request, HttpResponse& response, std::function<void()> next)>;
    
    /**
     * @brief 添加中间件
     * @param middleware 中间件函数
     */
    void use(Middleware middleware);

    // 静态文件服务
    /**
     * @brief 设置静态文件目录
     * @param path URL路径前缀
     * @param directory 本地目录路径
     */
    void serve_static(const std::string& path, const std::string& directory);

    // 配置和状态
    /**
     * @brief 获取服务器配置
     */
    const HttpServerConfig& get_config() const { return config_; }
    
    /**
     * @brief 获取服务器统计信息
     */
    HttpServerStats get_stats() const;
    
    /**
     * @brief 获取SSL上下文管理器
     */
    SSLContextManager* get_ssl_context() const { return ssl_context_manager_.get(); }
    
    /**
     * @brief 处理请求（由连接对象调用）
     */
    void handle_request(const HttpRequest& request, HttpResponse& response);

private:
    bool initialize_socket();
    bool initialize_ssl();
    void accept_loop();
    void worker_thread();
    void add_connection(std::unique_ptr<HttpServerConnection> connection);
    void remove_connection(HttpServerConnection* connection);
    void cleanup_connections();
    std::string get_mime_type(const std::string& file_path) const;
    bool serve_file(const std::string& file_path, HttpResponse& response);
    
private:
    HttpServerConfig config_;           // 服务器配置
    std::atomic<bool> running_;         // 运行状态
    
    // 网络相关
    int listen_socket_;                 // 监听socket
    std::unique_ptr<SSLContextManager> ssl_context_manager_; // SSL上下文管理器
    
    // 线程管理
    std::thread accept_thread_;         // 接受连接的线程
    std::vector<std::thread> worker_threads_; // 工作线程
    
    // 连接管理
    std::vector<std::unique_ptr<HttpServerConnection>> connections_; // 活跃连接
    std::queue<std::unique_ptr<HttpServerConnection>> connection_queue_; // 连接队列
    std::mutex connections_mutex_;      // 连接互斥锁
    std::condition_variable connections_condition_; // 连接条件变量
    
    // 请求处理
    HttpRouter router_;                 // 路由管理器
    std::vector<Middleware> middlewares_; // 中间件列表
    std::unordered_map<std::string, std::string> static_directories_; // 静态目录映射
    
    // 统计信息
    HttpServerStats stats_;
    
    friend class HttpServerConnection;
};

/**
 * @brief HTTP服务器构建器
 * @details 便于配置和创建HTTP服务器的辅助类
 */
class HttpServerBuilder {
public:
    HttpServerBuilder();
    
    HttpServerBuilder& bind(const std::string& address, int port);
    HttpServerBuilder& threads(size_t count);
    HttpServerBuilder& max_connections(size_t count);
    HttpServerBuilder& max_request_size(size_t size);
    HttpServerBuilder& keep_alive_timeout(std::chrono::seconds timeout);
    HttpServerBuilder& request_timeout(std::chrono::seconds timeout);
    HttpServerBuilder& enable_ssl(const SSLConfig& ssl_config);
    HttpServerBuilder& enable_chunked(bool enable = true);
    HttpServerBuilder& chunk_size(size_t size);
    
    std::unique_ptr<HttpServer> build();
    
private:
    HttpServerConfig config_;
};

} // namespace stdhttps

#endif // STDHTTPS_HTTP_SERVER_H