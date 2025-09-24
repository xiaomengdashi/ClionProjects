/**
 * @file client_example.cpp
 * @brief HTTP客户端使用示例
 */

#include <iostream>
#include <string>
#include <fstream>
#include <thread>
#include <future>
#include <chrono>
#include <iomanip>

// 包含StdHTTPS库头文件
#include "http_client.h"
#include "ssl_handler.h"

using namespace stdhttps;

/**
 * @brief 基本HTTP客户端示例
 */
void basic_http_client_example() {
    std::cout << "\n=== 基本HTTP客户端示例 ===" << std::endl;
    
    // 创建HTTP客户端
    HttpClient client;
    
    std::cout << "1. 执行GET请求..." << std::endl;
    HttpResult result = client.get("http://httpbin.org/get");
    
    if (result.success) {
        std::cout << "GET请求成功！" << std::endl;
        std::cout << "状态码: " << result.status_code << std::endl;
        std::cout << "响应时间: " << result.elapsed_time.count() << "ms" << std::endl;
        std::cout << "响应体长度: " << result.response.get_body_size() << " 字节" << std::endl;
        std::cout << "Content-Type: " << result.response.get_header("content-type") << std::endl;
        
        // 显示响应体的前200个字符
        std::string body = result.response.get_body();
        if (body.length() > 200) {
            std::cout << "响应体预览: " << body.substr(0, 200) << "..." << std::endl;
        } else {
            std::cout << "响应体: " << body << std::endl;
        }
    } else {
        std::cout << "GET请求失败: " << result.error_message << std::endl;
    }
    
    std::cout << "\n2. 执行POST请求..." << std::endl;
    std::string json_data = R"({
    "name": "StdHTTPS测试",
    "version": "1.0",
    "features": ["HTTP/1.1", "Keep-Alive", "Chunked", "HTTPS"]
})";
    
    result = client.post("http://httpbin.org/post", json_data, "application/json");
    
    if (result.success) {
        std::cout << "POST请求成功！" << std::endl;
        std::cout << "状态码: " << result.status_code << std::endl;
        std::cout << "响应时间: " << result.elapsed_time.count() << "ms" << std::endl;
    } else {
        std::cout << "POST请求失败: " << result.error_message << std::endl;
    }
}

/**
 * @brief 客户端配置示例
 */
void configured_client_example() {
    std::cout << "\n=== 配置客户端示例 ===" << std::endl;
    
    // 创建客户端配置
    HttpClientConfig config;
    config.connect_timeout = std::chrono::seconds(10);
    config.request_timeout = std::chrono::seconds(15);
    config.response_timeout = std::chrono::seconds(30);
    config.max_redirects = 3;
    config.follow_redirects = true;
    config.user_agent = "StdHTTPS-Example/1.0";
    config.enable_compression = true;
    config.enable_keep_alive = true;
    
    // 创建配置的客户端
    HttpClient client(config);
    
    // 设置自定义头部
    client.set_header("X-Custom-Header", "StdHTTPS-Test");
    client.set_header("Accept", "application/json");
    
    std::cout << "执行配置的客户端请求..." << std::endl;
    HttpResult result = client.get("http://httpbin.org/headers");
    
    if (result.success) {
        std::cout << "请求成功！可以看到我们的自定义头部：" << std::endl;
        std::cout << result.response.get_body() << std::endl;
    } else {
        std::cout << "请求失败: " << result.error_message << std::endl;
    }
}

/**
 * @brief 异步请求示例
 */
