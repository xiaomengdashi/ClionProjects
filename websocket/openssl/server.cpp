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

#define PORT 8080

class WebSocketSSLServer {
public:
    WebSocketSSLServer(int port) : port_(port), ctx_(nullptr), server_fd_(-1) {}
    ~WebSocketSSLServer() { cleanup(); }
    void run();

private:
    std::string base64_encode(const unsigned char* input, int length);
    std::string websocket_accept_key(const std::string& key);
    void send_ws_frame(SSL* ssl, const std::string& msg);
    void handle_client(SSL* ssl, int client_fd);
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

std::string WebSocketSSLServer::websocket_accept_key(const std::string& key) {
    std::string GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string accept_src = key + GUID;
    unsigned char sha1_result[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(accept_src.c_str()), accept_src.size(), sha1_result);
    return base64_encode(sha1_result, SHA_DIGEST_LENGTH);
}

void WebSocketSSLServer::send_ws_frame(SSL* ssl, const std::string& msg) {
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

void WebSocketSSLServer::handle_client(SSL* ssl, int client_fd) {
    // 读取 HTTP 握手
    char buf[4096] = {0};
    int len = SSL_read(ssl, buf, sizeof(buf));
    std::string request(buf, len);

    // 解析 Sec-WebSocket-Key
    std::istringstream iss(request);
    std::string line, ws_key;
    while (std::getline(iss, line)) {
        if (line.find("Sec-WebSocket-Key:") != std::string::npos) {
            ws_key = line.substr(line.find(":") + 2);
            ws_key.erase(std::remove(ws_key.begin(), ws_key.end(), '\r'), ws_key.end());
            break;
        }
    }
    if (ws_key.empty()) {
        SSL_free(ssl);
        close(client_fd);
        return;
    }

    // 发送握手响应
    std::string accept_key = websocket_accept_key(ws_key);
    std::ostringstream oss;
    oss << "HTTP/1.1 101 Switching Protocols\r\n"
        << "Upgrade: websocket\r\n"
        << "Connection: Upgrade\r\n"
        << "Sec-WebSocket-Accept: " << accept_key << "\r\n\r\n";
    SSL_write(ssl, oss.str().c_str(), oss.str().size());

    // 发送欢迎消息
    send_ws_frame(ssl, "Hello from OpenSSL WebSocket!");

    // 简单回显循环
    while (true) {
        unsigned char frame[2048] = {0};
        int n = SSL_read(ssl, frame, sizeof(frame));
        if (n <= 0) break;
        // 只处理文本帧且不分片
        if ((frame[0] & 0x0F) == 0x1) {
            int payload_len = frame[1] & 0x7F;
            int mask_offset = 2;
            if (payload_len == 126) mask_offset = 4;
            else if (payload_len == 127) mask_offset = 10;
            unsigned char* mask = frame + mask_offset;
            unsigned char* payload = frame + mask_offset + 4;
            for (int i = 0; i < payload_len; ++i)
                payload[i] ^= mask[i % 4];
            std::string msg((char*)payload, payload_len);
            std::cout << "收到: " << msg << std::endl;
            send_ws_frame(ssl, "Echo: " + msg);
        }
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(client_fd);
}

void WebSocketSSLServer::run() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    ctx_ = SSL_CTX_new(TLS_server_method());
    SSL_CTX_use_certificate_file(ctx_, "server-cert.pem", SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(ctx_, "server-key.pem", SSL_FILETYPE_PEM);

    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);
    bind(server_fd_, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd_, 1);

    std::cout << "OpenSSL WebSocket server on wss://localhost:" << port_ << std::endl;

    while (true) {
        int client_fd = accept(server_fd_, nullptr, nullptr);
        SSL* ssl = SSL_new(ctx_);
        SSL_set_fd(ssl, client_fd);
        if (SSL_accept(ssl) <= 0) {
            SSL_free(ssl);
            close(client_fd);
            continue;
        }
        handle_client(ssl, client_fd);
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