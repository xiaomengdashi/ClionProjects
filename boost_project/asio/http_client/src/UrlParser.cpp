#include "UrlParser.h"
#include <regex>
#include <sstream>

HttpRequest UrlParser::ParseUrl(const std::string& url) {
    HttpRequest request;
    request.url_ = url;

    // Extract scheme (http or https)
    std::string scheme = ExtractScheme(url);
    request.is_https_ = (scheme == "https");

    // Extract host
    request.host_ = ExtractHost(url);

    // Extract port (with default based on scheme)
    request.port_ = ExtractPort(url, request.is_https_);

    // Extract path (with query string)
    request.path_ = ExtractPath(url);

    // Set default headers
    request.SetHeader("Accept", "*/*");
    request.SetHeader("Accept-Encoding", "identity");

    return request;
}

std::string UrlParser::ExtractScheme(const std::string& url) {
    size_t pos = url.find("://");
    if (pos != std::string::npos) {
        return url.substr(0, pos);
    }
    return "http";
}

std::string UrlParser::ExtractHost(const std::string& url) {
    // Find start of host (after scheme://)
    size_t start = url.find("://");
    if (start == std::string::npos) {
        start = 0;
    } else {
        start += 3;
    }

    // Find end of host (before port, path, or end of string)
    size_t end = start;
    while (end < url.length() && url[end] != ':' && url[end] != '/') {
        end++;
    }

    return url.substr(start, end - start);
}

std::string UrlParser::ExtractPort(const std::string& url, bool is_https) {
    // Find start of host
    size_t start = url.find("://");
    if (start == std::string::npos) {
        start = 0;
    } else {
        start += 3;
    }

    // Look for port number after host
    size_t host_end = start;
    while (host_end < url.length() && url[host_end] != ':' && url[host_end] != '/') {
        host_end++;
    }

    // Check if there's a port
    if (host_end < url.length() && url[host_end] == ':') {
        size_t port_end = host_end + 1;
        while (port_end < url.length() && isdigit(url[port_end])) {
            port_end++;
        }
        if (port_end > host_end + 1) {
            return url.substr(host_end + 1, port_end - host_end - 1);
        }
    }

    // Return default port
    return GetDefaultPort(is_https);
}

std::string UrlParser::ExtractPath(const std::string& url) {
    // Find start of host
    size_t start = url.find("://");
    if (start == std::string::npos) {
        start = 0;
    } else {
        start += 3;
    }

    // Skip host and port
    size_t path_start = start;
    while (path_start < url.length() && url[path_start] != '/') {
        path_start++;
    }

    // Extract path (including query string and fragment)
    if (path_start < url.length()) {
        return url.substr(path_start);
    }

    // Default to root path
    return "/";
}

std::string UrlParser::GetDefaultPort(bool is_https) {
    return is_https ? "443" : "80";
}
