/**
 * @file http_client.cpp
 * @brief HTTP客户端实现
 * @details 实现HTTP/HTTPS客户端功能
 */

#include "http_client.h"
#include <regex>
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <future>
#include <thread>

namespace stdhttps {

// HttpClient实现
HttpClient::HttpClient(const HttpClientConfig& config)
    : config_(config), ssl_config_set_(false) {
    
    // 创建连接池
    ConnectionPoolConfig pool_config;
    pool_config.max_connections_per_host = config_.max_connections_per_host;
    pool_config.max_total_connections = config_.max_total_connections;
    pool_config.connection_timeout = config_.connect_timeout;
    pool_config.keep_alive_timeout = std::chrono::seconds(60);
    pool_config.request_timeout = config_.request_timeout;
    
    connection_pool_ = std::unique_ptr<ConnectionPool>(new ConnectionPool(pool_config));
    connection_pool_->start();
    
    // 设置默认头部
    set_header("User-Agent", config_.user_agent);
    if (config_.enable_compression) {
        set_header("Accept-Encoding", "gzip, deflate");
    }
    if (config_.enable_keep_alive) {
        set_header("Connection", "keep-alive");
    }
}

HttpClient::~HttpClient() {
    if (connection_pool_) {
        connection_pool_->stop();
    }
}

// 同步请求方法
HttpResult HttpClient::request(const HttpRequest& request) {
    auto start_time = std::chrono::steady_clock::now();
    
    // 从Host头部获取主机信息以构建ParsedURL
    std::string host = request.get_header("Host");
    if (host.empty()) {
        return HttpResult::error("请求缺少Host头部");
    }
    
    ParsedURL url;
    size_t colon_pos = host.find(':');
    if (colon_pos != std::string::npos) {
        url.host = host.substr(0, colon_pos);
        url.port = std::stoi(host.substr(colon_pos + 1));
    } else {
        url.host = host;
        // 尝试根据其他线索判断是否HTTPS（简化处理）
        url.port = 80;
    }
    url.scheme = "http";
    url.is_ssl = false;
    url.path = request.get_uri();
    
    HttpResult result = execute_request(request, url);
    
    auto end_time = std::chrono::steady_clock::now();
    result.elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    return result;
}

HttpResult HttpClient::get(const std::string& url) {
    ParsedURL parsed = parse_url(url);
    if (parsed.scheme.empty()) {
        return HttpResult::error("无效的URL: " + url);
    }
    
    HttpRequest request = HttpRequest::create_get(parsed.path + 
                                                (parsed.query.empty() ? "" : "?" + parsed.query));
    setup_request_headers(const_cast<HttpRequest&>(request), parsed);
    
    // 直接调用带URL的execute_request
    auto start_time = std::chrono::steady_clock::now();
    HttpResult result = execute_request(request, parsed);
    auto end_time = std::chrono::steady_clock::now();
    result.elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    return result;
}

HttpResult HttpClient::post(const std::string& url, const std::string& body, 
                           const std::string& content_type) {
    ParsedURL parsed = parse_url(url);
    if (parsed.scheme.empty()) {
        return HttpResult::error("无效的URL: " + url);
    }
    
    HttpRequest request = HttpRequest::create_post(
        parsed.path + (parsed.query.empty() ? "" : "?" + parsed.query),
        body, content_type);
    setup_request_headers(const_cast<HttpRequest&>(request), parsed);
    
    // 直接调用带URL的execute_request
    auto start_time = std::chrono::steady_clock::now();
    HttpResult result = execute_request(request, parsed);
    auto end_time = std::chrono::steady_clock::now();
    result.elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    return result;
}

HttpResult HttpClient::put(const std::string& url, const std::string& body,
                          const std::string& content_type) {
    ParsedURL parsed = parse_url(url);
    if (parsed.scheme.empty()) {
        return HttpResult::error("无效的URL: " + url);
    }
    
    HttpRequest request(HttpMethod::PUT, parsed.path + 
                       (parsed.query.empty() ? "" : "?" + parsed.query));
    request.set_body(body);
    request.set_header("Content-Type", content_type);
    request.update_content_length();
    setup_request_headers(request, parsed);
    
    // 直接调用带URL的execute_request
    auto start_time = std::chrono::steady_clock::now();
    HttpResult result = execute_request(request, parsed);
    auto end_time = std::chrono::steady_clock::now();
    result.elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    return result;
}

HttpResult HttpClient::del(const std::string& url) {
    ParsedURL parsed = parse_url(url);
    if (parsed.scheme.empty()) {
        return HttpResult::error("无效的URL: " + url);
    }
    
    HttpRequest request(HttpMethod::DELETE, parsed.path + 
                       (parsed.query.empty() ? "" : "?" + parsed.query));
    setup_request_headers(request, parsed);
    
    // 直接调用带URL的execute_request
    auto start_time = std::chrono::steady_clock::now();
    HttpResult result = execute_request(request, parsed);
    auto end_time = std::chrono::steady_clock::now();
    result.elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    return result;
}

HttpResult HttpClient::head(const std::string& url) {
    ParsedURL parsed = parse_url(url);
    if (parsed.scheme.empty()) {
        return HttpResult::error("无效的URL: " + url);
    }
    
    HttpRequest request(HttpMethod::HEAD, parsed.path + 
                       (parsed.query.empty() ? "" : "?" + parsed.query));
    setup_request_headers(request, parsed);
    
    // 直接调用带URL的execute_request
    auto start_time = std::chrono::steady_clock::now();
    HttpResult result = execute_request(request, parsed);
    auto end_time = std::chrono::steady_clock::now();
    result.elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    return result;
}

// 异步请求方法
std::future<HttpResult> HttpClient::async_request(const HttpRequest& request, 
                                                  AsyncCallback callback) {
    auto promise = std::make_shared<std::promise<HttpResult>>();
    auto future = promise->get_future();
    
    // Create a new request from the const reference to avoid copy issues
    HttpRequest req(request.get_method(), request.get_uri(), request.get_version());
    req.set_body(request.get_body());
    // Copy headers - need to get all headers, not just by name
    for (const auto& header : request.get_all_headers()) {
        req.set_header(header.first, header.second);
    }
    
    std::thread([this, req = std::move(req), callback, promise]() mutable {
        HttpResult result = this->request(req);
        if (callback) {
            callback(result);
        }
        promise->set_value(std::move(result));
    }).detach();
    
    return future;
}

std::future<HttpResult> HttpClient::async_get(const std::string& url, 
                                              AsyncCallback callback) {
    auto promise = std::make_shared<std::promise<HttpResult>>();
    auto future = promise->get_future();
    
    std::thread([this, url, callback, promise]() {
        HttpResult result = this->get(url);
        if (callback) {
            callback(result);
        }
        promise->set_value(std::move(result));
    }).detach();
    
    return future;
}

std::future<HttpResult> HttpClient::async_post(const std::string& url, const std::string& body,
                                               AsyncCallback callback,
                                               const std::string& content_type) {
    auto promise = std::make_shared<std::promise<HttpResult>>();
    auto future = promise->get_future();
    
    std::thread([this, url, body, content_type, callback, promise]() {
        HttpResult result = this->post(url, body, content_type);
        if (callback) {
            callback(result);
        }
        promise->set_value(std::move(result));
    }).detach();
    
    return future;
}

// 便捷方法
bool HttpClient::download_file(const std::string& url, const std::string& file_path,
                              ProgressCallback progress_callback) {
    HttpResult result = get(url);
    if (!result.success) {
        return false;
    }
    
    std::ofstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    const std::string& body = result.response.get_body();
    file.write(body.data(), body.size());
    
    if (progress_callback) {
        progress_callback(body.size(), body.size());
    }
    
    return file.good();
}

HttpResult HttpClient::upload_file(const std::string& url, const std::string& file_path,
                                  const std::string& field_name) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        return HttpResult::error("无法打开文件: " + file_path);
    }
    
    // 读取文件内容
    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string file_content = buffer.str();
    
    // 创建multipart/form-data请求
    std::string boundary = "----HttpClientBoundary" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    
    std::ostringstream body;
    body << "--" << boundary << "\r\n";
    body << "Content-Disposition: form-data; name=\"" << field_name 
         << "\"; filename=\"" << file_path.substr(file_path.find_last_of("/\\") + 1) << "\"\r\n";
    body << "Content-Type: application/octet-stream\r\n\r\n";
    body << file_content;
    body << "\r\n--" << boundary << "--\r\n";
    
    return post(url, body.str(), "multipart/form-data; boundary=" + boundary);
}

