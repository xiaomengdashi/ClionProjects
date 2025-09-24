#ifndef CHAT_CLIENT_H
#define CHAT_CLIENT_H

#include "Message.h"
#include <thread>
#include <atomic>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>

// 聊天客户端类 - 客户端主逻辑
class ChatClient {
private:
    int clientSocket;                   // 客户端套接字
    std::string serverIP;               // 服务器IP地址
    int serverPort;                     // 服务器端口
    std::atomic<bool> connected;        // 连接状态
    std::atomic<bool> loggedIn;         // 登录状态
    std::string currentUser;            // 当前登录用户名
    std::string pendingDownloadPath;    // 待下载文件的本地路径
    bool awaitingFileResponse;          // 是否正在等待文件传输响应
    std::string pendingSessionId;       // 待响应的会话ID
    std::string pendingSender;          // 待响应的发送者
    std::string pendingFilename;        // 待响应的文件名
    std::thread receiveThread;          // 接收消息线程

public:
    // 构造函数
    ChatClient(const std::string& ip = "127.0.0.1", int port = 9999);
    
    // 析构函数
    ~ChatClient();
    
    // 连接到服务器
    bool connectToServer();
    
    // 断开连接
    void disconnect();
    
    // 用户注册
    bool registerUser(const std::string& username, const std::string& password, 
                     const std::string& email = "");
    
    // 用户登录
    bool loginUser(const std::string& username, const std::string& password);
    
    // 用户登出
    void logoutUser();
    
    // 发送群聊消息
    bool sendPublicMessage(const std::string& content);
    
    // 发送私聊消息
    bool sendPrivateMessage(const std::string& receiver, const std::string& content);
    
    // 请求在线用户列表
    void requestUserList();
    
    // 文件传输功能
    bool sendFileToUser(const std::string& receiver, const std::string& filePath);
    bool uploadGroupFile(const std::string& filePath);
    void requestGroupFileList();
    bool downloadGroupFile(const std::string& fileId, const std::string& localPath);
    void acceptFileTransfer(const std::string& sessionId, const std::string& sender, const std::string& savePath = ".");
    void rejectFileTransfer(const std::string& sessionId, const std::string& sender);
    
    // 运行客户端主循环
    void run();
    
    // 检查连接状态
    bool isConnected() const;
    
    // 检查登录状态
    bool isLoggedIn() const;
    
    // 获取当前用户名
    std::string getCurrentUser() const;

private:
    // 初始化套接字
    bool initSocket();
    
    // 发送消息到服务器
    bool sendMessage(const Message& msg);
    
    // 接收服务器消息线程函数
    void receiveMessages();
    
    // 处理接收到的消息
    void handleReceivedMessage(const Message& msg);
    
    // 处理文件传输相关消息
    void handleFileTransferRequest(const Message& msg);
    void handleFileTransferResponse(const Message& msg);
    void handleFileData(const Message& msg);
    
    // 显示菜单
    void showMenu();
    void showMainMenu();
    void showChatMenu();
    
    // 处理用户输入
    void handleUserInput();
    void handleLoginRegister();
    void handleChatInput();
    
    // 显示消息
    void displayMessage(const std::string& message);
    void displayPublicMessage(const Message& msg);
    void displayPrivateMessage(const Message& msg);
    void displaySystemMessage(const std::string& message);
    
    // 输入验证
    bool isValidUsername(const std::string& username) const;
    bool isValidPassword(const std::string& password) const;
    
    // 工具函数
    std::string trim(const std::string& str) const;
    std::string getCurrentTime() const;
    std::string extractFilename(const std::string& filePath) const;
    bool fileExists(const std::string& filePath) const;
    std::size_t getFileSize(const std::string& filePath) const;
    std::string encodeFileContent(const std::string& content) const;
    std::string decodeFileContent(const std::string& encoded) const;
    
    // 密码隐藏输入函数
    std::string getHiddenPassword(const std::string& prompt) const;
};

#endif // CHAT_CLIENT_H