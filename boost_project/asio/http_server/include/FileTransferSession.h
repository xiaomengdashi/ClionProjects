#ifndef BOOST_DEMO_FILETRANSFERSESSION_H
#define BOOST_DEMO_FILETRANSFERSESSION_H

#include <boost/asio.hpp>
#include <fstream>
#include <iostream>
#include <memory>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class FileTransferSession : public std::enable_shared_from_this<FileTransferSession> {
public:
    explicit FileTransferSession(tcp::socket socket);
    ~FileTransferSession();

    void StartDownloadRealFile(const std::string &file_path);
    void StartDownloadVirtualFile(const size_t file_size);

private:
    void SendFileChunk(std::ifstream file, std::streamsize file_size);
    void SendVirtualData(size_t sent_bytes);
    void SendResponse(const std::string &response);
    void GracefulShutdown();

private:
    tcp::socket socket_;
    asio::streambuf request_buffer_;
    std::ofstream output_file_;
    size_t file_size_ = 0;
    std::string dummy_buffer_;
};

#endif //BOOST_DEMO_FILETRANSFERSESSION_H