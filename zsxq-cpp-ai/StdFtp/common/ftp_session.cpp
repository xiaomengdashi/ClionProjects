/*
 * FTP会话管理实现文件
 * 实现FTP会话的管理和命令处理
 */
#include "../include/ftp_session.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>
#include <errno.h>

namespace FTP {

// 构造函数
Session::Session(int controlSocket, const sockaddr_in& clientAddr)
    : controlSocket_(controlSocket)
    , clientAddr_(clientAddr)
    , dataSocket_(-1)
    , passiveListenSocket_(-1)
    , dataMode_(DataConnectionMode::NONE)
    , activePort_(0)
    , state_(SessionState::CONNECTED)
    , authenticated_(false)
    , rootDirectory_("/tmp/ftp")  // 默认FTP根目录
    , currentDirectory_("/")
    , transferType_(TransferType::ASCII)
    , transferMode_(TransferMode::STREAM)
    , fileStructure_(FileStructure::FILE)
    , running_(false) {
    
    // 创建FTP根目录（如果不存在）
    struct stat st;
    if (stat(rootDirectory_.c_str(), &st) != 0) {
        mkdir(rootDirectory_.c_str(), 0755);
    }
    
    // 更新最后活动时间
    lastActivity_ = std::chrono::steady_clock::now();
}

// 析构函数
Session::~Session() {
    stop();
    if (controlSocket_ >= 0) {
        close(controlSocket_);
    }
    closeDataConnection();
}

// 启动会话处理
void Session::start() {
    running_ = true;
    sessionThread_ = std::thread(&Session::sessionLoop, this);
}

// 停止会话
void Session::stop() {
    running_ = false;
    if (sessionThread_.joinable()) {
        sessionThread_.join();
    }
}

// 发送响应到客户端
bool Session::sendResponse(int code, const std::string& message) {
    std::string response = Utils::formatResponse(code, message);
    ssize_t bytesSent = send(controlSocket_, response.c_str(), response.length(), 0);
    return bytesSent == static_cast<ssize_t>(response.length());
}

// 接收客户端命令
std::string Session::receiveCommand() {
    char buffer[Config::MAX_COMMAND_LENGTH];
    memset(buffer, 0, sizeof(buffer));
    
    ssize_t bytesReceived = recv(controlSocket_, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived <= 0) {
        return "";
    }
    
    // 更新最后活动时间
    lastActivity_ = std::chrono::steady_clock::now();
    
    return std::string(buffer, bytesReceived);
}

// 会话处理主循环
void Session::sessionLoop() {
    // 发送欢迎消息
    sendResponse(ResponseCode::SERVICE_READY, "Welcome to FTP Server");
    
    while (running_ && state_ != SessionState::DISCONNECTED) {
        std::string commandLine = receiveCommand();
        if (commandLine.empty()) {
            break;  // 连接已关闭
        }
        
        auto cmdPair = Utils::parseCommand(commandLine);
        handleCommand(cmdPair.first, cmdPair.second);
    }
    
    state_ = SessionState::DISCONNECTED;
}

// 处理客户端命令
void Session::handleCommand(const std::string& command, const std::string& params) {
    // 记录命令（调试用）
    std::cout << "命令: " << command << ", 参数: " << params << std::endl;
    
    // 未认证时只允许部分命令
    if (!authenticated_ && command != Commands::USER && command != Commands::PASS && 
        command != Commands::QUIT && command != Commands::SYST && command != Commands::FEAT) {
        sendResponse(ResponseCode::NOT_LOGGED_IN, "Please login first");
        return;
    }
    
    // 命令分发
    if (command == Commands::USER) {
        handleUSER(params);
    } else if (command == Commands::PASS) {
        handlePASS(params);
    } else if (command == Commands::QUIT) {
        handleQUIT();
    } else if (command == Commands::PWD) {
        handlePWD();
    } else if (command == Commands::CWD) {
        handleCWD(params);
    } else if (command == Commands::CDUP) {
        handleCDUP();
    } else if (command == Commands::MKD) {
        handleMKD(params);
    } else if (command == Commands::RMD) {
        handleRMD(params);
    } else if (command == Commands::DELE) {
        handleDELE(params);
    } else if (command == Commands::RNFR) {
        handleRNFR(params);
    } else if (command == Commands::RNTO) {
        handleRNTO(params);
    } else if (command == Commands::SIZE) {
        handleSIZE(params);
    } else if (command == Commands::MDTM) {
        handleMDTM(params);
    } else if (command == Commands::TYPE) {
        handleTYPE(params);
    } else if (command == Commands::PORT) {
        handlePORT(params);
    } else if (command == Commands::PASV) {
        handlePASV();
    } else if (command == Commands::LIST) {
        handleLIST(params);
    } else if (command == Commands::NLST) {
        handleNLST(params);
    } else if (command == Commands::RETR) {
        handleRETR(params);
    } else if (command == Commands::STOR) {
        handleSTOR(params);
    } else if (command == Commands::APPE) {
        handleAPPE(params);
    } else if (command == Commands::SYST) {
        handleSYST();
    } else if (command == Commands::STAT) {
        handleSTAT();
    } else if (command == Commands::HELP) {
        handleHELP(params);
    } else if (command == Commands::NOOP) {
        handleNOOP();
    } else if (command == Commands::FEAT) {
        handleFEAT();
    } else {
        sendResponse(ResponseCode::COMMAND_NOT_IMPLEMENTED, "Command not implemented");
    }
}

// 处理USER命令
void Session::handleUSER(const std::string& username) {
    username_ = username;
    state_ = SessionState::AUTHENTICATING;
    sendResponse(ResponseCode::USER_NAME_OK, "User name okay, need password");
}

// 处理PASS命令
void Session::handlePASS(const std::string& password) {
    if (username_.empty()) {
        sendResponse(ResponseCode::BAD_SEQUENCE, "Login with USER first");
        return;
    }
    
    // 简单的认证逻辑（实际应用中应该使用更安全的方式）
    // 这里接受任何用户名和密码组合
    password_ = password;
    authenticated_ = true;
    state_ = SessionState::AUTHENTICATED;
    sendResponse(ResponseCode::USER_LOGGED_IN, "User logged in, proceed");
}

// 处理QUIT命令
void Session::handleQUIT() {
    sendResponse(ResponseCode::SERVICE_CLOSING, "Goodbye");
    running_ = false;
    state_ = SessionState::DISCONNECTED;
}

// 处理PWD命令
void Session::handlePWD() {
    std::string response = "\"" + currentDirectory_ + "\" is current directory";
    sendResponse(ResponseCode::PATHNAME_CREATED, response);
}

// 处理CWD命令
void Session::handleCWD(const std::string& path) {
    if (setCurrentDirectory(path)) {
        sendResponse(ResponseCode::FILE_ACTION_OK, "Directory changed");
    } else {
        sendResponse(ResponseCode::FILE_UNAVAILABLE, "Failed to change directory");
    }
}

// 处理CDUP命令
void Session::handleCDUP() {
    handleCWD("..");
}

// 处理MKD命令
void Session::handleMKD(const std::string& dirname) {
    std::string fullPath = getAbsolutePath(dirname);
    if (!isPathValid(fullPath)) {
        sendResponse(ResponseCode::FILE_UNAVAILABLE, "Invalid path");
        return;
    }
    
    if (mkdir(fullPath.c_str(), 0755) == 0) {
        sendResponse(ResponseCode::PATHNAME_CREATED, "\"" + dirname + "\" created");
    } else {
        sendResponse(ResponseCode::FILE_UNAVAILABLE, "Failed to create directory");
    }
}

// 处理RMD命令
void Session::handleRMD(const std::string& dirname) {
    std::string fullPath = getAbsolutePath(dirname);
    if (!isPathValid(fullPath)) {
        sendResponse(ResponseCode::FILE_UNAVAILABLE, "Invalid path");
        return;
    }
    
    if (rmdir(fullPath.c_str()) == 0) {
        sendResponse(ResponseCode::FILE_ACTION_OK, "Directory removed");
    } else {
        sendResponse(ResponseCode::FILE_UNAVAILABLE, "Failed to remove directory");
    }
}

// 处理DELE命令
void Session::handleDELE(const std::string& filename) {
    std::string fullPath = getAbsolutePath(filename);
    if (!isPathValid(fullPath) || !isFileExists(fullPath)) {
        sendResponse(ResponseCode::FILE_UNAVAILABLE, "File not found");
        return;
    }
    
    if (unlink(fullPath.c_str()) == 0) {
        sendResponse(ResponseCode::FILE_ACTION_OK, "File deleted");
    } else {
        sendResponse(ResponseCode::FILE_UNAVAILABLE, "Failed to delete file");
    }
}

// 处理TYPE命令
void Session::handleTYPE(const std::string& type) {
    if (type.empty()) {
        sendResponse(ResponseCode::SYNTAX_ERROR_IN_PARAMETERS, "Type not specified");
        return;
    }
    
    char typeChar = std::toupper(type[0]);
    switch (typeChar) {
        case 'A':
            transferType_ = TransferType::ASCII;
            sendResponse(ResponseCode::COMMAND_OK, "Type set to ASCII");
            break;
        case 'I':
            transferType_ = TransferType::IMAGE;
            sendResponse(ResponseCode::COMMAND_OK, "Type set to IMAGE (Binary)");
            break;
        default:
            sendResponse(ResponseCode::COMMAND_NOT_IMPLEMENTED_FOR_PARAMETER, 
                        "Type not supported");
    }
}

// 处理PORT命令
void Session::handlePORT(const std::string& params) {
    if (Utils::parsePortCommand(params, activeHost_, activePort_)) {
        dataMode_ = DataConnectionMode::ACTIVE;
        closeDataConnection();  // 关闭任何现有的数据连接
        sendResponse(ResponseCode::COMMAND_OK, "PORT command successful");
    } else {
        sendResponse(ResponseCode::SYNTAX_ERROR_IN_PARAMETERS, "Invalid PORT parameters");
    }
}

// 处理PASV命令
void Session::handlePASV() {
    if (createPassiveListener()) {
        // 获取服务器地址
        struct sockaddr_in addr;
        socklen_t addrLen = sizeof(addr);
        getsockname(controlSocket_, (struct sockaddr*)&addr, &addrLen);
        std::string serverIP = inet_ntoa(addr.sin_addr);
        
        // 获取监听端口
        getsockname(passiveListenSocket_, (struct sockaddr*)&addr, &addrLen);
        int port = ntohs(addr.sin_port);
        
        dataMode_ = DataConnectionMode::PASSIVE;
        std::string response = Utils::generatePasvResponse(serverIP, port);
        send(controlSocket_, response.c_str(), response.length(), 0);
    } else {
        sendResponse(ResponseCode::CANT_OPEN_DATA_CONNECTION, "Cannot enter passive mode");
    }
}

// 处理LIST命令
void Session::handleLIST(const std::string& path) {
    std::string targetPath = path.empty() ? currentDirectory_ : path;
    std::string fullPath = getAbsolutePath(targetPath);
    
    if (!isPathValid(fullPath) || !isDirectory(fullPath)) {
        sendResponse(ResponseCode::FILE_UNAVAILABLE, "Directory not found");
        return;
    }
    
    // 打开数据连接
    sendResponse(ResponseCode::FILE_STATUS_OK, "Opening data connection for directory listing");
    
    if (!openDataConnection()) {
        sendResponse(ResponseCode::CANT_OPEN_DATA_CONNECTION, "Cannot open data connection");
        return;
    }
    
    // 发送目录列表
    std::string listing = formatDirListing(fullPath, true);
    if (sendData(listing)) {
        sendResponse(ResponseCode::CLOSING_DATA_CONNECTION, "Directory send OK");
    } else {
        sendResponse(ResponseCode::CONNECTION_CLOSED, "Transfer aborted");
    }
    
    closeDataConnection();
}

// 处理RETR命令（下载文件）
void Session::handleRETR(const std::string& filename) {
    std::string fullPath = getAbsolutePath(filename);
    
    if (!isPathValid(fullPath) || !isFileExists(fullPath) || isDirectory(fullPath)) {
        sendResponse(ResponseCode::FILE_UNAVAILABLE, "File not found");
        return;
    }
    
    // 获取文件大小
    struct stat fileStat;
    stat(fullPath.c_str(), &fileStat);
    std::string msg = "Opening data connection for " + filename + 
                     " (" + std::to_string(fileStat.st_size) + " bytes)";
    sendResponse(ResponseCode::FILE_STATUS_OK, msg);
    
    if (!openDataConnection()) {
        sendResponse(ResponseCode::CANT_OPEN_DATA_CONNECTION, "Cannot open data connection");
        return;
    }
    
    if (sendFile(fullPath)) {
        sendResponse(ResponseCode::CLOSING_DATA_CONNECTION, "Transfer complete");
    } else {
        sendResponse(ResponseCode::CONNECTION_CLOSED, "Transfer aborted");
    }
    
    closeDataConnection();
}

// 处理STOR命令（上传文件）
void Session::handleSTOR(const std::string& filename) {
    std::string fullPath = getAbsolutePath(filename);
    
    if (!isPathValid(fullPath)) {
        sendResponse(ResponseCode::FILE_UNAVAILABLE, "Invalid filename");
        return;
    }
    
    sendResponse(ResponseCode::FILE_STATUS_OK, "Opening data connection for file upload");
    
    if (!openDataConnection()) {
        sendResponse(ResponseCode::CANT_OPEN_DATA_CONNECTION, "Cannot open data connection");
        return;
    }
    
    if (receiveFile(fullPath, false)) {
        sendResponse(ResponseCode::CLOSING_DATA_CONNECTION, "Transfer complete");
    } else {
        sendResponse(ResponseCode::CONNECTION_CLOSED, "Transfer aborted");
    }
    
    closeDataConnection();
}

// 处理RNFR命令（重命名源）
void Session::handleRNFR(const std::string& filename) {
    std::string fullPath = getAbsolutePath(filename);
    if (!isPathValid(fullPath) || !isFileExists(fullPath)) {
        sendResponse(ResponseCode::FILE_UNAVAILABLE, "File not found");
        return;
    }
    
    renameFrom_ = fullPath;
    sendResponse(ResponseCode::FILE_ACTION_PENDING, "Ready for RNTO");
}

// 处理RNTO命令（重命名目标）
void Session::handleRNTO(const std::string& filename) {
    if (renameFrom_.empty()) {
        sendResponse(ResponseCode::BAD_SEQUENCE, "Use RNFR first");
        return;
    }
    
    std::string fullPath = getAbsolutePath(filename);
    if (!isPathValid(fullPath)) {
        sendResponse(ResponseCode::FILE_UNAVAILABLE, "Invalid filename");
        renameFrom_.clear();
        return;
    }
    
    if (rename(renameFrom_.c_str(), fullPath.c_str()) == 0) {
        sendResponse(ResponseCode::FILE_ACTION_OK, "File renamed");
    } else {
        sendResponse(ResponseCode::FILE_UNAVAILABLE, "Rename failed");
    }
    
    renameFrom_.clear();
}

// 处理SIZE命令
void Session::handleSIZE(const std::string& filename) {
    std::string fullPath = getAbsolutePath(filename);
    if (!isPathValid(fullPath) || !isFileExists(fullPath) || isDirectory(fullPath)) {
        sendResponse(ResponseCode::FILE_UNAVAILABLE, "File not found");
        return;
    }
    
    struct stat st;
    if (stat(fullPath.c_str(), &st) == 0) {
        std::string response = std::to_string(st.st_size);
        sendResponse(ResponseCode::FILE_STATUS, response);
    } else {
        sendResponse(ResponseCode::FILE_UNAVAILABLE, "Cannot get file size");
    }
}

// 处理MDTM命令
void Session::handleMDTM(const std::string& filename) {
    std::string fullPath = getAbsolutePath(filename);
    if (!isPathValid(fullPath) || !isFileExists(fullPath) || isDirectory(fullPath)) {
        sendResponse(ResponseCode::FILE_UNAVAILABLE, "File not found");
        return;
    }
    
    struct stat st;
    if (stat(fullPath.c_str(), &st) == 0) {
        char timeStr[15];
        struct tm* tm = gmtime(&st.st_mtime);
        strftime(timeStr, sizeof(timeStr), "%Y%m%d%H%M%S", tm);
        sendResponse(ResponseCode::FILE_STATUS, timeStr);
    } else {
        sendResponse(ResponseCode::FILE_UNAVAILABLE, "Cannot get modification time");
    }
}

// 处理NLST命令（简单文件列表）
void Session::handleNLST(const std::string& path) {
    std::string targetPath = path.empty() ? currentDirectory_ : path;
    std::string fullPath = getAbsolutePath(targetPath);
    
    if (!isPathValid(fullPath) || !isDirectory(fullPath)) {
        sendResponse(ResponseCode::FILE_UNAVAILABLE, "Directory not found");
        return;
    }
    
    // 打开数据连接
    sendResponse(ResponseCode::FILE_STATUS_OK, "Opening data connection for file list");
    
    if (!openDataConnection()) {
        sendResponse(ResponseCode::CANT_OPEN_DATA_CONNECTION, "Cannot open data connection");
        return;
    }
    
    // 发送文件列表（简单格式）
    std::string listing = formatDirListing(fullPath, false);
    if (sendData(listing)) {
        sendResponse(ResponseCode::CLOSING_DATA_CONNECTION, "Directory send OK");
    } else {
        sendResponse(ResponseCode::CONNECTION_CLOSED, "Transfer aborted");
    }
    
    closeDataConnection();
}

// 处理APPE命令（追加文件）
void Session::handleAPPE(const std::string& filename) {
    std::string fullPath = getAbsolutePath(filename);
    
    if (!isPathValid(fullPath)) {
        sendResponse(ResponseCode::FILE_UNAVAILABLE, "Invalid filename");
        return;
    }
    
    sendResponse(ResponseCode::FILE_STATUS_OK, "Opening data connection for file append");
    
    if (!openDataConnection()) {
        sendResponse(ResponseCode::CANT_OPEN_DATA_CONNECTION, "Cannot open data connection");
        return;
    }
    
    if (receiveFile(fullPath, true)) {  // true表示追加模式
        sendResponse(ResponseCode::CLOSING_DATA_CONNECTION, "Transfer complete");
    } else {
        sendResponse(ResponseCode::CONNECTION_CLOSED, "Transfer aborted");
    }
    
    closeDataConnection();
}

// 处理HELP命令
void Session::handleHELP(const std::string& command) {
    std::string helpText;
    
    if (command.empty()) {
        helpText = "214-The following commands are recognized:\r\n";
        helpText += " USER PASS QUIT PWD CWD CDUP LIST NLST\r\n";
        helpText += " RETR STOR DELE MKD RMD RNFR RNTO SIZE\r\n";
        helpText += " TYPE PORT PASV SYST STAT NOOP HELP FEAT\r\n";
        helpText += "214 Help OK";
    } else {
        // 提供特定命令的帮助
        helpText = "214 Help for " + command + " not available";
    }
    
    send(controlSocket_, helpText.c_str(), helpText.length(), 0);
}

// 处理系统命令
void Session::handleSYST() {
    sendResponse(ResponseCode::SYSTEM_TYPE, "UNIX Type: L8");
}

void Session::handleSTAT() {
    std::ostringstream status;
    status << "211-FTP Server Status\r\n";
    status << " Connected to " << inet_ntoa(clientAddr_.sin_addr) << "\r\n";
    status << " Logged in as " << username_ << "\r\n";
    status << " TYPE: " << (transferType_ == TransferType::ASCII ? "ASCII" : "BINARY") << "\r\n";
    status << " Current directory: " << currentDirectory_ << "\r\n";
    status << "211 End of status";
    
    send(controlSocket_, status.str().c_str(), status.str().length(), 0);
}

void Session::handleNOOP() {
    sendResponse(ResponseCode::COMMAND_OK, "NOOP OK");
}

void Session::handleFEAT() {
    std::string features = "211-Features:\r\n";
    features += " SIZE\r\n";
    features += " MDTM\r\n";
    features += " PASV\r\n";
    features += " PORT\r\n";
    features += " TYPE A\r\n";
    features += " TYPE I\r\n";
    features += "211 End";
    send(controlSocket_, features.c_str(), features.length(), 0);
}

// 数据连接管理
bool Session::openDataConnection() {
    if (dataMode_ == DataConnectionMode::ACTIVE) {
        return connectToActiveAddress();
    } else if (dataMode_ == DataConnectionMode::PASSIVE) {
        // 接受被动连接
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        dataSocket_ = accept(passiveListenSocket_, (struct sockaddr*)&clientAddr, &clientLen);
        return dataSocket_ >= 0;
    }
    return false;
}

void Session::closeDataConnection() {
    if (dataSocket_ >= 0) {
        close(dataSocket_);
        dataSocket_ = -1;
    }
    if (passiveListenSocket_ >= 0) {
        close(passiveListenSocket_);
        passiveListenSocket_ = -1;
    }
    dataMode_ = DataConnectionMode::NONE;
}

bool Session::sendData(const std::string& data) {
    if (dataSocket_ < 0) return false;
    
    ssize_t totalSent = 0;
    ssize_t dataLen = data.length();
    
    while (totalSent < dataLen) {
        ssize_t sent = send(dataSocket_, data.c_str() + totalSent, 
                           dataLen - totalSent, 0);
        if (sent <= 0) return false;
        totalSent += sent;
    }
    
    return true;
}

bool Session::sendFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) return false;
    
    char buffer[Config::BUFFER_SIZE];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        ssize_t toSend = file.gcount();
        ssize_t sent = 0;
        
        while (sent < toSend) {
            ssize_t result = send(dataSocket_, buffer + sent, toSend - sent, 0);
            if (result <= 0) {
                file.close();
                return false;
            }
            sent += result;
        }
    }
    
    file.close();
    return true;
}

