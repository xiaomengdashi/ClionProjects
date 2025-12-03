// HttpGetSession.h
#ifndef BOOST_DEMO_HTTPGETSESSION_H
#define BOOST_DEMO_HTTPGETSESSION_H

#include "HttpSessionBase.h"
#include "FileTransferSession.h"
#include "UrlDecode.hpp"

class HttpGetSession : public HttpSessionBase {
public:
    explicit HttpGetSession(tcp::socket socket, const std::string &path);
    ~HttpGetSession() override;

protected:
    void HandleRequest() override;

private:
    void HandleDownload(const std::string &path);
    void HandleFileDownload(const std::string &filename);
    void HandleVirtualFileDownload(size_t file_size);
    void HandleRootPath();

private:
    std::string path_;
};

#endif // BOOST_DEMO_HTTPGETSESSION_H