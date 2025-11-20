#include "HttpClientFactory.h"
#include "HttpClient.h"
#include "UrlParser.h"

std::shared_ptr<HttpClient<tcp::socket>>
HttpClientFactory::CreateHttpClient(std::shared_ptr<asio::io_context> io_context) {
    if (!io_context) {
        io_context = std::make_shared<asio::io_context>();
    }
    return std::make_shared<HttpClient<tcp::socket>>(io_context);
}

std::shared_ptr<HttpClient<ssl::stream<tcp::socket>>>
HttpClientFactory::CreateHttpsClient(std::shared_ptr<asio::io_context> io_context) {
    if (!io_context) {
        io_context = std::make_shared<asio::io_context>();
    }
    return std::make_shared<HttpClient<ssl::stream<tcp::socket>>>(io_context);
}

bool HttpClientFactory::IsHttpsUrl(const std::string& url) {
    HttpRequest req = UrlParser::ParseUrl(url);
    return req.IsHttps();
}
