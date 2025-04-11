#include "HttpServer.h"

HttpServer::HttpServer(asio::io_context &io_context, short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
  DoAccept();
}

void HttpServer::DoAccept() {
  acceptor_.async_accept(
      [this](boost::system::error_code ec, tcp::socket socket) {
        if (!ec) {
          std::make_shared<HttpSession>(std::move(socket))->Start();
        }
        DoAccept();
      }
    );
}