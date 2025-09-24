#include "ChatClient.h"
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <sys/stat.h>
#include <fstream>
#include <termios.h>

// 构造函数
ChatClient::ChatClient(const std::string& ip, int port) 
    : clientSocket(-1), serverIP(ip), serverPort(port),
      connected(false), loggedIn(false), awaitingFileResponse(false) {
}

// 析构函数
ChatClient::~ChatClient() {
    disconnect();
}

// 连接到服务器
bool ChatClient::connectToServer() {
    if (connected.load()) {
        std::cout << "已经连接到服务器" << std::endl;
        return true;
    }
    
    if (!initSocket()) {
        std::cout << "初始化套接字失败" << std::endl;
        return false;
    }
    
    // 设置服务器地址
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    
    if (inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cout << "无效的服务器IP地址: " << serverIP << std::endl;
        close(clientSocket);
        return false;
    }
    
    // 连接到服务器
    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cout << "连接服务器失败: " << strerror(errno) << std::endl;
        close(clientSocket);
        return false;
    }
    
    connected.store(true);
    
    // 启动接收消息线程
    receiveThread = std::thread(&ChatClient::receiveMessages, this);
    
    std::cout << "成功连接到服务器 " << serverIP << ":" << serverPort << std::endl;
    return true;
}

// 断开连接
void ChatClient::disconnect() {
    if (!connected.load()) {
        return;
    }
    
    connected.store(false);
    loggedIn.store(false);
    
    if (clientSocket != -1) {
        close(clientSocket);
        clientSocket = -1;
    }
    
    if (receiveThread.joinable()) {
        receiveThread.join();
    }
    
    std::cout << "已断开与服务器的连接" << std::endl;
}

// 用户注册
bool ChatClient::registerUser(const std::string& username, const std::string& password, 
                             const std::string& email) {
    if (!connected.load()) {
        std::cout << "请先连接到服务器" << std::endl;
        return false;
    }
    
    // 创建注册消息
    Message registerMsg(MessageType::REGISTER, username, email, password);
    
    if (sendMessage(registerMsg)) {
        std::cout << "注册请求已发送，等待服务器响应..." << std::endl;
        return true;
    } else {
        std::cout << "发送注册请求失败" << std::endl;
        return false;
    }
}

// 用户登录
bool ChatClient::loginUser(const std::string& username, const std::string& password) {
    if (!connected.load()) {
        std::cout << "请先连接到服务器" << std::endl;
        return false;
    }
    
    if (loggedIn.load()) {
        std::cout << "已经登录，当前用户: " << currentUser << std::endl;
        return true;
    }
    
    // 创建登录消息
    Message loginMsg(MessageType::LOGIN, username, password);
    
    if (sendMessage(loginMsg)) {
        std::cout << "登录请求已发送，等待服务器响应..." << std::endl;
        currentUser = username; // 临时设置，等待服务器确认
        return true;
    } else {
        std::cout << "发送登录请求失败" << std::endl;
        return false;
    }
}

// 用户登出
void ChatClient::logoutUser() {
    if (!loggedIn.load()) {
        std::cout << "当前未登录" << std::endl;
        return;
    }
    
    // 发送登出消息
    Message logoutMsg(MessageType::LOGOUT, currentUser, "");
    sendMessage(logoutMsg);
    
    loggedIn.store(false);
    currentUser.clear();
    
    std::cout << "已登出" << std::endl;
}

// 发送群聊消息
bool ChatClient::sendPublicMessage(const std::string& content) {
    if (!loggedIn.load()) {
        std::cout << "请先登录" << std::endl;
        return false;
    }
    
    if (content.empty()) {
        std::cout << "消息内容不能为空" << std::endl;
        return false;
    }
    
    Message chatMsg(MessageType::CHAT_PUBLIC, currentUser, content);
    return sendMessage(chatMsg);
}

