#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <map>

#define PORT 8080

enum class ClientState {
    SSL_HANDSHAKE,
    WS_HANDSHAKE,
    WS_CONNECTED
};

struct ClientContext {
    int fd;
    SSL* ssl;
    ClientState state;
    std::string handshake_buffer;
};

int set_non_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
        return -1;
    }
    return 0;
}

class WebSocketSSLServer {
public:
    WebSocketSSLServer(int port) : port_(port), ctx_(nullptr), server_fd_(-1) {}
    ~WebSocketSSLServer() { cleanup(); }
    void run();

private:
    std::string base64_encode(const unsigned char* input, int length);
    std::string websocket_accept_key(const std::string& key);
    void send_ws_frame(SSL* ssl, const std::string& msg);
    void handle_client_handshake(ClientContext& ctx);
    void handle_client_message(ClientContext& ctx);
    void cleanup_client(ClientContext& ctx);
    void cleanup();

    int port_;
    SSL_CTX* ctx_;
    int server_fd_;
};

std::string WebSocketSSLServer::base64_encode(const unsigned char* input, int length) {
    BIO* bmem = BIO_new(BIO_s_mem());
    BIO* b64 = BIO_new(BIO_f_base64());
    b64 = BIO_push(b64, bmem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, input, length);
    BIO_flush(b64);
    BUF_MEM* bptr;
    BIO_get_mem_ptr(b64, &bptr);
    std::string output(bptr->data, bptr->length);
    BIO_free_all(b64);
    return output;
}

/**
 * @brief 生成 WebSocket 握手响应中的 Sec-WebSocket-Accept 字段值
 * 
 * 该方法依据 WebSocket 协议规范，将客户端发送的 Sec-WebSocket-Key 与固定的 GUID 拼接，
 * 对拼接后的字符串进行 SHA-1 哈希计算，再将哈希结果进行 Base64 编码，
 * 最终得到 Sec-WebSocket-Accept 字段所需的值。
 * 
 * @param key 客户端发送的 Sec-WebSocket-Key 字符串
 * @return std::string 经过处理后生成的 Sec-WebSocket-Accept 字符串
 */
std::string WebSocketSSLServer::websocket_accept_key(const std::string& key) {
    std::string GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string accept_src = key + GUID;
    unsigned char sha1_result[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(accept_src.c_str()), accept_src.size(), sha1_result);
    return base64_encode(sha1_result, SHA_DIGEST_LENGTH);
}

/**
 * @brief 向指定的 SSL 连接发送 WebSocket 文本帧
 * 
 * 此方法会根据消息的长度按照 WebSocket 协议规范构建文本帧，
 * 并通过 SSL 连接将构建好的帧发送给客户端。
 * 
 * @param ssl SSL 连接对象指针，用于与客户端进行安全通信
 * @param msg 要发送的文本消息内容
 */
void WebSocketSSLServer:: send_ws_frame(SSL* ssl, const std::string& msg) {
    std::vector<unsigned char> frame;
    frame.push_back(0x81); // FIN + text
    if (msg.size() < 126) {
        frame.push_back(msg.size());
    } else {
        frame.push_back(126);
        frame.push_back((msg.size() >> 8) & 0xFF);
        frame.push_back(msg.size() & 0xFF);
    }
    frame.insert(frame.end(), msg.begin(), msg.end());
    SSL_write(ssl, frame.data(), frame.size());
}

/**
 * @brief 处理客户端的 WebSocket 握手过程
 * 
 * 该方法从客户端读取数据，解析其中的 Sec-WebSocket-Key，
 * 生成对应的 Sec-WebSocket-Accept 响应，发送握手响应和欢迎消息，
 * 最后将客户端状态设置为已连接。
 * 
 * @param ctx 客户端上下文，包含客户端文件描述符、SSL 指针、客户端状态和握手缓冲区
 */
