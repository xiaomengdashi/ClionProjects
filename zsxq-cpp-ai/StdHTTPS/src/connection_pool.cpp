/**
 * @file connection_pool.cpp
 * @brief HTTP连接池管理器实现
 * @details 实现HTTP/HTTPS连接的复用和管理功能
 */

#include "connection_pool.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <cstring>
#include <sstream>
#include <algorithm>

namespace stdhttps {

// HttpConnection实现
HttpConnection::HttpConnection(const std::string& host, int port, bool use_ssl)
    : host_(host), port_(port), use_ssl_(use_ssl), socket_fd_(-1)
    , state_(ConnectionState::CLOSED), keep_alive_(false)
    , ssl_ctx_(nullptr) {
    created_at_ = std::chrono::steady_clock::now();
    touch();
}

HttpConnection::~HttpConnection() {
    close();
}

bool HttpConnection::connect(std::chrono::seconds timeout) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_ == ConnectionState::CONNECTED) {
        return true;
    }
    
    cleanup();
    state_ = ConnectionState::CONNECTING;
    
    // 创建socket
    if (!create_socket()) {
        state_ = ConnectionState::ERROR;
        return false;
    }
    
    // 连接服务器
    if (!connect_socket()) {
        cleanup();
        state_ = ConnectionState::ERROR;
        return false;
    }
    
    // 如果使用SSL，进行SSL握手
    if (use_ssl_) {
        if (!ssl_ctx_) {
            set_error("SSL上下文未设置");
            cleanup();
            state_ = ConnectionState::ERROR;
            return false;
        }
        
        ssl_handler_ = std::unique_ptr<SSLHandler>(new SSLHandler(ssl_ctx_, false));
        ssl_handler_->set_write_callback([this](const void* data, size_t size) -> int {
            return ::send(socket_fd_, data, size, MSG_NOSIGNAL);
        });
        
        if (!ssl_handler_->start_handshake()) {
            set_error("SSL握手失败: " + ssl_handler_->get_last_error());
            cleanup();
            state_ = ConnectionState::ERROR;
            return false;
        }
        
        // 完成SSL握手（简化版本，实际应用中可能需要多次交互）
        char buffer[1024];
        auto start_time = std::chrono::steady_clock::now();
        
        while (!ssl_handler_->is_handshake_completed()) {
            auto now = std::chrono::steady_clock::now();
            if (now - start_time > timeout) {
                set_error("SSL握手超时");
                cleanup();
                state_ = ConnectionState::ERROR;
                return false;
            }
            
            int bytes_received = ::recv(socket_fd_, buffer, sizeof(buffer), MSG_DONTWAIT);
            if (bytes_received > 0) {
                auto error = ssl_handler_->handle_input(buffer, bytes_received);
                if (error != SSLError::NONE && error != SSLError::WANT_READ && error != SSLError::WANT_WRITE) {
                    set_error("SSL握手错误: " + ssl_handler_->get_last_error());
                    cleanup();
                    state_ = ConnectionState::ERROR;
                    return false;
                }
            } else if (bytes_received == 0) {
                set_error("连接被服务器关闭");
                cleanup();
                state_ = ConnectionState::ERROR;
                return false;
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                set_error("接收数据错误: " + std::string(strerror(errno)));
                cleanup();
                state_ = ConnectionState::ERROR;
                return false;
            }
            
            // 短暂休眠，避免忙等待
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    state_ = ConnectionState::CONNECTED;
    touch();
    return true;
}

bool HttpConnection::send(const std::string& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_ != ConnectionState::CONNECTED) {
        set_error("连接未建立");
        return false;
    }
    
    if (use_ssl_ && ssl_handler_) {
        size_t bytes_sent;
        auto error = ssl_handler_->send_data(data.data(), data.size(), bytes_sent);
        if (error != SSLError::NONE) {
            set_error("SSL发送数据错误: " + ssl_handler_->get_last_error());
            return false;
        }
        return bytes_sent == data.size();
    } else {
        ssize_t bytes_sent = ::send(socket_fd_, data.data(), data.size(), MSG_NOSIGNAL);
        if (bytes_sent < 0) {
            set_error("发送数据错误: " + std::string(strerror(errno)));
            return false;
        }
        return static_cast<size_t>(bytes_sent) == data.size();
    }
}

