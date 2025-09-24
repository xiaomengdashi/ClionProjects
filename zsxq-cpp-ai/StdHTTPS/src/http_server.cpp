/**
 * @file http_server.cpp
 * @brief HTTP服务器实现（第1部分 - HttpRouter和基础功能）
 * @details 实现HTTP服务器的路由管理和基础功能
 */

#include "http_server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <errno.h>
#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace stdhttps {

// HttpRouter实现
void HttpRouter::add_route(HttpMethod method, const std::string& path, RequestHandler handler) {
    std::unique_lock<std::mutex> lock(routes_mutex_);
    routes_.emplace_back(method, path, handler);
}

void HttpRouter::add_route(const std::string& method, const std::string& path, RequestHandler handler) {
    add_route(string_to_method(method), path, handler);
}

void HttpRouter::get(const std::string& path, RequestHandler handler) {
    add_route(HttpMethod::GET, path, handler);
}

void HttpRouter::post(const std::string& path, RequestHandler handler) {
    add_route(HttpMethod::POST, path, handler);
}

void HttpRouter::put(const std::string& path, RequestHandler handler) {
    add_route(HttpMethod::PUT, path, handler);
}

void HttpRouter::del(const std::string& path, RequestHandler handler) {
    add_route(HttpMethod::DELETE, path, handler);
}

void HttpRouter::set_default_handler(RequestHandler handler) {
    std::unique_lock<std::mutex> lock(routes_mutex_);
    default_handler_ = handler;
}

bool HttpRouter::route_request(const HttpRequest& request, HttpResponse& response) {
    std::unique_lock<std::mutex> lock(routes_mutex_);
    
    HttpMethod method = request.get_method();
    std::string path = request.get_path();
    
    // 查找匹配的路由
    for (const auto& route : routes_) {
        if (route.method == method && route.matcher(method, path)) {
            lock.unlock();
            route.handler(request, response);
            return true;
        }
    }
    
    // 使用默认处理器
    if (default_handler_) {
        lock.unlock();
        default_handler_(request, response);
        return true;
    }
    
    return false;
}

HttpRouter::RouteMatcher HttpRouter::create_matcher(const std::string& path) {
    return [path](HttpMethod method, const std::string& request_path) -> bool {
        return match_path(path, request_path);
    };
}

bool HttpRouter::match_path(const std::string& pattern, const std::string& path) {
    // 简单的路径匹配，支持 * 通配符
    if (pattern == path) {
        return true;
    }
    
    if (pattern.find('*') != std::string::npos) {
        // 处理通配符匹配
        size_t star_pos = pattern.find('*');
        std::string prefix = pattern.substr(0, star_pos);
        
        if (path.length() >= prefix.length() && 
            path.substr(0, prefix.length()) == prefix) {
            // 前缀匹配
            if (star_pos == pattern.length() - 1) {
                return true; // 末尾通配符
            }
            
            // 检查后缀
            std::string suffix = pattern.substr(star_pos + 1);
            if (path.length() >= suffix.length() &&
                path.substr(path.length() - suffix.length()) == suffix) {
                return true;
            }
        }
    }
    
    return false;
}

// HttpServerConnection实现
HttpServerConnection::HttpServerConnection(int socket_fd, HttpServer* server)
    : socket_fd_(socket_fd), server_(server), active_(true) {
    
    // 获取客户端地址
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    if (getpeername(socket_fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &addr_len) == 0) {
        char addr_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, addr_str, INET_ADDRSTRLEN);
        client_address_ = std::string(addr_str) + ":" + std::to_string(ntohs(client_addr.sin_port));
    } else {
        client_address_ = "unknown";
    }
    
    // 如果启用SSL，设置SSL处理器
    if (server_->get_config().enable_ssl) {
        setup_ssl();
    }
}

HttpServerConnection::~HttpServerConnection() {
    close();
}