void async_client_example() {
    std::cout << "\n=== 异步客户端示例 ===" << std::endl;
    
    HttpClient client;
    
    std::cout << "启动多个异步请求..." << std::endl;
    
    // 异步GET请求
    auto future1 = client.async_get("http://httpbin.org/delay/1", 
        [](const HttpResult& result) {
            std::cout << "异步GET完成，状态: " << result.status_code << std::endl;
        });
    
    // 异步POST请求
    auto future2 = client.async_post("http://httpbin.org/post", 
        R"({"async": true})", 
        [](const HttpResult& result) {
            std::cout << "异步POST完成，状态: " << result.status_code << std::endl;
        });
    
    // 同时执行其他工作
    std::cout << "异步请求已启动，正在执行其他工作..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "其他工作完成，等待异步请求结果..." << std::endl;
    
    // 等待异步请求完成
    HttpResult result1 = future1.get();
    HttpResult result2 = future2.get();
    
    std::cout << "所有异步请求完成！" << std::endl;
    std::cout << "GET结果: " << (result1.success ? "成功" : "失败") << std::endl;
    std::cout << "POST结果: " << (result2.success ? "成功" : "失败") << std::endl;
}

/**
 * @brief HTTPS客户端示例
 */
void https_client_example() {
    std::cout << "\n=== HTTPS客户端示例 ===" << std::endl;
    
    // 配置SSL
    SSLConfig ssl_config;
    ssl_config.verify_peer = true;
    ssl_config.verify_hostname = true;
    
    // 创建HTTPS客户端
    HttpClient client;
    client.set_ssl_config(ssl_config);
    
    std::cout << "执行HTTPS请求..." << std::endl;
    HttpResult result = client.get("https://httpbin.org/get");
    
    if (result.success) {
        std::cout << "HTTPS请求成功！" << std::endl;
        std::cout << "状态码: " << result.status_code << std::endl;
        std::cout << "这是一个安全的HTTPS连接" << std::endl;
    } else {
        std::cout << "HTTPS请求失败: " << result.error_message << std::endl;
        std::cout << "注意：可能需要配置正确的CA证书" << std::endl;
    }
}

/**
 * @brief 文件下载示例
 */