int HttpConnection::receive(char* buffer, size_t size, std::chrono::seconds timeout) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_ != ConnectionState::CONNECTED) {
        set_error("连接未建立");
        return -1;
    }
    
    // 设置超时
    struct pollfd pfd;
    pfd.fd = socket_fd_;
    pfd.events = POLLIN;
    
    int poll_result = poll(&pfd, 1, static_cast<int>(timeout.count() * 1000));
    if (poll_result == 0) {
        set_error("接收数据超时");
        return -1;
    } else if (poll_result < 0) {
        set_error("poll错误: " + std::string(strerror(errno)));
        return -1;
    }
    
    if (use_ssl_ && ssl_handler_) {
        // SSL接收
        char temp_buffer[8192];
        ssize_t bytes_received = ::recv(socket_fd_, temp_buffer, sizeof(temp_buffer), MSG_DONTWAIT);
        
        if (bytes_received > 0) {
            auto error = ssl_handler_->handle_input(temp_buffer, bytes_received);
            if (error != SSLError::NONE && error != SSLError::WANT_READ) {
                set_error("SSL处理输入错误: " + ssl_handler_->get_last_error());
                return -1;
            }
        }
        
        // 从SSL读取解密数据
        size_t ssl_bytes_received;
        auto error = ssl_handler_->receive_data(buffer, size, ssl_bytes_received);
        if (error == SSLError::NONE) {
            touch();
            return static_cast<int>(ssl_bytes_received);
        } else if (error == SSLError::WANT_READ) {
            return 0; // 需要更多数据
        } else {
            set_error("SSL接收数据错误: " + ssl_handler_->get_last_error());
            return -1;
        }
    } else {
        // 普通socket接收
        ssize_t bytes_received = ::recv(socket_fd_, buffer, size, 0);
        if (bytes_received > 0) {
            touch();
            return static_cast<int>(bytes_received);
        } else if (bytes_received == 0) {
            set_error("连接被服务器关闭");
            state_ = ConnectionState::CLOSED;
            return -1;
        } else {
            set_error("接收数据错误: " + std::string(strerror(errno)));
            return -1;
        }
    }
}

void HttpConnection::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_ == ConnectionState::CLOSED) {
        return;
    }
    
    state_ = ConnectionState::CLOSING;
    
    if (use_ssl_ && ssl_handler_) {
        ssl_handler_->shutdown();
        ssl_handler_.reset();
    }
    
    cleanup();
    state_ = ConnectionState::CLOSED;
}

bool HttpConnection::is_reusable() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return keep_alive_ && 
           state_ == ConnectionState::CONNECTED && 
           !is_expired();
}

bool HttpConnection::is_expired() const {
    auto now = std::chrono::steady_clock::now();
    auto idle_time = now - last_used_;
    return idle_time > std::chrono::seconds(60); // 60秒超时
}

std::string HttpConnection::get_connection_key() const {
    std::ostringstream oss;
    oss << host_ << ":" << port_ << ":" << (use_ssl_ ? "ssl" : "http");
    return oss.str();
}

void HttpConnection::set_ssl_context(SSL_CTX* ssl_ctx) {
    ssl_ctx_ = ssl_ctx;
}

void HttpConnection::touch() {
    last_used_ = std::chrono::steady_clock::now();
}

bool HttpConnection::create_socket() {
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        set_error("创建socket失败: " + std::string(strerror(errno)));
        return false;
    }
    
    // 设置非阻塞模式
    int flags = fcntl(socket_fd_, F_GETFL, 0);
    if (flags < 0 || fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
        set_error("设置socket为非阻塞模式失败: " + std::string(strerror(errno)));
        return false;
    }
    
    return true;
}

bool HttpConnection::connect_socket() {
    // 使用getaddrinfo替代gethostbyname（线程安全）
    struct addrinfo hints, *result;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    std::string port_str = std::to_string(port_);
    int status = getaddrinfo(host_.c_str(), port_str.c_str(), &hints, &result);
    if (status != 0) {
        set_error("无法解析主机名: " + host_ + " - " + gai_strerror(status));
        return false;
    }
    
    // 使用第一个可用的地址
    // 连接服务器
    int connect_result = ::connect(socket_fd_, result->ai_addr, result->ai_addrlen);
    int saved_errno = errno; // 保存errno以便在freeaddrinfo之后使用
    
    // 释放getaddrinfo分配的内存
    freeaddrinfo(result);
    
    if (connect_result == 0) {
        // 立即连接成功
        return true;
    } else if (saved_errno == EINPROGRESS) {
        // 连接正在进行中，等待完成
        struct pollfd pfd;
        pfd.fd = socket_fd_;
        pfd.events = POLLOUT;
        
        int poll_result = poll(&pfd, 1, 30000); // 30秒超时
        if (poll_result <= 0) {
            set_error("连接超时或poll错误");
            return false;
        }
        
        // 检查连接是否成功
        int socket_error;
        socklen_t len = sizeof(socket_error);
        if (getsockopt(socket_fd_, SOL_SOCKET, SO_ERROR, &socket_error, &len) < 0 || socket_error != 0) {
            set_error("连接失败: " + std::string(strerror(socket_error)));
            return false;
        }
        
        return true;
    } else {
        set_error("连接失败: " + std::string(strerror(saved_errno)));
        return false;
    }
}

