#include "../include/GetHandler.hpp"
#include <iostream>
#include <sstream>
#include <vector>

void GetHandler::read_request() {
    auto self(shared_from_this());
    asio::async_read_until(socket_, request_buffer_, "\r\n\r\n",
                           [this, self](boost::system::error_code ec, size_t) {
                               if (ec) return;
                               process_request();
                           });
}

void GetHandler::process_request() {

}
void GetHandler::handle_virtual(const std::string& size_str) {

}

void GetHandler::send_data(size_t remaining) {

}