// 配置管理
void HttpClient::set_header(const std::string& name, const std::string& value) {
    std::lock_guard<std::mutex> lock(headers_mutex_);
    // Remove existing header with same name
    default_headers_.erase(name);
    // Insert new header
    default_headers_.emplace(name, value);
}

void HttpClient::remove_header(const std::string& name) {
    std::lock_guard<std::mutex> lock(headers_mutex_);
    default_headers_.erase(name);
}

void HttpClient::clear_headers() {
    std::lock_guard<std::mutex> lock(headers_mutex_);
    default_headers_.clear();
}

void HttpClient::set_cookie(const std::string& cookie) {
    set_header("Cookie", cookie);
}

void HttpClient::set_basic_auth(const std::string& username, const std::string& password) {
    std::string credentials = username + ":" + password;
    // 简单的Base64编码（实际应用中应使用标准库）
    std::string encoded = "Basic " + credentials; // 简化版本，实际需要Base64编码
    set_header("Authorization", encoded);
}

void HttpClient::set_bearer_token(const std::string& token) {
    set_header("Authorization", "Bearer " + token);
}

// SSL配置
void HttpClient::set_ssl_config(const SSLConfig& ssl_config) {
    ssl_config_ = ssl_config;
    ssl_config_set_ = true;
    
    // 重新创建SSL上下文管理器
    ssl_context_manager_ = std::make_shared<SSLContextManager>(false);
    ssl_context_manager_->initialize(ssl_config_);
    
    // 更新连接池的SSL配置
    connection_pool_->set_ssl_context_manager(ssl_context_manager_);
}

