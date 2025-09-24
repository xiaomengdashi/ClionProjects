/**
 * @file http_parser.cpp
 * @brief HTTP协议解析器实现
 * @details 基于状态机的HTTP协议解析实现，参考workflow的设计思路
 */

#include "http_parser.h"
#include <algorithm>
#include <sstream>
#include <cctype>
#include <cstdlib>

namespace stdhttps {

// 常量定义
static const size_t MAX_LINE_LENGTH = 8192;    // 最大行长度
static const size_t MAX_HEADER_SIZE = 65536;   // 最大头部大小
static const size_t MAX_BODY_SIZE = 1024 * 1024 * 100; // 最大消息体大小（100MB）

HttpParser::HttpParser(bool is_response) 
    : is_response_(is_response)
    , state_(ParseState::START_LINE)
    , version_(1, 1)
    , expected_body_length_(0)
    , chunk_remaining_(0)
    , chunked_encoding_(false)
    , headers_complete_(false) {
}

int HttpParser::parse(const char* data, size_t size) {
    if (state_ == ParseState::ERROR || state_ == ParseState::COMPLETE) {
        return 0;
    }
    
    size_t total_consumed = 0;
    
    while (total_consumed < size && state_ != ParseState::COMPLETE && state_ != ParseState::ERROR) {
        size_t consumed = 0;
        int result = 0;
        
        switch (state_) {
            case ParseState::START_LINE:
                result = parse_start_line_state(data + total_consumed, 
                                               size - total_consumed, consumed);
                break;
                
            case ParseState::HEADER_NAME:
                result = parse_header_name_state(data + total_consumed, 
                                                size - total_consumed, consumed);
                break;
                
            case ParseState::HEADER_VALUE:
                result = parse_header_value_state(data + total_consumed, 
                                                 size - total_consumed, consumed);
                break;
                
            case ParseState::BODY:
                result = parse_body_state(data + total_consumed, 
                                         size - total_consumed, consumed);
                break;
                
            case ParseState::CHUNK_SIZE:
                result = parse_chunk_size_state(data + total_consumed, 
                                               size - total_consumed, consumed);
                break;
                
            case ParseState::CHUNK_DATA:
                result = parse_chunk_data_state(data + total_consumed, 
                                               size - total_consumed, consumed);
                break;
                
            case ParseState::CHUNK_TRAILER:
                result = parse_chunk_trailer_state(data + total_consumed, 
                                                  size - total_consumed, consumed);
                break;
                
            default:
                set_error("未知的解析状态");
                return -1;
        }
        
        if (result < 0) {
            return -1;
        }
        
        total_consumed += consumed;
        
        // 避免无限循环
        if (consumed == 0 && result == 0) {
            break;
        }
    }
    
    return static_cast<int>(total_consumed);
}

void HttpParser::reset() {
    state_ = ParseState::START_LINE;
    version_ = HttpVersion(1, 1);
    headers_.clear();
    body_.clear();
    error_message_.clear();
    buffer_.clear();
    current_header_name_.clear();
    expected_body_length_ = 0;
    chunk_remaining_ = 0;
    chunked_encoding_ = false;
    headers_complete_ = false;
}

std::string HttpParser::get_header(const std::string& name) const {
    std::string lower_name = to_lower(name);
    auto it = headers_.find(lower_name);
    return (it != headers_.end()) ? it->second : std::string();
}

bool HttpParser::has_header(const std::string& name) const {
    std::string lower_name = to_lower(name);
    return headers_.find(lower_name) != headers_.end();
}

bool HttpParser::is_chunked() const {
    return chunked_encoding_;
}

bool HttpParser::is_keep_alive() const {
    std::string connection = get_header("connection");
    std::string lower_conn = to_lower(connection);
    
    if (lower_conn == "close") {
        return false;
    } else if (lower_conn == "keep-alive") {
        return true;
    }
    
    // 根据HTTP版本确定默认行为
    return version_.is_keep_alive_default();
}

long long HttpParser::get_content_length() const {
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

bool HttpParser::should_read_body() const {
    if (!headers_complete_) {
        return false;
    }
    
    // HEAD请求的响应不应该有消息体
    if (is_response_) {
        // TODO: 需要从请求中获取方法信息
        // 暂时简化处理
    }
    
    // 检查是否为chunked编码
    if (is_chunked()) {
        return true;
    }
    
    // 检查Content-Length
    long long content_length = get_content_length();
    return content_length > 0;
}

int HttpParser::parse_start_line_state(const char* data, size_t size, size_t& consumed) {
    consumed = 0;
    size_t line_end = find_line_end(data, size);
    
    if (line_end == std::string::npos) {
        // 还没有找到完整的行，将数据添加到缓冲区
        buffer_.append(data, size);
        consumed = size;
        
        if (buffer_.size() > MAX_LINE_LENGTH) {
            set_error("起始行过长");
            return -1;
        }
        return 0;
    }
    
    // 找到完整的行
    std::string line = buffer_;
    line.append(data, line_end);
    consumed = line_end + 2; // +2 for CRLF
    
    // 去除行尾的CRLF
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    
    if (!parse_start_line(line)) {
        set_error("起始行格式错误");
        return -1;
    }
    
    buffer_.clear();
    state_ = ParseState::HEADER_NAME;
    return 1;
}

int HttpParser::parse_header_name_state(const char* data, size_t size, size_t& consumed) {
    consumed = 0;
    size_t line_end = find_line_end(data, size);
    
    if (line_end == std::string::npos) {
        buffer_.append(data, size);
        consumed = size;
        
        if (buffer_.size() > MAX_HEADER_SIZE) {
            set_error("头部过大");
            return -1;
        }
        return 0;
    }
    
    std::string line = buffer_;
    line.append(data, line_end);
    consumed = line_end + 2;
    
    // 去除CRLF
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    
    buffer_.clear();
    
    // 检查是否为空行（头部结束）
    if (line.empty()) {
        headers_complete_ = true;
        on_headers_complete();
        
        // 确定接下来的解析状态
        if (is_chunked()) {
            chunked_encoding_ = true;
            state_ = ParseState::CHUNK_SIZE;
        } else if (should_read_body()) {
            long long content_length = get_content_length();
            if (content_length > 0) {
                expected_body_length_ = static_cast<size_t>(content_length);
                state_ = ParseState::BODY;
            } else {
                state_ = ParseState::COMPLETE;
            }
        } else {
            state_ = ParseState::COMPLETE;
        }
        return 1;
    }
    
    // 解析头部字段
    if (!parse_header_line(line)) {
        set_error("头部字段格式错误");
        return -1;
    }
    
    return 1;
}

int HttpParser::parse_header_value_state(const char* data, size_t size, size_t& consumed) {
    // 在当前实现中，头部名称和值在同一行中解析
    // 这个状态主要用于处理多行头部值的情况
    consumed = 0;
    return 0;
}

int HttpParser::parse_body_state(const char* data, size_t size, size_t& consumed) {
    consumed = 0;
    
    size_t remaining_body = expected_body_length_ - body_.size();
    size_t to_consume = std::min(size, remaining_body);
    
    body_.append(data, to_consume);
    consumed = to_consume;
    
    if (body_.size() >= expected_body_length_) {
        state_ = ParseState::COMPLETE;
    }
    
    return consumed > 0 ? 1 : 0;
}

int HttpParser::parse_chunk_size_state(const char* data, size_t size, size_t& consumed) {
    consumed = 0;
    size_t line_end = find_line_end(data, size);
    
    if (line_end == std::string::npos) {
        buffer_.append(data, size);
        consumed = size;
        
        if (buffer_.size() > 32) { // chunk size行不应该很长
            set_error("chunk大小行格式错误");
            return -1;
        }
        return 0;
    }
    
    std::string line = buffer_;
    line.append(data, line_end);
    consumed = line_end + 2;
    
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    
    buffer_.clear();
    
    // 解析chunk大小（十六进制）
    size_t semicolon_pos = line.find(';');
    std::string size_str = line.substr(0, semicolon_pos);
    
    try {
        chunk_remaining_ = std::stoull(size_str, nullptr, 16);
    } catch (const std::exception&) {
        set_error("chunk大小格式错误");
        return -1;
    }
    
    if (chunk_remaining_ == 0) {
        // 最后一个chunk，接下来解析trailer
        state_ = ParseState::CHUNK_TRAILER;
    } else {
        state_ = ParseState::CHUNK_DATA;
    }
    
    return 1;
}

int HttpParser::parse_chunk_data_state(const char* data, size_t size, size_t& consumed) {
    consumed = 0;
    
    size_t to_consume = std::min(size, chunk_remaining_);
    body_.append(data, to_consume);
    consumed = to_consume;
    chunk_remaining_ -= to_consume;
    
    if (chunk_remaining_ == 0) {
        // 还需要消费CRLF
        if (size > to_consume && size >= to_consume + 2) {
            if (data[to_consume] == '\r' && data[to_consume + 1] == '\n') {
                consumed += 2;
                state_ = ParseState::CHUNK_SIZE;
            } else {
                set_error("chunk数据后缺少CRLF");
                return -1;
            }
        } else {
            // 等待更多数据来读取CRLF
            // 这里简化处理，直接转到下一个状态
            state_ = ParseState::CHUNK_SIZE;
        }
    }
    
    return consumed > 0 ? 1 : 0;
}

int HttpParser::parse_chunk_trailer_state(const char* data, size_t size, size_t& consumed) {
    consumed = 0;
    size_t line_end = find_line_end(data, size);
    
    if (line_end == std::string::npos) {
        buffer_.append(data, size);
        consumed = size;
        return 0;
    }
    
    std::string line = buffer_;
    line.append(data, line_end);
    consumed = line_end + 2;
    
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    
    buffer_.clear();
    
    if (line.empty()) {
        // trailer结束
        state_ = ParseState::COMPLETE;
    } else {
        // 解析trailer字段（与头部字段格式相同）
        parse_header_line(line);
    }
    
    return 1;
}

void HttpParser::set_error(const std::string& message) {
    state_ = ParseState::ERROR;
    error_message_ = message;
}

std::string HttpParser::to_lower(const std::string& str) const {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), 
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

bool HttpParser::parse_header_line(const std::string& line) {
    size_t colon_pos = line.find(':');
    if (colon_pos == std::string::npos) {
        return false;
    }
    
    std::string name = line.substr(0, colon_pos);
    std::string value = line.substr(colon_pos + 1);
    
    // 去除名称和值的前后空格
    name.erase(0, name.find_first_not_of(" \t"));
    name.erase(name.find_last_not_of(" \t") + 1);
    value.erase(0, value.find_first_not_of(" \t"));
    value.erase(value.find_last_not_of(" \t") + 1);
    
    if (name.empty()) {
        return false;
    }
    
    // 头部名称转为小写存储
    std::string lower_name = to_lower(name);
    headers_.emplace(lower_name, value);
    
    return true;
}

size_t HttpParser::find_line_end(const char* data, size_t size) const {
    for (size_t i = 0; i < size - 1; ++i) {
        if (data[i] == '\r' && data[i + 1] == '\n') {
            return i;
        }
    }
    return std::string::npos;
}

// HttpRequestParser实现
bool HttpRequestParser::parse_start_line(const std::string& line) {
    std::istringstream iss(line);
    std::string method_str, uri_str, version_str;
    
    if (!(iss >> method_str >> uri_str >> version_str)) {
        return false;
    }
    
    // 解析方法
    method_ = string_to_method(method_str);
    if (method_ == HttpMethod::UNKNOWN) {
        return false;
    }
    
    // 保存URI
    uri_ = uri_str;
    parse_uri();
    
    // 解析版本
    if (version_str.substr(0, 5) != "HTTP/") {
        return false;
    }
    
    std::string version_part = version_str.substr(5);
    size_t dot_pos = version_part.find('.');
    if (dot_pos == std::string::npos) {
        return false;
    }
    
    try {
        version_.major = std::stoi(version_part.substr(0, dot_pos));
        version_.minor = std::stoi(version_part.substr(dot_pos + 1));
    } catch (const std::exception&) {
        return false;
    }
    
    return true;
}

void HttpRequestParser::on_headers_complete() {
    // 请求头部解析完成后的处理
    // 可以在这里进行请求验证等操作
}

void HttpRequestParser::parse_uri() {
    size_t query_pos = uri_.find('?');
    if (query_pos != std::string::npos) {
        path_ = uri_.substr(0, query_pos);
        query_ = uri_.substr(query_pos + 1);
    } else {
        path_ = uri_;
        query_.clear();
    }
}

HttpMethod HttpRequestParser::string_to_method(const std::string& method) const {
    if (method == "GET") return HttpMethod::GET;
    if (method == "POST") return HttpMethod::POST;
    if (method == "PUT") return HttpMethod::PUT;
    if (method == "DELETE") return HttpMethod::DELETE;
    if (method == "HEAD") return HttpMethod::HEAD;
    if (method == "OPTIONS") return HttpMethod::OPTIONS;
    if (method == "PATCH") return HttpMethod::PATCH;
    if (method == "CONNECT") return HttpMethod::CONNECT;
    if (method == "TRACE") return HttpMethod::TRACE;
    return HttpMethod::UNKNOWN;
}

std::string HttpRequestParser::get_method_string() const {
    return method_to_string(method_);
}

// HttpResponseParser实现
bool HttpResponseParser::parse_start_line(const std::string& line) {
    std::istringstream iss(line);
    std::string version_str, status_str;
    
    if (!(iss >> version_str >> status_str)) {
        return false;
    }
    
    // 解析版本
    if (version_str.substr(0, 5) != "HTTP/") {
        return false;
    }
    
    std::string version_part = version_str.substr(5);
    size_t dot_pos = version_part.find('.');
    if (dot_pos == std::string::npos) {
        return false;
    }
    
    try {
        version_.major = std::stoi(version_part.substr(0, dot_pos));
        version_.minor = std::stoi(version_part.substr(dot_pos + 1));
    } catch (const std::exception&) {
        return false;
    }
    
    // 解析状态码
    try {
        status_code_ = std::stoi(status_str);
    } catch (const std::exception&) {
        return false;
    }
    
    // 获取原因短语（剩余部分）
    std::string remaining_line = line;
    size_t status_end = remaining_line.find(status_str) + status_str.length();
    if (status_end < remaining_line.length()) {
        reason_phrase_ = remaining_line.substr(status_end + 1); // +1跳过空格
    } else {
        reason_phrase_ = get_default_reason_phrase(status_code_);
    }
    
    return true;
}

void HttpResponseParser::on_headers_complete() {
    // 响应头部解析完成后的处理
}

// 工具函数实现
std::string method_to_string(HttpMethod method) {
    switch (method) {
        case HttpMethod::GET: return "GET";
        case HttpMethod::POST: return "POST";
        case HttpMethod::PUT: return "PUT";
        case HttpMethod::DELETE: return "DELETE";
        case HttpMethod::HEAD: return "HEAD";
        case HttpMethod::OPTIONS: return "OPTIONS";
        case HttpMethod::PATCH: return "PATCH";
        case HttpMethod::CONNECT: return "CONNECT";
        case HttpMethod::TRACE: return "TRACE";
        default: return "UNKNOWN";
    }
}

HttpMethod string_to_method(const std::string& method) {
    if (method == "GET") return HttpMethod::GET;
    if (method == "POST") return HttpMethod::POST;
    if (method == "PUT") return HttpMethod::PUT;
    if (method == "DELETE") return HttpMethod::DELETE;
    if (method == "HEAD") return HttpMethod::HEAD;
    if (method == "OPTIONS") return HttpMethod::OPTIONS;
    if (method == "PATCH") return HttpMethod::PATCH;
    if (method == "CONNECT") return HttpMethod::CONNECT;
    if (method == "TRACE") return HttpMethod::TRACE;
    return HttpMethod::UNKNOWN;
}

std::string get_default_reason_phrase(int status_code) {
    switch (status_code) {
        case 100: return "Continue";
        case 101: return "Switching Protocols";
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 204: return "No Content";
        case 206: return "Partial Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 409: return "Conflict";
        case 412: return "Precondition Failed";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        default: return "Unknown Status";
    }
}

} // namespace stdhttps