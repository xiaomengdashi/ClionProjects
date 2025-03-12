#include <boost/asio.hpp>
#include <iostream>
#include <fstream>
#include <memory>
#include <filesystem>

#include "UrlDecode.hpp"


namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class HttpConnection : public std::enable_shared_from_this<HttpConnection> {
public:
    explicit HttpConnection(tcp::socket socket) : socket_(std::move(socket)) {}
    ~HttpConnection() {
        std::cout << "Connection closed" << std::endl;
    }

    void start() {
        socket_.set_option(tcp::no_delay(true));
        read_request();
    }

private:
    void read_request() {
        auto self(shared_from_this());
        asio::async_read_until(socket_, request_buffer_, "\r\n\r\n",
                               [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
                                   if (ec) return;

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
                                       handle_download(path);
                                   }
                                   else if (request_line.find("POST") != std::string::npos) {
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
                                       read_body();
                                   }
                                   else {
                                       send_response("HTTP/1.1 405 Method Not Allowed\r\n\r\n");
                                   }
                               });
    }

    void handle_download(const std::string& path) {
        UrlParser parser(path);

        // 获取文件参数
        std::string filename = parser.get_param("file");
        std::cout << "file_name: " << filename << std::endl;
        if (!filename.empty()) {
            // 处理真实文件下载
            download_real_file(filename);
            return;
        }

        // 获取虚拟文件大小参数
        std::string size_str = parser.get_param("size");
        if (!size_str.empty()) {
            try {
                size_t file_size = std::stoul(size_str);
                std::cout << "file_size: " << file_size << std::endl;
                download_virtual_file(file_size);
                return;
            } catch(...) {
                send_response("HTTP/1.1 400 Bad Request\r\n\r\n");
                return;
            }
        }

        send_response("HTTP/1.1 400 Bad Request\r\n\r\n");
    }

    void download_real_file(const std::string& file_path) {
        // 打开文件
        std::ifstream file(file_path, std::ios::binary | std::ios::ate);
        if (!file) {
            send_response("HTTP/1.1 404 Not Found\r\n\r\n");
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
                          [this, self = shared_from_this(), file_size, file = std::move(file)]
                                  (boost::system::error_code ec, std::size_t) mutable {
                              if (!ec) {
                                  // 分块发送文件内容
                                  send_file_chunk(std::move(file), file_size);
                              }
                          });
    }

    void download_virtual_file(const size_t file_size) {
        // 解析文件大小
        file_size_ = file_size;
        try {
            if (file_size_ > 1024 * 1024 * 1024) { // 限制最大1GB
                send_response("HTTP/1.1 413 Payload Too Large\r\n\r\n");
                return;
            }
        } catch (...) {
            send_response("HTTP/1.1 400 Bad Request\r\n\r\n");
            return;
        }

        // 构造HTTP头
        std::ostringstream header;
        header << "HTTP/1.1 200 OK\r\n"
               << "Content-Type: application/octet-stream\r\n"
               << "Content-Length: " << file_size_ << "\r\n"
               << "Content-Disposition: attachment; filename=\"virtual_file.bin\"\r\n\r\n";
        auto header_str = std::make_shared<std::string>(header.str());
        auto header_len = header_str->size();

        // 生成首个数据分片
        size_t chunk_size = 1 * 1024 - header_len;  // 默认头小于1KB，避免分片问题
        size_t first_chunk = std::min(chunk_size, file_size_);
        auto data = std::make_shared<std::vector<char>>(first_chunk, 'a');

        // 合并头和首块数据
        std::vector<asio::const_buffer> buffers{
                asio::buffer(*header_str),
                asio::buffer(*data)
        };

        asio::async_write(socket_, buffers,
                          [this, self = shared_from_this(), header_str, data, sent_bytes = first_chunk]
                                  (boost::system::error_code ec, std::size_t) {
                              if (!ec && sent_bytes < file_size_) {
                                  send_virtual_data(sent_bytes); // 继续发送剩余分片
                              } else if (ec) {
                                  graceful_shutdown();
                              }
                          });
    }

    void send_file_chunk(std::ifstream file, std::streamsize file_size) {
        auto self(shared_from_this());
        constexpr size_t chunk_size = 1 * 1024; // 1MB分块

        // 动态分配缓冲区
        auto buffer = std::make_shared<std::vector<char>>(chunk_size);

        file.read(buffer->data(), chunk_size);
        size_t bytes_read = file.gcount();

        asio::async_write(socket_, asio::buffer(buffer->data(), bytes_read),
                          [this, self, file = std::move(file), file_size, buffer]
                                  (boost::system::error_code ec, std::size_t) mutable {
                              if (ec || file.eof()) {
                                  output_file_.close();
                                  socket_.shutdown(asio::socket_base::shutdown_send);
                                  // 传输完成或出错
                                  return;
                              }

                              // 继续发送下一块
                              send_file_chunk(std::move(file), file_size);
                          });
    }

    void send_virtual_data(size_t sent_bytes) {
        auto self(shared_from_this());
        constexpr size_t chunk_size = 1 * 1024;

        size_t remaining = file_size_ - sent_bytes;
        auto data = std::make_shared<std::vector<char>>(std::min(chunk_size, remaining), 'a');

        asio::async_write(socket_, asio::buffer(*data),
                          [this, self, data, sent_bytes](boost::system::error_code ec, std::size_t bytes_transferred) {
                              if (!ec && (sent_bytes + bytes_transferred) < file_size_) {
                                  send_virtual_data(sent_bytes + bytes_transferred);
                              } else {
                                  graceful_shutdown();
                              }
                          });
    }

    void read_body() {
        auto self(shared_from_this());

        /***
        streambuf 三部曲:
            prepare(N)：准备接收数据的缓冲区（输入序列）
            async_read_some：将数据写入输入序列
            commit(N)：将输入序列的数据转移到可读序列（消费序列）
            data(): 获取已提交的缓冲区内容
         ***/
        // 增大缓冲区大小
        socket_.async_read_some(request_buffer_.prepare( 1024),
                                [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
                                    if (ec) {
                                        // 处理错误
                                        output_file_.close();
                                        std::cerr << "Error during file upload: " << ec.message() << std::endl;
                                        return;
                                    }

                                    // 提交读取到的数据
                                    request_buffer_.commit(bytes_transferred);


                                    // 打印最后10个字节
                                    const auto* data = static_cast<const char*>(request_buffer_.data().data());
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
                                        send_response("HTTP/1.1 200 OK\r\n\r\nFile uploaded successfully");
                                        std::cout << "File upload completed: " << temp_file_path_ << std::endl;
                                        return;
                                    }

                                    // 继续读取
                                    read_body();
                                });
    }

    void send_response(const std::string& response) {
        auto self(shared_from_this());
        asio::async_write(socket_, asio::buffer(response),
                          [this, self](boost::system::error_code ec, std::size_t) {
                              if (!ec) {
                                  // 关闭连接
                                  graceful_shutdown();
                              }
                          });
    }

    // 服务端优雅关闭示例
    void graceful_shutdown() {
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

private:
    tcp::socket socket_;
    asio::streambuf request_buffer_;
    std::ofstream output_file_;
    std::filesystem::path temp_file_path_;
    size_t content_length_ = 0;
    size_t received_bytes_ = 0;
    size_t file_size_ = 0;
    std::string dummy_buffer_;
};

class HttpServer {
public:
    HttpServer(asio::io_context& io_context, short port)
            : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
                [this](boost::system::error_code ec, tcp::socket socket) {
                    if (!ec) {
                        std::make_shared<HttpConnection>(std::move(socket))->start();
                    }
                    do_accept();
                });
    }

    tcp::acceptor acceptor_;
};

int main(int argc, char* argv[]) {
    try {

        asio::io_context io_context;
        HttpServer server(io_context, 8080);
        io_context.run();
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}