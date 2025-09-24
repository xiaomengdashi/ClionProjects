#include "ChatServer.h"
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

// 构造函数
ChatServer::ChatServer(int serverPort) : serverSocket(-1), port(serverPort), running(false) {
    logMessage("聊天服务器初始化完成");
}

// 析构函数
ChatServer::~ChatServer() {
    stop();
    cleanupThreads();
}

// 启动服务器
bool ChatServer::start() {
    if (running.load()) {
        logMessage("服务器已经在运行中");
        return false;
    }
    
    if (!initSocket()) {
        logMessage("服务器套接字初始化失败");
        return false;
    }
    
    running.store(true);
    logMessage("服务器启动成功，监听端口: " + std::to_string(port));
    
    // 开始监听客户端连接
    acceptLoop();
    
    return true;
}

// 停止服务器
void ChatServer::stop() {
    if (!running.load()) {
        return;
    }
    
    running.store(false);
    
    if (serverSocket != -1) {
        close(serverSocket);
        serverSocket = -1;
    }
    
    cleanupThreads();
    logMessage("服务器已停止");
}

// 检查服务器是否在运行
bool ChatServer::isRunning() const {
    return running.load();
}

// 初始化服务器套接字
bool ChatServer::initSocket() {
    // 创建套接字
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        logMessage("创建套接字失败: " + std::string(strerror(errno)));
        return false;
    }
    
    // 设置套接字选项，允许地址重用
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        logMessage("设置套接字选项失败: " + std::string(strerror(errno)));
        close(serverSocket);
        return false;
    }
    
    // 设置服务器地址
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);
    
    // 绑定套接字
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        logMessage("绑定套接字失败: " + std::string(strerror(errno)));
        close(serverSocket);
        return false;
    }
    
    // 开始监听
    if (listen(serverSocket, 10) < 0) {
        logMessage("监听失败: " + std::string(strerror(errno)));
        close(serverSocket);
        return false;
    }
    
    return true;
}

// 监听客户端连接的主循环
void ChatServer::acceptLoop() {
    while (running.load()) {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        
        // 接受客户端连接
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        
        if (clientSocket < 0) {
            if (running.load()) {
                logMessage("接受客户端连接失败: " + std::string(strerror(errno)));
            }
            continue;
        }
        
        // 获取客户端IP地址
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
        
        logMessage("新客户端连接: " + std::string(clientIP) + ":" + 
                  std::to_string(ntohs(clientAddr.sin_port)) + 
                  " (socket: " + std::to_string(clientSocket) + ")");
        
        // 创建新线程处理客户端
        std::lock_guard<std::mutex> lock(threadsMutex);
        clientThreads.emplace_back(&ChatServer::handleClient, this, clientSocket, clientAddr);
    }
}

// 处理单个客户端连接
void ChatServer::handleClient(int clientSocket, struct sockaddr_in clientAddr) {
    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
    
    std::string clientInfo = std::string(clientIP) + ":" + std::to_string(ntohs(clientAddr.sin_port));
    
    try {
        while (running.load()) {
            // 接收客户端消息
            Message msg = receiveMessage(clientSocket);
            
            if (msg.type == MessageType::LOGOUT || msg.sender.empty()) {
                // 客户端断开连接
                break;
            }
            
            // 处理消息
            handleMessage(msg, clientSocket);
        }
    } catch (const std::exception& e) {
        logMessage("处理客户端 " + clientInfo + " 时发生异常: " + e.what());
    }
    
    // 清理客户端连接
    cleanupClient(clientSocket);
    logMessage("客户端断开连接: " + clientInfo);
}

