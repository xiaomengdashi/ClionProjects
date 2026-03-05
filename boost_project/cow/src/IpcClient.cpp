#include "IpcClient.h"

#include <boost/asio.hpp>
#include <boost/asio/local/stream_protocol.hpp>

#include <iterator>
#include <stdexcept>

IpcClient::IpcClient(const std::string& socket_path)
    : socket_path_(socket_path) {}

std::string IpcClient::sendCommand(const std::string& cmd) {
    boost::asio::io_context ioc;
    boost::asio::local::stream_protocol::socket socket(ioc);

    boost::system::error_code ec;
    socket.connect(boost::asio::local::stream_protocol::endpoint(socket_path_), ec);
    if (ec)
        throw std::runtime_error("Cannot connect to cow daemon: " + ec.message()
                                 + "\n(Is cow running? Try: cow start)");

    // 发送命令（以 '\n' 结尾）
    std::string request = cmd + "\n";
    boost::asio::write(socket, boost::asio::buffer(request), ec);
    if (ec)
        throw std::runtime_error("Write error: " + ec.message());

    // 读到 EOF（服务端关闭连接）
    boost::asio::streambuf buf;
    boost::asio::read(socket, buf, boost::asio::transfer_all(), ec);
    // EOF 是正常情况，其他错误才抛出
    if (ec && ec != boost::asio::error::eof)
        throw std::runtime_error("Read error: " + ec.message());

    std::istream is(&buf);
    return std::string(std::istreambuf_iterator<char>(is), {});
}
