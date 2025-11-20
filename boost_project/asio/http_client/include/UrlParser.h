#ifndef HTTP_CLIENT_URL_PARSER_H
#define HTTP_CLIENT_URL_PARSER_H

#include "HttpRequest.h"
#include <string>

class UrlParser {
public:
    static HttpRequest ParseUrl(const std::string& url);

private:
    static std::string ExtractScheme(const std::string& url);
    static std::string ExtractHost(const std::string& url);
    static std::string ExtractPort(const std::string& url, bool is_https);
    static std::string ExtractPath(const std::string& url);
    static std::string GetDefaultPort(bool is_https);
};

#endif // HTTP_CLIENT_URL_PARSER_H