void HttpClient::set_ca_file(const std::string& ca_file) {
    if (!ssl_config_set_) {
        ssl_config_ = SSLConfig();
        ssl_config_set_ = true;
    }
    ssl_config_.ca_file = ca_file;
    set_ssl_config(ssl_config_);
}

void HttpClient::set_client_cert(const std::string& cert_file, const std::string& key_file) {
    if (!ssl_config_set_) {
        ssl_config_ = SSLConfig();
        ssl_config_set_ = true;
    }
    ssl_config_.cert_file = cert_file;
    ssl_config_.key_file = key_file;
    set_ssl_config(ssl_config_);
}

ConnectionStats HttpClient::get_connection_stats() const {
    return connection_pool_->get_stats();
}

void HttpClient::cleanup_connections() {
    connection_pool_->cleanup_expired_connections();
}

// 工具方法
ParsedURL HttpClient::parse_url(const std::string& url) {
    ParsedURL result;
    
    // 使用正则表达式解析URL
    std::regex url_regex(R"(^(https?):\/\/([^\/\?#:]+)(?::(\d+))?([^?#]*)(?:\?([^#]*))?(?:#(.*))?$)");
    std::smatch matches;
    
    if (!std::regex_match(url, matches, url_regex)) {
        return result; // 返回空结果表示解析失败
    }
    
    result.scheme = matches[1].str();
    result.host = matches[2].str();
    
    // 端口号
    if (matches[3].matched) {
        result.port = std::stoi(matches[3].str());
    } else {
        result.port = (result.scheme == "https") ? 443 : 80;
    }
    
    result.path = matches[4].matched ? matches[4].str() : "/";
    result.query = matches[5].matched ? matches[5].str() : "";
    result.fragment = matches[6].matched ? matches[6].str() : "";
    result.is_ssl = (result.scheme == "https");
    
    return result;
}

