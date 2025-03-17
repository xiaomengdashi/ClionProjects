// HttpServer.cpp
#include "../include/HttpServer.hpp"
#include "../include/GetHandler.hpp"
#include "../include/PostHandler.hpp"

class HttpServer::TempHandler : public HttpHandler {
public:
    TempHandler(tcp::socket socket, HttpServer& server)
            : HttpHandler(std::move(socket)), server_(server) {}

    void read_request() override {
        asio::async_read_until(socket_, request_buffer_, ' ',
                               [this, self = shared_from_this()](boost::system::error_code ec, size_t) {
                                   if (ec) return;

                                   std::istream stream(&request_buffer_);
                                   std::string method;
                                   stream >> method;

                                   server_.create_handler(std::move(socket_), method)->start();
                               });
    }

private:
    HttpServer& server_;
};

HttpServer::HttpServer(boost::asio::io_context& io, short port)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)) {
    accept_connection();
}

void HttpServer::accept_connection() {
    acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<TempHandler>(std::move(socket), *this)->start();
                }
                accept_connection();
            });
}

std::shared_ptr<HttpHandler> HttpServer::create_handler(tcp::socket socket, const std::string& method) {
    std::cout << "Method: " << method << "\n";

    if (method == "POST") {
        return std::make_shared<PostHandler>(std::move(socket));
    }
    return std::make_shared<GetHandler>(std::move(socket));
}