// 发送私聊消息
bool ChatClient::sendPrivateMessage(const std::string& receiver, const std::string& content) {
    if (!loggedIn.load()) {
        std::cout << "请先登录" << std::endl;
        return false;
    }
    
    if (receiver.empty() || content.empty()) {
        std::cout << "接收者和消息内容不能为空" << std::endl;
        return false;
    }
    
    if (receiver == currentUser) {
        std::cout << "不能给自己发私聊消息" << std::endl;
        return false;
    }
    
    Message privateMsg(MessageType::CHAT_PRIVATE, currentUser, receiver, content);
    return sendMessage(privateMsg);
}

// 请求在线用户列表
void ChatClient::requestUserList() {
    if (!loggedIn.load()) {
        std::cout << "请先登录" << std::endl;
        return;
    }
    
    Message userListMsg(MessageType::USER_LIST, currentUser, "");
    sendMessage(userListMsg);
}

// 发送文件给指定用户
bool ChatClient::sendFileToUser(const std::string& receiver, const std::string& filePath) {
    if (!loggedIn.load()) {
        std::cout << "请先登录" << std::endl;
        return false;
    }
    
    if (!fileExists(filePath)) {
        std::cout << "文件不存在: " << filePath << std::endl;
        return false;
    }
    
    std::string filename = extractFilename(filePath);
    
    // 发送文件传输请求，在content中同时包含文件名和路径
    // 格式: filename|filepath
    std::string requestContent = filename + "|" + filePath;
    Message fileRequest(MessageType::FILE_TRANSFER_REQUEST, currentUser, receiver, requestContent);
    
    if (sendMessage(fileRequest)) {
        std::cout << "文件传输请求已发送给 " << receiver << ": " << filename << std::endl;
        return true;
    } else {
        std::cout << "发送文件传输请求失败" << std::endl;
        return false;
    }
}

