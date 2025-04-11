#ifndef BOOST_DEMO_HTTPPOSTSESSION_H
#define BOOST_DEMO_HTTPPOSTSESSION_H

#include <boost/asio.hpp>
#include <fstream>
#include <iostream>
#include <memory>
#include <filesystem>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class HttpPostSession : public std::enable_shared_from_this<HttpPostSession> {
public:
    explicit HttpPostSession(tcp::socket socket);
    ~HttpPostSession();

    void Start();

private:
    void ReadRequest();
    void ReadBody();
    void SendResponse(const std::string &response);
    void GracefulShutdown();

private:
    tcp::socket socket_;
    asio::streambuf request_buffer_;
    std::ofstream output_file_;
    std::filesystem::path temp_file_path_;
    size_t content_length_ = 0;
    size_t received_bytes_ = 0;
    std::string dummy_buffer_;
};

#endif //BOOST_DEMO_HTTPPOSTSESSION_H