std::string HttpClient::url_encode(const std::string& str) {
    std::ostringstream encoded;
    encoded.fill('0');
    encoded << std::hex;
    
    for (char c : str) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded << c;
        } else {
            encoded << std::uppercase;
            encoded << '%' << std::setw(2) << static_cast<int>(static_cast<unsigned char>(c));
            encoded << std::nouppercase;
        }
    }
    
    return encoded.str();
}

std::string HttpClient::url_decode(const std::string& str) {
    std::string decoded;
    decoded.reserve(str.size());
    
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%' && i + 2 < str.size()) {
            int value;
            std::istringstream hex_stream(str.substr(i + 1, 2));
            if (hex_stream >> std::hex >> value) {
                decoded += static_cast<char>(value);
                i += 2;
            } else {
                decoded += str[i];
            }
        } else if (str[i] == '+') {
            decoded += ' ';
        } else {
            decoded += str[i];
        }
    }
    
    return decoded;
}

// 私有方法 - 兼容性版本（不带ParsedURL参数）
HttpResult HttpClient::execute_request(const HttpRequest& request) {
    // 从Host头部获取主机信息以构建ParsedURL
    std::string host = request.get_header("Host");
    if (host.empty()) {
        return HttpResult::error("请求缺少Host头部");
    }
    
    ParsedURL url;
    size_t colon_pos = host.find(':');
    if (colon_pos != std::string::npos) {
        url.host = host.substr(0, colon_pos);
        url.port = std::stoi(host.substr(colon_pos + 1));
    } else {
        url.host = host;
        url.port = 80;
    }
    url.scheme = "http";
    url.is_ssl = false;
    url.path = request.get_uri();
    
    return execute_request(request, url);
}

// 私有方法 - 带ParsedURL参数的版本
HttpResult HttpClient::execute_request(const HttpRequest& request, const ParsedURL& url) {
    if (config_.follow_redirects) {
        // Create a new request from the const reference to avoid copy issues
        HttpRequest req(request.get_method(), request.get_uri(), request.get_version());
        req.set_body(request.get_body());
        // Copy headers
        for (const auto& header : request.get_all_headers()) {
            req.set_header(header.first, header.second);
        }
        return handle_redirects(std::move(req), url);
    }
    
    // 直接执行请求，不处理重定向
    return execute_request_internal(request, url);
}

// 内部执行请求的实际实现
HttpResult HttpClient::execute_request_internal(const HttpRequest& request, const ParsedURL& url) {
    // 获取连接
    auto connection = get_connection(url);
    if (!connection) {
        return handle_connection_error("无法获取连接");
    }
    
    // 发送请求
    if (!send_request(connection, request)) {
        return_connection(connection, false);
        return HttpResult::error("发送请求失败");
    }
    
    // 接收响应
    HttpResult result = receive_response(connection);
    
    // 归还连接
    bool reusable = result.success && 
                   result.response.is_keep_alive() && 
                   request.is_keep_alive();
    return_connection(connection, reusable);
    
    return result;
}

HttpResult HttpClient::handle_redirects(HttpRequest&& request, const ParsedURL& url, size_t redirect_count) {
    if (redirect_count >= config_.max_redirects) {
        return HttpResult::error("重定向次数过多");
    }
    
    HttpResult result = execute_request_internal(request, url);
    
    if (result.success && result.status_code >= 300 && result.status_code < 400) {
        std::string location = result.response.get_header("Location");
        if (!location.empty()) {
            ParsedURL new_url = parse_url(location);
            if (!new_url.scheme.empty()) {
                HttpRequest new_request(request.get_method(), request.get_uri(), request.get_version());
                // Copy headers from original request
                for (const auto& header : request.get_all_headers()) {
                    new_request.set_header(header.first, header.second);
                }
                new_request.set_body(request.get_body());
                new_request.set_uri(new_url.path + 
                                   (new_url.query.empty() ? "" : "?" + new_url.query));
                setup_request_headers(new_request, new_url);
                return handle_redirects(std::move(new_request), new_url, redirect_count + 1);
            }
        }
    }
    
    return result;
}

