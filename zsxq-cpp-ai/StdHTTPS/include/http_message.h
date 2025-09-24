/**
 * @file http_message.h
 * @brief HTTP消息类头文件
 * @details 提供HTTP请求和响应消息的封装，支持消息构建、序列化和便捷访问
 */

#ifndef STDHTTPS_HTTP_MESSAGE_H
#define STDHTTPS_HTTP_MESSAGE_H

#include "http_parser.h"
#include <memory>
#include <vector>
#include <iostream>

namespace stdhttps {

/**
 * @brief HTTP消息基类
 * @details 封装HTTP消息的通用功能，包括头部管理、消息体处理和序列化
 */
class HttpMessage {
public:
    /**
     * @brief 构造函数
     * @param is_response 是否为响应消息
     */
    explicit HttpMessage(bool is_response = false);
    
    /**
     * @brief 析构函数
     */
    virtual ~HttpMessage() = default;
    
    /**
     * @brief 拷贝构造函数
     */
    HttpMessage(const HttpMessage& other);
    
    /**
     * @brief 移动构造函数
     */
    HttpMessage(HttpMessage&& other) noexcept;
    
    /**
     * @brief 拷贝赋值操作符
     */
    HttpMessage& operator=(const HttpMessage& other);
    
    /**
     * @brief 移动赋值操作符
     */
    HttpMessage& operator=(HttpMessage&& other) noexcept;

    // HTTP版本管理
    void set_version(int major, int minor);
    void set_version(const HttpVersion& version);
    const HttpVersion& get_version() const { return version_; }
    std::string get_version_string() const { return version_.to_string(); }

    // 头部管理
    /**
     * @brief 设置头部字段
     * @param name 头部名称
     * @param value 头部值
     */
    void set_header(const std::string& name, const std::string& value);
    
    /**
     * @brief 添加头部字段（支持重复头部）
     * @param name 头部名称
     * @param value 头部值
     */
    void add_header(const std::string& name, const std::string& value);
    
    /**
     * @brief 获取头部字段值
     * @param name 头部名称
     * @return 头部值，如果不存在返回空字符串
     */
    std::string get_header(const std::string& name) const;
    
    /**
     * @brief 获取所有指定名称的头部值
     * @param name 头部名称
     * @return 头部值列表
     */
    std::vector<std::string> get_headers(const std::string& name) const;
    
    /**
     * @brief 检查是否存在指定头部
     * @param name 头部名称
     */
    bool has_header(const std::string& name) const;
    
    /**
     * @brief 删除头部字段
     * @param name 头部名称
     */
    void remove_header(const std::string& name);
    
    /**
     * @brief 获取所有头部字段
     */
    const HttpHeaders& get_all_headers() const { return headers_; }
    
    /**
     * @brief 清空所有头部字段
     */
    void clear_headers();

    // 消息体管理
    /**
     * @brief 设置消息体
     * @param body 消息体内容
     */
    void set_body(const std::string& body);
    
    /**
     * @brief 设置消息体
     * @param data 消息体数据
     * @param size 数据大小
     */
    void set_body(const char* data, size_t size);
    
    /**
     * @brief 获取消息体
     */
    const std::string& get_body() const { return body_; }
    
    /**
     * @brief 获取消息体大小
     */
    size_t get_body_size() const { return body_.size(); }
    
    /**
     * @brief 追加消息体内容
     * @param data 要追加的数据
     */
    void append_body(const std::string& data);
    
    /**
     * @brief 追加消息体内容
     * @param data 要追加的数据
     * @param size 数据大小
     */
    void append_body(const char* data, size_t size);
    
    /**
     * @brief 清空消息体
     */
    void clear_body();

    // 便捷属性访问
    /**
     * @brief 是否为chunked传输编码
     */
    bool is_chunked() const;
    
    /**
     * @brief 设置chunked传输编码
     * @param chunked 是否启用chunked编码
     */
    void set_chunked(bool chunked);
    
