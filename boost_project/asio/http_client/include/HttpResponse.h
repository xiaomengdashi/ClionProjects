#ifndef HTTP_CLIENT_HTTP_RESPONSE_H
#define HTTP_CLIENT_HTTP_RESPONSE_H

#include <string>
#include <map>
#include <chrono>

class HttpResponse {
public:
    HttpResponse() = default;

    // Getters
    int GetStatusCode() const { return status_code_; }
    const std::map<std::string, std::string>& GetHeaders() const { return headers_; }
    const std::string& GetBody() const { return body_; }
    const std::string& GetErrorMessage() const { return error_message_; }
    long long GetResponseTimeMs() const { return response_time_ms_; }

    // Setters
    void SetStatusCode(int code) { status_code_ = code; }
    void SetHeader(const std::string& key, const std::string& value);
    void SetHeaders(const std::map<std::string, std::string>& headers);
    void SetBody(const std::string& body) { body_ = body; }
    void AppendBody(const std::string& data) { body_ += data; }
    void SetErrorMessage(const std::string& msg) { error_message_ = msg; }
    void SetResponseTimeMs(long long ms) { response_time_ms_ = ms; }

    // Helper methods
    bool IsSuccess() const { return status_code_ >= 200 && status_code_ < 300; }
    bool IsRedirect() const { return status_code_ >= 300 && status_code_ < 400; }
    bool IsClientError() const { return status_code_ >= 400 && status_code_ < 500; }
    bool IsServerError() const { return status_code_ >= 500; }
    bool HasError() const { return !error_message_.empty(); }

    std::string GetHeader(const std::string& key) const;

private:
    int status_code_ = 0;
    std::map<std::string, std::string> headers_;
    std::string body_;
    std::string error_message_;
    long long response_time_ms_ = 0;
};

#endif // HTTP_CLIENT_HTTP_RESPONSE_H
