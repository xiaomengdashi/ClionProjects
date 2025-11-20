#ifndef HTTP_CLIENT_HTTP_REQUEST_H
#define HTTP_CLIENT_HTTP_REQUEST_H

#include <string>
#include <map>
#include <memory>

class HttpRequest {
public:
    enum class Method {
        GET,
        POST,
        PUT,
        DELETE,
        HEAD,
        OPTIONS,
        PATCH
    };

    HttpRequest() = default;
    explicit HttpRequest(const std::string& url);

    // Getters
    Method GetMethod() const { return method_; }
    const std::string& GetUrl() const { return url_; }
    const std::string& GetHost() const { return host_; }
    const std::string& GetPort() const { return port_; }
    const std::string& GetPath() const { return path_; }
    const std::map<std::string, std::string>& GetHeaders() const { return headers_; }
    const std::string& GetBody() const { return body_; }
    int GetTimeoutMs() const { return timeout_ms_; }
    bool IsHttps() const { return is_https_; }

    // Setters
    HttpRequest& SetMethod(Method m) { method_ = m; return *this; }
    HttpRequest& SetUrl(const std::string& url) { url_ = url; return *this; }
    HttpRequest& SetHeader(const std::string& key, const std::string& value);
    HttpRequest& SetHeaders(const std::map<std::string, std::string>& headers);
    HttpRequest& SetBody(const std::string& body) { body_ = body; return *this; }
    HttpRequest& SetTimeoutMs(int timeout) { timeout_ms_ = timeout; return *this; }

    // Get HTTP method string
    std::string GetMethodString() const;

    // Build HTTP request header
    std::string BuildRequestHeader() const;

private:
    Method method_ = Method::GET;
    std::string url_;
    std::string host_;
    std::string port_;
    std::string path_;
    std::map<std::string, std::string> headers_;
    std::string body_;
    int timeout_ms_ = 5000;
    bool is_https_ = false;

    friend class UrlParser;
};

#endif // HTTP_CLIENT_HTTP_REQUEST_H
