/**
 * @file http_client.h
 * @brief HTTP客户端头文件
 * @details 实现HTTP/HTTPS客户端，支持连接池和异步请求
 */

#ifndef STDHTTPS_HTTP_CLIENT_H
#define STDHTTPS_HTTP_CLIENT_H

#include "http_message.h"
#include "connection_pool.h"
#include "ssl_handler.h"
#include <memory>
#include <future>
#include <chrono>
#include <functional>
#include <sstream>

namespace stdhttps {

/**
 * @brief HTTP客户端配置
 */
struct HttpClientConfig {
    std::chrono::seconds connect_timeout;       // 连接超时时间
    std::chrono::seconds request_timeout;       // 请求超时时间
    std::chrono::seconds response_timeout;      // 响应超时时间
    size_t max_redirects;                      // 最大重定向次数
    bool follow_redirects;                     // 是否跟随重定向
    bool verify_ssl;                           // 是否验证SSL证书
    std::string user_agent;                    // User-Agent字符串
    size_t max_response_size;                  // 最大响应大小
    bool enable_compression;                   // 是否启用压缩
    bool enable_keep_alive;                    // 是否启用keep-alive
    
    // 连接池配置
    size_t max_connections_per_host;           // 每个主机的最大连接数
    size_t max_total_connections;              // 总的最大连接数
    
    HttpClientConfig()
        : connect_timeout(30)
        , request_timeout(30)
        , response_timeout(60)
        , max_redirects(5)
        , follow_redirects(true)
        , verify_ssl(true)
        , user_agent("StdHTTPS/1.0")
        , max_response_size(10 * 1024 * 1024)  // 10MB
        , enable_compression(true)
        , enable_keep_alive(true)
        , max_connections_per_host(8)
        , max_total_connections(100) {}
};

/**
 * @brief HTTP响应结果
 */
struct HttpResult {
    bool success;                       // 是否成功
    int status_code;                    // HTTP状态码
    std::string error_message;          // 错误信息
    HttpResponse response;              // HTTP响应
    std::chrono::milliseconds elapsed_time; // 请求耗时
    
    HttpResult() : success(false), status_code(0), elapsed_time(0) {}
    
    explicit HttpResult(HttpResponse&& resp) 
        : success(true), status_code(resp.get_status_code()), response(std::move(resp)), elapsed_time(0) {}
    
    static HttpResult error(const std::string& message) {
        HttpResult result;
        result.success = false;
        result.error_message = message;
        return result;
    }
};

/**
 * @brief URL解析结果
 */
struct ParsedURL {
    std::string scheme;         // 协议 (http/https)
    std::string host;           // 主机名
    int port;                   // 端口号
    std::string path;           // 路径
    std::string query;          // 查询参数
    std::string fragment;       // 片段标识符
    bool is_ssl;               // 是否SSL连接
    
    ParsedURL() : port(80), is_ssl(false) {}
    
    std::string to_string() const {
        std::ostringstream oss;
        oss << scheme << "://" << host;
        if ((scheme == "http" && port != 80) || (scheme == "https" && port != 443)) {
            oss << ":" << port;
        }
        oss << path;
        if (!query.empty()) {
            oss << "?" << query;
        }
        if (!fragment.empty()) {
            oss << "#" << fragment;
        }
        return oss.str();
    }
};

/**
 * @brief HTTP客户端类
 * @details 提供同步和异步的HTTP请求功能
 */
class HttpClient {
public:
    /**
     * @brief 异步响应回调函数类型
     */
    using AsyncCallback = std::function<void(const HttpResult&)>;
    
    /**
     * @brief 进度回调函数类型
     * @param downloaded 已下载字节数
     * @param total 总字节数（可能为0表示未知）
     */
    using ProgressCallback = std::function<void(size_t downloaded, size_t total)>;

    /**
     * @brief 构造函数
     * @param config 客户端配置
     */
    explicit HttpClient(const HttpClientConfig& config = HttpClientConfig());
    
    /**
     * @brief 析构函数
     */
    ~HttpClient();
    
    /**
     * @brief 禁用拷贝构造
     */
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    // 同步请求方法
    /**
     * @brief 执行HTTP请求
     * @param request HTTP请求对象
     * @return 请求结果
     */
    HttpResult request(const HttpRequest& request);
    
    /**
     * @brief 执行GET请求
     * @param url 请求URL
     * @return 请求结果
     */
    HttpResult get(const std::string& url);
    