    /**
     * @brief 是否为keep-alive连接
     */
    bool is_keep_alive() const;
    
    /**
     * @brief 设置keep-alive连接
     * @param keep_alive 是否启用keep-alive
     */
    void set_keep_alive(bool keep_alive);
    
    /**
     * @brief 获取Content-Length
     * @return Content-Length值，如果未设置返回-1
     */
    long long get_content_length() const;
    
    /**
     * @brief 设置Content-Length
     * @param length Content-Length值
     */
    void set_content_length(long long length);
    
    /**
     * @brief 自动设置Content-Length为当前消息体大小
     */
    void update_content_length();

    // 序列化和反序列化
    /**
     * @brief 序列化为字符串
     * @return HTTP消息的字符串表示
     */
    virtual std::string to_string() const = 0;
    
    /**
     * @brief 从数据解析消息
     * @param data 输入数据
     * @param size 数据大小
     * @return 解析消费的字节数，-1表示错误
     */
    virtual int parse(const char* data, size_t size) = 0;
    
    /**
     * @brief 检查消息是否解析完成
     */
    virtual bool is_complete() const = 0;
    
    /**
     * @brief 检查是否有解析错误
     */
    virtual bool has_error() const = 0;
    
    /**
     * @brief 获取错误信息
     */
    virtual std::string get_error() const = 0;
    
    /**
     * @brief 重置消息状态
     */
    virtual void reset();

protected:
    /**
     * @brief 构建起始行（由子类实现）
     */
    virtual std::string build_start_line() const = 0;
    
    /**
     * @brief 构建头部字符串
     */
    std::string build_headers() const;
    
    /**
     * @brief 标准化头部名称（转小写）
     */
    std::string normalize_header_name(const std::string& name) const;

protected:
    bool is_response_;              // 是否为响应消息
    HttpVersion version_;           // HTTP版本
    HttpHeaders headers_;           // HTTP头部字段
    std::string body_;              // 消息体
};

/**
 * @brief HTTP请求消息类
 */
class HttpRequest : public HttpMessage {
public:
    /**
     * @brief 构造函数
     */
    HttpRequest();
    
    /**
     * @brief 构造函数
     * @param method HTTP方法
     * @param uri 请求URI
     * @param version HTTP版本
     */
    HttpRequest(HttpMethod method, const std::string& uri, const HttpVersion& version = HttpVersion(1, 1));
    
    /**
     * @brief 构造函数
     * @param method HTTP方法字符串
     * @param uri 请求URI
     * @param version HTTP版本
     */
    HttpRequest(const std::string& method, const std::string& uri, const HttpVersion& version = HttpVersion(1, 1));

    // 请求特定属性
    /**
     * @brief 设置HTTP方法
     * @param method HTTP方法
     */
    void set_method(HttpMethod method);
    
    /**
     * @brief 设置HTTP方法
     * @param method HTTP方法字符串
     */
    void set_method(const std::string& method);
    
    /**
     * @brief 获取HTTP方法
     */
    HttpMethod get_method() const { return method_; }
    
    /**
     * @brief 获取HTTP方法字符串
     */
    std::string get_method_string() const;
    
    /**
     * @brief 设置请求URI
     * @param uri 请求URI
     */
    void set_uri(const std::string& uri);
    
    /**
     * @brief 获取请求URI
     */
    const std::string& get_uri() const { return uri_; }
    
    /**
     * @brief 获取路径部分
     */
    const std::string& get_path() const { return path_; }
    
    /**
     * @brief 获取查询参数部分
     */
    const std::string& get_query() const { return query_; }
    
    /**
     * @brief 解析查询参数为键值对
     * @return 查询参数的键值对映射
     */
    std::map<std::string, std::string> get_query_params() const;
    
    /**
     * @brief 获取指定查询参数值
     * @param name 参数名称
     * @return 参数值，如果不存在返回空字符串
     */
    std::string get_query_param(const std::string& name) const;