void HttpServerConnection::handle_connection() {
    try {
        bool keep_alive = true;
        auto& config = server_->get_config();
        
        while (active_ && keep_alive) {
            HttpRequest request;
            
            // 读取请求
            if (!read_request(request)) {
                break;
            }
            
            // 更新统计信息
            server_->stats_.total_requests++;
            
            // 创建响应
            HttpResponse response;
            response.set_version(request.get_version());
            
            try {
                // 处理请求
                handle_request(request, response);
                server_->stats_.successful_requests++;
            } catch (const std::exception& e) {
                // 处理异常
                response = HttpResponse::create_error(500, "Internal Server Error");
                server_->stats_.failed_requests++;
            }
            
            // 检查是否keep-alive
            keep_alive = request.is_keep_alive() && response.is_keep_alive() && 
                        request.get_version().major >= 1 && request.get_version().minor >= 1;
            
            if (!keep_alive) {
                response.set_keep_alive(false);
            }
            
            // 发送响应
            if (!send_response(response)) {
                break;
            }
            
            // 如果不是keep-alive，退出循环
            if (!keep_alive) {
                break;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "连接处理异常: " << e.what() << std::endl;
    }
    
    active_ = false;
}

void HttpServerConnection::close() {
    if (socket_fd_ >= 0) {
        if (ssl_handler_) {
            ssl_handler_->shutdown();
            ssl_handler_.reset();
        }
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
    active_ = false;
}

bool HttpServerConnection::setup_ssl() {
    auto* ssl_ctx = server_->get_ssl_context();
    if (!ssl_ctx || !ssl_ctx->is_initialized()) {
        return false;
    }
    
    ssl_handler_ = std::unique_ptr<SSLHandler>(new SSLHandler(ssl_ctx->get_context(), true));
    ssl_handler_->set_write_callback([this](const void* data, size_t size) -> int {
        return ::send(socket_fd_, data, size, MSG_NOSIGNAL);
    });
    
    return ssl_handler_->start_handshake();
}

bool HttpServerConnection::read_request(HttpRequest& request) {
    char buffer[BUFFER_SIZE];
    auto timeout = server_->get_config().request_timeout;
    auto start_time = std::chrono::steady_clock::now();
    
    request.reset();
    
    while (!request.is_complete() && active_) {
        // 检查超时
        auto now = std::chrono::steady_clock::now();
        if (now - start_time > timeout) {
            send_error_response(408, "Request Timeout");
            return false;
        }
        
        // 使用poll检查数据可用性
        struct pollfd pfd;
        pfd.fd = socket_fd_;
        pfd.events = POLLIN;
        
        int poll_result = poll(&pfd, 1, 1000); // 1秒超时
        if (poll_result == 0) {
            continue; // 超时，继续循环
        } else if (poll_result < 0) {
            return false; // poll错误
        }
        
        // 读取数据
        ssize_t bytes_read;
        if (ssl_handler_) {
            // SSL连接
            bytes_read = ::recv(socket_fd_, buffer, sizeof(buffer), MSG_DONTWAIT);
            if (bytes_read > 0) {
                auto error = ssl_handler_->handle_input(buffer, bytes_read);
                if (error != SSLError::NONE && error != SSLError::WANT_READ) {
                    return false;
                }
                
                // 从SSL读取解密数据
                size_t ssl_bytes_read;
                error = ssl_handler_->receive_data(buffer, sizeof(buffer), ssl_bytes_read);
                if (error == SSLError::NONE) {
                    bytes_read = static_cast<ssize_t>(ssl_bytes_read);
                } else if (error == SSLError::WANT_READ) {
                    continue; // 需要更多数据
                } else {
                    return false;
                }
            }
        } else {
            // 普通连接
            bytes_read = ::recv(socket_fd_, buffer, sizeof(buffer), 0);
        }
        
        if (bytes_read <= 0) {
            return false;
        }
        
        // 更新统计信息
        server_->stats_.bytes_received += bytes_read;
        
        // 解析数据
        int parsed = request.parse(buffer, bytes_read);
        if (parsed < 0) {
            send_error_response(400, "Bad Request");
            return false;
        }
        
        // 检查请求大小限制
        if (request.get_body_size() > server_->get_config().max_request_size) {
            send_error_response(413, "Payload Too Large");
            return false;
        }
    }
    
    return request.is_complete() && !request.has_error();
}

bool HttpServerConnection::send_response(const HttpResponse& response) {
    std::string response_data = response.to_string();
    
    // 更新统计信息
    server_->stats_.bytes_sent += response_data.size();
    
    const char* data = response_data.data();
    size_t total_size = response_data.size();
    size_t sent = 0;
    
    while (sent < total_size && active_) {
        ssize_t bytes_sent;
        
        if (ssl_handler_) {
            // SSL连接
            size_t ssl_bytes_sent;
            auto error = ssl_handler_->send_data(data + sent, total_size - sent, ssl_bytes_sent);
            if (error == SSLError::NONE) {
                bytes_sent = static_cast<ssize_t>(ssl_bytes_sent);
            } else {
                return false;
            }
        } else {
            // 普通连接
            bytes_sent = ::send(socket_fd_, data + sent, total_size - sent, MSG_NOSIGNAL);
        }
        
        if (bytes_sent <= 0) {
            return false;
        }
        
        sent += bytes_sent;
    }
    
    return sent == total_size;
}

void HttpServerConnection::handle_request(const HttpRequest& request, HttpResponse& response) {
    // 委托给服务器处理
    server_->handle_request(request, response);
}

void HttpServerConnection::send_error_response(int status_code, const std::string& message) {
    HttpResponse error_response = HttpResponse::create_error(status_code, message);
    error_response.set_keep_alive(false);
    send_response(error_response);
}

// HttpServer实现
HttpServer::HttpServer(const HttpServerConfig& config)
    : config_(config), running_(false), listen_socket_(-1) {
    
    // 忽略SIGPIPE信号
    signal(SIGPIPE, SIG_IGN);
    
    // 如果启用SSL，初始化SSL上下文
    if (config_.enable_ssl) {
        initialize_ssl();
    }
}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::start() {
    if (running_) {
        return true;
    }
    
    // 初始化socket
    if (!initialize_socket()) {
        return false;
    }
    
    running_ = true;
    stats_.start_time = std::chrono::steady_clock::now();
    
    // 启动工作线程
    for (size_t i = 0; i < config_.worker_threads; ++i) {
        worker_threads_.emplace_back(&HttpServer::worker_thread, this);
    }
    
    // 启动接受连接的线程
    accept_thread_ = std::thread(&HttpServer::accept_loop, this);
    
    std::cout << "HTTP服务器启动成功，监听 " << config_.bind_address 
              << ":" << config_.port << std::endl;
    
    return true;
}

void HttpServer::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    // 关闭监听socket
    if (listen_socket_ >= 0) {
        ::close(listen_socket_);
        listen_socket_ = -1;
    }
    
    // 通知所有等待的线程
    connections_condition_.notify_all();
    
    // 等待接受线程结束
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
    
    // 等待工作线程结束
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    // 清理连接
    cleanup_connections();
    
    std::cout << "HTTP服务器已停止" << std::endl;
}

void HttpServer::wait_for_shutdown() {
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
    
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

// 路由管理方法
void HttpServer::add_route(HttpMethod method, const std::string& path, RequestHandler handler) {
    router_.add_route(method, path, handler);
}

void HttpServer::add_route(const std::string& method, const std::string& path, RequestHandler handler) {
    router_.add_route(method, path, handler);
}

void HttpServer::get(const std::string& path, RequestHandler handler) {
    router_.get(path, handler);
}

void HttpServer::post(const std::string& path, RequestHandler handler) {
    router_.post(path, handler);
}

void HttpServer::put(const std::string& path, RequestHandler handler) {
    router_.put(path, handler);
}

void HttpServer::del(const std::string& path, RequestHandler handler) {
    router_.del(path, handler);
}

void HttpServer::set_default_handler(RequestHandler handler) {
    router_.set_default_handler(handler);
}

// 中间件支持
void HttpServer::use(Middleware middleware) {
    middlewares_.push_back(middleware);
}

// 静态文件服务
void HttpServer::serve_static(const std::string& path, const std::string& directory) {
    static_directories_[path] = directory;
}

HttpServerStats HttpServer::get_stats() const {
    return stats_;
}

void HttpServer::handle_request(const HttpRequest& request, HttpResponse& response) {
    // 执行中间件链
    size_t middleware_index = 0;
    
    std::function<void()> next = [&]() {
        if (middleware_index < middlewares_.size()) {
            auto current_middleware = middlewares_[middleware_index++];
            current_middleware(request, response, next);
        } else {
            // 执行路由处理
            if (!router_.route_request(request, response)) {
                // 尝试静态文件服务
                std::string path = request.get_path();
                bool served = false;
                
                for (const auto& static_dir : static_directories_) {
                    if (path.find(static_dir.first) == 0) {
                        std::string file_path = static_dir.second + 
                                              path.substr(static_dir.first.length());
                        if (serve_file(file_path, response)) {
                            served = true;
                            break;
                        }
                    }
                }
                
                if (!served) {
                    response = HttpResponse::create_error(404, "Not Found");
                }
            }
        }
    };
    
    next();
}

// 私有方法实现将在下一部分继续...

bool HttpServer::initialize_socket() {
    // 创建socket
    listen_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket_ < 0) {
        std::cerr << "创建socket失败: " << strerror(errno) << std::endl;
        return false;
    }
    
    // 设置socket选项
    int opt = 1;
    if (setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "设置SO_REUSEADDR失败: " << strerror(errno) << std::endl;
        ::close(listen_socket_);
        return false;
    }
    
    // 绑定地址
    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config_.port);
    
    if (config_.bind_address == "0.0.0.0") {
        server_addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, config_.bind_address.c_str(), &server_addr.sin_addr) <= 0) {
            std::cerr << "无效的绑定地址: " << config_.bind_address << std::endl;
            ::close(listen_socket_);
            return false;
        }
    }
    
    if (bind(listen_socket_, reinterpret_cast<struct sockaddr*>(&server_addr), 
             sizeof(server_addr)) < 0) {
        std::cerr << "绑定地址失败: " << strerror(errno) << std::endl;
        ::close(listen_socket_);
        return false;
    }
    
    // 开始监听
    if (listen(listen_socket_, SOMAXCONN) < 0) {
        std::cerr << "监听失败: " << strerror(errno) << std::endl;
        ::close(listen_socket_);
        return false;
    }
    
    return true;
}

