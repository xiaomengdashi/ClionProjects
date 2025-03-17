// PostHandler.cpp
#include "../include/PostHandler.hpp"
#include <algorithm>
#include <iterator>

void PostHandler::read_request() {
    auto self(shared_from_this());
    asio::async_read_until(socket_, request_buffer_, "\r\n\r\n",
                           [this, self](boost::system::error_code ec, size_t bytes) {
                               if (ec) return;
                               process_header();
                           });
}

void PostHandler::process_header() {
    std::istream stream(&request_buffer_);
    std::string header;
    while (std::getline(stream, header) && header != "\r") {
        if (header.find("Content-Length:") != std::string::npos) {
            content_length_ = std::stoul(header.substr(16));
            break;
        }
    }

    temp_file_.open("upload.tmp", std::ios::binary);
    received_ = request_buffer_.size();

    if (received_ > 0) {
        std::copy(
                std::istreambuf_iterator<char>(&request_buffer_),
                std::istreambuf_iterator<char>(),
                std::ostreambuf_iterator<char>(temp_file_)
        );
    }

    read_body();
}

void PostHandler::read_body() {
    auto self(shared_from_this());
    socket_.async_read_some(request_buffer_.prepare(1024),
                            [this, self](auto ec, size_t bytes) {
                                if (ec) {
                                    temp_file_.close();
                                    return;
                                }

                                request_buffer_.commit(bytes);
                                received_ += bytes;

                                std::copy(
                                        std::istreambuf_iterator<char>(&request_buffer_),
                                        std::istreambuf_iterator<char>(),
                                        std::ostreambuf_iterator<char>(temp_file_)
                                );

                                if (received_ >= content_length_) complete_upload();
                                else read_body();
                            });
}

void PostHandler::complete_upload() {
    temp_file_.close();
    send_response("HTTP/1.1 200 OK\r\n\r\nUpload successful");
    std::cout << "Received " << received_ << " bytes\n";
}
