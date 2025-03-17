#ifndef GET_HANDLER_HPP
#define GET_HANDLER_HPP

#include "HttpHandler.hpp"
#include "UrlDecode.hpp"

class GetHandler : public HttpHandler {
public:
    using HttpHandler::HttpHandler;

private:
    void read_request() override;
    void process_request();
    void handle_virtual(const std::string& size_str);
    void send_data(size_t remaining);
};

#endif // GET_HANDLER_HPP
