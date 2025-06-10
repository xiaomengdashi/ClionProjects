#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <iostream>
#include <string>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = net::ip::tcp;

int main()
{
    try
    {
        std::string host = "localhost";
        std::string port = "8080";

        net::io_context ioc;
        ssl::context ctx{ssl::context::tlsv12_client};

        // 1. 设置为验证对端
        ctx.set_verify_mode(ssl::verify_peer);

        // 2. 加载 CA 证书（或自签名证书）
        ctx.load_verify_file("server-cert.pem");

        // 解析域名
        tcp::resolver resolver{ioc};
        auto const results = resolver.resolve(host, port);

        // 建立SSL流
        beast::ssl_stream<tcp::socket> stream{ioc, ctx};

        // 连接服务器
        net::connect(stream.next_layer(), results.begin(), results.end());

        // SSL握手
        stream.handshake(ssl::stream_base::client);

        // 3. 可选：设置主机名校验（C++17 及以上）
        SSL_set1_host(stream.native_handle(), host.c_str());

        // WebSocket握手
        websocket::stream<beast::ssl_stream<tcp::socket>> ws{std::move(stream)};
        ws.handshake(host + ":" + port, "/");

        std::cout << "已连接到 wss://" << host << ":" << port << std::endl;

        for (;;)
        {
            std::cout << "输入要发送的消息 (exit 退出): ";
            std::string msg;
            std::getline(std::cin, msg);
            if (msg == "exit") break;

            ws.write(net::buffer(msg));

            beast::flat_buffer buffer;
            ws.read(buffer);
            std::cout << "收到服务器消息: " << beast::make_printable(buffer.data()) << std::endl;
        }

        ws.close(websocket::close_code::normal);
    }
    catch (std::exception const& e)
    {
        std::cerr << "错误: " << e.what() << std::endl;
    }
}