#ifndef BOOST_DEMO_HTTPSESSION_H
#define BOOST_DEMO_HTTPSESSION_H

#include <boost/asio.hpp>
#include <iostream>
#include <memory>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
    explicit HttpSession(tcp::socket socket);
    ~HttpSession();
    void Start();

private:
    void ReadRequestHeader();
    void SendResponse(const std::string &response);
    void GracefulShutdown();

private:
    tcp::socket socket_;
    asio::streambuf request_buffer_;
    std::string dummy_buffer_;
};

#endif //BOOST_DEMO_HTTPSESSION_H