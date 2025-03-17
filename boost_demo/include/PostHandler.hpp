// PostHandler.hpp
#ifndef POST_HANDLER_HPP
#define POST_HANDLER_HPP

#include "HttpHandler.hpp"
#include <fstream>
#include <iostream>

class PostHandler : public HttpHandler {
public:
    using HttpHandler::HttpHandler;

private:
    void read_request() override;
    void process_header();
    void read_body();
    void complete_upload();

    std::ofstream temp_file_;
    size_t content_length_ = 0;
    size_t received_ = 0;
};

#endif // POST_HANDLER_HPP