// 处理不同类型的消息
void ChatServer::handleMessage(const Message& msg, int clientSocket) {
    switch (msg.type) {
        case MessageType::LOGIN:
            handleLogin(msg, clientSocket);
            break;
        case MessageType::REGISTER:
            handleRegister(msg, clientSocket);
            break;
        case MessageType::CHAT_PUBLIC:
            handlePublicChat(msg, clientSocket);
            break;
        case MessageType::CHAT_PRIVATE:
            handlePrivateChat(msg, clientSocket);
            break;
        case MessageType::LOGOUT:
            handleLogout(msg, clientSocket);
            break;
        case MessageType::USER_LIST:
            handleUserListRequest(msg, clientSocket);
            break;
        case MessageType::FILE_TRANSFER_REQUEST:
            handleFileTransferRequest(msg, clientSocket);
            break;
        case MessageType::FILE_TRANSFER_ACCEPT:
        case MessageType::FILE_TRANSFER_REJECT:
            handleFileTransferResponse(msg, clientSocket);
            break;
        case MessageType::FILE_UPLOAD_GROUP:
            handleGroupFileUpload(msg, clientSocket);
            break;
        case MessageType::FILE_LIST_REQUEST:
            handleFileListRequest(msg, clientSocket);
            break;
        case MessageType::FILE_DOWNLOAD_REQUEST:
            handleFileDownloadRequest(msg, clientSocket);
            break;
        default:
            logMessage("收到未知类型的消息: " + std::to_string(static_cast<int>(msg.type)));
            break;
    }
}

// 处理登录消息
void ChatServer::handleLogin(const Message& msg, int clientSocket) {
    // 消息格式: 用户名在sender中，密码在content中
    std::string username = msg.sender;
    std::string password = msg.content;
    
    bool success = userManager.loginUser(username, password, clientSocket);
    
    if (success) {
        sendResponse(clientSocket, true, "登录成功");
        logMessage("用户登录成功: " + username);
        
        // 通知其他用户有新用户上线
        Message notification(MessageType::RESPONSE, "系统", username + " 上线了");
        broadcastMessage(notification, username);
    } else {
        std::string errorMsg = "登录失败";
        if (!userManager.userExists(username)) {
            errorMsg = "用户不存在";
        } else if (userManager.isUserOnline(username)) {
            errorMsg = "用户已在线";
        } else {
            errorMsg = "密码错误";
        }
        sendResponse(clientSocket, false, errorMsg);
        logMessage("用户登录失败: " + username + " (" + errorMsg + ")");
    }
}

// 处理注册消息
void ChatServer::handleRegister(const Message& msg, int clientSocket) {
    // 消息格式: 用户名在sender中，密码在content中，邮箱在receiver中
    std::string username = msg.sender;
    std::string password = msg.content;
    std::string email = msg.receiver;
    
    bool success = userManager.registerUser(username, password, email);
    
    if (success) {
        sendResponse(clientSocket, true, "注册成功");
        logMessage("用户注册成功: " + username);
    } else {
        sendResponse(clientSocket, false, "注册失败，用户名已存在");
        logMessage("用户注册失败: " + username + " (用户名已存在)");
    }
}

// 处理群聊消息
void ChatServer::handlePublicChat(const Message& msg, int clientSocket) {
    std::string username = userManager.getUsernameBySocket(clientSocket);
    if (username.empty()) {
        sendResponse(clientSocket, false, "请先登录");
        return;
    }
    
    // 创建群聊消息并广播
    Message chatMsg(MessageType::CHAT_PUBLIC, username, msg.content);
    broadcastMessage(chatMsg);
    
    logMessage("群聊消息 [" + username + "]: " + msg.content);
}

