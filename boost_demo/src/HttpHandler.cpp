#include "../include/HttpHandler.hpp"

HttpHandler::HttpHandler(tcp::socket socket) : socket_(std::move(socket)) {}

void HttpHandler::start() {
    socket_.set_option(tcp::no_delay(true));
    read_request();
}

void HttpHandler::send_response(const std::string& response) {
    auto self(shared_from_this());
    asio::async_write(socket_, asio::buffer(response),
                      [this, self](boost::system::error_code ec, std::size_t) {
                          if (!ec) shutdown();
                      });
}

void HttpHandler::shutdown() {
    boost::system::error_code ec;
    socket_.shutdown(tcp::socket::shutdown_send, ec);
}
