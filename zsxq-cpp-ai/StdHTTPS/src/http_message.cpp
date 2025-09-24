/**
 * @file http_message.cpp
 * @brief HTTP消息类实现
 * @details 实现HTTP请求和响应消息的封装功能
 */

#include "http_message.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace stdhttps {

// HttpMessage实现
HttpMessage::HttpMessage(bool is_response) 
    : is_response_(is_response), version_(1, 1) {
}

HttpMessage::HttpMessage(const HttpMessage& other)
    : is_response_(other.is_response_)
    , version_(other.version_)
    , headers_(other.headers_)
    , body_(other.body_) {
}

HttpMessage::HttpMessage(HttpMessage&& other) noexcept
    : is_response_(other.is_response_)
    , version_(std::move(other.version_))
    , headers_(std::move(other.headers_))
    , body_(std::move(other.body_)) {
}

HttpMessage& HttpMessage::operator=(const HttpMessage& other) {
    if (this != &other) {
        is_response_ = other.is_response_;
        version_ = other.version_;
        headers_ = other.headers_;
        body_ = other.body_;
    }
    return *this;
}

HttpMessage& HttpMessage::operator=(HttpMessage&& other) noexcept {
    if (this != &other) {
        is_response_ = other.is_response_;
        version_ = std::move(other.version_);
        headers_ = std::move(other.headers_);
        body_ = std::move(other.body_);
    }
    return *this;
}

void HttpMessage::set_version(int major, int minor) {
    version_.major = major;
    version_.minor = minor;
}

void HttpMessage::set_version(const HttpVersion& version) {
    version_ = version;
}

void HttpMessage::set_header(const std::string& name, const std::string& value) {
    std::string norm_name = normalize_header_name(name);
    
    // 删除已存在的同名头部
    auto range = headers_.equal_range(norm_name);
    headers_.erase(range.first, range.second);
    
    // 添加新的头部
    headers_.emplace(norm_name, value);
}

void HttpMessage::add_header(const std::string& name, const std::string& value) {
    std::string norm_name = normalize_header_name(name);
    headers_.emplace(norm_name, value);
}

std::string HttpMessage::get_header(const std::string& name) const {
    std::string norm_name = normalize_header_name(name);
    auto it = headers_.find(norm_name);
    return (it != headers_.end()) ? it->second : std::string();
}

std::vector<std::string> HttpMessage::get_headers(const std::string& name) const {
    std::string norm_name = normalize_header_name(name);
    std::vector<std::string> values;
    
    auto range = headers_.equal_range(norm_name);
    for (auto it = range.first; it != range.second; ++it) {
        values.push_back(it->second);
    }
    
    return values;
}

bool HttpMessage::has_header(const std::string& name) const {
    std::string norm_name = normalize_header_name(name);
    return headers_.find(norm_name) != headers_.end();
}

void HttpMessage::remove_header(const std::string& name) {
    std::string norm_name = normalize_header_name(name);
    auto range = headers_.equal_range(norm_name);
    headers_.erase(range.first, range.second);
}

void HttpMessage::clear_headers() {
    headers_.clear();
}

void HttpMessage::set_body(const std::string& body) {
    body_ = body;
}

void HttpMessage::set_body(const char* data, size_t size) {
    body_.assign(data, size);
}

void HttpMessage::append_body(const std::string& data) {
    body_.append(data);
}

void HttpMessage::append_body(const char* data, size_t size) {
    body_.append(data, size);
}

void HttpMessage::clear_body() {
    body_.clear();
}

bool HttpMessage::is_chunked() const {
    std::string transfer_encoding = get_header("transfer-encoding");
    std::transform(transfer_encoding.begin(), transfer_encoding.end(), 
                   transfer_encoding.begin(), ::tolower);
    return transfer_encoding.find("chunked") != std::string::npos;
}

void HttpMessage::set_chunked(bool chunked) {
    if (chunked) {
        set_header("transfer-encoding", "chunked");
        remove_header("content-length");
    } else {
        remove_header("transfer-encoding");
        update_content_length();
    }
}

bool HttpMessage::is_keep_alive() const {
    std::string connection = get_header("connection");
    std::transform(connection.begin(), connection.end(), 
                   connection.begin(), ::tolower);
    
    if (connection == "close") {
        return false;
    } else if (connection == "keep-alive") {
        return true;
    }
    
    // 根据HTTP版本确定默认行为
    return version_.is_keep_alive_default();
}

void HttpMessage::set_keep_alive(bool keep_alive) {
    if (keep_alive) {
        if (!version_.is_keep_alive_default()) {
            set_header("connection", "keep-alive");
        }
    } else {
        set_header("connection", "close");
    }
}

long long HttpMessage::get_content_length() const {
    std::string length_str = get_header("content-length");
    if (length_str.empty()) {
        return -1;
    }
    
    try {
        return std::stoll(length_str);
    } catch (const std::exception&) {
        return -1;
    }
}

