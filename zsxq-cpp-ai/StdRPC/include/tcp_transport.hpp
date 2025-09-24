#ifndef TCP_TRANSPORT_HPP
#define TCP_TRANSPORT_HPP

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include "protocol.hpp"

namespace stdrpc {

/**
 * TCP连接类
 * 封装TCP套接字的基本操作
 */
class TcpConnection {
private:
    int socket_fd_ = -1;                    // 套接字文件描述符
    std::string remote_addr_;               // 远程地址
    uint16_t remote_port_ = 0;              // 远程端口
    std::atomic<bool> connected_{false};    // 连接状态

public:
    /**
     * 默认构造函数
     */
    TcpConnection() = default;

    /**
     * 从已存在的套接字构造
     * @param socket_fd 套接字文件描述符
     * @param addr 远程地址
     * @param port 远程端口
     */
    TcpConnection(int socket_fd, const std::string& addr, uint16_t port)
        : socket_fd_(socket_fd), remote_addr_(addr), remote_port_(port),
          connected_(true) {}

    /**
     * 析构函数
     */
    ~TcpConnection() {
        close();
    }

    // 禁用拷贝构造和赋值
    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;

    // 启用移动构造和赋值
    TcpConnection(TcpConnection&& other) noexcept {
        socket_fd_ = other.socket_fd_;
        remote_addr_ = std::move(other.remote_addr_);
        remote_port_ = other.remote_port_;
        connected_ = other.connected_.load();
        other.socket_fd_ = -1;
        other.connected_ = false;
    }

    TcpConnection& operator=(TcpConnection&& other) noexcept {
        if (this != &other) {
            close();
            socket_fd_ = other.socket_fd_;
            remote_addr_ = std::move(other.remote_addr_);
            remote_port_ = other.remote_port_;
            connected_ = other.connected_.load();
            other.socket_fd_ = -1;
            other.connected_ = false;
        }
        return *this;
    }

    /**
     * 连接到服务器
     * @param addr 服务器地址
     * @param port 服务器端口
     * @return 成功返回true，失败返回false
     */
    bool connect(const std::string& addr, uint16_t port) {
        // 创建套接字
        socket_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd_ < 0) {
            return false;
        }

        // 设置服务器地址
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        if (::inet_pton(AF_INET, addr.c_str(), &server_addr.sin_addr) <= 0) {
            ::close(socket_fd_);
            socket_fd_ = -1;
            return false;
        }

        // 连接服务器
        if (::connect(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            ::close(socket_fd_);
            socket_fd_ = -1;
            return false;
        }

        remote_addr_ = addr;
        remote_port_ = port;
        connected_ = true;

        // 设置套接字选项
        int opt = 1;
        ::setsockopt(socket_fd_, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));

        return true;
    }

    /**
     * 发送数据
     * @param data 数据缓冲区
     * @param size 数据大小
     * @return 成功发送的字节数，失败返回-1
     */
    ssize_t send(const void* data, size_t size) {
        if (!connected_ || socket_fd_ < 0) {
            return -1;
        }

        const uint8_t* buffer = static_cast<const uint8_t*>(data);
        size_t total_sent = 0;

        // 循环发送直到所有数据发送完成
        while (total_sent < size) {
            ssize_t sent = ::send(socket_fd_, buffer + total_sent,
                                 size - total_sent, MSG_NOSIGNAL);
            if (sent <= 0) {
                if (errno == EINTR) {
                    continue;  // 被信号中断，重试
                }
                connected_ = false;
                return -1;
            }
            total_sent += sent;
        }

        return total_sent;
    }

    /**
     * 接收数据
     * @param buffer 接收缓冲区
     * @param size 缓冲区大小
     * @param timeout_ms 超时时间（毫秒），-1表示无限等待
     * @return 成功接收的字节数，失败返回-1，超时返回0
     */
    ssize_t receive(void* buffer, size_t size, int timeout_ms = -1) {
        if (!connected_ || socket_fd_ < 0) {
            return -1;
        }

        if (timeout_ms >= 0) {
            // 使用poll等待数据
            struct pollfd pfd;
            pfd.fd = socket_fd_;
            pfd.events = POLLIN;
            int ret = ::poll(&pfd, 1, timeout_ms);
            if (ret == 0) {
                return 0;  // 超时
            } else if (ret < 0) {
                if (errno != EINTR) {
                    connected_ = false;
                }
                return -1;
            }
        }

        ssize_t received = ::recv(socket_fd_, buffer, size, 0);
        if (received <= 0) {
            if (received == 0 || errno != EINTR) {
                connected_ = false;
            }
            return received;
        }

        return received;
    }

    /**
     * 接收指定大小的数据
     * @param buffer 接收缓冲区
     * @param size 期望接收的大小
     * @param timeout_ms 超时时间（毫秒）
     * @return 成功返回true，失败返回false
     */
    bool receiveAll(void* buffer, size_t size, int timeout_ms = -1) {
        uint8_t* data = static_cast<uint8_t*>(buffer);
        size_t total_received = 0;

        while (total_received < size) {
            ssize_t received = receive(data + total_received,
                                      size - total_received, timeout_ms);
            if (received <= 0) {
                return false;
            }
            total_received += received;
        }

        return true;
    }

    /**
     * 发送消息
     * @param msg 消息对象
     * @return 成功返回true，失败返回false
     */
    bool sendMessage(const Message& msg) {
        std::vector<uint8_t> data = msg.serialize();
        return send(data.data(), data.size()) == static_cast<ssize_t>(data.size());
    }

