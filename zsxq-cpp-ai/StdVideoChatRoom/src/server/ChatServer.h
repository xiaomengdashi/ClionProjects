#ifndef CHAT_SERVER_H
#define CHAT_SERVER_H

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <memory>
#include <thread>
#include "WebSocketHandler.h"

/**
 * 聊天服务器主类
 * 负责提供HTTPS服务和静态文件服务
 */
class ChatServer {
public:
    ChatServer(const std::string& cert_path, const std::string& key_path);
    ~ChatServer();

    /**
     * 启动服务器
     * @param http_port HTTPS端口
     * @param ws_port WebSocket端口
     */
    void start(int http_port, int ws_port);

    /**
     * 停止服务器
     */
    void stop();

private:
    /**
     * 设置HTTP路由
     */
    void setupRoutes();

    /**
     * 获取文件的MIME类型
     */
    std::string getMimeType(const std::string& path);

    /**
     * 处理文件上传
     */
    void handleFileUpload(const httplib::Request& req, httplib::Response& res);

    /**
     * 处理文件下载
     */
    void handleFileDownload(const httplib::Request& req, httplib::Response& res);

    /**
     * 获取房间文件列表
     */
    void handleFileList(const httplib::Request& req, httplib::Response& res);

    /**
     * 验证用户权限
     */
    bool validateUserPermission(const httplib::Request& req, const std::string& roomId, std::string& userId);

private:
    httplib::SSLServer* https_server_;
    std::unique_ptr<WebSocketHandler> ws_handler_;
    std::unique_ptr<std::thread> ws_thread_;
    std::string public_dir_;
};

#endif // CHAT_SERVER_H 