void WebSocketSSLServer::handle_client_handshake(ClientContext& ctx) {
    // 用于存储从 SSL 连接读取的握手数据的缓冲区
    char buf[4096] = {0};
    // 从 SSL 连接读取数据，返回读取的字节数
    int len = SSL_read(ctx.ssl, buf, sizeof(buf));
    // 检查读取是否失败或没有数据可读
    if (len <= 0) {
        // 获取 SSL 错误码
        int err = SSL_get_error(ctx.ssl, len);
        // 若不是由于需要读写等待的错误，则清理客户端资源
        if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
            cleanup_client(ctx);
        }
        return;
    }
    // 将读取的数据追加到客户端的握手缓冲区
    ctx.handshake_buffer.append(buf, len);

    // 解析 Sec-WebSocket-Key
    // 创建一个字符串输入流，用于逐行解析握手缓冲区中的数据
    std::istringstream iss(ctx.handshake_buffer);
    // 存储从输入流中读取的每一行数据
    std::string line, ws_key;
    // 逐行读取输入流中的数据
    while (std::getline(iss, line)) {
        // 检查当前行是否包含 Sec-WebSocket-Key 字段
        if (line.find("Sec-WebSocket-Key:") != std::string::npos) {
            // 提取 Sec-WebSocket-Key 的值
            ws_key = line.substr(line.find(":") + 2);
            // 移除 Sec-WebSocket-Key 值中的换行符
            ws_key.erase(std::remove(ws_key.begin(), ws_key.end(), '\r'), ws_key.end());
            break;
        }
    }
    // 若未找到 Sec-WebSocket-Key 则返回
    if (ws_key.empty()) {
        return;
    }

    // 发送握手响应
    // 生成 Sec-WebSocket-Accept 字段的值
    std::string accept_key = websocket_accept_key(ws_key);
    // 创建一个字符串输出流，用于构建握手响应消息
    std::ostringstream oss;
    oss << "HTTP/1.1 101 Switching Protocols\r\n"
        << "Upgrade: websocket\r\n"
        << "Connection: Upgrade\r\n"
        << "Sec-WebSocket-Accept: " << accept_key << "\r\n\r\n";
    // 通过 SSL 连接发送握手响应消息
    SSL_write(ctx.ssl, oss.str().c_str(), oss.str().size());

    // 发送欢迎消息
    send_ws_frame(ctx.ssl, "Hello from OpenSSL WebSocket!");

    // 将客户端状态设置为 WebSocket 连接已建立
    ctx.state = ClientState::WS_CONNECTED;
}

/**
 * @brief 处理客户端发送的 WebSocket 消息
 * 
 * 该方法从客户端的 SSL 连接中读取数据，解析 WebSocket 帧，
 * 根据不同的帧类型（如关闭帧、Ping 帧、文本帧）进行相应处理。
 * 
 * @param ctx 客户端上下文，包含客户端文件描述符、SSL 指针、客户端状态和握手缓冲区
 */