// 兼容性版本 - 不带ParsedURL参数
HttpResult HttpClient::handle_redirects(HttpRequest&& request, size_t redirect_count) {
    // 从Host头部获取主机信息以构建ParsedURL
    std::string host = request.get_header("Host");
    if (host.empty()) {
        return HttpResult::error("请求缺少Host头部");
    }
    
    ParsedURL url;
    size_t colon_pos = host.find(':');
    if (colon_pos != std::string::npos) {
        url.host = host.substr(0, colon_pos);
        url.port = std::stoi(host.substr(colon_pos + 1));
    } else {
        url.host = host;
        url.port = 80;
    }
    url.scheme = "http";
    url.is_ssl = false;
    url.path = request.get_uri();
    
    return handle_redirects(std::move(request), url, redirect_count);
}

bool HttpClient::setup_request_headers(HttpRequest& request, const ParsedURL& url) {
    // 设置Host头部
    request.set_header("Host", url.host);
    
    // 添加默认头部
    std::lock_guard<std::mutex> lock(headers_mutex_);
    for (const auto& header : default_headers_) {
        if (!request.has_header(header.first)) {
            request.set_header(header.first, header.second);
        }
    }
    
    return true;
}

std::shared_ptr<HttpConnection> HttpClient::get_connection(const ParsedURL& url) {
    return connection_pool_->get_connection(url.host, url.port, url.is_ssl, config_.connect_timeout);
}

void HttpClient::return_connection(std::shared_ptr<HttpConnection> connection, bool reusable) {
    connection_pool_->return_connection(connection, reusable);
}

bool HttpClient::send_request(std::shared_ptr<HttpConnection> connection, const HttpRequest& request) {
    std::string request_data = request.to_string();
    return connection->send(request_data);
}

HttpResult HttpClient::receive_response(std::shared_ptr<HttpConnection> connection) {
    HttpResponse response;
    char buffer[8192];
    
    auto start_time = std::chrono::steady_clock::now();
    
    while (!response.is_complete()) {
        auto now = std::chrono::steady_clock::now();
        if (now - start_time > config_.response_timeout) {
            return handle_timeout_error();
        }
        
        int bytes_received = connection->receive(buffer, sizeof(buffer), std::chrono::seconds(5));
        if (bytes_received <= 0) {
            return HttpResult::error("接收响应失败");
        }
        
        int parsed = response.parse(buffer, bytes_received);
        if (parsed < 0) {
            return HttpResult::error("解析响应失败: " + response.get_error());
        }
        
        // 检查响应大小限制
        if (response.get_body_size() > config_.max_response_size) {
            return HttpResult::error("响应过大");
        }
    }
    
    return HttpResult(std::move(response));
}

HttpResult HttpClient::handle_connection_error(const std::string& message) {
    return HttpResult::error("连接错误: " + message);
}

HttpResult HttpClient::handle_timeout_error() {
    return HttpResult::error("请求超时");
}

HttpResult HttpClient::handle_ssl_error(const std::string& message) {
    return HttpResult::error("SSL错误: " + message);
}

// HttpClientFactory实现
std::unique_ptr<HttpClient> HttpClientFactory::create_default() {
    return std::unique_ptr<HttpClient>(new HttpClient());
}

std::unique_ptr<HttpClient> HttpClientFactory::create_https(const SSLConfig& ssl_config) {
    auto client = std::unique_ptr<HttpClient>(new HttpClient());
    client->set_ssl_config(ssl_config);
    return client;
}