void HttpConnection::cleanup() {
    if (socket_fd_ >= 0) {
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
    ssl_handler_.reset();
}

void HttpConnection::set_error(const std::string& message) {
    error_message_ = message;
}

// ConnectionPool实现
ConnectionPool::ConnectionPool(const ConnectionPoolConfig& config)
    : config_(config), running_(false) {
}

ConnectionPool::~ConnectionPool() {
    stop();
}

std::shared_ptr<HttpConnection> ConnectionPool::get_connection(const std::string& host, 
                                                              int port, 
                                                              bool use_ssl,
                                                              std::chrono::seconds timeout) {
    std::string key = make_connection_key(host, port, use_ssl);
    
    std::unique_lock<std::mutex> lock(connections_mutex_);
    
    // 尝试从空闲连接中获取可复用的连接
    auto idle_it = idle_connections_.find(key);
    if (idle_it != idle_connections_.end() && !idle_it->second.empty()) {
        auto connection = idle_it->second.front();
        idle_it->second.pop();
        
        if (connection && connection->is_reusable()) {
            // 移动到活跃连接
            active_connections_[key].push_back(connection);
            lock.unlock();
            
            // 更新统计
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.active_connections++;
            stats_.idle_connections--;
            
            return connection;
        }
    }
    
    // 检查是否超过最大连接数限制
    auto active_it = active_connections_.find(key);
    size_t current_host_connections = (active_it != active_connections_.end()) ? active_it->second.size() : 0;
    
    if (current_host_connections >= config_.max_connections_per_host) {
        // 等待连接可用
        auto wait_until = std::chrono::steady_clock::now() + timeout;
        while (current_host_connections >= config_.max_connections_per_host && 
               std::chrono::steady_clock::now() < wait_until) {
            if (connections_condition_.wait_for(lock, std::chrono::seconds(1)) == std::cv_status::timeout) {
                break;
            }
            active_it = active_connections_.find(key);
            current_host_connections = (active_it != active_connections_.end()) ? active_it->second.size() : 0;
        }
        
        if (current_host_connections >= config_.max_connections_per_host) {
            return nullptr; // 获取连接超时
        }
    }
    
    lock.unlock();
    
    // 创建新连接
    auto connection = create_connection(host, port, use_ssl);
    if (!connection) {
        return nullptr;
    }
    
    if (!connection->connect(timeout)) {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.failed_connections++;
        return nullptr;
    }
    
    // 添加到活跃连接
    lock.lock();
    active_connections_[key].push_back(connection);
    lock.unlock();
    
    // 更新统计
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    stats_.total_connections++;
    stats_.active_connections++;
    
    return connection;
}

void ConnectionPool::return_connection(std::shared_ptr<HttpConnection> connection, bool reusable) {
    if (!connection) {
        return;
    }
    
    std::string key = connection->get_connection_key();
    
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    // 从活跃连接中移除
    auto active_it = active_connections_.find(key);
    if (active_it != active_connections_.end()) {
        auto& active_list = active_it->second;
        active_list.erase(
            std::remove(active_list.begin(), active_list.end(), connection),
            active_list.end()
        );
    }
    
    if (reusable && connection->is_reusable()) {
        // 添加到空闲连接
        idle_connections_[key].push(connection);
        
        // 更新统计
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.active_connections--;
        stats_.idle_connections++;
    } else {
        // 关闭连接
        connection->close();
        
        // 更新统计
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.active_connections--;
    }
    
    connections_condition_.notify_all();
}

void ConnectionPool::close_connections(const std::string& host, int port, bool use_ssl) {
    std::string key = make_connection_key(host, port, use_ssl);
    
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    // 关闭空闲连接
    auto idle_it = idle_connections_.find(key);
    if (idle_it != idle_connections_.end()) {
        while (!idle_it->second.empty()) {
            auto connection = idle_it->second.front();
            idle_it->second.pop();
            connection->close();
            
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.idle_connections--;
        }
    }
    
    // 关闭活跃连接
    auto active_it = active_connections_.find(key);
    if (active_it != active_connections_.end()) {
        for (auto& connection : active_it->second) {
            connection->close();
        }
        
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.active_connections -= active_it->second.size();
        
        active_it->second.clear();
    }
}

void ConnectionPool::close_all_connections() {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    // 关闭所有空闲连接
    for (auto& pair : idle_connections_) {
        while (!pair.second.empty()) {
            auto connection = pair.second.front();
            pair.second.pop();
            connection->close();
        }
    }
    
    // 关闭所有活跃连接
    for (auto& pair : active_connections_) {
        for (auto& connection : pair.second) {
            connection->close();
        }
        pair.second.clear();
    }
    
    idle_connections_.clear();
    active_connections_.clear();
    
    // 重置统计
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    stats_.active_connections = 0;
    stats_.idle_connections = 0;
}

void ConnectionPool::cleanup_expired_connections() {
    remove_expired_connections();
}

ConnectionStats ConnectionPool::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void ConnectionPool::set_ssl_context_manager(std::shared_ptr<SSLContextManager> ssl_context) {
    ssl_context_manager_ = ssl_context;
}

void ConnectionPool::start() {
    if (running_) {
        return;
    }
    
    running_ = true;
    cleanup_thread_ = std::thread(&ConnectionPool::cleanup_worker, this);
}

void ConnectionPool::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }
    
    close_all_connections();
}