// 处理私聊消息
void ChatServer::handlePrivateChat(const Message& msg, int clientSocket) {
    std::string senderName = userManager.getUsernameBySocket(clientSocket);
    if (senderName.empty()) {
        sendResponse(clientSocket, false, "请先登录");
        return;
    }
    
    std::string receiverName = msg.receiver;
    
    // 检查接收者是否在线
    if (!userManager.isUserOnline(receiverName)) {
        sendResponse(clientSocket, false, "用户 " + receiverName + " 不在线");
        return;
    }
    
    // 获取接收者的socket
    int receiverSocket = userManager.getSocketByUsername(receiverName);
    if (receiverSocket == -1) {
        sendResponse(clientSocket, false, "发送失败");
        return;
    }
    
    // 创建私聊消息并发送给接收者
    Message privateMsg(MessageType::CHAT_PRIVATE, senderName, receiverName, msg.content);
    
    if (sendMessage(receiverSocket, privateMsg)) {
        // 给发送者发送确认消息
        sendResponse(clientSocket, true, "私聊消息已发送给 " + receiverName);
        logMessage("私聊消息 [" + senderName + " -> " + receiverName + "]: " + msg.content);
    } else {
        sendResponse(clientSocket, false, "发送失败");
    }
}

// 处理登出消息
void ChatServer::handleLogout(const Message& msg, int clientSocket) {
    (void)msg; // 消除未使用参数警告
    std::string username = userManager.getUsernameBySocket(clientSocket);
    if (!username.empty()) {
        userManager.logoutUser(username);
        
        // 通知其他用户有用户下线
        Message notification(MessageType::RESPONSE, "系统", username + " 下线了");
        broadcastMessage(notification, username);
        
        logMessage("用户登出: " + username);
    }
}

// 处理用户列表请求
void ChatServer::handleUserListRequest(const Message& msg, int clientSocket) {
    (void)msg; // 消除未使用参数警告
    std::vector<std::string> onlineUsers = userManager.getOnlineUserList();
    
    std::string userListStr = "在线用户 (" + std::to_string(onlineUsers.size()) + "人):\\n";
    for (const auto& user : onlineUsers) {
        userListStr += "- " + user + "\\n";
    }
    
    sendResponse(clientSocket, true, userListStr);
}

// 处理文件传输请求
void ChatServer::handleFileTransferRequest(const Message& msg, int clientSocket) {
    std::string senderName = userManager.getUsernameBySocket(clientSocket);
    if (senderName.empty()) {
        sendResponse(clientSocket, false, "请先登录");
        return;
    }
    
    std::string receiverName = msg.receiver;
    std::string requestContent = msg.content; // 格式: filename|filepath
    
    // 调试输出
    logMessage("DEBUG: 收到文件传输请求 - sender: " + senderName + ", receiver: " + receiverName + ", content: [" + requestContent + "]");
    
    // 解析文件名和文件路径
    size_t separatorPos = requestContent.find('|');
    if (separatorPos == std::string::npos) {
        sendResponse(clientSocket, false, "文件传输请求格式错误，content: [" + requestContent + "]");
        return;
    }
    
    std::string filename = requestContent.substr(0, separatorPos);
    std::string filePath = requestContent.substr(separatorPos + 1);
    
    // 检查接收者是否在线
    if (!userManager.isUserOnline(receiverName)) {
        sendResponse(clientSocket, false, "用户 " + receiverName + " 不在线");
        return;
    }
    
    // 获取接收者的socket
    int receiverSocket = userManager.getSocketByUsername(receiverName);
    if (receiverSocket == -1) {
        sendResponse(clientSocket, false, "发送失败");
        return;
    }
    
    // 创建文件传输会话
    std::string sessionId = fileManager.createTransferSession(senderName, receiverName, filePath, filename);
    if (sessionId.empty()) {
        sendResponse(clientSocket, false, "创建文件传输会话失败");
        return;
    }
    
    // 转发文件传输请求给接收者，在receiver字段中包含会话ID
    Message transferRequest(MessageType::FILE_TRANSFER_REQUEST, senderName, sessionId, filename);
    
    if (sendMessage(receiverSocket, transferRequest)) {
        sendResponse(clientSocket, true, "文件传输请求已发送给 " + receiverName);
        logMessage("文件传输请求 [" + senderName + " -> " + receiverName + "]: " + filename + " (会话ID: " + sessionId + ")");
    } else {
        sendResponse(clientSocket, false, "发送失败");
    }
}

