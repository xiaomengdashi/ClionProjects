#include "HttpGetSession.h"
#include <sstream>

HttpGetSession::HttpGetSession(tcp::socket socket, const std::string &path)
    : HttpSessionBase(std::move(socket)), path_(path) {}

HttpGetSession::~HttpGetSession() {
    std::cout << "GET session closed" << std::endl;
}

void HttpGetSession::HandleRequest() {
    HandleDownload(path_);
}

void HttpGetSession::HandleDownload(const std::string &path) {
    UrlParser parser(path);
    
    // 获取文件参数
    std::string filename = parser.GetParam("file");

    // 获取虚拟文件大小参数
    std::string size_str = parser.GetParam("size");

    // 优先检查查询参数
    if (!filename.empty()) {
        // 处理真实文件下载
        HandleFileDownload(filename);
    } else if (!size_str.empty()) {
        try {
            size_t file_size = std::stoul(size_str);
            HandleVirtualFileDownload(file_size);
        } catch (...) {
            SendResponse("HTTP/1.1 400 Bad Request\r\n\r\n");
        }
    } else {
        // 检查是否为根路径（没有查询参数）
        std::string parsed_path = parser.path();
        if (parsed_path == "/" || parsed_path.empty()) {
            // 返回欢迎页面
            HandleRootPath();
        } else {
            SendResponse("HTTP/1.1 400 Bad Request\r\n\r\n");
        }
    }
}

void HttpGetSession::HandleFileDownload(const std::string &filename) {
    auto file_transfer_session = std::make_shared<FileTransferSession>(std::move(socket_));
    file_transfer_session->StartDownloadRealFile(filename);
}

void HttpGetSession::HandleVirtualFileDownload(size_t file_size) {
    auto file_transfer_session = std::make_shared<FileTransferSession>(std::move(socket_));
    file_transfer_session->StartDownloadVirtualFile(file_size);
}

void HttpGetSession::HandleRootPath() {
    // 生成欢迎页面HTML
    std::string html_content = R"(<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>HTTP Server - 欢迎</title>
    <style>
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            max-width: 800px;
            margin: 50px auto;
            padding: 20px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: #333;
        }
        .container {
            background: white;
            border-radius: 10px;
            padding: 40px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.3);
        }
        h1 {
            color: #667eea;
            text-align: center;
            margin-bottom: 30px;
        }
        .info {
            background: #f5f5f5;
            padding: 20px;
            border-radius: 5px;
            margin: 20px 0;
        }
        .endpoint {
            background: #e8f4f8;
            padding: 15px;
            margin: 10px 0;
            border-left: 4px solid #667eea;
            border-radius: 3px;
        }
        code {
            background: #f0f0f0;
            padding: 2px 6px;
            border-radius: 3px;
            font-family: 'Courier New', monospace;
        }
        a {
            color: #667eea;
            text-decoration: none;
        }
        a:hover {
            text-decoration: underline;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🚀 HTTP Server 运行中</h1>
        
        <div class="info">
            <h2>服务器信息</h2>
            <p><strong>地址:</strong> <code>127.0.0.1:9090</code></p>
            <p><strong>状态:</strong> <span style="color: green;">✓ 运行中</span></p>
        </div>
        
        <div class="info">
            <h2>可用端点</h2>
            
            <div class="endpoint">
                <h3>📥 下载真实文件</h3>
                <p>格式: <code>/?file=文件名</code></p>
                <p>示例: <a href="/?file=test.txt">/?file=test.txt</a></p>
            </div>
            
            <div class="endpoint">
                <h3>📦 下载虚拟文件</h3>
                <p>格式: <code>/?size=文件大小(字节)</code></p>
                <p>示例: <a href="/?size=1024">/?size=1024</a> (生成1KB文件)</p>
                <p>示例: <a href="/?size=1048576">/?size=1048576</a> (生成1MB文件)</p>
            </div>
            
            <div class="endpoint">
                <h3>📤 上传文件 (POST)</h3>
                <p>使用 POST 请求上传文件到服务器</p>
                <p>文件将保存为 <code>uploaded_file.tmp</code></p>
            </div>
        </div>
        
        <div class="info">
            <h2>技术栈</h2>
            <ul>
                <li>Boost.Asio 异步网络库</li>
                <li>C++17 标准</li>
                <li>多线程 I/O 处理 (N+1 io_context 架构)</li>
            </ul>
        </div>
    </div>
</body>
</html>)";

    // 构造HTTP响应
    std::ostringstream response;
    response << "HTTP/1.1 200 OK\r\n"
            << "Content-Type: text/html; charset=UTF-8\r\n"
            << "Content-Length: " << html_content.length() << "\r\n"
            << "Connection: close\r\n"
            << "\r\n"
            << html_content;
    
    SendResponse(response.str());
}