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

                                   // 解析请求头
                                   std::istream request_stream(&request_buffer_);
                                   std::string request_line;
                                   std::getline(request_stream, request_line);

                                   // 检查是否为POST请求
                                   if (request_line.find("POST") != std::string::npos) {
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
                                   } else {
                                       send_response("HTTP/1.1 405 Method Not Allowed\r\n\r\n");
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