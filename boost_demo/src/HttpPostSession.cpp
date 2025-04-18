#include "HttpPostSession.h"

HttpPostSession::HttpPostSession(tcp::socket socket, std::size_t content_length)
    : HttpSessionBase(std::move(socket)), content_length_(content_length) {}

HttpPostSession::~HttpPostSession() {
    std::cout << "POST session closed" << std::endl;
}

void HttpPostSession::HandleRequest() {
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
}

void HttpPostSession::ReadBody() {
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
        }
    );
}