// 处理文件传输响应
void ChatServer::handleFileTransferResponse(const Message& msg, int clientSocket) {
    std::string responderName = userManager.getUsernameBySocket(clientSocket);
    if (responderName.empty()) {
        sendResponse(clientSocket, false, "请先登录");
        return;
    }
    
    std::string senderName = msg.receiver;  // 原发送者
    std::string sessionId = msg.content;    // 会话ID
    
    // 检查原发送者是否在线
    if (!userManager.isUserOnline(senderName)) {
        sendResponse(clientSocket, false, "原发送者已离线");
        return;
    }
    
    int senderSocket = userManager.getSocketByUsername(senderName);
    if (senderSocket == -1) {
        sendResponse(clientSocket, false, "响应发送失败");
        return;
    }
    
    // 处理接受或拒绝
    if (msg.type == MessageType::FILE_TRANSFER_ACCEPT) {
        if (fileManager.acceptFileTransfer(sessionId)) {
            // 通知发送者文件传输被接受
            Message response(MessageType::FILE_TRANSFER_ACCEPT, responderName, senderName, sessionId);
            sendMessage(senderSocket, response);
            
            // 获取文件传输会话信息
            FileTransferSession session = fileManager.getTransferSession(sessionId);
            if (session.isAccepted && !session.fileInfo.fileId.empty()) {
                // 读取文件内容并发送给接收者
                std::ifstream file(session.fileInfo.filePath, std::ios::binary);
                if (file.is_open()) {
                    std::string fileContent((std::istreambuf_iterator<char>(file)),
                                           std::istreambuf_iterator<char>());
                    file.close();
                    
                    // 编码文件内容
                    std::string encodedContent = encodeFileContent(fileContent);
                    
                    // 发送文件数据给接收者
                    std::string dataContent = session.fileInfo.filename + "#" + 
                                            std::to_string(fileContent.size()) + "#" + encodedContent;
                    Message fileDataMsg(MessageType::FILE_DATA, "server", responderName, dataContent);
                    sendMessage(clientSocket, fileDataMsg);
                    
                    logMessage("私聊文件发送: " + session.fileInfo.filename + " -> " + responderName);
                } else {
                    sendResponse(clientSocket, false, "文件读取失败");
                }
            }
            
            sendResponse(clientSocket, true, "已接受文件传输");
            logMessage("文件传输被接受: " + sessionId);
        } else {
            sendResponse(clientSocket, false, "接受文件传输失败");
        }
    } else if (msg.type == MessageType::FILE_TRANSFER_REJECT) {
        if (fileManager.rejectFileTransfer(sessionId)) {
            // 通知发送者文件传输被拒绝
            Message response(MessageType::FILE_TRANSFER_REJECT, responderName, senderName, sessionId);
            sendMessage(senderSocket, response);
            sendResponse(clientSocket, true, "已拒绝文件传输");
            logMessage("文件传输被拒绝: " + sessionId);
        } else {
            sendResponse(clientSocket, false, "拒绝文件传输失败");
        }
    }
}