bool HttpServer::initialize_ssl() {
    if (!config_.enable_ssl) {
        return true;
    }
    
    ssl_context_manager_ = std::unique_ptr<SSLContextManager>(new SSLContextManager(true));
    return ssl_context_manager_->initialize(config_.ssl_config);
}

void HttpServer::accept_loop() {
    while (running_) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        int client_socket = accept(listen_socket_, 
                                 reinterpret_cast<struct sockaddr*>(&client_addr), 
                                 &addr_len);
        
        if (client_socket < 0) {
            if (running_) {
                std::cerr << "接受连接失败: " << strerror(errno) << std::endl;
            }
            continue;
        }
        
        // 检查连接数限制
        if (stats_.active_connections >= config_.max_connections) {
            ::close(client_socket);
            continue;
        }
        
        // 创建连接对象
        auto connection = std::unique_ptr<HttpServerConnection>(new HttpServerConnection(client_socket, this));
        stats_.total_connections++;
        stats_.active_connections++;
        
        add_connection(std::move(connection));
    }
}

void HttpServer::worker_thread() {
    while (running_) {
        std::unique_ptr<HttpServerConnection> connection;
        
        {
            std::unique_lock<std::mutex> lock(connections_mutex_);
            connections_condition_.wait(lock, [this] {
                return !connection_queue_.empty() || !running_;
            });
            
            if (!running_) {
                break;
            }
            
            if (!connection_queue_.empty()) {
                connection = std::move(connection_queue_.front());
                connection_queue_.pop();
            }
        }
        
        if (connection) {
            connection->handle_connection();
            remove_connection(connection.get());
        }
    }
}

