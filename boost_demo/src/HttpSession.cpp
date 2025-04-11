#include "HttpSession.h"
#include "FileTransferSession.h"

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

HttpSession::HttpSession(tcp::socket socket) : socket_(std::move(socket)) {}

HttpSession::~HttpSession() {
    std::cout << "Connection closed" << std::endl;
}

void HttpSession::Start() {
    socket_.set_option(tcp::no_delay(true));
    ReadRequest();
}

void HttpSession::ReadRequest() {
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

            // 新增GET请求处理分支
            if (request_line.find("GET") != std::string::npos) {
                // 解析请求路径（示例路径：/download?file=filename）
                size_t start = request_line.find(' ') + 1;
                size_t end = request_line.find(' ', start);
                std::string path = request_line.substr(start, end - start);

                // 处理文件下载
                HandleDownload(path);
            } else if (request_line.find("POST") != std::string::npos) {
                // 解析Content-Length
                std::string header;
                while (std::getline(request_stream, header) && header != "\r") {
                    if (header.find("Content-Length:") != std::string::npos) {
                        content_length_ = std::stoul(header.substr(16));
                        std::cout << content_length_ << std::endl;
                        break;
                    }
                }

                // 创建临时文件
                temp_file_path_ = "uploaded_file.tmp";
                output_file_.open(temp_file_path_, std::ios::binary);

                // 处理缓冲区现有数据
                const size_t preloaded = request_buffer_.size();
                if (preloaded > 0) {
                    // 立即处理已读取的body数据
                    received_bytes_ += preloaded;
                    output_file_ << &request_buffer_; // 直接写入文件
                }

                // 读取剩余数据
                ReadBody();
            } else {
                SendResponse("HTTP/1.1 405 Method Not Allowed\r\n\r\n");
            }
        });
}

void HttpSession::HandleDownload(const std::string &path) {
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

void HttpSession::ReadBody() {
    auto self(shared_from_this());

    /***
    streambuf 三部曲:
        prepare(N)：准备接收数据的缓冲区（输入序列）
        async_read_some：将数据写入输入序列
        commit(N)：将输入序列的数据转移到可读序列（消费序列）
        data(): 获取已提交的缓冲区内容
        ***/
    // 增大缓冲区大小
    socket_.async_read_some(
        request_buffer_.prepare(1024),
        [this, self](boost::system::error_code ec,
                     std::size_t bytes_transferred) {
            if (ec) {
                // 处理错误
                output_file_.close();
                std::cerr << "Error during file upload: " << ec.message()
                          << std::endl;
                return;
            }

            // 提交读取到的数据
            request_buffer_.commit(bytes_transferred);

            // 打印最后10个字节
            const auto *data =
                static_cast<const char *>(request_buffer_.data().data());
            size_t buffer_size = request_buffer_.size();
            if (buffer_size >= 10) {
                std::string last_10_bytes(data + buffer_size - 10, 10);
                std::cout << "Last 10 bytes: " << last_10_bytes << std::endl;
            }

            // 将数据写入文件
            std::ostream output_stream(&request_buffer_);
            std::copy(std::istreambuf_iterator<char>(&request_buffer_),
                      std::istreambuf_iterator<char>(),
                      std::ostreambuf_iterator<char>(output_file_));

            received_bytes_ += bytes_transferred;

            std::cout << "Received bytes: " << received_bytes_ << std::endl;

            // 检查是否已经接收完所有数据
            if (received_bytes_ >= content_length_) {
                output_file_.close();
                SendResponse("HTTP/1.1 200 OK\r\n\r\nFile uploaded successfully");
                std::cout << "File upload completed: " << temp_file_path_
                          << std::endl;
                return;
            }

            // 继续读取
            ReadBody();
        });
}

void HttpSession::SendResponse(const std::string &response) {
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
void HttpSession::GracefulShutdown() {
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