    // 便捷方法
    /**
     * @brief 创建GET请求
     * @param uri 请求URI
     * @param version HTTP版本
     * @return HTTP请求对象
     */
    static HttpRequest create_get(const std::string& uri, const HttpVersion& version = HttpVersion(1, 1));
    
    /**
     * @brief 创建POST请求
     * @param uri 请求URI
     * @param body 请求体
     * @param content_type Content-Type头部值
     * @param version HTTP版本
     * @return HTTP请求对象
     */
    static HttpRequest create_post(const std::string& uri, const std::string& body,
                                  const std::string& content_type = "application/json",
                                  const HttpVersion& version = HttpVersion(1, 1));

    // 重写基类方法
    std::string to_string() const override;
    int parse(const char* data, size_t size) override;
    bool is_complete() const override;
    bool has_error() const override;
    std::string get_error() const override;
    void reset() override;

protected:
    std::string build_start_line() const override;

private:
    void parse_uri();               // 解析URI，分离路径和查询参数

private:
    std::unique_ptr<HttpRequestParser> parser_;  // HTTP解析器
    HttpMethod method_;             // HTTP方法
    std::string uri_;               // 完整URI
    std::string path_;              // URI路径部分
    std::string query_;             // URI查询参数部分
};

/**
 * @brief HTTP响应消息类
 */
class HttpResponse : public HttpMessage {
public:
    /**
     * @brief 构造函数
     */
    HttpResponse();
    
    /**
     * @brief 构造函数
     * @param status_code 状态码
     * @param reason_phrase 状态描述短语
     * @param version HTTP版本
     */
    HttpResponse(int status_code, const std::string& reason_phrase = "",
                const HttpVersion& version = HttpVersion(1, 1));

    // 响应特定属性
    /**
     * @brief 设置状态码
     * @param status_code 状态码
     */
    void set_status_code(int status_code);
    
    /**
     * @brief 获取状态码
     */
    int get_status_code() const { return status_code_; }
    
    /**
     * @brief 设置状态描述短语
     * @param reason_phrase 状态描述短语
     */
    void set_reason_phrase(const std::string& reason_phrase);
    
    /**
     * @brief 获取状态描述短语
     */
    const std::string& get_reason_phrase() const { return reason_phrase_; }

    // 便捷方法
    /**
     * @brief 创建成功响应
     * @param body 响应体
     * @param content_type Content-Type头部值
     * @param version HTTP版本
     * @return HTTP响应对象
     */
    static HttpResponse create_ok(const std::string& body,
                                 const std::string& content_type = "text/html",
                                 const HttpVersion& version = HttpVersion(1, 1));
    
    /**
     * @brief 创建JSON响应
     * @param json_body JSON响应体
     * @param version HTTP版本
     * @return HTTP响应对象
     */
    static HttpResponse create_json(const std::string& json_body,
                                   const HttpVersion& version = HttpVersion(1, 1));
    
    /**
     * @brief 创建错误响应
     * @param status_code 错误状态码
     * @param message 错误消息
     * @param version HTTP版本
     * @return HTTP响应对象
     */
    static HttpResponse create_error(int status_code, const std::string& message = "",
                                    const HttpVersion& version = HttpVersion(1, 1));

    // 重写基类方法
    std::string to_string() const override;
    int parse(const char* data, size_t size) override;
    bool is_complete() const override;
    bool has_error() const override;
    std::string get_error() const override;
    void reset() override;

protected:
    std::string build_start_line() const override;

private:
    std::unique_ptr<HttpResponseParser> parser_;  // HTTP解析器
    int status_code_;               // HTTP状态码
    std::string reason_phrase_;     // 状态描述短语
};

// 便捷的输出操作符重载
std::ostream& operator<<(std::ostream& os, const HttpRequest& request);
std::ostream& operator<<(std::ostream& os, const HttpResponse& response);

} // namespace stdhttps

#endif // STDHTTPS_HTTP_MESSAGE_H