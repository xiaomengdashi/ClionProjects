#include <boost/asio.hpp>
#include <iostream>
#include <fstream>
#include <memory>
#include <filesystem>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class HttpConnection : public std::enable_shared_from_this<HttpConnection> {
public:
    HttpConnection(tcp::socket socket) : socket_(std::move(socket)) {}

    void start() {
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
                                       // ... 原有POST处理代码保持不变 ...
                                   }
                                   else {
                                       send_response("HTTP/1.1 405 Method Not Allowed\r\n\r\n");
                                   }
                               });
    }

// 新增文件下载处理方法
    void handle_download(const std::string& path) {
        // 简单路径解析（示例）
        size_t file_param = path.find("file=");
        if (file_param == std::string::npos) {
            send_response("HTTP/1.1 400 Bad Request\r\n\r\n");
            return;
        }

        std::string filename = path.substr(file_param + 5);
        std::filesystem::path file_path(filename);

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
                          [this, self = shared_from_this(), file = std::move(file), file_size]
                                  (boost::system::error_code ec, std::size_t) mutable {
                              if (!ec) {
                                  // 分块发送文件内容
                                  send_file_chunk(std::move(file), file_size);
                              }
                          });
    }

// 新增分块发送方法
    void send_file_chunk(std::ifstream file, std::streamsize file_size) {
        auto self(shared_from_this());
        constexpr size_t chunk_size = 1 * 1024 * 1024; // 1MB分块

        // 动态分配缓冲区
        auto buffer = std::make_shared<std::vector<char>>(chunk_size);

        file.read(buffer->data(), chunk_size);
        size_t bytes_read = file.gcount();

        asio::async_write(socket_, asio::buffer(buffer->data(), bytes_read),
                          [this, self, file = std::move(file), file_size, buffer, bytes_read]
                                  (boost::system::error_code ec, std::size_t) mutable {
                              if (ec || file.eof()) {
                                  // 传输完成或出错
                                  return;
                              }

                              // 继续发送下一块
                              send_file_chunk(std::move(file), file_size);
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
                                  boost::system::error_code ignored_ec;
                                  socket_.shutdown(tcp::socket::shutdown_both, ignored_ec);
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