void HttpMessage::set_content_length(long long length) {
    set_header("content-length", std::to_string(length));
}

void HttpMessage::update_content_length() {
    if (!is_chunked()) {
        set_content_length(static_cast<long long>(body_.size()));
    }
}

void HttpMessage::reset() {
    headers_.clear();
    body_.clear();
}

std::string HttpMessage::build_headers() const {
    std::ostringstream oss;
    
    for (const auto& header : headers_) {
        // 格式化头部名称（首字母大写）
        std::string formatted_name = header.first;
        bool capitalize_next = true;
        for (char& c : formatted_name) {
            if (capitalize_next && std::isalpha(c)) {
                c = std::toupper(c);
                capitalize_next = false;
            } else if (c == '-') {
                capitalize_next = true;
            } else {
                c = std::tolower(c);
            }
        }
        
        oss << formatted_name << ": " << header.second << "\r\n";
    }
    
    oss << "\r\n"; // 头部结束标记
    return oss.str();
}

std::string HttpMessage::normalize_header_name(const std::string& name) const {
    std::string result = name;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

// HttpRequest实现
HttpRequest::HttpRequest() 
    : HttpMessage(false)
    , parser_(std::unique_ptr<HttpRequestParser>(new HttpRequestParser()))
    , method_(HttpMethod::GET) {
}

HttpRequest::HttpRequest(HttpMethod method, const std::string& uri, const HttpVersion& version)
    : HttpMessage(false)
    , parser_(std::unique_ptr<HttpRequestParser>(new HttpRequestParser()))
    , method_(method)
    , uri_(uri) {
    set_version(version);
    parse_uri();
}

HttpRequest::HttpRequest(const std::string& method, const std::string& uri, const HttpVersion& version)
    : HttpMessage(false)
    , parser_(std::unique_ptr<HttpRequestParser>(new HttpRequestParser()))
    , method_(string_to_method(method))
    , uri_(uri) {
    set_version(version);
    parse_uri();
}

void HttpRequest::set_method(HttpMethod method) {
    method_ = method;
}

void HttpRequest::set_method(const std::string& method) {
    method_ = string_to_method(method);
}

std::string HttpRequest::get_method_string() const {
    return method_to_string(method_);
}

void HttpRequest::set_uri(const std::string& uri) {
    uri_ = uri;
    parse_uri();
}

std::map<std::string, std::string> HttpRequest::get_query_params() const {
    std::map<std::string, std::string> params;
    if (query_.empty()) {
        return params;
    }
    
    std::istringstream iss(query_);
    std::string pair;
    
    while (std::getline(iss, pair, '&')) {
        size_t eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = pair.substr(0, eq_pos);
            std::string value = pair.substr(eq_pos + 1);
            
            // 简单的URL解码（这里只处理基本情况）
            // 实际应用中可能需要更完整的URL解码
            params[key] = value;
        } else {
            params[pair] = "";
        }
    }
    
    return params;
}

std::string HttpRequest::get_query_param(const std::string& name) const {
    auto params = get_query_params();
    auto it = params.find(name);
    return (it != params.end()) ? it->second : std::string();
}

HttpRequest HttpRequest::create_get(const std::string& uri, const HttpVersion& version) {
    HttpRequest request(HttpMethod::GET, uri, version);
    request.update_content_length(); // GET通常没有body，Content-Length为0
    return request;
}

HttpRequest HttpRequest::create_post(const std::string& uri, const std::string& body,
                                   const std::string& content_type, const HttpVersion& version) {
    HttpRequest request(HttpMethod::POST, uri, version);
    request.set_body(body);
    request.set_header("content-type", content_type);
    request.update_content_length();
    return request;
}

std::string HttpRequest::to_string() const {
    std::ostringstream oss;
    
    // 构建请求行
    oss << build_start_line() << "\r\n";
    
    // 构建头部
    oss << build_headers();
    
    // 构建消息体
    if (!body_.empty()) {
        oss << body_;
    }
    
    return oss.str();
}

int HttpRequest::parse(const char* data, size_t size) {
    int result = parser_->parse(data, size);
    
    if (result > 0 && parser_->is_complete()) {
        // 从解析器中提取数据
        method_ = parser_->get_method();
        uri_ = parser_->get_uri();
        path_ = parser_->get_path();
        query_ = parser_->get_query();
        version_ = parser_->get_version();
        headers_ = parser_->get_headers();
        body_ = parser_->get_body();
    }
    
    return result;
}

bool HttpRequest::is_complete() const {
    return parser_->is_complete();
}

bool HttpRequest::has_error() const {
    return parser_->has_error();
}

std::string HttpRequest::get_error() const {
    return parser_->get_error();
}

void HttpRequest::reset() {
    HttpMessage::reset();
    parser_->reset();
    method_ = HttpMethod::GET;
    uri_.clear();
    path_.clear();
    query_.clear();
}