void WebSocketSSLServer::handle_client_message(ClientContext& ctx) {
    // 用于存储从客户端读取的 WebSocket 帧数据
    std::vector<unsigned char> frame_buffer;
    // 单次读取的缓冲区大小
    const size_t buffer_size = 4096;
    // 读取数据的临时缓冲区
    unsigned char buffer[buffer_size];
    // 记录每次读取的字节数
    int n;
    // 循环读取数据，直到读取的数据长度小于缓冲区大小
    do {
        // 从 SSL 连接中读取数据
        n = SSL_read(ctx.ssl, buffer, buffer_size);
        if (n <= 0) {
            // 获取 SSL 错误码
            int err = SSL_get_error(ctx.ssl, n);
            // 若不是由于需要读写等待的错误，则清理客户端资源
            if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
                cleanup_client(ctx);
            }
            return;
        }
        // 将读取的数据追加到帧缓冲区
        frame_buffer.insert(frame_buffer.end(), buffer, buffer + n);
    } while (n == buffer_size);

    // 当前处理的帧数据在缓冲区中的偏移量
    size_t offset = 0;
    // 遍历帧缓冲区，处理所有接收到的帧
    while (offset < frame_buffer.size()) {
        // 指向当前处理的帧的指针
        unsigned char* frame = frame_buffer.data() + offset;
        // 提取负载长度，去除掩码位
        uint64_t payload_len = frame[1] & 0x7F; 
        // 掩码密钥的偏移量，初始为 2 字节
        int mask_offset = 2;
        // 根据负载长度的特殊值调整负载长度和掩码偏移量
        if (payload_len == 126) {
            // 16 位扩展负载长度
            payload_len = (frame[2] << 8) | frame[3];
            mask_offset = 4;
        } else if (payload_len == 127) {
            // 64 位扩展负载长度
            payload_len = 0;
            for (int i = 0; i < 8; ++i) {
                payload_len = (payload_len << 8) | frame[2 + i];
            }
            mask_offset = 10;
        }
        // 检查是否为关闭帧
        if ((frame[0] & 0xFF) == 0x88) {
            // 构造关闭响应帧
            std::vector<unsigned char> close_frame = {0x88, 0x00};
            // 发送关闭响应帧
            SSL_write(ctx.ssl, close_frame.data(), close_frame.size());
            // 清理客户端资源
            cleanup_client(ctx);
            return;
        }
        // 检查是否为 Ping 帧
        if ((frame[0] & 0xFF) == 0x89) {
            // 指向掩码密钥的指针
            unsigned char* mask = frame + mask_offset;
            // 指向负载数据的指针
            unsigned char* payload = frame + mask_offset + 4;
            // 构造 Pong 响应帧
            std::vector<unsigned char> pong_frame = {0x8A, static_cast<unsigned char>(payload_len)};
            // 若有负载数据，添加到 Pong 帧中
            if (payload_len > 0) {
                pong_frame.insert(pong_frame.end(), payload, payload + payload_len);
            }
            // 发送 Pong 响应帧
            SSL_write(ctx.ssl, pong_frame.data(), pong_frame.size());
            // 更新偏移量，跳过当前处理的帧
            offset += 2 + (payload_len < 126 ? 0 : (payload_len == 126 ? 2 : 8)) + 4 + payload_len;
            continue;
        }
        // 检查是否为文本帧且不分片
        if ((frame[0] & 0x0F) == 0x1) {
            // 指向掩码密钥的指针
            unsigned char* mask = frame + mask_offset;
            // 指向负载数据的指针
            unsigned char* payload = frame + mask_offset + 4;
            // 对负载数据进行掩码解密
            for (uint64_t i = 0; i < payload_len; ++i)
                payload[i] ^= mask[i % 4];
            // 将解密后的负载数据转换为字符串
            std::string msg((char*)payload, payload_len);
            // 打印接收到的消息
            std::cout << "收到: " << msg << std::endl;
            // 发送回显消息
            send_ws_frame(ctx.ssl, "Echo: " + msg);
        }
        // 更新偏移量，跳过当前处理的帧
        offset += 2 + (payload_len < 126 ? 0 : (payload_len == 126 ? 2 : 8)) + 4 + payload_len;
    }
}

/**
 * @brief 清理客户端相关资源
 * 
 * 该方法用于释放与指定客户端相关的资源，包括关闭 SSL 连接、释放 SSL 对象和关闭客户端套接字。
 * 
 * @param ctx 客户端上下文，包含客户端文件描述符、SSL 指针、客户端状态和握手缓冲区
 */
void WebSocketSSLServer::cleanup_client(ClientContext& ctx) {
    // 发起 SSL 连接的正常关闭过程，通知对端关闭 SSL 连接
    SSL_shutdown(ctx.ssl);
    // 释放 SSL 对象，回收相关的内存资源
    SSL_free(ctx.ssl);
    // 关闭客户端套接字，终止与客户端的网络连接
    close(ctx.fd);
}

/**
 * @brief 启动 WebSocket SSL 服务器并开始监听客户端连接
 * 
 * 该方法完成以下主要任务：
 * 1. 初始化 OpenSSL 库。
 * 2. 创建 SSL 上下文并加载证书和私钥。
 * 3. 创建并配置监听套接字。
 * 4. 使用 epoll 进行 I/O 多路复用。
 * 5. 进入主循环，处理新连接和客户端事件。
 */