std::unique_ptr<HttpClient> HttpClientFactory::create_configured(const HttpClientConfig& config) {
    return std::unique_ptr<HttpClient>(new HttpClient(config));
}

// HttpClientBuilder实现
HttpClientBuilder::HttpClientBuilder() : ssl_config_set_(false) {
}

HttpClientBuilder& HttpClientBuilder::timeout(std::chrono::seconds connect_timeout,
                                              std::chrono::seconds request_timeout,
                                              std::chrono::seconds response_timeout) {
    config_.connect_timeout = connect_timeout;
    config_.request_timeout = request_timeout;
    config_.response_timeout = response_timeout;
    return *this;
}

HttpClientBuilder& HttpClientBuilder::max_redirects(size_t count) {
    config_.max_redirects = count;
    return *this;
}

HttpClientBuilder& HttpClientBuilder::follow_redirects(bool enable) {
    config_.follow_redirects = enable;
    return *this;
}

HttpClientBuilder& HttpClientBuilder::verify_ssl(bool enable) {
    config_.verify_ssl = enable;
    return *this;
}

HttpClientBuilder& HttpClientBuilder::user_agent(const std::string& ua) {
    config_.user_agent = ua;
    return *this;
}

HttpClientBuilder& HttpClientBuilder::max_response_size(size_t size) {
    config_.max_response_size = size;
    return *this;
}

HttpClientBuilder& HttpClientBuilder::enable_compression(bool enable) {
    config_.enable_compression = enable;
    return *this;
}

HttpClientBuilder& HttpClientBuilder::enable_keep_alive(bool enable) {
    config_.enable_keep_alive = enable;
    return *this;
}

HttpClientBuilder& HttpClientBuilder::connection_pool(size_t max_per_host, size_t max_total) {
    config_.max_connections_per_host = max_per_host;
    config_.max_total_connections = max_total;
    return *this;
}

HttpClientBuilder& HttpClientBuilder::header(const std::string& name, const std::string& value) {
    // Remove existing header with same name
    headers_.erase(name);
    // Insert new header
    headers_.emplace(name, value);
    return *this;
}

HttpClientBuilder& HttpClientBuilder::cookie(const std::string& cookie) {
    return header("Cookie", cookie);
}

HttpClientBuilder& HttpClientBuilder::basic_auth(const std::string& username, const std::string& password) {
    std::string credentials = username + ":" + password;
    return header("Authorization", "Basic " + credentials); // 简化版本
}

HttpClientBuilder& HttpClientBuilder::bearer_token(const std::string& token) {
    return header("Authorization", "Bearer " + token);
}

HttpClientBuilder& HttpClientBuilder::ssl_config(const SSLConfig& config) {
    ssl_config_ = config;
    ssl_config_set_ = true;
    return *this;
}

HttpClientBuilder& HttpClientBuilder::ca_file(const std::string& file) {
    if (!ssl_config_set_) {
        ssl_config_ = SSLConfig();
        ssl_config_set_ = true;
    }
    ssl_config_.ca_file = file;
    return *this;
}

HttpClientBuilder& HttpClientBuilder::client_cert(const std::string& cert_file, const std::string& key_file) {
    if (!ssl_config_set_) {
        ssl_config_ = SSLConfig();
        ssl_config_set_ = true;
    }
    ssl_config_.cert_file = cert_file;
    ssl_config_.key_file = key_file;
    return *this;
}

std::unique_ptr<HttpClient> HttpClientBuilder::build() {
    auto client = std::unique_ptr<HttpClient>(new HttpClient(config_));
    
    // 设置头部
    for (const auto& header : headers_) {
        client->set_header(header.first, header.second);
    }
    
    // 设置SSL配置
    if (ssl_config_set_) {
        client->set_ssl_config(ssl_config_);
    }
    
    return client;
}

} // namespace stdhttps