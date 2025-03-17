#ifndef HTTP_HANDLER_HPP
#define HTTP_HANDLER_HPP

#include <boost/asio.hpp>
#include <memory>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class HttpHandler : public std::enable_shared_from_this<HttpHandler> {
public:
    explicit HttpHandler(tcp::socket socket);
    virtual ~HttpHandler() = default;

    void start();

protected:
    virtual void read_request() = 0;
    void send_response(const std::string& response);
    void shutdown();

    tcp::socket socket_;
    asio::streambuf request_buffer_;
};

#endif // HTTP_HANDLER_HPP
