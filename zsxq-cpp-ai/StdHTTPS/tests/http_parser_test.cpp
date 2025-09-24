/**
 * @file http_parser_test.cpp
 * @brief HTTP解析器测试程序
 */

#include <iostream>
#include <cassert>
#include <string>
#include "http_parser.h"

using namespace stdhttps;

void test_request_parsing() {
    std::cout << "测试HTTP请求解析..." << std::endl;
    
    HttpRequestParser parser;
    
    std::string request_data = 
        "GET /test?name=value HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "User-Agent: Test/1.0\r\n"
        "Connection: keep-alive\r\n"
        "Content-Length: 13\r\n"
        "\r\n"
        "Hello, World!";
    
    int result = parser.parse(request_data.data(), request_data.size());
    assert(result > 0);
    assert(parser.is_complete());
    
    assert(parser.get_method() == HttpMethod::GET);
    assert(parser.get_uri() == "/test?name=value");
    assert(parser.get_path() == "/test");
    assert(parser.get_query() == "name=value");
    assert(parser.get_version().major == 1);
    assert(parser.get_version().minor == 1);
    assert(parser.get_header("host") == "example.com");
    assert(parser.get_header("user-agent") == "Test/1.0");
    assert(parser.is_keep_alive() == true);
    assert(parser.get_body() == "Hello, World!");
    
    std::cout << "HTTP请求解析测试通过！" << std::endl;
}

void test_response_parsing() {
    std::cout << "测试HTTP响应解析..." << std::endl;
    
    HttpResponseParser parser;
    
    std::string response_data = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: 12\r\n"
        "Connection: close\r\n"
        "\r\n"
        "Hello, Test!";
    
    int result = parser.parse(response_data.data(), response_data.size());
    assert(result > 0);
    assert(parser.is_complete());
    
    assert(parser.get_status_code() == 200);
    assert(parser.get_reason_phrase() == "OK");
    assert(parser.get_header("content-type") == "text/html");
    assert(parser.is_keep_alive() == false);
    assert(parser.get_body() == "Hello, Test!");
    
    std::cout << "HTTP响应解析测试通过！" << std::endl;
}

void test_chunked_parsing() {
    std::cout << "测试Chunked编码解析..." << std::endl;
    
    HttpResponseParser parser;
    
    std::string response_data = 
        "HTTP/1.1 200 OK\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\n"
        "Hello\r\n"
        "7\r\n"
        ", World\r\n"
        "1\r\n"
        "!\r\n"
        "0\r\n"
        "\r\n";
    
    int result = parser.parse(response_data.data(), response_data.size());
    assert(result > 0);
    assert(parser.is_complete());
    assert(parser.is_chunked() == true);
    assert(parser.get_body() == "Hello, World!");
    
    std::cout << "Chunked编码解析测试通过！" << std::endl;
}

int main() {
    std::cout << "运行HTTP解析器测试..." << std::endl;
    
    try {
        test_request_parsing();
        test_response_parsing();
        test_chunked_parsing();
        
        std::cout << "所有测试通过！" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "测试失败: " << e.what() << std::endl;
        return 1;
    }
}