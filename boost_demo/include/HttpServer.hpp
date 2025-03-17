// HttpServer.hpp
#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include <iostream>
#include <boost/asio.hpp>
#include "HttpHandler.hpp"


class HttpServer {
public:
    HttpServer(boost::asio::io_context& io, short port);

private:
    void accept_connection();
    std::shared_ptr<HttpHandler> create_handler(tcp::socket socket, const std::string& method);

    class TempHandler;
    boost::asio::ip::tcp::acceptor acceptor_;
};

#endif // HTTP_SERVER_HPP
