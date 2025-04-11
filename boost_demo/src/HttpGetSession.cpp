#include "HttpGetSession.h"
#include "FileTransferSession.h"

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

HttpGetSession::HttpGetSession(tcp::socket socket) : socket_(std::move(socket)) {}

HttpGetSession::~HttpGetSession() {
    std::cout << "GET session closed" << std::endl;
}

void HttpGetSession::Start() {
    socket_.set_option(tcp::no_delay(true));
    ReadRequest();
}

void HttpGetSession::ReadRequest() {
    auto self(shared_from_this());
    asio::async_read_until(
        socket_, request_buffer_, "\r\n\r\n",
        [this, self](boost::system::error_code ec,
                     std::size_t bytes_transferred) {
            if (ec)
                return;

            std::istream request_stream(&request_buffer_);
            std::string request_line;
            std::getline(request_stream, request_line);

            // 解析请求路径（示例路径：/download?file=filename）
            size_t start = request_line.find(' ') + 1;
            size_t end = request_line.find(' ', start);
            std::string path = request_line.substr(start, end - start);

            // 处理文件下载
            HandleDownload(path);
        });
}

void HttpGetSession::HandleDownload(const std::string &path) {
    UrlParser parser(path);

    // 获取文件参数
    std::string filename = parser.GetParam("file");
    std::cout << "file_name: " << filename << std::endl;
    if (!filename.empty()) {
        // 处理真实文件下载
        auto file_transfer_session = std::make_shared<FileTransferSession>(std::move(socket_));
        file_transfer_session->StartDownloadRealFile(filename);
        return;
    }

    // 获取虚拟文件大小参数
    std::string size_str = parser.GetParam("size");
    if (!size_str.empty()) {
        try {
            size_t file_size = std::stoul(size_str);
            std::cout << "file_size: " << file_size << std::endl;
            auto file_transfer_session = std::make_shared<FileTransferSession>(std::move(socket_));
            file_transfer_session->StartDownloadVirtualFile(file_size);
            return;
        } catch (...) {
            SendResponse("HTTP/1.1 400 Bad Request\r\n\r\n");
            return;
        }
    }

    SendResponse("HTTP/1.1 400 Bad Request\r\n\r\n");
}

void HttpGetSession::SendResponse(const std::string &response) {
    auto self(shared_from_this());
    asio::async_write(socket_, asio::buffer(response),
                      [this, self](boost::system::error_code ec, std::size_t) {
                          if (!ec) {
                              // 关闭连接
                              GracefulShutdown();
                          }
                      });
}

// 服务端优雅关闭示例
void HttpGetSession::GracefulShutdown() {
    auto self(shared_from_this());

    // 1. 发送FIN（停止写入）
    boost::system::error_code ec;
    socket_.shutdown(tcp::socket::shutdown_send, ec);
    if (ec) {
        std::cerr << "Shutdown error: " << ec.message() << std::endl;
        return;
    }

    // 2. 继续读取客户端数据（直到收到FIN）
    asio::async_read(socket_, asio::dynamic_buffer(dummy_buffer_),
                     [this, self](boost::system::error_code ec, size_t) {
                         std::cout << "Received data" << std::endl;
                         if (ec == asio::error::eof) {
                             // 3. 安全关闭连接
                             socket_.close();
                         } else if (ec) {
                             std::cerr << "Read error: " << ec.message() << std::endl;
                         }
                     });
}