void file_download_example() {
    std::cout << "\n=== 文件下载示例 ===" << std::endl;
    
    HttpClient client;
    
    // 设置进度回调
    auto progress_callback = [](size_t downloaded, size_t total) {
        if (total > 0) {
            double percentage = (double)downloaded / total * 100.0;
            std::cout << "下载进度: " << std::fixed << std::setprecision(1) 
                      << percentage << "% (" << downloaded << "/" << total << " 字节)\r";
        } else {
            std::cout << "已下载: " << downloaded << " 字节\r";
        }
        std::cout.flush();
    };
    
    std::cout << "开始下载测试文件..." << std::endl;
    bool success = client.download_file(
        "http://httpbin.org/json", 
        "downloaded_test.json", 
        progress_callback
    );
    
    std::cout << std::endl; // 换行
    
    if (success) {
        std::cout << "文件下载成功：downloaded_test.json" << std::endl;
        
        // 读取并显示文件内容
        std::ifstream file("downloaded_test.json");
        if (file.is_open()) {
            std::string content((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
            std::cout << "文件内容: " << content << std::endl;
            file.close();
        }
    } else {
        std::cout << "文件下载失败！" << std::endl;
    }
}

/**
 * @brief 客户端构建器示例
 */
void client_builder_example() {
    std::cout << "\n=== 客户端构建器示例 ===" << std::endl;
    
    // 使用构建器模式创建客户端
    auto client = HttpClientBuilder()
        .timeout(std::chrono::seconds(15), std::chrono::seconds(30), std::chrono::seconds(60))
        .max_redirects(5)
        .follow_redirects(true)
        .user_agent("StdHTTPS-Builder/1.0")
        .enable_compression(true)
        .enable_keep_alive(true)
        .connection_pool(10, 50)
        .header("X-API-Version", "1.0")
        .header("Accept", "application/json")
        .build();
    
    std::cout << "使用构建器创建的客户端执行请求..." << std::endl;
    HttpResult result = client->get("http://httpbin.org/headers");
    
    if (result.success) {
        std::cout << "构建器客户端请求成功！" << std::endl;
        std::cout << "可以看到我们设置的头部：" << std::endl;
        std::cout << result.response.get_body() << std::endl;
    } else {
        std::cout << "构建器客户端请求失败: " << result.error_message << std::endl;
    }
}

/**
 * @brief 连接池和性能测试示例
 */
void performance_test_example() {
    std::cout << "\n=== 性能测试示例 ===" << std::endl;
    
    HttpClient client;
    
    const int num_requests = 10;
    auto start_time = std::chrono::steady_clock::now();
    
    std::cout << "执行 " << num_requests << " 个并发请求..." << std::endl;
    
    std::vector<std::future<HttpResult>> futures;
    
    // 启动并发请求
    for (int i = 0; i < num_requests; ++i) {
        futures.push_back(client.async_get("http://httpbin.org/get?id=" + std::to_string(i)));
    }
    
    // 等待所有请求完成
    int success_count = 0;
    for (auto& future : futures) {
        HttpResult result = future.get();
        if (result.success) {
            success_count++;
        }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "性能测试结果：" << std::endl;
    std::cout << "总请求数: " << num_requests << std::endl;
    std::cout << "成功请求: " << success_count << std::endl;
    std::cout << "失败请求: " << (num_requests - success_count) << std::endl;
    std::cout << "总耗时: " << total_time.count() << "ms" << std::endl;
    std::cout << "平均耗时: " << (total_time.count() / num_requests) << "ms/请求" << std::endl;
    
    // 显示连接池统计
    ConnectionStats stats = client.get_connection_stats();
    std::cout << "连接池统计：" << std::endl;
    std::cout << "总连接数: " << stats.total_connections << std::endl;
    std::cout << "活跃连接: " << stats.active_connections << std::endl;
    std::cout << "空闲连接: " << stats.idle_connections << std::endl;
}

/**
 * @brief URL解析工具示例
 */
void url_parsing_example() {
    std::cout << "\n=== URL解析工具示例 ===" << std::endl;
    
    std::vector<std::string> test_urls = {
        "http://example.com/path?param=value",
        "https://api.example.com:8443/v1/users?id=123&name=test#section1",
        "http://localhost:8080/",
        "https://secure.example.com/api"
    };
    
    for (const auto& url : test_urls) {
        std::cout << "解析URL: " << url << std::endl;
        ParsedURL parsed = HttpClient::parse_url(url);
        
        std::cout << "  协议: " << parsed.scheme << std::endl;
        std::cout << "  主机: " << parsed.host << std::endl;
        std::cout << "  端口: " << parsed.port << std::endl;
        std::cout << "  路径: " << parsed.path << std::endl;
        std::cout << "  查询: " << parsed.query << std::endl;
        std::cout << "  片段: " << parsed.fragment << std::endl;
        std::cout << "  SSL: " << (parsed.is_ssl ? "是" : "否") << std::endl;
        std::cout << "  重构: " << parsed.to_string() << std::endl;
        std::cout << std::endl;
    }
}

int main(int argc, char* argv[]) {
    // 初始化SSL库
    SSLInitializer ssl_init;
    
    std::cout << "StdHTTPS 客户端示例程序" << std::endl;
    std::cout << "基于workflow设计思路的学习版HTTP协议栈" << std::endl;
    
    try {
        if (argc > 1) {
            std::string mode = argv[1];
            if (mode == "basic") {
                basic_http_client_example();
            } else if (mode == "config") {
                configured_client_example();
            } else if (mode == "async") {
                async_client_example();
            } else if (mode == "https") {
                https_client_example();
            } else if (mode == "download") {
                file_download_example();
            } else if (mode == "builder") {
                client_builder_example();
            } else if (mode == "performance") {
                performance_test_example();
            } else if (mode == "url") {
                url_parsing_example();
            } else if (mode == "all") {
                // 运行所有示例
                basic_http_client_example();
                configured_client_example();
                async_client_example();
                file_download_example();
                client_builder_example();
                url_parsing_example();
            } else {
                std::cout << "用法: " << argv[0] << " [basic|config|async|https|download|builder|performance|url|all]" << std::endl;
                return 1;
            }
        } else {
            // 默认运行基本示例
            basic_http_client_example();
        }
    } catch (const std::exception& e) {
        std::cerr << "示例执行出错: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}