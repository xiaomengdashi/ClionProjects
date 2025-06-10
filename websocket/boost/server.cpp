#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

// 加载证书
void load_server_certificate(ssl::context& ctx)
{
    ctx.set_options(
        ssl::context::default_workarounds |
        ssl::context::no_sslv2 |
        ssl::context::single_dh_use);

    ctx.use_certificate_chain_file("server-cert.pem");
    ctx.use_private_key_file("server-key.pem", ssl::context::pem);
}

class session : public std::enable_shared_from_this<session>
{
    websocket::stream<beast::ssl_stream<tcp::socket>> ws_;
    beast::flat_buffer buffer_;

public:
    explicit session(tcp::socket socket, ssl::context& ctx)
        : ws_(std::move(socket), ctx) {}

    void run()
    {
        auto self = shared_from_this();
        ws_.next_layer().async_handshake(ssl::stream_base::server,
            [self](beast::error_code ec) {
                if (!ec) self->do_accept();
            });
    }

    void do_accept()
    {
        auto self = shared_from_this();
        ws_.async_accept([self](beast::error_code ec) {
            if (!ec) self->do_read();
        });
    }

    void do_read()
    {
        auto self = shared_from_this();
        ws_.async_read(buffer_, [self](beast::error_code ec, std::size_t) {
            if (!ec) {
                // 打印读取的内容
                std::cout << "read: " << beast::buffers_to_string(self->buffer_.data()) << std::endl;
                self->do_write();
            }
        });
    }

    void do_write()
    {
        auto self = shared_from_this();
        ws_.text(ws_.got_text());
        ws_.async_write(buffer_.data(), [self](beast::error_code ec, std::size_t) {
            if (!ec) {
                self->buffer_.consume(self->buffer_.size());
                self->do_read();
            }
        });
    }
};

class listener : public std::enable_shared_from_this<listener>
{
    net::io_context& ioc_;
    ssl::context& ctx_;
    tcp::acceptor acceptor_;

public:
    listener(net::io_context& ioc, ssl::context& ctx, tcp::endpoint endpoint)
        : ioc_(ioc), ctx_(ctx), acceptor_(ioc)
    {
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(net::socket_base::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen();
    }

    void run() { do_accept(); }

    void do_accept()
    {
        acceptor_.async_accept([self = shared_from_this()](beast::error_code ec, tcp::socket socket) {
            if (!ec)
                std::make_shared<session>(std::move(socket), self->ctx_)->run();
            self->do_accept();
        });
    }
};

int main()
{
    try
    {
        net::io_context ioc{1};
        ssl::context ctx{ssl::context::tlsv12};

        load_server_certificate(ctx);

        auto endpoint = tcp::endpoint{tcp::v4(), 8080};
        std::make_shared<listener>(ioc, ctx, endpoint)->run();

        std::cout << "WSS server started on port 8080\n";
        ioc.run();
    }
    catch (std::exception const& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}