    /**
     * 接收消息
     * @param timeout_ms 超时时间（毫秒）
     * @return 接收到的消息，失败返回nullptr
     */
    std::unique_ptr<Message> receiveMessage(int timeout_ms = -1) {
        // 先接收消息头
        uint8_t header_buf[MessageHeader::getHeaderSize()];
        if (!receiveAll(header_buf, MessageHeader::getHeaderSize(), timeout_ms)) {
            return nullptr;
        }

        // 解析消息头
        Serializer serializer(header_buf, MessageHeader::getHeaderSize());
        MessageHeader header;
        if (!header.deserialize(serializer)) {
            return nullptr;
        }

        // 接收消息体
        std::vector<uint8_t> full_msg(MessageHeader::getHeaderSize() + header.body_size);
        std::memcpy(full_msg.data(), header_buf, MessageHeader::getHeaderSize());

        if (header.body_size > 0) {
            if (!receiveAll(full_msg.data() + MessageHeader::getHeaderSize(),
                          header.body_size, timeout_ms)) {
                return nullptr;
            }
        }

        // 反序列化完整消息
        return Message::deserialize(full_msg.data(), full_msg.size());
    }

    /**
     * 关闭连接
     */
    void close() {
        if (socket_fd_ >= 0) {
            ::shutdown(socket_fd_, SHUT_RDWR);
            ::close(socket_fd_);
            socket_fd_ = -1;
        }
        connected_ = false;
    }

    /**
     * 检查是否已连接
     */
    bool isConnected() const {
        return connected_;
    }

    /**
     * 获取套接字文件描述符
     */
    int getSocketFd() const {
        return socket_fd_;
    }

    /**
     * 获取远程地址
     */
    const std::string& getRemoteAddress() const {
        return remote_addr_;
    }

    /**
     * 获取远程端口
     */
    uint16_t getRemotePort() const {
        return remote_port_;
    }

    /**
     * 设置套接字为非阻塞模式
     * @return 成功返回true，失败返回false
     */
    bool setNonBlocking(bool enable = true) {
        if (socket_fd_ < 0) {
            return false;
        }

        int flags = ::fcntl(socket_fd_, F_GETFL, 0);
        if (flags < 0) {
            return false;
        }

        if (enable) {
            flags |= O_NONBLOCK;
        } else {
            flags &= ~O_NONBLOCK;
        }

        return ::fcntl(socket_fd_, F_SETFL, flags) >= 0;
    }
};

/**
 * TCP服务器监听器
 * 用于监听和接受客户端连接
 */
class TcpListener {
private:
    int listen_fd_ = -1;              // 监听套接字
    uint16_t port_ = 0;                // 监听端口
    std::atomic<bool> listening_{false};  // 监听状态

public:
    /**
     * 默认构造函数
     */
    TcpListener() = default;

    /**
     * 析构函数
     */
    ~TcpListener() {
        stop();
    }

    /**
     * 开始监听
     * @param port 监听端口
     * @param backlog 等待队列大小
     * @return 成功返回true，失败返回false
     */
    bool listen(uint16_t port, int backlog = 128) {
        // 创建套接字
        listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd_ < 0) {
            return false;
        }

        // 设置地址重用
        int opt = 1;
        if (::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            ::close(listen_fd_);
            listen_fd_ = -1;
            return false;
        }

        // 绑定地址
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (::bind(listen_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            ::close(listen_fd_);
            listen_fd_ = -1;
            return false;
        }

        // 开始监听
        if (::listen(listen_fd_, backlog) < 0) {
            ::close(listen_fd_);
            listen_fd_ = -1;
            return false;
        }

        port_ = port;
        listening_ = true;
        return true;
    }

    /**
     * 接受客户端连接
     * @param timeout_ms 超时时间（毫秒），-1表示无限等待
     * @return 客户端连接对象，失败或超时返回nullptr
     */
    std::unique_ptr<TcpConnection> accept(int timeout_ms = -1) {
        if (!listening_ || listen_fd_ < 0) {
            return nullptr;
        }

        if (timeout_ms >= 0) {
            // 使用poll等待连接
            struct pollfd pfd;
            pfd.fd = listen_fd_;
            pfd.events = POLLIN;
            int ret = ::poll(&pfd, 1, timeout_ms);
            if (ret <= 0) {
                return nullptr;  // 超时或错误
            }
        }

        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = ::accept(listen_fd_, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) {
            return nullptr;
        }

        // 获取客户端地址信息
        char addr_str[INET_ADDRSTRLEN];
        ::inet_ntop(AF_INET, &client_addr.sin_addr, addr_str, INET_ADDRSTRLEN);
        uint16_t client_port = ntohs(client_addr.sin_port);

        return std::make_unique<TcpConnection>(client_fd, std::string(addr_str), client_port);
    }

    /**
     * 停止监听
     */
    void stop() {
        if (listen_fd_ >= 0) {
            ::close(listen_fd_);
            listen_fd_ = -1;
        }
        listening_ = false;
    }

    /**
     * 检查是否正在监听
     */
    bool isListening() const {
        return listening_;
    }

    /**
     * 获取监听端口
     */
    uint16_t getPort() const {
        return port_;
    }
};

} // namespace stdrpc

#endif // TCP_TRANSPORT_HPP