bool Session::receiveFile(const std::string& filepath, bool append) {
    std::ios::openmode mode = std::ios::binary;
    if (append) {
        mode |= std::ios::app;
    } else {
        mode |= std::ios::trunc;
    }
    
    std::ofstream file(filepath, mode);
    if (!file) return false;
    
    char buffer[Config::BUFFER_SIZE];
    ssize_t bytesReceived;
    
    while ((bytesReceived = recv(dataSocket_, buffer, sizeof(buffer), 0)) > 0) {
        file.write(buffer, bytesReceived);
        if (!file) {
            file.close();
            return false;
        }
    }
    
    file.close();
    return bytesReceived == 0;  // 0表示对方正常关闭连接
}

// 创建被动模式监听套接字
bool Session::createPassiveListener() {
    // 关闭现有的监听套接字
    if (passiveListenSocket_ >= 0) {
        close(passiveListenSocket_);
    }
    
    passiveListenSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (passiveListenSocket_ < 0) return false;
    
    // 设置地址重用
    int reuse = 1;
    setsockopt(passiveListenSocket_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = 0;  // 让系统分配端口
    
    if (bind(passiveListenSocket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(passiveListenSocket_);
        passiveListenSocket_ = -1;
        return false;
    }
    
    if (listen(passiveListenSocket_, 1) < 0) {
        close(passiveListenSocket_);
        passiveListenSocket_ = -1;
        return false;
    }
    
    return true;
}

// 连接到主动模式地址
bool Session::connectToActiveAddress() {
    dataSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (dataSocket_ < 0) return false;
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(activePort_);
    inet_pton(AF_INET, activeHost_.c_str(), &addr.sin_addr);
    
    if (connect(dataSocket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(dataSocket_);
        dataSocket_ = -1;
        return false;
    }
    
    return true;
}

// 工具函数
std::string Session::getAbsolutePath(const std::string& path) {
    if (path.empty()) {
        return rootDirectory_ + currentDirectory_;
    }
    
    if (path[0] == '/') {
        // 绝对路径
        return rootDirectory_ + path;
    } else {
        // 相对路径
        std::string fullPath = currentDirectory_;
        if (fullPath.back() != '/') {
            fullPath += '/';
        }
        fullPath += path;
        
        // 规范化路径（简单处理）
        // TODO: 实现完整的路径规范化
        return rootDirectory_ + fullPath;
    }
}

bool Session::isPathValid(const std::string& path) {
    // 确保路径在FTP根目录内
    // 简单的路径验证：确保不包含..
    if (path.find("..") != std::string::npos) {
        return false;
    }
    
    // 确保路径以根目录开始
    return path.find(rootDirectory_) == 0;
}

bool Session::setCurrentDirectory(const std::string& path) {
    std::string newPath = getAbsolutePath(path);
    
    if (!isPathValid(newPath) || !isDirectory(newPath)) {
        return false;
    }
    
    // 从完整路径中移除根目录部分
    std::string relativePath = newPath.substr(rootDirectory_.length());
    if (relativePath.empty()) {
        relativePath = "/";
    }
    
    currentDirectory_ = relativePath;
    return true;
}

bool Session::isFileExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool Session::isDirectory(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return false;
}

std::string Session::formatDirListing(const std::string& path, bool detailed) {
    return Utils::formatFileList(path, detailed);
}

// SessionManager 实现
SessionManager& SessionManager::getInstance() {
    static SessionManager instance;
    return instance;
}

SessionManager::SessionPtr SessionManager::createSession(int socket, const sockaddr_in& clientAddr) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto session = std::make_shared<Session>(socket, clientAddr);
    sessions_.push_back(session);
    return session;
}

void SessionManager::removeSession(SessionPtr session) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(
        std::remove(sessions_.begin(), sessions_.end(), session),
        sessions_.end()
    );
}

size_t SessionManager::getSessionCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.size();
}

void SessionManager::cleanupTimeoutSessions() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 移除已断开的会话
    sessions_.erase(
        std::remove_if(sessions_.begin(), sessions_.end(),
                      [](const SessionPtr& session) {
                          return session->getState() == SessionState::DISCONNECTED;
                      }),
        sessions_.end()
    );
}

} // namespace FTP