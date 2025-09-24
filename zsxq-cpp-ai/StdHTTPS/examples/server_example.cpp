/**
 * @file server_example.cpp
 * @brief HTTP服务器使用示例
 */

#include <iostream>
#include <string>
#include <fstream>
#include <signal.h>
#include <thread>

// 包含StdHTTPS库头文件
#include "http_server.h"
#include "ssl_handler.h"

using namespace stdhttps;

// 全局变量，用于优雅关闭服务器
// std::unique_ptr<HttpServer> g_server; // Disabled due to C++11 limitations

/**
 * @brief 信号处理函数
 */
void signal_handler(int signal) {
    std::cout << "\n收到信号 " << signal << "，正在关闭服务器..." << std::endl;
    // Note: g_server disabled due to C++11 limitations
    // Manual shutdown required
}

/**
 * @brief 创建自签名证书用于HTTPS测试
 */
bool create_test_certificates() {
    std::cout << "创建测试用的自签名证书..." << std::endl;
    
    bool success = SSLUtils::generate_self_signed_cert(
        "server.crt",       // 证书文件
        "server.key",       // 私钥文件
        365,                // 有效期365天
        "CN",               // 国家
        "StdHTTPS Test",    // 组织
        "localhost"         // 通用名称
    );
    
    if (success) {
        std::cout << "测试证书创建成功：server.crt, server.key" << std::endl;
    } else {
        std::cout << "测试证书创建失败！" << std::endl;
    }
    
    return success;
}

/**
 * @brief 基本的HTTP服务器示例
 */
void basic_http_server_example() {
    std::cout << "\n=== 基本HTTP服务器示例 ===" << std::endl;
    
    // 创建HTTP服务器配置
    HttpServerConfig config;
    config.bind_address = "127.0.0.1";
    config.port = 8080;
    config.worker_threads = 4;
    config.max_connections = 100;
    
    // 创建服务器实例
    HttpServer server(config);
    
    // 添加路由处理器
    server.get("/", [](const HttpRequest& request, HttpResponse& response) {
        std::cout << "处理根路径请求，来自: " << request.get_header("host") << std::endl;
        
        std::string html = R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>StdHTTPS 测试页面</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        .container { max-width: 800px; margin: 0 auto; }
        .header { color: #333; border-bottom: 2px solid #ddd; padding-bottom: 10px; }
        .content { margin: 20px 0; }
        .code { background: #f4f4f4; padding: 10px; border-radius: 4px; }
    </style>
</head>
<body>
    <div class="container">
        <h1 class="header">欢迎使用 StdHTTPS!</h1>
        <div class="content">
            <p>这是一个学习版的HTTP协议栈实现，支持以下特性：</p>
            <ul>
                <li>HTTP/1.1 协议支持</li>
                <li>Keep-Alive 连接复用</li>
                <li>Chunked 传输编码</li>
                <li>HTTPS/TLS 支持</li>
                <li>多线程处理</li>
                <li>路由管理</li>
            </ul>
            <h3>测试API:</h3>
            <ul>
                <li><a href="/hello">GET /hello</a> - 简单问候</li>
                <li><a href="/json">GET /json</a> - JSON响应</li>
                <li><a href="/info">GET /info</a> - 服务器信息</li>
            </ul>
        </div>
    </div>
</body>
</html>)";
        
        response.set_status_code(200);
        response.set_header("Content-Type", "text/html; charset=utf-8");
        response.set_body(html);
        response.update_content_length();
    });
    
    // Hello世界API
    server.get("/hello", [](const HttpRequest& request, HttpResponse& response) {
        std::string name = request.get_query_param("name");
        if (name.empty()) {
            name = "世界";
        }
        
        std::string message = "你好，" + name + "！";
        response = HttpResponse::create_ok(message, "text/plain; charset=utf-8");
        std::cout << "Hello API调用，参数name=" << name << std::endl;
    });
    
    // JSON API示例
    server.get("/json", [](const HttpRequest& request, HttpResponse& response) {
        std::string json = R"({
    "message": "这是一个JSON响应",
    "timestamp": ")" + std::to_string(time(nullptr)) + R"(",
    "server": "StdHTTPS/1.0",
    "features": [
        "HTTP/1.1",
        "Keep-Alive",
        "Chunked Transfer",
        "HTTPS/TLS"
    ]
})";
        
        response = HttpResponse::create_json(json);
        std::cout << "JSON API调用" << std::endl;
    });
    
    // 服务器信息API
    server.get("/info", [&server](const HttpRequest& request, HttpResponse& response) {
        HttpServerStats stats = server.get_stats();
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - stats.start_time);
        
        std::string info = "服务器信息:\n";
        info += "运行时间: " + std::to_string(uptime.count()) + " 秒\n";
        info += "总连接数: " + std::to_string(stats.total_connections.load()) + "\n";
        info += "活跃连接: " + std::to_string(stats.active_connections.load()) + "\n";
        info += "总请求数: " + std::to_string(stats.total_requests.load()) + "\n";
        info += "成功请求: " + std::to_string(stats.successful_requests.load()) + "\n";
        info += "失败请求: " + std::to_string(stats.failed_requests.load()) + "\n";
        
        response = HttpResponse::create_ok(info, "text/plain; charset=utf-8");
        std::cout << "信息API调用" << std::endl;
    });
    
    // POST API示例
    server.post("/echo", [](const HttpRequest& request, HttpResponse& response) {
        std::string body = request.get_body();
        std::cout << "Echo API收到数据: " << body.substr(0, 100) 
                  << (body.length() > 100 ? "..." : "") << std::endl;
        
        std::string echo_response = "收到的数据:\n" + body;
        response = HttpResponse::create_ok(echo_response, "text/plain; charset=utf-8");
    });
    
    // 设置默认处理器（404错误）
    server.set_default_handler([](const HttpRequest& request, HttpResponse& response) {
        std::cout << "404请求: " << request.get_method_string() << " " 
                  << request.get_path() << std::endl;
        
        std::string html = R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>404 - 页面未找到</title>
</head>
<body>
    <h1>404 - 页面未找到</h1>
    <p>请求的路径不存在: )" + request.get_path() + R"(</p>
    <p><a href="/">返回首页</a></p>
</body>
</html>)";
        
        response = HttpResponse::create_error(404, "Not Found");
        response.set_body(html);
        response.set_header("Content-Type", "text/html; charset=utf-8");
        response.update_content_length();
    });
    
    // 启动服务器
    if (server.start()) {
        std::cout << "HTTP服务器启动成功，访问: http://127.0.0.1:8080" << std::endl;
        std::cout << "按Ctrl+C停止服务器" << std::endl;
        
        // 保存全局服务器引用
        // 等待服务器关闭
        server.wait_for_shutdown();
        
        std::cout << "HTTP服务器已关闭" << std::endl;
    } else {
        std::cerr << "HTTP服务器启动失败！" << std::endl;
    }
}

