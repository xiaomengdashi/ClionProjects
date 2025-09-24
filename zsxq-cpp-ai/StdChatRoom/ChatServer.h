#ifndef CHAT_SERVER_H
#define CHAT_SERVER_H

#include "Message.h"
#include "UserManager.h"
#include "FileManager.h"
#include <thread>
#include <vector>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>

// 聊天服务器类 - 主服务器逻辑
class ChatServer {
private:
    int serverSocket;                           // 服务器套接字
    int port;                                   // 监听端口
    std::atomic<bool> running;                  // 服务器运行状态
    UserManager userManager;                    // 用户管理器
    FileManager fileManager;                    // 文件管理器
    std::vector<std::thread> clientThreads;    // 客户端处理线程列表
    mutable std::mutex threadsMutex;           // 线程列表互斥锁

public:
    // 构造函数
    ChatServer(int serverPort = 9999);
    
    // 析构函数
    ~ChatServer();
    
    // 启动服务器
    bool start();
    
    // 停止服务器
    void stop();
    
    // 检查服务器是否在运行
    bool isRunning() const;

private:
    // 初始化服务器套接字
    bool initSocket();
    
    // 监听客户端连接的主循环
    void acceptLoop();
    
    // 处理单个客户端连接
    void handleClient(int clientSocket, struct sockaddr_in clientAddr);
    
    // 处理不同类型的消息
    void handleMessage(const Message& msg, int clientSocket);
    void handleLogin(const Message& msg, int clientSocket);
    void handleRegister(const Message& msg, int clientSocket);
    void handlePublicChat(const Message& msg, int clientSocket);
    void handlePrivateChat(const Message& msg, int clientSocket);
    void handleLogout(const Message& msg, int clientSocket);
    void handleUserListRequest(const Message& msg, int clientSocket);
    
    // 文件传输处理函数
    void handleFileTransferRequest(const Message& msg, int clientSocket);
    void handleFileTransferResponse(const Message& msg, int clientSocket);
    void handleGroupFileUpload(const Message& msg, int clientSocket);
    void handleFileListRequest(const Message& msg, int clientSocket);
    void handleFileDownloadRequest(const Message& msg, int clientSocket);
    
    // 发送消息到客户端
    bool sendMessage(int clientSocket, const Message& msg);
    
    // 接收来自客户端的消息
    Message receiveMessage(int clientSocket);
    
    // 广播消息给所有在线用户
    void broadcastMessage(const Message& msg, const std::string& excludeUser = "");
    
    // 发送响应消息
    void sendResponse(int clientSocket, bool success, const std::string& message);
    
    // 清理断开的客户端连接
    void cleanupClient(int clientSocket);
    
    // 日志输出
    void logMessage(const std::string& message);
    
    // 清理所有线程
    void cleanupThreads();
    
    // 文件内容编码/解码
    std::string encodeFileContent(const std::string& content) const;
    std::string decodeFileContent(const std::string& encoded) const;
};

#endif // CHAT_SERVER_H