// 上传群文件
bool ChatClient::uploadGroupFile(const std::string& filePath) {
    if (!loggedIn.load()) {
        std::cout << "请先登录" << std::endl;
        return false;
    }
    
    if (!fileExists(filePath)) {
        std::cout << "文件不存在: " << filePath << std::endl;
        return false;
    }
    
    std::string filename = extractFilename(filePath);
    
    // 读取文件内容
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cout << "无法读取文件: " << filePath << std::endl;
        return false;
    }
    
    std::string fileContent((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
    file.close();
    
    // 编码文件内容
    std::string encodedContent = encodeFileContent(fileContent);
    
    // 构建文件数据消息，格式: filename#size#encoded_content
    std::string dataContent = filename + "#" + std::to_string(fileContent.size()) + "#" + encodedContent;
    
    // 发送群文件上传请求，使用FILE_DATA消息类型
    Message uploadMsg(MessageType::FILE_UPLOAD_GROUP, currentUser, "GROUP_UPLOAD", dataContent);
    
    if (sendMessage(uploadMsg)) {
        std::cout << "群文件上传请求已发送: " << filename << " (" << fileContent.size() << " 字节)" << std::endl;
        return true;
    } else {
        std::cout << "发送群文件上传请求失败" << std::endl;
        return false;
    }
}

// 请求群文件列表
void ChatClient::requestGroupFileList() {
    if (!loggedIn.load()) {
        std::cout << "请先登录" << std::endl;
        return;
    }
    
    Message fileListMsg(MessageType::FILE_LIST_REQUEST, currentUser, "");
    sendMessage(fileListMsg);
}

// 下载群文件
bool ChatClient::downloadGroupFile(const std::string& fileId, const std::string& localPath) {
    if (!loggedIn.load()) {
        std::cout << "请先登录" << std::endl;
        return false;
    }
    
    // 保存待下载文件的本地路径
    pendingDownloadPath = localPath;
    
    // 发送文件下载请求
    Message downloadMsg(MessageType::FILE_DOWNLOAD_REQUEST, currentUser, localPath, fileId);
    
    if (sendMessage(downloadMsg)) {
        std::cout << "文件下载请求已发送，文件ID: " << fileId << std::endl;
        return true;
    } else {
        std::cout << "发送文件下载请求失败" << std::endl;
        return false;
    }
}

// 接受文件传输
void ChatClient::acceptFileTransfer(const std::string& sessionId, const std::string& sender, const std::string& savePath) {
    if (!loggedIn.load()) {
        std::cout << "请先登录" << std::endl;
        return;
    }
    
    // 保存私聊文件传输的路径
    pendingDownloadPath = savePath;
    
    Message acceptMsg(MessageType::FILE_TRANSFER_ACCEPT, currentUser, sender, sessionId);
    if (sendMessage(acceptMsg)) {
        std::cout << "已接受来自 " << sender << " 的文件传输" << std::endl;
    } else {
        std::cout << "接受文件传输失败" << std::endl;
    }
}

// 拒绝文件传输
void ChatClient::rejectFileTransfer(const std::string& sessionId, const std::string& sender) {
    if (!loggedIn.load()) {
        std::cout << "请先登录" << std::endl;
        return;
    }
    
    Message rejectMsg(MessageType::FILE_TRANSFER_REJECT, currentUser, sender, sessionId);
    if (sendMessage(rejectMsg)) {
        std::cout << "已拒绝来自 " << sender << " 的文件传输" << std::endl;
    } else {
        std::cout << "拒绝文件传输失败" << std::endl;
    }
}

// 运行客户端主循环
void ChatClient::run() {
    std::cout << "欢迎使用C++聊天室客户端" << std::endl;
    std::cout << "正在连接到服务器..." << std::endl;
    
    if (!connectToServer()) {
        std::cout << "连接服务器失败，程序退出" << std::endl;
        return;
    }
    
    while (connected.load()) {
        if (!loggedIn.load()) {
            handleLoginRegister();
            // 给接收线程一点时间处理登录响应
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } else {
            // 登录后的聊天循环
            while (connected.load() && loggedIn.load()) {
                handleChatInput();
            }
        }
    }
}

// 检查连接状态
bool ChatClient::isConnected() const {
    return connected.load();
}

// 检查登录状态
bool ChatClient::isLoggedIn() const {
    return loggedIn.load();
}

// 获取当前用户名
std::string ChatClient::getCurrentUser() const {
    return currentUser;
}

// 初始化套接字
bool ChatClient::initSocket() {
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        std::cout << "创建套接字失败: " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

// 发送消息到服务器
bool ChatClient::sendMessage(const Message& msg) {
    if (!connected.load() || clientSocket == -1) {
        return false;
    }
    
    std::string data = msg.serialize() + "\n";
    
    ssize_t sent = send(clientSocket, data.c_str(), data.length(), 0);
    return sent == static_cast<ssize_t>(data.length());
}

// 接收服务器消息线程函数
void ChatClient::receiveMessages() {
    char buffer[8192];
    std::string receivedData;
    
    while (connected.load()) {
        memset(buffer, 0, sizeof(buffer));
        
        ssize_t received = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        
        if (received <= 0) {
            if (connected.load()) {
                std::cout << "\n服务器连接断开" << std::endl;
                connected.store(false);
                loggedIn.store(false);
            }
            break;
        }
        
        // 将接收到的数据添加到缓冲区
        receivedData.append(buffer, received);
        
        // 处理所有完整的消息（以换行符分隔）
        size_t pos = 0;
        while ((pos = receivedData.find('\n')) != std::string::npos) {
            std::string messageData = receivedData.substr(0, pos);
            receivedData.erase(0, pos + 1);
            
            if (!messageData.empty()) {
                try {
                    Message msg = Message::deserialize(messageData);
                    handleReceivedMessage(msg);
                } catch (const std::exception& e) {
                    std::cout << "接收消息解析失败: " << e.what() << std::endl;
                    std::cout << "原始数据: " << messageData << std::endl;
                }
            }
        }
    }
}

// 处理接收到的消息
void ChatClient::handleReceivedMessage(const Message& msg) {
    switch (msg.type) {
        case MessageType::RESPONSE:
            // 服务器响应消息
            if (msg.sender == "服务器") {
                // 检查是否为群文件列表响应或用户列表响应
                if (msg.content.find("群文件列表") != std::string::npos || 
                    msg.content.find("在线用户") != std::string::npos) {
                    // 将 \\n 转换为真正的换行符
                    std::string displayContent = msg.content;
                    size_t pos = 0;
                    while ((pos = displayContent.find("\\n", pos)) != std::string::npos) {
                        displayContent.replace(pos, 2, "\n");
                        pos += 1;
                    }
                    std::cout << "\n" << displayContent << std::endl;
                } else {
                    std::cout << "\n[服务器] " << msg.content << std::endl;
                }
                
                // 检查是否为登录成功响应
                if (msg.content == "登录成功") {
                    loggedIn.store(true);
                    std::cout << "欢迎, " << currentUser << "!" << std::endl;
                    showChatMenu();
                    std::cout << "> ";
                    std::cout.flush();
                } else if (msg.content.find("登录失败") != std::string::npos) {
                    currentUser.clear();
                    loggedIn.store(false);
                }
            } else {
                displaySystemMessage("[" + msg.sender + "] " + msg.content);
            }
            break;
            
        case MessageType::CHAT_PUBLIC:
            displayPublicMessage(msg);
            break;
            
        case MessageType::CHAT_PRIVATE:
            displayPrivateMessage(msg);
            break;
            
        case MessageType::FILE_TRANSFER_REQUEST:
            handleFileTransferRequest(msg);
            break;
            
        case MessageType::FILE_TRANSFER_ACCEPT:
        case MessageType::FILE_TRANSFER_REJECT:
            handleFileTransferResponse(msg);
            break;
            
        case MessageType::FILE_DATA:
            handleFileData(msg);
            break;
            
        default:
            std::cout << "\n收到未知类型消息" << std::endl;
            break;
    }
    
    // 显示提示符
    if (loggedIn.load() && !awaitingFileResponse) {
        // 如果是服务器响应且不是登录相关的，显示提示符
        if (msg.type == MessageType::RESPONSE && msg.sender == "服务器" && 
            msg.content != "登录成功" && msg.content.find("登录失败") == std::string::npos) {
            std::cout << "\n> ";
            std::cout.flush();
        } else if (msg.type != MessageType::RESPONSE) {
            // 对于聊天消息，显示提示符
            std::cout << "\n> ";
            std::cout.flush();
        }
    }
}

// 处理文件传输请求
void ChatClient::handleFileTransferRequest(const Message& msg) {
    std::string sender = msg.sender;
    std::string filename = msg.content;
    std::string sessionId = msg.receiver; // 会话ID存储在receiver字段
    
    std::cout << "\n[文件传输] " << sender << " 想要发送文件给你: " << filename << std::endl;
    std::cout << "是否接受? (y/n): ";
    std::cout.flush();
    
    // 设置等待文件传输响应状态
    awaitingFileResponse = true;
    pendingSessionId = sessionId;
    pendingSender = sender;
    pendingFilename = filename;
}

// 处理文件传输响应
void ChatClient::handleFileTransferResponse(const Message& msg) {
    std::string responder = msg.sender;
    std::string sessionId = msg.content;
    
    if (msg.type == MessageType::FILE_TRANSFER_ACCEPT) {
        std::cout << "\n[文件传输] " << responder << " 接受了你的文件传输请求 (会话ID: " << sessionId << ")" << std::endl;
    } else if (msg.type == MessageType::FILE_TRANSFER_REJECT) {
        std::cout << "\n[文件传输] " << responder << " 拒绝了你的文件传输请求 (会话ID: " << sessionId << ")" << std::endl;
    }
}

// 处理文件数据
void ChatClient::handleFileData(const Message& msg) {
    // 解析文件数据消息，格式: filename#size#encoded_content
    std::string dataContent = msg.content;
    
    // 查找第一个#
    size_t firstHash = dataContent.find('#');
    if (firstHash == std::string::npos) {
        std::cout << "\n[文件下载] 文件数据格式错误" << std::endl;
        return;
    }
    
    // 查找第二个#
    size_t secondHash = dataContent.find('#', firstHash + 1);
    if (secondHash == std::string::npos) {
        std::cout << "\n[文件下载] 文件数据格式错误" << std::endl;
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
        std::cout << "\n[文件下载] 文件大小不匹配，期望: " << expectedSize << ", 实际: " << fileContent.size() << std::endl;
        return;
    }
    
    // 确定保存路径
    std::string savePath = pendingDownloadPath;
    if (savePath.empty()) {
        savePath = filename; // 默认使用原文件名
    } else if (savePath == ".") {
        savePath = filename; // 当前目录下使用原文件名
    } else {
        // 检查路径是否为目录
        struct stat pathStat;
        if (stat(savePath.c_str(), &pathStat) == 0 && S_ISDIR(pathStat.st_mode)) {
            savePath = savePath + "/" + filename;
        }
    }
    
    // 保存文件到本地
    std::ofstream outFile(savePath, std::ios::binary);
    if (!outFile.is_open()) {
        std::cout << "\n[文件下载] 无法创建文件: " << savePath << std::endl;
        return;
    }
    
    outFile.write(fileContent.c_str(), fileContent.size());
    outFile.close();
    
    if (outFile.good()) {
        std::cout << "\n[文件下载] 文件保存成功: " << savePath << " (" << fileContent.size() << " 字节)" << std::endl;
    } else {
        std::cout << "\n[文件下载] 文件保存失败" << std::endl;
    }
    
    // 清空待下载路径
    pendingDownloadPath.clear();
}

// 显示菜单
void ChatClient::showMenu() {
    std::cout << "\n=== C++聊天室客户端 ===" << std::endl;
    std::cout << "1. 登录" << std::endl;
    std::cout << "2. 注册" << std::endl;
    std::cout << "3. 退出" << std::endl;
    std::cout << "请选择: ";
}

void ChatClient::showMainMenu() {
    showMenu();
}

void ChatClient::showChatMenu() {
    std::cout << "\n=== 聊天室命令 ===" << std::endl;
    std::cout << "输入消息发送群聊" << std::endl;
    std::cout << "@用户名 消息内容 - 发送私聊" << std::endl;
    std::cout << "/list - 查看在线用户" << std::endl;
    std::cout << "/files - 查看群文件列表" << std::endl;
    std::cout << "/upload 文件路径 - 上传群文件" << std::endl;
    std::cout << "/download 文件ID 本地路径 - 下载群文件" << std::endl;
    std::cout << "/send 用户名 文件路径 - 发送文件给用户" << std::endl;
    std::cout << "/logout - 登出" << std::endl;
    std::cout << "/quit - 退出程序" << std::endl;
    std::cout << "\n> ";
}

// 处理用户输入
void ChatClient::handleUserInput() {
    // 这个函数在run()中被调用
}

void ChatClient::handleLoginRegister() {
    // 如果已经登录，直接返回
    if (loggedIn.load()) {
        return;
    }
    
    showMainMenu();
    
    std::string choice;
    std::getline(std::cin, choice);
    choice = trim(choice);
    
    if (choice == "1") {
        // 登录
        std::string username, password;
        
        std::cout << "用户名: ";
        std::getline(std::cin, username);
        username = trim(username);
        
        if (!isValidUsername(username)) {
            std::cout << "用户名格式无效" << std::endl;
            return;
        }
        
        password = getHiddenPassword("密码: ");
        password = trim(password);
        
        if (!isValidPassword(password)) {
            std::cout << "密码格式无效" << std::endl;
            return;
        }
        
        loginUser(username, password);
        
    } else if (choice == "2") {
        // 注册
        std::string username, password, email;
        
        std::cout << "用户名: ";
        std::getline(std::cin, username);
        username = trim(username);
        
        if (!isValidUsername(username)) {
            std::cout << "用户名格式无效（3-20个字符，只能包含字母、数字、下划线）" << std::endl;
            return;
        }
        
        password = getHiddenPassword("密码: ");
        password = trim(password);
        
        if (!isValidPassword(password)) {
            std::cout << "密码格式无效（至少6个字符）" << std::endl;
            return;
        }
        
        std::cout << "邮箱（可选）: ";
        std::getline(std::cin, email);
        email = trim(email);
        
        registerUser(username, password, email);
        
    } else if (choice == "3") {
        // 退出
        std::cout << "再见！" << std::endl;
        disconnect();
    } else {
        std::cout << "无效选择" << std::endl;
    }
}

void ChatClient::handleChatInput() {
    std::string input;
    std::getline(std::cin, input);
    input = trim(input);
    
    if (input.empty()) {
        return;
    }
    
    // 如果正在等待文件传输响应，处理y/n输入
    if (awaitingFileResponse) {
        if (input == "y" || input == "Y") {
            // 接受文件传输，询问保存路径
            std::cout << "请输入保存文件的路径 (直接按回车使用当前目录): ";
            std::cout.flush();
            
            std::string savePath;
            std::getline(std::cin, savePath);
            savePath = trim(savePath);
            
            if (savePath.empty()) {
                savePath = "."; // 默认当前目录
            }
            
            // 发送接受消息
            acceptFileTransfer(pendingSessionId, pendingSender, savePath);
            std::cout << "已接受文件传输，文件将保存到: " << savePath << std::endl;
            
            // 重置状态
            awaitingFileResponse = false;
            pendingSessionId.clear();
            pendingSender.clear();
            pendingFilename.clear();
            
        } else if (input == "n" || input == "N") {
            // 拒绝文件传输
            rejectFileTransfer(pendingSessionId, pendingSender);
            std::cout << "已拒绝文件传输" << std::endl;
            
            // 重置状态
            awaitingFileResponse = false;
            pendingSessionId.clear();
            pendingSender.clear();
            pendingFilename.clear();
            
        } else {
            // 无效输入，重新提示
            std::cout << "请输入 y (接受) 或 n (拒绝): ";
            std::cout.flush();
        }
        return;
    }
    
    // 处理命令
    if (input[0] == '/') {
        if (input == "/list") {
            requestUserList();
        } else if (input == "/files") {
            requestGroupFileList();
        } else if (input == "/logout") {
            logoutUser();
        } else if (input == "/quit") {
            std::cout << "再见！" << std::endl;
            disconnect();
        } else if (input.substr(0, 8) == "/upload ") {
            // 上传群文件: /upload 文件路径
            std::string filePath = input.substr(8);
            filePath = trim(filePath);
            if (!filePath.empty()) {
                uploadGroupFile(filePath);
            } else {
                std::cout << "用法: /upload 文件路径" << std::endl;
            }
        } else if (input.substr(0, 10) == "/download ") {
            // 下载群文件: /download 文件ID 本地路径
            std::string params = input.substr(10);
            std::istringstream iss(params);
            std::string fileId, localPath;
            if (iss >> fileId >> localPath) {
                downloadGroupFile(fileId, localPath);
            } else {
                std::cout << "用法: /download 文件ID 本地路径" << std::endl;
            }
        } else if (input.substr(0, 6) == "/send ") {
            // 发送文件给用户: /send 用户名 文件路径
            std::string params = input.substr(6);
            std::istringstream iss(params);
            std::string username, filePath;
            if (iss >> username >> filePath) {
                sendFileToUser(username, filePath);
            } else {
                std::cout << "用法: /send 用户名 文件路径" << std::endl;
            }
        } else {
            std::cout << "未知命令: " << input << std::endl;
            std::cout << "输入 /help 查看可用命令" << std::endl;
        }
    } else if (input[0] == '@') {
        // 私聊消息
        size_t spacePos = input.find(' ');
        if (spacePos != std::string::npos && spacePos > 1) {
            std::string receiver = input.substr(1, spacePos - 1);
            std::string content = input.substr(spacePos + 1);
            
            if (!receiver.empty() && !content.empty()) {
                sendPrivateMessage(receiver, content);
            } else {
                std::cout << "私聊格式: @用户名 消息内容" << std::endl;
            }
        } else {
            std::cout << "私聊格式: @用户名 消息内容" << std::endl;
        }
    } else {
        // 群聊消息
        sendPublicMessage(input);
    }
}

// 显示消息
void ChatClient::displayMessage(const std::string& message) {
    std::cout << "\n" << message << std::endl;
}

void ChatClient::displayPublicMessage(const Message& msg) {
    std::cout << "\n[群聊] " << msg.sender << " (" << msg.getTimeString() << "): " << msg.content << std::endl;
}

void ChatClient::displayPrivateMessage(const Message& msg) {
    std::cout << "\n[私聊] " << msg.sender << " -> 我 (" << msg.getTimeString() << "): " << msg.content << std::endl;
}

void ChatClient::displaySystemMessage(const std::string& message) {
    std::cout << "\n[系统] " << message << std::endl;
}

// 输入验证
bool ChatClient::isValidUsername(const std::string& username) const {
    if (username.length() < 3 || username.length() > 20) {
        return false;
    }
    
    for (char c : username) {
        if (!std::isalnum(c) && c != '_') {
            return false;
        }
    }
    
    return true;
}

bool ChatClient::isValidPassword(const std::string& password) const {
    return password.length() >= 6;
}

// 工具函数
std::string ChatClient::trim(const std::string& str) const {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::string ChatClient::getCurrentTime() const {
    time_t now = time(nullptr);
    char timeStr[100];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", localtime(&now));
    return std::string(timeStr);
}

// 从文件路径提取文件名
std::string ChatClient::extractFilename(const std::string& filePath) const {
    size_t lastSlash = filePath.find_last_of('/');
    if (lastSlash != std::string::npos) {
        return filePath.substr(lastSlash + 1);
    }
    return filePath;
}

// 检查文件是否存在
bool ChatClient::fileExists(const std::string& filePath) const {
    struct stat buffer;
    return (stat(filePath.c_str(), &buffer) == 0);
}

// 获取文件大小
std::size_t ChatClient::getFileSize(const std::string& filePath) const {
    struct stat buffer;
    if (stat(filePath.c_str(), &buffer) == 0) {
        return buffer.st_size;
    }
    return 0;
}

// 文件内容编码（十六进制编码）
std::string ChatClient::encodeFileContent(const std::string& content) const {
    std::ostringstream encoded;
    for (unsigned char c : content) {
        encoded << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    }
    return encoded.str();
}

// 文件内容解码（十六进制解码）
std::string ChatClient::decodeFileContent(const std::string& encoded) const {
    std::string decoded;
    for (size_t i = 0; i < encoded.length(); i += 2) {
        if (i + 1 < encoded.length()) {
            std::string byteStr = encoded.substr(i, 2);
            try {
                unsigned char byte = static_cast<unsigned char>(std::stoi(byteStr, nullptr, 16));
                decoded.push_back(byte);
            } catch (const std::exception& e) {
                // 如果解码失败，跳过这个字节
                continue;
            }
        }
    }
    return decoded;
}

// 密码隐藏输入函数
std::string ChatClient::getHiddenPassword(const std::string& prompt) const {
    std::cout << prompt;
    std::cout.flush();
    
    // 保存当前终端设置
    struct termios oldTermios, newTermios;
    tcgetattr(STDIN_FILENO, &oldTermios);
    newTermios = oldTermios;
    
    // 关闭回显
    newTermios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newTermios);
    
    std::string password;
    char ch;
    
    while (true) {
        ch = getchar();
        
        if (ch == '\n' || ch == '\r') {
            // 回车键，结束输入
            break;
        } else if (ch == 127 || ch == 8) {
            // 退格键
            if (!password.empty()) {
                password.pop_back();
                std::cout << "\b \b";  // 删除显示的*
                std::cout.flush();
            }
        } else if (ch >= 32 && ch <= 126) {
            // 可打印字符
            password.push_back(ch);
            std::cout << '*';
            std::cout.flush();
        }
    }
    
    // 恢复终端设置
    tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);
    
    std::cout << std::endl;  // 输入结束后换行
    return password;
}