// 处理群文件上传
void ChatServer::handleGroupFileUpload(const Message& msg, int clientSocket) {
    std::string username = userManager.getUsernameBySocket(clientSocket);
    if (username.empty()) {
        sendResponse(clientSocket, false, "请先登录");
        return;
    }
    
    // 解析文件数据消息，格式: filename#size#encoded_content
    std::string dataContent = msg.content;
    
    // 查找第一个#
    size_t firstHash = dataContent.find('#');
    if (firstHash == std::string::npos) {
        sendResponse(clientSocket, false, "文件上传数据格式错误");
        return;
    }
    
    // 查找第二个#
    size_t secondHash = dataContent.find('#', firstHash + 1);
    if (secondHash == std::string::npos) {
        sendResponse(clientSocket, false, "文件上传数据格式错误");
        return;
    }
    
    std::string filename = dataContent.substr(0, firstHash);
    std::string sizeStr = dataContent.substr(firstHash + 1, secondHash - firstHash - 1);
    std::string encodedContent = dataContent.substr(secondHash + 1);
    
    // 解码文件内容
    std::string fileContent = decodeFileContent(encodedContent);
    
    // 验证文件大小
    size_t expectedSize = std::stoul(sizeStr);
    if (fileContent.size() != expectedSize) {
        sendResponse(clientSocket, false, "文件大小不匹配，上传失败");
        return;
    }
    
    // 创建临时文件路径
    std::string tempFilePath = "./temp_files/upload_" + filename;
    
    // 保存文件内容到临时文件
    std::ofstream outFile(tempFilePath, std::ios::binary);
    if (!outFile.is_open()) {
        sendResponse(clientSocket, false, "无法创建临时文件");
        return;
    }
    
    outFile.write(fileContent.c_str(), fileContent.size());
    outFile.close();
    
    if (!outFile.good()) {
        sendResponse(clientSocket, false, "文件保存失败");
        return;
    }
    
    // 上传群文件
    if (fileManager.uploadGroupFile(username, tempFilePath, filename)) {
        sendResponse(clientSocket, true, "群文件上传成功: " + filename);
        
        // 通知所有在线用户有新的群文件
        Message notification(MessageType::RESPONSE, "系统", 
                           username + " 上传了群文件: " + filename);
        broadcastMessage(notification, username);
        
        logMessage("群文件上传 [" + username + "]: " + filename + " (" + sizeStr + " 字节)");
        
        // 删除临时文件
        unlink(tempFilePath.c_str());
    } else {
        sendResponse(clientSocket, false, "群文件上传失败");
        // 删除临时文件
        unlink(tempFilePath.c_str());
    }
}

// 处理文件列表请求
void ChatServer::handleFileListRequest(const Message& msg, int clientSocket) {
    (void)msg; // 消除未使用参数警告
    
    std::string username = userManager.getUsernameBySocket(clientSocket);
    if (username.empty()) {
        sendResponse(clientSocket, false, "请先登录");
        return;
    }
    
    // 获取群文件列表
    std::vector<FileInfo> fileList = fileManager.getGroupFileList();
    
    std::string fileListStr = "群文件列表 (" + std::to_string(fileList.size()) + "个文件):\\n";
    
    if (fileList.empty()) {
        fileListStr += "暂无群文件\\n";
        fileListStr += "提示: 使用 /upload <本地文件路径> [文件名] 命令上传文件到群";
    } else {
        fileListStr += "========================================\\n";
        for (size_t i = 0; i < fileList.size(); i++) {
            const auto& file = fileList[i];
            fileListStr += "[" + std::to_string(i + 1) + "] " + file.filename + "\\n";
            fileListStr += "    大小: " + file.getFileSizeString() + "\\n";
            fileListStr += "    上传者: " + file.uploader + "\\n";
            fileListStr += "    上传时间: " + file.getUploadTimeString() + "\\n";
            fileListStr += "    文件ID: " + file.fileId + "\\n";
            if (i < fileList.size() - 1) {
                fileListStr += "----------------------------------------\\n";
            }
        }
        fileListStr += "========================================\\n";
        fileListStr += "使用 /download <文件ID> <本地保存路径> 下载文件";
    }
    
    sendResponse(clientSocket, true, fileListStr);
}

