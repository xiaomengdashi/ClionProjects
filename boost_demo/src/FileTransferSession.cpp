#include "FileTransferSession.h"

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

FileTransferSession::FileTransferSession(tcp::socket socket) : socket_(std::move(socket)) {}

FileTransferSession::~FileTransferSession() {
    std::cout << "File transfer session closed" << std::endl;
}

void FileTransferSession::StartDownloadRealFile(const std::string &file_path) {
    // 打开文件
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file) {
        SendResponse("HTTP/1.1 404 Not Found\r\n\r\n");
        return;
    }

    // 获取文件大小
    auto file_size = file.tellg();
    file.seekg(0);

    // 发送HTTP头
    std::ostringstream header;
    header << "HTTP/1.1 200 OK\r\n"
           << "Content-Type: application/octet-stream\r\n"
           << "Content-Length: " << file_size << "\r\n\r\n";

    asio::async_write(socket_, asio::buffer(header.str()),
                      [this, self = shared_from_this(), file_size,
                       file = std::move(file)](boost::system::error_code ec,
                                              std::size_t) mutable {
                          if (!ec) {
                              // 分块发送文件内容
                              SendFileChunk(std::move(file), file_size);
                          }
                      });
}

void FileTransferSession::StartDownloadVirtualFile(const size_t file_size) {
    // 解析文件大小
    file_size_ = file_size;
    try {
        if (file_size_ > 1024 * 1024 * 1024) { // 限制最大1GB
            SendResponse("HTTP/1.1 413 Payload Too Large\r\n\r\n");
            return;
        }
    } catch (...) {
        SendResponse("HTTP/1.1 400 Bad Request\r\n\r\n");
        return;
    }

    // 构造HTTP头
    std::ostringstream header;
    header << "HTTP/1.1 200 OK\r\n"
           << "Content-Type: application/octet-stream\r\n"
           << "Content-Length: " << file_size_ << "\r\n"
           << "Content-Disposition: attachment; "
              "filename=\"virtual_file.bin\"\r\n\r\n";
    auto header_str = std::make_shared<std::string>(header.str());
    auto header_len = header_str->size();

    // 生成首个数据分片
    size_t chunk_size = 1 * 1024 - header_len; // 默认头小于1KB，避免分片问题
    size_t first_chunk = std::min(chunk_size, file_size_);
    auto data = std::make_shared<std::vector<char>>(first_chunk, 'a');

    // 合并头和首块数据
    std::vector<asio::const_buffer> buffers{asio::buffer(*header_str),
                                            asio::buffer(*data)};

    asio::async_write(socket_, buffers,
                      [this, self = shared_from_this(), header_str, data,
                       sent_bytes = first_chunk](boost::system::error_code ec, std::size_t) {
                          if (!ec && sent_bytes < file_size_) {
                              SendVirtualData(sent_bytes); // 继续发送剩余分片
                          } else if (ec) {
                              GracefulShutdown();
                          }
                      });
}

void FileTransferSession::SendFileChunk(std::ifstream file, std::streamsize file_size) {
    auto self(shared_from_this());
    constexpr size_t chunk_size = 1 * 1024; // 1MB分块

    // 动态分配缓冲区
    auto buffer = std::make_shared<std::vector<char>>(chunk_size);

    file.read(buffer->data(), chunk_size);
    size_t bytes_read = file.gcount();

    asio::async_write(socket_, asio::buffer(buffer->data(), bytes_read),
                      [this, self, file = std::move(file), file_size, buffer](
                          boost::system::error_code ec, std::size_t) mutable {
                          if (ec || file.eof()) {
                              output_file_.close();
                              socket_.shutdown(asio::socket_base::shutdown_send);
                              // 传输完成或出错
                              return;
                          }

                          // 继续发送下一块
                          SendFileChunk(std::move(file), file_size);
                      });
}

void FileTransferSession::SendVirtualData(size_t sent_bytes) {
    auto self(shared_from_this());
    constexpr size_t chunk_size = 1 * 1024;

    size_t remaining = file_size_ - sent_bytes;
    auto data =
        std::make_shared<std::vector<char>>(std::min(chunk_size, remaining), 'a');

    asio::async_write(socket_, asio::buffer(*data),
                      [this, self, data, sent_bytes](boost::system::error_code ec,
                                                   std::size_t bytes_transferred) {
                          if (!ec && (sent_bytes + bytes_transferred) < file_size_) {
                              SendVirtualData(sent_bytes + bytes_transferred);
                          } else {
                              GracefulShutdown();
                          }
                      });
}

void FileTransferSession::SendResponse(const std::string &response) {
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
void FileTransferSession::GracefulShutdown() {
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