    /**
     * @brief 执行POST请求
     * @param url 请求URL
     * @param body 请求体
     * @param content_type Content-Type头部
     * @return 请求结果
     */
    HttpResult post(const std::string& url, const std::string& body, 
                   const std::string& content_type = "application/json");
    
    /**
     * @brief 执行PUT请求
     * @param url 请求URL
     * @param body 请求体
     * @param content_type Content-Type头部
     * @return 请求结果
     */
    HttpResult put(const std::string& url, const std::string& body,
                  const std::string& content_type = "application/json");
    
    /**
     * @brief 执行DELETE请求
     * @param url 请求URL
     * @return 请求结果
     */
    HttpResult del(const std::string& url);
    
    /**
     * @brief 执行HEAD请求
     * @param url 请求URL
     * @return 请求结果
     */
    HttpResult head(const std::string& url);

    // 异步请求方法
    /**
     * @brief 异步执行HTTP请求
     * @param request HTTP请求对象
     * @param callback 响应回调函数
     * @return future对象
     */
    std::future<HttpResult> async_request(const HttpRequest& request, 
                                         AsyncCallback callback = nullptr);
    
    /**
     * @brief 异步GET请求
     * @param url 请求URL
     * @param callback 响应回调函数
     * @return future对象
     */
    std::future<HttpResult> async_get(const std::string& url, 
                                     AsyncCallback callback = nullptr);
    
    /**
     * @brief 异步POST请求
     * @param url 请求URL
     * @param body 请求体
     * @param callback 响应回调函数
     * @param content_type Content-Type头部
     * @return future对象
     */
    std::future<HttpResult> async_post(const std::string& url, const std::string& body,
                                      AsyncCallback callback = nullptr,
                                      const std::string& content_type = "application/json");

    // 便捷方法
    /**
     * @brief 下载文件
     * @param url 文件URL
     * @param file_path 保存路径
     * @param progress_callback 进度回调
     * @return 是否成功
     */
    bool download_file(const std::string& url, const std::string& file_path,
                      ProgressCallback progress_callback = nullptr);
    
    /**
     * @brief 上传文件
     * @param url 上传URL
     * @param file_path 文件路径
     * @param field_name 表单字段名
     * @return 请求结果
     */
    HttpResult upload_file(const std::string& url, const std::string& file_path,
                          const std::string& field_name = "file");

    // 配置管理
    /**
     * @brief 设置请求头
     * @param name 头部名称
     * @param value 头部值
     */
    void set_header(const std::string& name, const std::string& value);
    
    /**
     * @brief 移除请求头
     * @param name 头部名称
     */
    void remove_header(const std::string& name);
    
    /**
     * @brief 清空所有请求头
     */
    void clear_headers();
    
    /**
     * @brief 设置Cookie
     * @param cookie Cookie字符串
     */
    void set_cookie(const std::string& cookie);
    
    /**
     * @brief 设置Basic认证
     * @param username 用户名
     * @param password 密码
     */
    void set_basic_auth(const std::string& username, const std::string& password);
    
    /**
     * @brief 设置Bearer Token认证
     * @param token 令牌
     */
    void set_bearer_token(const std::string& token);

    // SSL配置
    /**
     * @brief 设置SSL配置
     * @param ssl_config SSL配置
     */
    void set_ssl_config(const SSLConfig& ssl_config);
    
    /**
     * @brief 设置CA证书文件
     * @param ca_file CA证书文件路径
     */
    void set_ca_file(const std::string& ca_file);
    
    /**
     * @brief 设置客户端证书
     * @param cert_file 证书文件路径
     * @param key_file 私钥文件路径
     */
    void set_client_cert(const std::string& cert_file, const std::string& key_file);

    // 状态和统计
    /**
     * @brief 获取客户端配置
     */
    const HttpClientConfig& get_config() const { return config_; }
    
    /**
     * @brief 获取连接池统计信息
     */
    ConnectionStats get_connection_stats() const;
    
    /**
     * @brief 清理连接池
     */
    void cleanup_connections();

    // 工具方法
    /**
     * @brief 解析URL
     * @param url URL字符串
     * @return 解析结果
     */
    static ParsedURL parse_url(const std::string& url);
    
    /**
     * @brief URL编码
     * @param str 原始字符串
     * @return 编码后的字符串
     */
    static std::string url_encode(const std::string& str);
    
