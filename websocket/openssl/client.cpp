#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <random>

#define PORT 8080
#define HOST "127.0.0.1"

class WebSocketSSLClient {
public:
    WebSocketSSLClient(const std::string& host, int port)
        : host_(host), port_(port), ctx_(nullptr), ssl_(nullptr), sockfd_(-1) {}
    ~WebSocketSSLClient() { cleanup(); }

    bool connect_server();
    void run();

private:
    std::string base64_encode(const unsigned char* input, int length);
    std::string random_key();
    void send_ws_frame(const std::string& msg);
    std::string recv_ws_frame();
    void cleanup();

    std::string host_;
    int port_;
    SSL_CTX* ctx_;
    SSL* ssl_;
    int sockfd_;
};

std::string WebSocketSSLClient::base64_encode(const unsigned char* input, int length) {
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

std::string WebSocketSSLClient::random_key() {
    std::random_device rd;
    std::uniform_int_distribution<int> dist(0, 255);
    unsigned char key[16];
    for (int i = 0; i < 16; ++i) key[i] = dist(rd);
    return base64_encode(key, 16);
}

void WebSocketSSLClient::send_ws_frame(const std::string& msg) {
    std::vector<unsigned char> frame;
    frame.push_back(0x81); // FIN + text
    size_t len = msg.size();
    if (len < 126) {
        frame.push_back(0x80 | len); // mask bit + len
    } else if (len < 65536) {
        frame.push_back(0x80 | 126);
        frame.push_back((len >> 8) & 0xFF);
        frame.push_back(len & 0xFF);
    } else {
        frame.push_back(0x80 | 127);
        for (int i = 7; i >= 0; --i)
            frame.push_back((len >> (8 * i)) & 0xFF);
    }
    // 生成掩码
    unsigned char mask[4];
    for (int i = 0; i < 4; ++i) mask[i] = rand() % 256;
    frame.insert(frame.end(), mask, mask + 4);
    // mask payload
    for (size_t i = 0; i < len; ++i)
        frame.push_back(msg[i] ^ mask[i % 4]);
    SSL_write(ssl_, frame.data(), frame.size());
}

std::string WebSocketSSLClient::recv_ws_frame() {
    unsigned char frame[2048] = {0};
    int n = SSL_read(ssl_, frame, sizeof(frame));
    if (n <= 0) return "";
    if ((frame[0] & 0x0F) == 0x1) { // text frame
        int payload_len = frame[1] & 0x7F;
        int offset = 2;
        if (payload_len == 126) {
            payload_len = (frame[2] << 8) | frame[3];
            offset = 4;
        } else if (payload_len == 127) {
            // 不处理超长帧
            return "";
        }
        std::string msg((char*)frame + offset, payload_len);
        return msg;
    }
    return "";
}

bool WebSocketSSLClient::connect_server() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    ctx_ = SSL_CTX_new(TLS_client_method());
    
    // 加载 CA 证书，用于验证服务器证书
    if (SSL_CTX_load_verify_locations(ctx_, "ca.pem", nullptr) != 1) {
        std::cerr << "Failed to load CA certificate" << std::endl;
        return false;
    }
    
    // 开启证书验证
    SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);

    ssl_ = SSL_new(ctx_);

    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port_);
    inet_pton(AF_INET, host_.c_str(), &serv_addr.sin_addr);
    if (::connect(sockfd_, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "TCP connect failed" << std::endl;
        return false;
    }

    SSL_set_fd(ssl_, sockfd_);
    if (SSL_connect(ssl_) != 1) {
        int err = SSL_get_error(ssl_, SSL_connect(ssl_));
        std::cerr << "SSL connect failed, error code: " << err << std::endl;
        ERR_print_errors_fp(stderr);
        return false;
    }

    // 发送 WebSocket 握手
    std::string ws_key = random_key();
    std::ostringstream oss;
    oss << "GET / HTTP/1.1\r\n"
        << "Host: " << host_ << ":" << port_ << "\r\n"
        << "Upgrade: websocket\r\n"
        << "Connection: Upgrade\r\n"
        << "Sec-WebSocket-Key: " << ws_key << "\r\n"
        << "Sec-WebSocket-Version: 13\r\n"
        << "\r\n";
    std::string handshake = oss.str();
    SSL_write(ssl_, handshake.c_str(), handshake.size());

    // 读取握手响应
    char buf[4096] = {0};
    int len = SSL_read(ssl_, buf, sizeof(buf));
    std::string response(buf, len);
    if (response.find("101") == std::string::npos) {
        std::cerr << "WebSocket handshake failed" << std::endl;
        return false;
    }
    std::cout << "WebSocket 握手成功！" << std::endl;

    // 读取欢迎消息
    std::string msg = recv_ws_frame();
    if (!msg.empty()) std::cout << "收到: " << msg << std::endl;
    return true;
}

void WebSocketSSLClient::run() {
    if (!connect_server()) {
        cleanup();
        return;
    }
    // 简单交互
    while (true) {
        std::cout << "输入消息(exit退出): ";
        std::string input;
        std::getline(std::cin, input);
        if (input == "exit") break;
        send_ws_frame(input);
        std::string reply = recv_ws_frame();
        if (!reply.empty()) std::cout << "收到: " << reply << std::endl;
    }
    cleanup();
}

void WebSocketSSLClient::cleanup() {
    if (ssl_) {
        SSL_shutdown(ssl_);
        SSL_free(ssl_);
        ssl_ = nullptr;
    }
    if (sockfd_ != -1) {
        close(sockfd_);
        sockfd_ = -1;
    }
    if (ctx_) {
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
    }
}

int main() {
    WebSocketSSLClient client(HOST, PORT);
    client.run();
    return 0;
}