void HttpServer::add_connection(std::unique_ptr<HttpServerConnection> connection) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connection_queue_.push(std::move(connection));
    connections_condition_.notify_one();
}

void HttpServer::remove_connection(HttpServerConnection* connection) {
    stats_.active_connections--;
    
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_.erase(
        std::remove_if(connections_.begin(), connections_.end(),
                      [connection](const std::unique_ptr<HttpServerConnection>& conn) {
                          return conn.get() == connection;
                      }),
        connections_.end()
    );
}

void HttpServer::cleanup_connections() {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    // 清理连接队列
    while (!connection_queue_.empty()) {
        connection_queue_.pop();
    }
    
    // 关闭所有活跃连接
    for (auto& connection : connections_) {
        connection->close();
    }
    connections_.clear();
    
    stats_.active_connections = 0;
}

std::string HttpServer::get_mime_type(const std::string& file_path) const {
    size_t dot_pos = file_path.find_last_of('.');
    if (dot_pos == std::string::npos) {
        return "application/octet-stream";
    }
    
    std::string extension = file_path.substr(dot_pos + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    static std::unordered_map<std::string, std::string> mime_types = {
        {"html", "text/html"},
        {"htm", "text/html"},
        {"css", "text/css"},
        {"js", "text/javascript"},
        {"json", "application/json"},
        {"png", "image/png"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"gif", "image/gif"},
        {"svg", "image/svg+xml"},
        {"txt", "text/plain"},
        {"pdf", "application/pdf"},
        {"zip", "application/zip"}
    };
    
    auto it = mime_types.find(extension);
    return (it != mime_types.end()) ? it->second : "application/octet-stream";
}

bool HttpServer::serve_file(const std::string& file_path, HttpResponse& response) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // 获取文件大小
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // 读取文件内容
    std::string content(file_size, '\0');
    file.read(&content[0], file_size);
    
    // 设置响应
    response.set_status_code(200);
    response.set_header("Content-Type", get_mime_type(file_path));
    response.set_body(content);
    response.update_content_length();
    
    return true;
}

