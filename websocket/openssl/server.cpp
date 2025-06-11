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
    char buf[4096] = {0};
    int len = SSL_read(ctx.ssl, buf, sizeof(buf));
    if (len <= 0) {
        int err = SSL_get_error(ctx.ssl, len);
        if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
            cleanup_client(ctx);
        }
        return;
    }
    ctx.handshake_buffer.append(buf, len);

    // 解析 Sec-WebSocket-Key
    std::istringstream iss(ctx.handshake_buffer);
    std::string line, ws_key;
    while (std::getline(iss, line)) {
        if (line.find("Sec-WebSocket-Key:") != std::string::npos) {
            ws_key = line.substr(line.find(":") + 2);
            ws_key.erase(std::remove(ws_key.begin(), ws_key.end(), '\r'), ws_key.end());
            break;
        }
    }
    if (ws_key.empty()) {
        return;
    }

    // 发送握手响应
    std::string accept_key = websocket_accept_key(ws_key);
    std::ostringstream oss;
    oss << "HTTP/1.1 101 Switching Protocols\r\n"
        << "Upgrade: websocket\r\n"
        << "Connection: Upgrade\r\n"
        << "Sec-WebSocket-Accept: " << accept_key << "\r\n\r\n";
    SSL_write(ctx.ssl, oss.str().c_str(), oss.str().size());

    // 发送欢迎消息
    send_ws_frame(ctx.ssl, "Hello from OpenSSL WebSocket!");

    ctx.state = ClientState::WS_CONNECTED;
}

void WebSocketSSLServer::handle_client_message(ClientContext& ctx) {
    std::vector<unsigned char> frame_buffer;
    const size_t buffer_size = 4096;
    unsigned char buffer[buffer_size];
    int n;
    do {
        n = SSL_read(ctx.ssl, buffer, buffer_size);
        if (n <= 0) {
            int err = SSL_get_error(ctx.ssl, n);
            if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
                cleanup_client(ctx);
            }
            return;
        }
        frame_buffer.insert(frame_buffer.end(), buffer, buffer + n);
    } while (n == buffer_size);

    size_t offset = 0;
    while (offset < frame_buffer.size()) {
        unsigned char* frame = frame_buffer.data() + offset;
        uint64_t payload_len = frame[1] & 0x7F; // 提升 payload_len 作用域
        int mask_offset = 2;
        if (payload_len == 126) {
            payload_len = (frame[2] << 8) | frame[3];
            mask_offset = 4;
        } else if (payload_len == 127) {
            payload_len = 0;
            for (int i = 0; i < 8; ++i) {
                payload_len = (payload_len << 8) | frame[2 + i];
            }
            mask_offset = 10;
        }
        // 检查关闭帧
        if ((frame[0] & 0xFF) == 0x88) {
            // 发送关闭响应
            std::vector<unsigned char> close_frame = {0x88, 0x00};
            SSL_write(ctx.ssl, close_frame.data(), close_frame.size());
            cleanup_client(ctx);
            return;
        }
        // 检查 Ping 帧
        if ((frame[0] & 0xFF) == 0x89) {
            unsigned char* mask = frame + mask_offset;
            unsigned char* payload = frame + mask_offset + 4;
            std::vector<unsigned char> pong_frame = {0x8A, static_cast<unsigned char>(payload_len)};
            if (payload_len > 0) {
                pong_frame.insert(pong_frame.end(), payload, payload + payload_len);
            }
            SSL_write(ctx.ssl, pong_frame.data(), pong_frame.size());
            offset += 2 + (payload_len < 126 ? 0 : (payload_len == 126 ? 2 : 8)) + 4 + payload_len;
            continue;
        }
        // 只处理文本帧且不分片
        if ((frame[0] & 0x0F) == 0x1) {
            unsigned char* mask = frame + mask_offset;
            unsigned char* payload = frame + mask_offset + 4;
            for (uint64_t i = 0; i < payload_len; ++i)
                payload[i] ^= mask[i % 4];
            std::string msg((char*)payload, payload_len);
            std::cout << "收到: " << msg << std::endl;
            send_ws_frame(ctx.ssl, "Echo: " + msg);
        }
        offset += 2 + (payload_len < 126 ? 0 : (payload_len == 126 ? 2 : 8)) + 4 + payload_len;
    }
}

void WebSocketSSLServer::cleanup_client(ClientContext& ctx) {
    SSL_shutdown(ctx.ssl);
    SSL_free(ctx.ssl);
    close(ctx.fd);
}

void WebSocketSSLServer::run() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    ctx_ = SSL_CTX_new(TLS_server_method());
    if (SSL_CTX_use_certificate_file(ctx_, "server-cert.pem", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        return;
    }
    if (SSL_CTX_use_PrivateKey_file(ctx_, "server-key.pem", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        return;
    }
    if (!SSL_CTX_check_private_key(ctx_)) {
        std::cerr << "Private key does not match the public certificate" << std::endl;
        return;
    }

    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ == -1) {
        perror("socket");
        return;
    }

    // 设置 SO_REUSEADDR 选项
    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        close(server_fd_);
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);
    if (bind(server_fd_, (sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(server_fd_);
        return;
    }
    if (listen(server_fd_, 1) == -1) {
        perror("listen");
        close(server_fd_);
        return;
    }

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        close(server_fd_);
        return;
    }
    epoll_event ev{}, events[64];
    ev.events = EPOLLIN;
    ev.data.fd = server_fd_;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd_, &ev) == -1) {
        perror("epoll_ctl");
        close(server_fd_);
        close(epoll_fd);
        return;
    }

    std::map<int, ClientContext> clients;

    while (true) {
        int n = epoll_wait(epoll_fd, events, 64, -1);
        if (n == -1) {
            perror("epoll_wait");
            continue;
        }
        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            if (fd == server_fd_) {
                // 新连接
                int client_fd = accept(server_fd_, nullptr, nullptr);
                if (client_fd == -1) {
                    perror("accept");
                    continue;
                }
                if (set_non_blocking(client_fd) == -1) {
                    close(client_fd);
                    continue;
                }
                SSL* ssl = SSL_new(ctx_);
                if (!ssl) {
                    close(client_fd);
                    continue;
                }
                SSL_set_fd(ssl, client_fd);
                clients[client_fd] = {client_fd, ssl, ClientState::SSL_HANDSHAKE, ""};
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
                    perror("epoll_ctl");
                    SSL_free(ssl);
                    close(client_fd);
                    clients.erase(client_fd);
                }
            } else {
                auto it = clients.find(fd);
                if (it == clients.end()) continue;
                auto& ctx = it->second;
                if (ctx.state == ClientState::SSL_HANDSHAKE) {
                    int ret = SSL_accept(ctx.ssl);
                    if (ret == 1) {
                        ctx.state = ClientState::WS_HANDSHAKE;
                    } else {
                        int err = SSL_get_error(ctx.ssl, ret);
                        if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
                            cleanup_client(ctx);
                            clients.erase(fd);
                        }
                    }
                } else if (ctx.state == ClientState::WS_HANDSHAKE) {
                    handle_client_handshake(ctx);
                } else if (ctx.state == ClientState::WS_CONNECTED) {
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