// 处理文件下载请求
void ChatServer::handleFileDownloadRequest(const Message& msg, int clientSocket) {
    std::string username = userManager.getUsernameBySocket(clientSocket);
    if (username.empty()) {
        sendResponse(clientSocket, false, "请先登录");
        return;
    }
    
    std::string fileId = msg.content;
    std::string clientPath = msg.receiver; // 客户端想要保存的路径
    
    // 检查文件是否存在
    FileInfo fileInfo = fileManager.getFileInfo(fileId);
    if (fileInfo.fileId.empty()) {
        sendResponse(clientSocket, false, "文件不存在: " + fileId);
        return;
    }
    
    // 读取文件内容
    std::ifstream file(fileInfo.filePath, std::ios::binary);
    if (!file.is_open()) {
        sendResponse(clientSocket, false, "无法读取文件: " + fileInfo.filename);
        return;
    }
    
    // 读取整个文件内容
    std::string fileContent((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
    file.close();
    
    // 编码文件内容以避免特殊字符问题
    std::string encodedContent = encodeFileContent(fileContent);
    
    // 构建文件数据消息，在content中包含文件信息和编码后的内容
    // 格式: filename#size#encoded_content
    std::string dataContent = fileInfo.filename + "#" + std::to_string(fileContent.size()) + "#" + encodedContent;
    Message fileDataMsg(MessageType::FILE_DATA, "server", username, dataContent);
    
    // 发送文件数据给客户端
    if (sendMessage(clientSocket, fileDataMsg)) {
        sendResponse(clientSocket, true, "文件下载成功: " + fileInfo.filename);
        logMessage("文件下载 [" + username + "]: " + fileInfo.filename);
    } else {
        sendResponse(clientSocket, false, "文件发送失败");
    }
}

// 发送消息到客户端
bool ChatServer::sendMessage(int clientSocket, const Message& msg) {
    std::string data = msg.serialize() + "\n";
    
    ssize_t sent = send(clientSocket, data.c_str(), data.length(), 0);
    return sent == static_cast<ssize_t>(data.length());
}

// 接收来自客户端的消息
Message ChatServer::receiveMessage(int clientSocket) {
    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    
    ssize_t received = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    
    if (received <= 0) {
        // 连接断开或出错
        return Message(); // 返回空消息
    }
    
    std::string data(buffer, received);
    
    // 移除末尾的换行符
    if (!data.empty() && data.back() == '\n') {
        data.pop_back();
    }
    
    try {
        return Message::deserialize(data);
    } catch (const std::exception& e) {
        logMessage("消息反序列化失败: " + std::string(e.what()));
        return Message();
    }
}

// 广播消息给所有在线用户
void ChatServer::broadcastMessage(const Message& msg, const std::string& excludeUser) {
    std::vector<std::string> onlineUsers = userManager.getOnlineUserList();
    
    for (const auto& username : onlineUsers) {
        if (username != excludeUser) {
            int userSocket = userManager.getSocketByUsername(username);
            if (userSocket != -1) {
                sendMessage(userSocket, msg);
            }
        }
    }
}

// 发送响应消息
void ChatServer::sendResponse(int clientSocket, bool success, const std::string& message) {
    (void)success; // 消除未使用参数警告
    Message response(MessageType::RESPONSE, "服务器", message);
    sendMessage(clientSocket, response);
}

// 清理断开的客户端连接
void ChatServer::cleanupClient(int clientSocket) {
    // 从用户管理器中移除用户
    userManager.logoutUser(clientSocket);
    
    // 关闭套接字
    close(clientSocket);
}

// 日志输出
void ChatServer::logMessage(const std::string& message) {
    time_t now = time(nullptr);
    char timeStr[100];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    std::cout << "[" << timeStr << "] " << message << std::endl;
}

// 清理所有线程
void ChatServer::cleanupThreads() {
    std::lock_guard<std::mutex> lock(threadsMutex);
    
    for (auto& thread : clientThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    clientThreads.clear();
}

// 文件内容编码（十六进制编码）
std::string ChatServer::encodeFileContent(const std::string& content) const {
    std::ostringstream encoded;
    for (unsigned char c : content) {
        encoded << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    }
    return encoded.str();
}

// 文件内容解码（十六进制解码）
std::string ChatServer::decodeFileContent(const std::string& encoded) const {
    std::string decoded;
    for (size_t i = 0; i < encoded.length(); i += 2) {
        std::string byteStr = encoded.substr(i, 2);
        unsigned char byte = static_cast<unsigned char>(std::stoi(byteStr, nullptr, 16));
        decoded.push_back(byte);
    }
    return decoded;
}