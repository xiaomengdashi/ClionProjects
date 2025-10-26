#ifndef BOOST_DEMO_HTTPSESSIONBASE_H
#define BOOST_DEMO_HTTPSESSIONBASE_H

#include <boost/asio.hpp>
#include <iostream>
#include <memory>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class HttpSessionBase : public std::enable_shared_from_this<HttpSessionBase> {
public:
    explicit HttpSessionBase(tcp::socket socket);
    virtual ~HttpSessionBase();

    void Start();

protected:
    virtual void HandleRequest() = 0;
    void SendResponse(const std::string &response);
    void GracefulShutdown();

protected:
    tcp::socket socket_;
    asio::streambuf request_buffer_;
    std::string dummy_buffer_;
};

#endif // BOOST_DEMO_HTTPSESSIONBASE_H

