#include "HttpGetSession.h"

HttpGetSession::HttpGetSession(tcp::socket socket, const std::string &path)
    : HttpSessionBase(std::move(socket)), path_(path) {}

HttpGetSession::~HttpGetSession() {
    std::cout << "GET session closed" << std::endl;
}

void HttpGetSession::HandleRequest() {
    HandleDownload(path_);
}

void HttpGetSession::HandleDownload(const std::string &path) {
    UrlParser parser(path);

    // 获取文件参数
    std::string filename = parser.GetParam("file");

    // 获取虚拟文件大小参数
    std::string size_str = parser.GetParam("size");

    if (!filename.empty()) {
        // 处理真实文件下载
        HandleFileDownload(filename);
    } else if (!size_str.empty()) {
        try {
            size_t file_size = std::stoul(size_str);
            HandleVirtualFileDownload(file_size);
        } catch (...) {
            SendResponse("HTTP/1.1 400 Bad Request\r\n\r\n");
        }
    } else {
        SendResponse("HTTP/1.1 400 Bad Request\r\n\r\n");
    }
}

void HttpGetSession::HandleFileDownload(const std::string &filename) {
    auto file_transfer_session = std::make_shared<FileTransferSession>(std::move(socket_));
    file_transfer_session->StartDownloadRealFile(filename);
}

void HttpGetSession::HandleVirtualFileDownload(size_t file_size) {
    auto file_transfer_session = std::make_shared<FileTransferSession>(std::move(socket_));
    file_transfer_session->StartDownloadVirtualFile(file_size);
}