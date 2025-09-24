/**
 * @file http_parser.h
 * @brief HTTP协议解析器头文件
 */

#ifndef STDHTTPS_HTTP_PARSER_H
#define STDHTTPS_HTTP_PARSER_H

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace stdhttps {

/**
 * @brief HTTP请求方法枚举
 */
enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    HEAD,
    OPTIONS,
    PATCH,
    CONNECT,
    TRACE,
    UNKNOWN
};

/**
 * @brief HTTP解析器状态枚举
 */
enum class ParseState {
    START_LINE,         // 解析起始行（请求行或状态行）
    HEADER_NAME,        // 解析头部名称
    HEADER_VALUE,       // 解析头部值
    HEADER_COMPLETE,    // 头部解析完成
    BODY,              // 解析消息体
    CHUNK_SIZE,        // 解析chunk大小
    CHUNK_DATA,        // 解析chunk数据
    CHUNK_TRAILER,     // 解析chunk trailer
    COMPLETE,          // 解析完成
    ERROR              // 解析错误
};

/**
 * @brief HTTP版本结构体
 */
struct HttpVersion {
    int major;  // 主版本号
    int minor;  // 次版本号
    
    HttpVersion(int maj = 1, int min = 1) : major(maj), minor(min) {}
    
    std::string to_string() const {
        return "HTTP/" + std::to_string(major) + "." + std::to_string(minor);
    }
    
    bool is_keep_alive_default() const {
        // HTTP/1.1默认keep-alive，HTTP/1.0默认关闭
        return major > 1 || (major == 1 && minor >= 1);
    }
};

/**
 * @brief HTTP头部字段类型定义
 */
using HttpHeaders = std::multimap<std::string, std::string>;

/**
 * @brief HTTP解析器基类
 * @details 提供HTTP协议解析的核心功能，支持状态机驱动的增量解析
 */
class HttpParser {
public:
    /**
     * @brief 构造函数
     * @param is_response 是否为响应解析器（false表示请求解析器）
     */
    explicit HttpParser(bool is_response = false);
    
    /**
     * @brief 析构函数
     */
    virtual ~HttpParser() = default;

    /**
     * @brief 解析数据
     * @param data 输入数据
     * @param size 数据大小
     * @return 实际消费的字节数，-1表示解析错误
     */
    int parse(const char* data, size_t size);
    
    /**
     * @brief 重置解析器状态
     */
    void reset();
    
    /**
     * @brief 获取解析状态
     */
    ParseState get_state() const { return state_; }
    
    /**
     * @brief 解析是否完成
     */
    bool is_complete() const { return state_ == ParseState::COMPLETE; }
    
    /**
     * @brief 是否有解析错误
     */
    bool has_error() const { return state_ == ParseState::ERROR; }
    
    /**
     * @brief 获取错误信息
     */
    const std::string& get_error() const { return error_message_; }
    
    // HTTP通用字段访问器
    const HttpVersion& get_version() const { return version_; }
    const HttpHeaders& get_headers() const { return headers_; }
    const std::string& get_body() const { return body_; }
    
    /**
     * @brief 获取指定名称的头部值
     * @param name 头部名称（不区分大小写）
     * @return 头部值，如果不存在返回空字符串
     */
    std::string get_header(const std::string& name) const;
    
    /**
     * @brief 检查是否存在指定头部
     */
    bool has_header(const std::string& name) const;
    
    /**
     * @brief 是否为chunked传输编码
     */
    bool is_chunked() const;
    
    /**
     * @brief 是否为keep-alive连接
     */
    bool is_keep_alive() const;
    
    /**
     * @brief 获取Content-Length
     * @return Content-Length值，如果不存在返回-1
     */
    long long get_content_length() const;
    
    /**
     * @brief 是否需要读取消息体
     */
    bool should_read_body() const;

protected:
    /**
     * @brief 解析起始行（由子类实现）
     * @param line 起始行内容
     * @return 是否解析成功
     */
    virtual bool parse_start_line(const std::string& line) = 0;
    
    /**
     * @brief 头部解析完成回调（由子类实现）
     */
    virtual void on_headers_complete() {}

private:
    // 解析状态机相关方法
    int parse_start_line_state(const char* data, size_t size, size_t& consumed);
    int parse_header_name_state(const char* data, size_t size, size_t& consumed);
    int parse_header_value_state(const char* data, size_t size, size_t& consumed);
    int parse_body_state(const char* data, size_t size, size_t& consumed);
    int parse_chunk_size_state(const char* data, size_t size, size_t& consumed);
    int parse_chunk_data_state(const char* data, size_t size, size_t& consumed);
    int parse_chunk_trailer_state(const char* data, size_t size, size_t& consumed);
    
    // 辅助方法
    void set_error(const std::string& message);
    std::string to_lower(const std::string& str) const;
    bool parse_header_line(const std::string& line);
    size_t find_line_end(const char* data, size_t size) const;

protected:
    bool is_response_;              // 是否为响应解析器
    ParseState state_;              // 当前解析状态
    HttpVersion version_;           // HTTP版本
    HttpHeaders headers_;           // HTTP头部
    std::string body_;              // 消息体
    std::string error_message_;     // 错误信息
    
private:
    std::string buffer_;            // 解析缓冲区
    std::string current_header_name_;   // 当前解析的头部名称
    size_t expected_body_length_;   // 期望的消息体长度
    size_t chunk_remaining_;        // 当前chunk的剩余字节数
    bool chunked_encoding_;         // 是否为chunked编码
    bool headers_complete_;         // 头部是否解析完成
};

/**
 * @brief HTTP请求解析器
 */
class HttpRequestParser : public HttpParser {
public:
    HttpRequestParser() : HttpParser(false) {}
    
    // 请求特定字段访问器
    HttpMethod get_method() const { return method_; }
    const std::string& get_uri() const { return uri_; }
    const std::string& get_path() const { return path_; }
    const std::string& get_query() const { return query_; }
    
    /**
     * @brief 获取方法字符串
     */
    std::string get_method_string() const;

protected:
    bool parse_start_line(const std::string& line) override;
    void on_headers_complete() override;

private:
    void parse_uri();               // 解析URI，分离路径和查询参数
    HttpMethod string_to_method(const std::string& method) const;

private:
    HttpMethod method_;             // HTTP方法
    std::string uri_;               // 完整URI
    std::string path_;              // URI路径部分
    std::string query_;             // URI查询参数部分
};

/**
 * @brief HTTP响应解析器
 */
class HttpResponseParser : public HttpParser {
public:
    HttpResponseParser() : HttpParser(true) {}
    
    // 响应特定字段访问器
    int get_status_code() const { return status_code_; }
    const std::string& get_reason_phrase() const { return reason_phrase_; }

protected:
    bool parse_start_line(const std::string& line) override;
    void on_headers_complete() override;

private:
    int status_code_;               // HTTP状态码
    std::string reason_phrase_;     // 状态描述短语
};

// 工具函数
/**
 * @brief 方法枚举转字符串
 */
std::string method_to_string(HttpMethod method);

/**
 * @brief 字符串转方法枚举
 */
HttpMethod string_to_method(const std::string& method);

/**
 * @brief 获取状态码的默认描述短语
 */
std::string get_default_reason_phrase(int status_code);

} // namespace stdhttps

#endif // STDHTTPS_HTTP_PARSER_H