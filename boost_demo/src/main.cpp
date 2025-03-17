#include "../include/HttpServer.hpp"
#include <boost/asio.hpp>
#include <iostream>

int main() {
    try {
        boost::asio::io_context io;
        HttpServer server(io, 8080);
        io.run();
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << "\n";
    }
    return 0;
}