std::string HttpRequest::build_start_line() const {
    return get_method_string() + " " + uri_ + " " + get_version_string();
}

void HttpRequest::parse_uri() {
    size_t query_pos = uri_.find('?');
    if (query_pos != std::string::npos) {
        path_ = uri_.substr(0, query_pos);
        query_ = uri_.substr(query_pos + 1);
    } else {
        path_ = uri_;
        query_.clear();
    }
}

// HttpResponse实现
HttpResponse::HttpResponse()
    : HttpMessage(true)
    , parser_(std::unique_ptr<HttpResponseParser>(new HttpResponseParser()))
    , status_code_(200)
    , reason_phrase_("OK") {
}

HttpResponse::HttpResponse(int status_code, const std::string& reason_phrase, const HttpVersion& version)
    : HttpMessage(true)
    , parser_(std::unique_ptr<HttpResponseParser>(new HttpResponseParser()))
    , status_code_(status_code)
    , reason_phrase_(reason_phrase.empty() ? get_default_reason_phrase(status_code) : reason_phrase) {
    set_version(version);
}

void HttpResponse::set_status_code(int status_code) {
    status_code_ = status_code;
    if (reason_phrase_.empty() || reason_phrase_ == get_default_reason_phrase(status_code_)) {
        reason_phrase_ = get_default_reason_phrase(status_code);
    }
}

void HttpResponse::set_reason_phrase(const std::string& reason_phrase) {
    reason_phrase_ = reason_phrase;
}

HttpResponse HttpResponse::create_ok(const std::string& body, const std::string& content_type, 
                                    const HttpVersion& version) {
    HttpResponse response(200, "OK", version);
    response.set_body(body);
    response.set_header("content-type", content_type);
    response.update_content_length();
    return response;
}

HttpResponse HttpResponse::create_json(const std::string& json_body, const HttpVersion& version) {
    return create_ok(json_body, "application/json", version);
}

HttpResponse HttpResponse::create_error(int status_code, const std::string& message, 
                                       const HttpVersion& version) {
    std::string reason = message.empty() ? get_default_reason_phrase(status_code) : message;
    HttpResponse response(status_code, reason, version);
    
    // 创建简单的错误页面
    std::ostringstream body;
    body << "<html><body><h1>" << status_code << " " << reason << "</h1></body></html>";
    
    response.set_body(body.str());
    response.set_header("content-type", "text/html");
    response.update_content_length();
    return response;
}

std::string HttpResponse::to_string() const {
    std::ostringstream oss;
    
    // 构建状态行
    oss << build_start_line() << "\r\n";
    
    // 构建头部
    oss << build_headers();
    
    // 构建消息体
    if (!body_.empty()) {
        oss << body_;
    }
    
    return oss.str();
}

int HttpResponse::parse(const char* data, size_t size) {
    int result = parser_->parse(data, size);
    
    if (result > 0 && parser_->is_complete()) {
        // 从解析器中提取数据
        status_code_ = parser_->get_status_code();
        reason_phrase_ = parser_->get_reason_phrase();
        version_ = parser_->get_version();
        headers_ = parser_->get_headers();
        body_ = parser_->get_body();
    }
    
    return result;
}

bool HttpResponse::is_complete() const {
    return parser_->is_complete();
}

bool HttpResponse::has_error() const {
    return parser_->has_error();
}

std::string HttpResponse::get_error() const {
    return parser_->get_error();
}

void HttpResponse::reset() {
    HttpMessage::reset();
    parser_->reset();
    status_code_ = 200;
    reason_phrase_ = "OK";
}

std::string HttpResponse::build_start_line() const {
    return get_version_string() + " " + std::to_string(status_code_) + " " + reason_phrase_;
}

// 输出操作符重载
std::ostream& operator<<(std::ostream& os, const HttpRequest& request) {
    os << "HTTP请求:\n";
    os << "方法: " << request.get_method_string() << "\n";
    os << "URI: " << request.get_uri() << "\n";
    os << "版本: " << request.get_version_string() << "\n";
    os << "头部:\n";
    
    for (const auto& header : request.get_all_headers()) {
        os << "  " << header.first << ": " << header.second << "\n";
    }
    
    if (!request.get_body().empty()) {
        os << "消息体 (" << request.get_body_size() << " 字节):\n";
        os << request.get_body() << "\n";
    }
    
    return os;
}

std::ostream& operator<<(std::ostream& os, const HttpResponse& response) {
    os << "HTTP响应:\n";
    os << "状态: " << response.get_status_code() << " " << response.get_reason_phrase() << "\n";
    os << "版本: " << response.get_version_string() << "\n";
    os << "头部:\n";
    
    for (const auto& header : response.get_all_headers()) {
        os << "  " << header.first << ": " << header.second << "\n";
    }
    
    if (!response.get_body().empty()) {
        os << "消息体 (" << response.get_body_size() << " 字节):\n";
        os << response.get_body() << "\n";
    }
    
    return os;
}

} // namespace stdhttps