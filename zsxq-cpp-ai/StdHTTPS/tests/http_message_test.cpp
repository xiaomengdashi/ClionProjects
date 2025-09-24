/**
 * @file http_message_test.cpp
 * @brief HTTP消息测试程序
 */

#include <iostream>
#include <cassert>
#include <string>
#include "http_message.h"

using namespace stdhttps;

void test_http_request() {
    std::cout << "测试HTTP请求消息..." << std::endl;
    
    HttpRequest request = HttpRequest::create_get("/test");
    request.set_header("Host", "example.com");
    request.set_header("User-Agent", "Test/1.0");
    
    assert(request.get_method() == HttpMethod::GET);
    assert(request.get_uri() == "/test");
    assert(request.get_header("host") == "example.com");
    
    std::string request_str = request.to_string();
    assert(request_str.find("GET /test HTTP/1.1") != std::string::npos);
    assert(request_str.find("Host: example.com") != std::string::npos);
    
    std::cout << "HTTP请求消息测试通过！" << std::endl;
}

void test_http_response() {
    std::cout << "测试HTTP响应消息..." << std::endl;
    
    HttpResponse response = HttpResponse::create_ok("Hello, World!", "text/plain");
    
    assert(response.get_status_code() == 200);
    assert(response.get_reason_phrase() == "OK");
    assert(response.get_body() == "Hello, World!");
    assert(response.get_header("content-type") == "text/plain");
    
    std::string response_str = response.to_string();
    assert(response_str.find("HTTP/1.1 200 OK") != std::string::npos);
    assert(response_str.find("Content-Type: text/plain") != std::string::npos);
    
    std::cout << "HTTP响应消息测试通过！" << std::endl;
}

void test_json_response() {
    std::cout << "测试JSON响应..." << std::endl;
    
    std::string json_data = R"({"message": "test", "status": "ok"})";
    HttpResponse response = HttpResponse::create_json(json_data);
    
    assert(response.get_status_code() == 200);
    assert(response.get_header("content-type") == "application/json");
    assert(response.get_body() == json_data);
    
    std::cout << "JSON响应测试通过！" << std::endl;
}

void test_error_response() {
    std::cout << "测试错误响应..." << std::endl;
    
    HttpResponse response = HttpResponse::create_error(404, "Not Found");
    
    assert(response.get_status_code() == 404);
    assert(response.get_reason_phrase() == "Not Found");
    assert(response.get_header("content-type") == "text/html");
    assert(response.get_body().find("404") != std::string::npos);
    
    std::cout << "错误响应测试通过！" << std::endl;
}

int main() {
    std::cout << "运行HTTP消息测试..." << std::endl;
    
    try {
        test_http_request();
        test_http_response();
        test_json_response();
        test_error_response();
        
        std::cout << "所有测试通过！" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "测试失败: " << e.what() << std::endl;
        return 1;
    }
}