    /**
     * @brief URL解码
     * @param str 编码后的字符串
     * @return 解码后的字符串
     */
    static std::string url_decode(const std::string& str);

private:
    // 内部请求处理
    HttpResult execute_request(const HttpRequest& request);
    HttpResult execute_request(const HttpRequest& request, const ParsedURL& url);
    HttpResult execute_request_internal(const HttpRequest& request, const ParsedURL& url);
    HttpResult handle_redirects(HttpRequest&& request, size_t redirect_count = 0);
    HttpResult handle_redirects(HttpRequest&& request, const ParsedURL& url, size_t redirect_count = 0);
    bool setup_request_headers(HttpRequest& request, const ParsedURL& url);
    
    // 连接管理
    std::shared_ptr<HttpConnection> get_connection(const ParsedURL& url);
    void return_connection(std::shared_ptr<HttpConnection> connection, bool reusable);
    
    // 数据处理
    bool send_request(std::shared_ptr<HttpConnection> connection, const HttpRequest& request);
    HttpResult receive_response(std::shared_ptr<HttpConnection> connection);
    
    // SSL处理
    void setup_ssl_for_url(const ParsedURL& url);
    
    // 错误处理
    HttpResult handle_connection_error(const std::string& message);
    HttpResult handle_timeout_error();
    HttpResult handle_ssl_error(const std::string& message);

private:
    HttpClientConfig config_;           // 客户端配置
    HttpHeaders default_headers_;       // 默认请求头
    
    // 连接池
    std::unique_ptr<ConnectionPool> connection_pool_;
    
    // SSL配置
    SSLConfig ssl_config_;
    std::shared_ptr<SSLContextManager> ssl_context_manager_;
    bool ssl_config_set_;               // SSL配置是否已设置
    
    // 同步控制
    std::mutex headers_mutex_;          // 头部访问互斥锁
};

/**
 * @brief HTTP客户端工厂类
 * @details 提供创建不同类型HTTP客户端的便捷方法
 */
class HttpClientFactory {
public:
    /**
     * @brief 创建默认HTTP客户端
     * @return HTTP客户端实例
     */
    static std::unique_ptr<HttpClient> create_default();
    
    /**
     * @brief 创建HTTPS客户端
     * @param ssl_config SSL配置
     * @return HTTP客户端实例
     */
    static std::unique_ptr<HttpClient> create_https(const SSLConfig& ssl_config);
    
    /**
     * @brief 创建配置的HTTP客户端
     * @param config 客户端配置
     * @return HTTP客户端实例
     */
    static std::unique_ptr<HttpClient> create_configured(const HttpClientConfig& config);

private:
    HttpClientFactory() = delete; // 工具类，不允许实例化
};

/**
 * @brief HTTP客户端构建器
 * @details 便于配置和创建HTTP客户端的辅助类
 */
class HttpClientBuilder {
public:
    HttpClientBuilder();
    
    HttpClientBuilder& timeout(std::chrono::seconds connect_timeout,
                              std::chrono::seconds request_timeout = std::chrono::seconds(30),
                              std::chrono::seconds response_timeout = std::chrono::seconds(60));
    
    HttpClientBuilder& max_redirects(size_t count);
    HttpClientBuilder& follow_redirects(bool enable = true);
    HttpClientBuilder& verify_ssl(bool enable = true);
    HttpClientBuilder& user_agent(const std::string& ua);
    HttpClientBuilder& max_response_size(size_t size);
    HttpClientBuilder& enable_compression(bool enable = true);
    HttpClientBuilder& enable_keep_alive(bool enable = true);
    HttpClientBuilder& connection_pool(size_t max_per_host, size_t max_total);
    
    HttpClientBuilder& header(const std::string& name, const std::string& value);
    HttpClientBuilder& cookie(const std::string& cookie);
    HttpClientBuilder& basic_auth(const std::string& username, const std::string& password);
    HttpClientBuilder& bearer_token(const std::string& token);
    
    HttpClientBuilder& ssl_config(const SSLConfig& config);
    HttpClientBuilder& ca_file(const std::string& file);
    HttpClientBuilder& client_cert(const std::string& cert_file, const std::string& key_file);
    
    std::unique_ptr<HttpClient> build();

private:
    HttpClientConfig config_;
    HttpHeaders headers_;
    SSLConfig ssl_config_;
    bool ssl_config_set_;
};

} // namespace stdhttps

#endif // STDHTTPS_HTTP_CLIENT_H