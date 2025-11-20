#include "HttpRequest.h"
#include <sstream>

HttpRequest::HttpRequest(const std::string& url) : url_(url) {
    // URL parsing will be done by UrlParser
}

HttpRequest& HttpRequest::SetHeader(const std::string& key, const std::string& value) {
    headers_[key] = value;
    return *this;
}

HttpRequest& HttpRequest::SetHeaders(const std::map<std::string, std::string>& headers) {
    headers_ = headers;
    return *this;
}

std::string HttpRequest::GetMethodString() const {
    switch (method_) {
        case Method::GET:
            return "GET";
        case Method::POST:
            return "POST";
        case Method::PUT:
            return "PUT";
        case Method::DELETE:
            return "DELETE";
        case Method::HEAD:
            return "HEAD";
        case Method::OPTIONS:
            return "OPTIONS";
        case Method::PATCH:
            return "PATCH";
        default:
            return "GET";
    }
}

std::string HttpRequest::BuildRequestHeader() const {
    std::ostringstream oss;

    // Request line
    oss << GetMethodString() << " " << path_ << " HTTP/1.1\r\n";

    // Host header (required for HTTP/1.1)
    oss << "Host: " << host_;
    // Add port if not default
    if ((is_https_ && port_ != "443") || (!is_https_ && port_ != "80")) {
        oss << ":" << port_;
    }
    oss << "\r\n";

    // Default headers
    oss << "Connection: close\r\n";
    oss << "User-Agent: Boost-ASIO-HttpClient/1.0\r\n";

    // Content-Length for POST/PUT
    if (!body_.empty()) {
        oss << "Content-Length: " << body_.length() << "\r\n";
        if (headers_.find("Content-Type") == headers_.end()) {
            oss << "Content-Type: application/octet-stream\r\n";
        }
    }

    // Custom headers
    for (const auto& [key, value] : headers_) {
        oss << key << ": " << value << "\r\n";
    }

    oss << "\r\n";

    return oss.str();
}
