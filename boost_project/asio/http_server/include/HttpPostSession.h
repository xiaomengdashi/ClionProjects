// HttpPostSession.h
#ifndef BOOST_DEMO_HTTPPOSTSESSION_H
#define BOOST_DEMO_HTTPPOSTSESSION_H

#include "HttpSessionBase.h"
#include <fstream>
#include <filesystem>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class HttpPostSession : public HttpSessionBase {
public:
    explicit HttpPostSession(tcp::socket socket, std::size_t content_length);
    ~HttpPostSession();

protected:
    void HandleRequest() override;

private:
    void ReadBody();

private:
    std::ofstream output_file_;
    std::filesystem::path temp_file_path_;
    size_t content_length_ = 0;
    size_t received_bytes_ = 0;
};

#endif // BOOST_DEMO_HTTPPOSTSESSION_H