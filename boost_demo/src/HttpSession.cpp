#include "HttpSession.h"
#include "HttpGetSession.h"
#include "HttpPostSession.h"

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

HttpSession::HttpSession(tcp::socket socket) : socket_(std::move(socket)) {}

HttpSession::~HttpSession() {
    std::cout << "Connection closed" << std::endl;
}

void HttpSession::Start() {
    socket_.set_option(tcp::no_delay(true));
    ReadRequestHeader();
}

void HttpSession::ReadRequestHeader() {
    auto self(shared_from_this());
    asio::async_read_until(
        socket_, request_buffer_, "\r\n\r\n",
        [this, self](boost::system::error_code ec,
               std::size_t bytes_transferred) {
            if (ec)
                return;

            std::istream request_stream(&request_buffer_);
            std::string request_line;
            std::getline(request_stream, request_line);

            // 解析请求方法和路径
            size_t start = request_line.find(' ') + 1;
            size_t end = request_line.find(' ', start);
            std::string path = request_line.substr(start, end - start);

            // 解析Content-Length
            std::string header;
            size_t content_length = 0;
            while (std::getline(request_stream, header) && header != "\r") {
                if (header.find("Content-Length:") != std::string::npos) {
                    content_length = std::stoul(header.substr(16));
                    break;
                }
            }

            // 根据请求方法创建子会话
            if (request_line.find("POST") != std::string::npos) {
                auto post_session = std::make_shared<HttpPostSession>(std::move(socket_), content_length);
                post_session->Start();
            } else if (request_line.find("GET") != std::string::npos) {
                auto get_session = std::make_shared<HttpGetSession>(std::move(socket_), path);
                get_session->Start();
            } else {
                SendResponse("HTTP/1.1 405 Method Not Allowed\r\n\r\n");
            }
        });
}

void HttpSession::SendResponse(const std::string &response) {
    auto self(shared_from_this());
    asio::async_write(socket_, asio::buffer(response),
                      [this, self](boost::system::error_code ec, std::size_t) {
                          if (!ec) {
                              // 关闭连接
                              GracefulShutdown();
                          }
                      });
}

// 服务端优雅关闭示例
void HttpSession::GracefulShutdown() {
    auto self(shared_from_this());

    // 1. 发送FIN（停止写入）
    boost::system::error_code ec;
    socket_.shutdown(tcp::socket::shutdown_send, ec);
    if (ec) {
        std::cerr << "Shutdown error: " << ec.message() << std::endl;
        return;
    }

    // 2. 继续读取客户端数据（直到收到FIN）
    asio::async_read(socket_, asio::dynamic_buffer(dummy_buffer_),
                     [this, self](boost::system::error_code ec, size_t) {
                         std::cout << "Received data" << std::endl;
                         if (ec == asio::error::eof) {
                             // 3. 安全关闭连接
                             socket_.close();
                         } else if (ec) {
                             std::cerr << "Read error: " << ec.message() << std::endl;
                         }
                     });
}