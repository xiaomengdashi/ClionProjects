#include "HttpSessionBase.h"

HttpSessionBase::HttpSessionBase(tcp::socket socket)
    : socket_(std::move(socket)) {}

HttpSessionBase::~HttpSessionBase() {}

void HttpSessionBase::Start() {
    socket_.set_option(tcp::no_delay(true));
    HandleRequest();
}

void HttpSessionBase::SendResponse(const std::string &response) {
    auto self(shared_from_this());
    asio::async_write(socket_, asio::buffer(response),
                      [this, self](boost::system::error_code ec, std::size_t) {
                          if (!ec) {
                              // 关闭连接
                              GracefulShutdown();
                          }
                      });
}

void HttpSessionBase::GracefulShutdown() {
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
        }
    );
}