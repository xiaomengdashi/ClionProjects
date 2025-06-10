#include <openssl/ssl.h>
#include <openssl/err.h>
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
#include <random>

#define PORT 8080
#define HOST "127.0.0.1"

// base64编码
std::string base64_encode(const unsigned char* input, int length) {
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

// 生成随机 Sec-WebSocket-Key
std::string random_key() {
    std::random_device rd;
    std::uniform_int_distribution<int> dist(0, 255);
    unsigned char key[16];
    for (int i = 0; i < 16; ++i) key[i] = dist(rd);
    return base64_encode(key, 16);
}

// 发送 WebSocket 帧（文本，客户端需加掩码）
void send_ws_frame(SSL* ssl, const std::string& msg) {
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
    SSL_write(ssl, frame.data(), frame.size());
}

// 读取 WebSocket 文本帧
std::string recv_ws_frame(SSL* ssl) {
    unsigned char frame[2048] = {0};
    int n = SSL_read(ssl, frame, sizeof(frame));
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

int main() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    SSL* ssl = SSL_new(ctx);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, HOST, &serv_addr.sin_addr);
    connect(sockfd, (sockaddr*)&serv_addr, sizeof(serv_addr));

    SSL_set_fd(ssl, sockfd);
    if (SSL_connect(ssl) <= 0) {
        std::cerr << "SSL connect failed" << std::endl;
        SSL_free(ssl);
        close(sockfd);
        return 1;
    }

    // 发送 WebSocket 握手
    std::string ws_key = random_key();
    std::ostringstream oss;
    oss << "GET / HTTP/1.1\r\n"
        << "Host: " << HOST << ":" << PORT << "\r\n"
        << "Upgrade: websocket\r\n"
        << "Connection: Upgrade\r\n"
        << "Sec-WebSocket-Key: " << ws_key << "\r\n"
        << "Sec-WebSocket-Version: 13\r\n"
        << "\r\n";
    std::string handshake = oss.str();
    SSL_write(ssl, handshake.c_str(), handshake.size());

    // 读取握手响应
    char buf[4096] = {0};
    int len = SSL_read(ssl, buf, sizeof(buf));
    std::string response(buf, len);
    if (response.find("101") == std::string::npos) {
        std::cerr << "WebSocket handshake failed" << std::endl;
        SSL_free(ssl);
        close(sockfd);
        return 1;
    }
    std::cout << "WebSocket 握手成功！" << std::endl;

    // 读取欢迎消息
    std::string msg = recv_ws_frame(ssl);
    if (!msg.empty()) std::cout << "收到: " << msg << std::endl;

    // 简单交互
    while (true) {
        std::cout << "输入消息(exit退出): ";
        std::string input;
        std::getline(std::cin, input);
        if (input == "exit") break;
        send_ws_frame(ssl, input);
        std::string reply = recv_ws_frame(ssl);
        if (!reply.empty()) std::cout << "收到: " << reply << std::endl;
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(sockfd);
    SSL_CTX_free(ctx);
    return 0;
}