std::string ConnectionPool::make_connection_key(const std::string& host, int port, bool use_ssl) const {
    std::ostringstream oss;
    oss << host << ":" << port << ":" << (use_ssl ? "ssl" : "http");
    return oss.str();
}

void ConnectionPool::cleanup_worker() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        remove_expired_connections();
    }
}

void ConnectionPool::remove_expired_connections() {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    for (auto& pair : idle_connections_) {
        auto& queue = pair.second;
        std::queue<std::shared_ptr<HttpConnection>> temp_queue;
        
        while (!queue.empty()) {
            auto connection = queue.front();
            queue.pop();
            
            if (connection && !connection->is_expired()) {
                temp_queue.push(connection);
            } else {
                if (connection) {
                    connection->close();
                }
                
                std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                stats_.idle_connections--;
            }
        }
        
        queue = std::move(temp_queue);
    }
}

std::shared_ptr<HttpConnection> ConnectionPool::create_connection(const std::string& host, int port, bool use_ssl) {
    auto connection = std::make_shared<HttpConnection>(host, port, use_ssl);
    
    if (use_ssl && ssl_context_manager_ && ssl_context_manager_->is_initialized()) {
        connection->set_ssl_context(ssl_context_manager_->get_context());
    }
    
    return connection;
}

// ConnectionPoolFactory实现
std::unique_ptr<ConnectionPool> ConnectionPoolFactory::create_http_pool(size_t max_connections_per_host,
                                                                       size_t max_total_connections) {
    ConnectionPoolConfig config;
    config.max_connections_per_host = max_connections_per_host;
    config.max_total_connections = max_total_connections;
    config.enable_ssl = false;
    
    return std::unique_ptr<ConnectionPool>(new ConnectionPool(config));
}

std::unique_ptr<ConnectionPool> ConnectionPoolFactory::create_https_pool(const SSLConfig& ssl_config,
                                                                        size_t max_connections_per_host,
                                                                        size_t max_total_connections) {
    ConnectionPoolConfig config;
    config.max_connections_per_host = max_connections_per_host;
    config.max_total_connections = max_total_connections;
    config.enable_ssl = true;
    config.ssl_config = ssl_config;
    
    auto pool = std::unique_ptr<ConnectionPool>(new ConnectionPool(config));
    
    // 创建SSL上下文管理器
    auto ssl_context = std::make_shared<SSLContextManager>(false);
    if (ssl_context->initialize(ssl_config)) {
        pool->set_ssl_context_manager(ssl_context);
    }
    
    return pool;
}

std::unique_ptr<ConnectionPool> ConnectionPoolFactory::create_pool(const ConnectionPoolConfig& config) {
    auto pool = std::unique_ptr<ConnectionPool>(new ConnectionPool(config));
    
    if (config.enable_ssl) {
        auto ssl_context = std::make_shared<SSLContextManager>(false);
        if (ssl_context->initialize(config.ssl_config)) {
            pool->set_ssl_context_manager(ssl_context);
        }
    }
    
    return pool;
}

// ConnectionWrapper实现
ConnectionWrapper::ConnectionWrapper(std::shared_ptr<HttpConnection> connection, 
                                   std::shared_ptr<ConnectionPool> pool)
    : connection_(connection), pool_(pool), reusable_(true), released_(false) {
}

ConnectionWrapper::~ConnectionWrapper() {
    if (!released_ && connection_ && pool_) {
        pool_->return_connection(connection_, reusable_);
    }
}

ConnectionWrapper::ConnectionWrapper(ConnectionWrapper&& other) noexcept
    : connection_(std::move(other.connection_))
    , pool_(std::move(other.pool_))
    , reusable_(other.reusable_)
    , released_(other.released_) {
    other.released_ = true;
}

ConnectionWrapper& ConnectionWrapper::operator=(ConnectionWrapper&& other) noexcept {
    if (this != &other) {
        if (!released_ && connection_ && pool_) {
            pool_->return_connection(connection_, reusable_);
        }
        
        connection_ = std::move(other.connection_);
        pool_ = std::move(other.pool_);
        reusable_ = other.reusable_;
        released_ = other.released_;
        
        other.released_ = true;
    }
    return *this;
}

void ConnectionWrapper::release() {
    released_ = true;
}

} // namespace stdhttps