/**
 * @brief HTTPS服务器示例
 */
void https_server_example() {
    std::cout << "\n=== HTTPS服务器示例 ===" << std::endl;
    
    // 创建测试证书
    if (!create_test_certificates()) {
        std::cerr << "无法创建测试证书，跳过HTTPS示例" << std::endl;
        return;
    }
    
    // 配置SSL
    SSLConfig ssl_config;
    ssl_config.cert_file = "server.crt";
    ssl_config.key_file = "server.key";
    ssl_config.verify_peer = false; // 测试环境不验证客户端证书
    
    // 创建HTTPS服务器配置
    HttpServerConfig config;
    config.bind_address = "127.0.0.1";
    config.port = 8443;
    config.worker_threads = 4;
    config.enable_ssl = true;
    config.ssl_config = ssl_config;
    
    // 创建服务器实例
    HttpServer server(config);
    
    // 添加HTTPS专用路由
    server.get("/", [](const HttpRequest& request, HttpResponse& response) {
        std::string html = R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>HTTPS 测试页面</title>
    <style>body { font-family: Arial, sans-serif; margin: 40px; color: #333; }</style>
</head>
<body>
    <h1>🔒 HTTPS 连接成功！</h1>
    <p>这是一个安全的HTTPS连接。</p>
    <p>证书信息：自签名测试证书</p>
    <p>加密状态：TLS加密</p>
</body>
</html>)";
        
        response.set_status_code(200);
        response.set_header("Content-Type", "text/html; charset=utf-8");
        response.set_body(html);
        response.update_content_length();
    });
    
    server.get("/secure", [](const HttpRequest& request, HttpResponse& response) {
        response = HttpResponse::create_json(R"({
    "message": "这是一个安全的API端点",
    "encrypted": true,
    "protocol": "HTTPS"
})");
    });
    
    // 启动HTTPS服务器
    if (server.start()) {
        std::cout << "HTTPS服务器启动成功，访问: https://127.0.0.1:8443" << std::endl;
        std::cout << "注意：浏览器会显示证书警告，这是正常的（测试用自签名证书）" << std::endl;
        std::cout << "按Ctrl+C停止服务器" << std::endl;
        
        // 等待服务器关闭
        server.wait_for_shutdown();
        
        std::cout << "HTTPS服务器已关闭" << std::endl;
    } else {
        std::cerr << "HTTPS服务器启动失败！" << std::endl;
    }
}

/**
 * @brief 使用构建器模式创建服务器
 */
void server_builder_example() {
    std::cout << "\n=== 服务器构建器示例 ===" << std::endl;
    
    // 使用构建器模式创建服务器
    auto server = HttpServerBuilder()
        .bind("127.0.0.1", 8090)
        .threads(6)
        .max_connections(200)
        .keep_alive_timeout(std::chrono::seconds(120))
        .request_timeout(std::chrono::seconds(60))
        .enable_chunked(true)
        .chunk_size(4096)
        .build();
    
    // 添加一些示例路由
    server->get("/builder", [](const HttpRequest& request, HttpResponse& response) {
        response = HttpResponse::create_ok("通过构建器创建的服务器！", "text/plain; charset=utf-8");
    });
    
    // 添加chunked响应示例
    server->get("/chunked", [](const HttpRequest& request, HttpResponse& response) {
        response.set_status_code(200);
        response.set_header("Content-Type", "text/plain; charset=utf-8");
        response.set_chunked(true);
        
        // 模拟分块数据
        std::string data;
        for (int i = 1; i <= 5; ++i) {
            data += "数据块 " + std::to_string(i) + "\n";
        }
        response.set_body(data);
    });
    
    if (server->start()) {
        std::cout << "构建器服务器启动成功，访问: http://127.0.0.1:8090" << std::endl;
        // g_server disabled due to C++11 limitations
        server->wait_for_shutdown();
    }
}

int main(int argc, char* argv[]) {
    // 初始化SSL库
    SSLInitializer ssl_init;
    
    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "StdHTTPS 服务器示例程序" << std::endl;
    std::cout << "基于workflow设计思路的学习版HTTP协议栈" << std::endl;
    
    if (argc > 1) {
        std::string mode = argv[1];
        if (mode == "https") {
            https_server_example();
        } else if (mode == "builder") {
            server_builder_example();
        } else {
            std::cout << "用法: " << argv[0] << " [http|https|builder]" << std::endl;
            return 1;
        }
    } else {
        basic_http_server_example();
    }
    
    return 0;
}