void WebSocketSSLServer::run() {
    // 初始化 OpenSSL 库
    SSL_library_init();
    // 加载错误信息字符串，用于后续错误输出
    SSL_load_error_strings();
    // 添加所有可用的加密算法
    OpenSSL_add_all_algorithms();

    // 创建一个新的 SSL 上下文，使用 TLS 服务器方法
    ctx_ = SSL_CTX_new(TLS_server_method());
    // 从 PEM 文件中加载服务器证书
    if (SSL_CTX_use_certificate_file(ctx_, "server-cert.pem", SSL_FILETYPE_PEM) <= 0) {
        // 打印 OpenSSL 错误信息到标准错误输出
        ERR_print_errors_fp(stderr);
        return;
    }
    // 从 PEM 文件中加载服务器私钥
    if (SSL_CTX_use_PrivateKey_file(ctx_, "server-key.pem", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        return;
    }
    // 检查私钥和证书是否匹配
    if (!SSL_CTX_check_private_key(ctx_)) {
        std::cerr << "Private key does not match the public certificate" << std::endl;
        return;
    }

    // 创建一个 TCP 套接字
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ == -1) {
        // 打印套接字创建失败的错误信息
        perror("socket");
        return;
    }

    // 设置 SO_REUSEADDR 选项，允许地址重用
    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        close(server_fd_);
        return;
    }

    // 配置服务器地址结构
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    // 监听所有可用的网络接口
    addr.sin_addr.s_addr = INADDR_ANY;
    // 将端口号转换为网络字节序
    addr.sin_port = htons(port_);
    // 将套接字绑定到指定地址和端口
    if (bind(server_fd_, (sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(server_fd_);
        return;
    }
    // 开始监听客户端连接，最大连接队列长度为 1
    if (listen(server_fd_, 1) == -1) {
        perror("listen");
        close(server_fd_);
        return;
    }

    // 创建一个 epoll 实例
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        close(server_fd_);
        return;
    }
    // 定义 epoll 事件结构体
    epoll_event ev{}, events[64];
    // 监听读事件
    ev.events = EPOLLIN;
    ev.data.fd = server_fd_;
    // 将监听套接字添加到 epoll 实例中
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd_, &ev) == -1) {
        perror("epoll_ctl");
        close(server_fd_);
        close(epoll_fd);
        return;
    }

    // 存储客户端上下文的映射，键为客户端文件描述符
    std::map<int, ClientContext> clients;

    // 进入主循环，持续处理客户端事件
    while (true) {
        // 等待事件发生，-1 表示无限等待
        int n = epoll_wait(epoll_fd, events, 64, -1);
        if (n == -1) {
            perror("epoll_wait");
            continue;
        }
        // 处理所有发生的事件
        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            if (fd == server_fd_) {
                // 有新的客户端连接
                int client_fd = accept(server_fd_, nullptr, nullptr);
                if (client_fd == -1) {
                    perror("accept");
                    continue;
                }
                // 将客户端套接字设置为非阻塞模式
                if (set_non_blocking(client_fd) == -1) {
                    close(client_fd);
                    continue;
                }
                // 为新客户端创建一个 SSL 对象
                SSL* ssl = SSL_new(ctx_);
                if (!ssl) {
                    close(client_fd);
                    continue;
                }
                // 将 SSL 对象与客户端套接字关联
                SSL_set_fd(ssl, client_fd);
                // 将新客户端的上下文信息添加到映射中
                clients[client_fd] = {client_fd, ssl, ClientState::SSL_HANDSHAKE, ""};
                // 监听读事件，使用边缘触发模式
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = client_fd;
                // 将客户端套接字添加到 epoll 实例中
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
                    perror("epoll_ctl");
                    SSL_free(ssl);
                    close(client_fd);
                    clients.erase(client_fd);
                }
            } else {
                // 查找客户端上下文
                auto it = clients.find(fd);
                if (it == clients.end()) continue;
                auto& ctx = it->second;
                if (ctx.state == ClientState::SSL_HANDSHAKE) {
                    // 执行 SSL 握手
                    int ret = SSL_accept(ctx.ssl);
                    if (ret == 1) {
                        // SSL 握手成功，进入 WebSocket 握手阶段
                        ctx.state = ClientState::WS_HANDSHAKE;
                    } else {
                        // 获取 SSL 错误码
                        int err = SSL_get_error(ctx.ssl, ret);
                        if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
                            // 清理客户端资源
                            cleanup_client(ctx);
                            clients.erase(fd);
                        }
                    }
                } else if (ctx.state == ClientState::WS_HANDSHAKE) {
                    // 处理 WebSocket 握手
                    handle_client_handshake(ctx);
                } else if (ctx.state == ClientState::WS_CONNECTED) {
                    // 处理客户端消息
                    handle_client_message(ctx);
                }
            }
        }
    }
}

void WebSocketSSLServer::cleanup() {
    if (server_fd_ != -1) {
        close(server_fd_);
        server_fd_ = -1;
    }
    if (ctx_) {
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
    }
}

int main() {
    WebSocketSSLServer server(PORT);
    server.run();
    return 0;
}