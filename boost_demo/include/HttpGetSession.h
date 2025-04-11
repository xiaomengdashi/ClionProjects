#ifndef BOOST_DEMO_HTTPGETSESSION_H
#define BOOST_DEMO_HTTPGETSESSION_H

#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include "UrlDecode.hpp"

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class HttpGetSession : public std::enable_shared_from_this<HttpGetSession> {
public:
    explicit HttpGetSession(tcp::socket socket);
    ~HttpGetSession();

    void Start();

private:
    void ReadRequest();
    void HandleDownload(const std::string &path);
    void SendResponse(const std::string &response);
    void GracefulShutdown();

private:
    tcp::socket socket_;
    asio::streambuf request_buffer_;
    std::string dummy_buffer_;
};

#endif //BOOST_DEMO_HTTPGETSESSION_H