// HttpServerBuilder实现
HttpServerBuilder::HttpServerBuilder() {
    // 使用默认配置
}

HttpServerBuilder& HttpServerBuilder::bind(const std::string& address, int port) {
    config_.bind_address = address;
    config_.port = port;
    return *this;
}

HttpServerBuilder& HttpServerBuilder::threads(size_t count) {
    config_.worker_threads = count;
    return *this;
}

HttpServerBuilder& HttpServerBuilder::max_connections(size_t count) {
    config_.max_connections = count;
    return *this;
}

HttpServerBuilder& HttpServerBuilder::max_request_size(size_t size) {
    config_.max_request_size = size;
    return *this;
}

HttpServerBuilder& HttpServerBuilder::keep_alive_timeout(std::chrono::seconds timeout) {
    config_.keep_alive_timeout = timeout;
    return *this;
}

HttpServerBuilder& HttpServerBuilder::request_timeout(std::chrono::seconds timeout) {
    config_.request_timeout = timeout;
    return *this;
}

HttpServerBuilder& HttpServerBuilder::enable_ssl(const SSLConfig& ssl_config) {
    config_.enable_ssl = true;
    config_.ssl_config = ssl_config;
    return *this;
}

HttpServerBuilder& HttpServerBuilder::enable_chunked(bool enable) {
    config_.enable_chunked = enable;
    return *this;
}

HttpServerBuilder& HttpServerBuilder::chunk_size(size_t size) {
    config_.default_chunk_size = size;
    return *this;
}

std::unique_ptr<HttpServer> HttpServerBuilder::build() {
    return std::unique_ptr<HttpServer>(new HttpServer(config_));
}

} // namespace stdhttps