/*
 * FTP客户端主程序
 * 实现FTP客户端的连接、命令发送和文件传输功能
 */
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "../include/ftp_protocol.h"

using namespace FTP;

// FTP客户端类
class FTPClient {
public:
    // 构造函数
    FTPClient() 
        : controlSocket_(-1)
        , dataSocket_(-1)
        , passiveListenSocket_(-1)
        , connected_(false)
        , loggedIn_(false)
        , transferType_(TransferType::ASCII)
        , verbose_(true) {
    }
    
    // 析构函数
    ~FTPClient() {
        disconnect();
    }
    
    // 连接到FTP服务器
    bool connect(const std::string& host, int port = Config::DEFAULT_CONTROL_PORT) {
        if (connected_) {
            std::cout << "已经连接到服务器" << std::endl;
            return false;
        }
        
        // 解析主机名
        struct hostent* server = gethostbyname(host.c_str());
        if (!server) {
            std::cerr << "无法解析主机名: " << host << std::endl;
            return false;
        }
        
        // 创建套接字
        controlSocket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (controlSocket_ < 0) {
            std::cerr << "创建套接字失败" << std::endl;
            return false;
        }
        
        // 设置服务器地址
        struct sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        memcpy(&serverAddr.sin_addr.s_addr, server->h_addr, server->h_length);
        serverAddr.sin_port = htons(port);
        
        // 连接到服务器
        if (::connect(controlSocket_, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            std::cerr << "连接失败: " << strerror(errno) << std::endl;
            close(controlSocket_);
            controlSocket_ = -1;
            return false;
        }
        
        // 保存服务器信息
        serverHost_ = host;
        serverPort_ = port;
        connected_ = true;
        
        // 接收欢迎消息
        std::string welcome = receiveResponse();
        if (verbose_) {
            std::cout << welcome;
        }
        
        std::cout << "已连接到 " << host << ":" << port << std::endl;
        return true;
    }
    
    // 断开连接
    void disconnect() {
        if (connected_) {
            sendCommand("QUIT");
            receiveResponse();
            connected_ = false;
            loggedIn_ = false;
        }
        
        if (controlSocket_ >= 0) {
            close(controlSocket_);
            controlSocket_ = -1;
        }
        
        closeDataConnection();
    }
    
    // 登录
    bool login(const std::string& username, const std::string& password) {
        if (!connected_) {
            std::cerr << "未连接到服务器" << std::endl;
            return false;
        }
        
        // 发送用户名
        if (!sendCommand("USER " + username)) {
            return false;
        }
        
        int code = parseResponse(receiveResponse());
        if (code != ResponseCode::USER_NAME_OK && code != ResponseCode::USER_LOGGED_IN) {
            std::cerr << "用户名错误" << std::endl;
            return false;
        }
        
        // 发送密码
        if (!sendCommand("PASS " + password)) {
            return false;
        }
        
        code = parseResponse(receiveResponse());
        if (code != ResponseCode::USER_LOGGED_IN) {
            std::cerr << "密码错误" << std::endl;
            return false;
        }
        
        loggedIn_ = true;
        username_ = username;
        std::cout << "登录成功" << std::endl;
        
        // 获取当前目录
        pwd();
        
        return true;
    }
    
    // 获取当前工作目录
    std::string pwd() {
        if (!checkConnection()) return "";
        
        sendCommand("PWD");
        std::string response = receiveResponse();
        
        // 解析目录路径
        size_t start = response.find('"');
        size_t end = response.rfind('"');
        if (start != std::string::npos && end != std::string::npos && start < end) {
            currentDirectory_ = response.substr(start + 1, end - start - 1);
            if (verbose_) {
                std::cout << "当前目录: " << currentDirectory_ << std::endl;
            }
            return currentDirectory_;
        }
        
        return "";
    }
    
    // 改变目录
    bool cd(const std::string& path) {
        if (!checkConnection()) return false;
        
        sendCommand("CWD " + path);
        int code = parseResponse(receiveResponse());
        
        if (code == ResponseCode::FILE_ACTION_OK) {
            pwd();  // 更新当前目录
            return true;
        }
        
        return false;
    }
    
    // 创建目录
    bool mkdir(const std::string& dirname) {
        if (!checkConnection()) return false;
        
        sendCommand("MKD " + dirname);
        int code = parseResponse(receiveResponse());
        
        return code == ResponseCode::PATHNAME_CREATED;
    }
    
    // 删除目录
    bool rmdir(const std::string& dirname) {
        if (!checkConnection()) return false;
        
        sendCommand("RMD " + dirname);
        int code = parseResponse(receiveResponse());
        
        return code == ResponseCode::FILE_ACTION_OK;
    }
    
    // 删除文件
    bool deleteFile(const std::string& filename) {
        if (!checkConnection()) return false;
        
        sendCommand("DELE " + filename);
        int code = parseResponse(receiveResponse());
        
        return code == ResponseCode::FILE_ACTION_OK;
    }
    
    // 列出目录内容
    bool list(const std::string& path = "") {
        if (!checkConnection()) return false;
        
        // 进入被动模式
        if (!enterPassiveMode()) {
            std::cerr << "无法进入被动模式" << std::endl;
            return false;
        }
        
        // 发送LIST命令
        sendCommand("LIST " + path);
        std::string response = receiveResponse();
        int code = parseResponse(response);
        
        if (code != ResponseCode::FILE_STATUS_OK && 
            code != ResponseCode::DATA_CONNECTION_ALREADY_OPEN) {
            std::cerr << "LIST命令失败" << std::endl;
            closeDataConnection();
            return false;
        }
        
        // 接收目录列表
        std::string listing = receiveData();
        std::cout << listing;
        
        closeDataConnection();
        
        // 接收完成响应
        response = receiveResponse();
        code = parseResponse(response);
        
        return code == ResponseCode::CLOSING_DATA_CONNECTION;
    }
    
    // 下载文件
    bool get(const std::string& remoteFile, const std::string& localFile = "") {
        if (!checkConnection()) return false;
        
        std::string targetFile = localFile.empty() ? remoteFile : localFile;
        
        // 设置二进制传输模式
        setType(TransferType::IMAGE);
        
        // 进入被动模式
        if (!enterPassiveMode()) {
            std::cerr << "无法进入被动模式" << std::endl;
            return false;
        }
        
        // 发送RETR命令
        sendCommand("RETR " + remoteFile);
        std::string response = receiveResponse();
        int code = parseResponse(response);
        
        if (code != ResponseCode::FILE_STATUS_OK && 
            code != ResponseCode::DATA_CONNECTION_ALREADY_OPEN) {
            std::cerr << "无法下载文件: " << remoteFile << std::endl;
            closeDataConnection();
            return false;
        }
        
        // 接收并保存文件
        std::ofstream file(targetFile, std::ios::binary);
        if (!file) {
            std::cerr << "无法创建本地文件: " << targetFile << std::endl;
            closeDataConnection();
            return false;
        }
        
        char buffer[Config::BUFFER_SIZE];
        ssize_t bytesReceived;
        size_t totalBytes = 0;
        
        std::cout << "正在下载 " << remoteFile << " -> " << targetFile << std::endl;
        
        while ((bytesReceived = recv(dataSocket_, buffer, sizeof(buffer), 0)) > 0) {
            file.write(buffer, bytesReceived);
            totalBytes += bytesReceived;
            
            // 显示进度
            std::cout << "\r已下载: " << totalBytes << " 字节" << std::flush;
        }
        
        std::cout << std::endl;
        file.close();
        closeDataConnection();
        
        // 接收完成响应
        response = receiveResponse();
        code = parseResponse(response);
        
        if (code == ResponseCode::CLOSING_DATA_CONNECTION) {
            std::cout << "文件下载成功" << std::endl;
            return true;
        }
        
        return false;
    }
    
    // 上传文件
    bool put(const std::string& localFile, const std::string& remoteFile = "") {
        if (!checkConnection()) return false;
        
        // 检查本地文件是否存在
        struct stat st;
        if (stat(localFile.c_str(), &st) != 0) {
            std::cerr << "本地文件不存在: " << localFile << std::endl;
            return false;
        }
        
        // 获取文件名（简单处理）
        std::string targetFile = remoteFile;
        if (targetFile.empty()) {
            size_t pos = localFile.find_last_of("/\\");
            targetFile = (pos == std::string::npos) ? localFile : localFile.substr(pos + 1);
        }
        
        // 设置二进制传输模式
        setType(TransferType::IMAGE);
        
        // 进入被动模式
        if (!enterPassiveMode()) {
            std::cerr << "无法进入被动模式" << std::endl;
            return false;
        }
        
        // 发送STOR命令
        sendCommand("STOR " + targetFile);
        std::string response = receiveResponse();
        int code = parseResponse(response);
        
        if (code != ResponseCode::FILE_STATUS_OK && 
            code != ResponseCode::DATA_CONNECTION_ALREADY_OPEN) {
            std::cerr << "无法上传文件" << std::endl;
            closeDataConnection();
            return false;
        }
        
        // 读取并发送文件
        std::ifstream file(localFile, std::ios::binary);
        if (!file) {
            std::cerr << "无法打开本地文件: " << localFile << std::endl;
            closeDataConnection();
            return false;
        }
        
        char buffer[Config::BUFFER_SIZE];
        size_t totalBytes = 0;
        
        std::cout << "正在上传 " << localFile << " -> " << targetFile << std::endl;
        
        while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
            size_t toSend = file.gcount();
            ssize_t sent = 0;
            
            while (sent < static_cast<ssize_t>(toSend)) {
                ssize_t result = send(dataSocket_, buffer + sent, toSend - sent, 0);
                if (result <= 0) {
                    std::cerr << "\n发送数据失败" << std::endl;
                    file.close();
                    closeDataConnection();
                    return false;
                }
                sent += result;
                totalBytes += result;
            }
            
            // 显示进度
            std::cout << "\r已上传: " << totalBytes << " 字节" << std::flush;
        }
        
        std::cout << std::endl;
        file.close();
        closeDataConnection();
        
        // 接收完成响应
        response = receiveResponse();
        code = parseResponse(response);
        
        if (code == ResponseCode::CLOSING_DATA_CONNECTION) {
            std::cout << "文件上传成功" << std::endl;
            return true;
        }
        
        return false;
    }
    
    // 设置传输类型
    bool setType(TransferType type) {
        if (!checkConnection()) return false;
        
        char typeChar = static_cast<char>(type);
        sendCommand(std::string("TYPE ") + typeChar);
        int code = parseResponse(receiveResponse());
        
        if (code == ResponseCode::COMMAND_OK) {
            transferType_ = type;
            return true;
        }
        
        return false;
    }
    
    // 设置详细模式
    void setVerbose(bool verbose) {
        verbose_ = verbose;
    }
    
    // 执行原始命令
    bool executeCommand(const std::string& command) {
        if (!checkConnection()) return false;
        
        sendCommand(command);
        std::string response = receiveResponse();
        std::cout << response;
        
        return parseResponse(response) < 400;
    }
    
private:
    // 检查连接状态
    bool checkConnection() {
        if (!connected_) {
            std::cerr << "未连接到服务器" << std::endl;
            return false;
        }
        if (!loggedIn_) {
            std::cerr << "未登录" << std::endl;
            return false;
        }
        return true;
    }
    
    // 发送命令
    bool sendCommand(const std::string& command) {
        if (controlSocket_ < 0) return false;
        
        std::string cmd = command + "\r\n";
        
        if (verbose_) {
            // 隐藏密码
            if (command.substr(0, 5) == "PASS ") {
                std::cout << ">>> PASS ****" << std::endl;
            } else {
                std::cout << ">>> " << command << std::endl;
            }
        }
        
        ssize_t sent = send(controlSocket_, cmd.c_str(), cmd.length(), 0);
        return sent == static_cast<ssize_t>(cmd.length());
    }
    
    // 接收响应
    std::string receiveResponse() {
        char buffer[Config::MAX_COMMAND_LENGTH];
        memset(buffer, 0, sizeof(buffer));
        
        ssize_t received = recv(controlSocket_, buffer, sizeof(buffer) - 1, 0);
        if (received <= 0) {
            return "";
        }
        
        std::string response(buffer, received);
        
        // 处理多行响应
        while (response.length() >= 4) {
            // 检查是否是多行响应
            if (response[3] == '-') {
                // 继续接收直到找到结束行
                std::string code = response.substr(0, 3);
                std::string endMarker = code + " ";
                
                while (response.find("\r\n" + endMarker) == std::string::npos) {
                    memset(buffer, 0, sizeof(buffer));
                    received = recv(controlSocket_, buffer, sizeof(buffer) - 1, 0);
                    if (received <= 0) break;
                    response += std::string(buffer, received);
                }
            }
            break;
        }
        
        if (verbose_) {
            std::cout << "<<< " << response;
            if (response.back() != '\n') {
                std::cout << std::endl;
            }
        }
        
        return response;
    }
    
    // 解析响应码
    int parseResponse(const std::string& response) {
        if (response.length() < 3) return -1;
        
        try {
            return std::stoi(response.substr(0, 3));
        } catch (...) {
            return -1;
        }
    }
    
    // 进入被动模式
    bool enterPassiveMode() {
        sendCommand("PASV");
        std::string response = receiveResponse();
        int code = parseResponse(response);
        
        if (code != ResponseCode::ENTERING_PASSIVE_MODE) {
            return false;
        }
        
        // 解析PASV响应获取IP和端口
        size_t start = response.find('(');
        size_t end = response.find(')');
        if (start == std::string::npos || end == std::string::npos) {
            return false;
        }
        
        std::string params = response.substr(start + 1, end - start - 1);
        std::vector<int> values;
        std::istringstream iss(params);
        std::string token;
        
        while (std::getline(iss, token, ',')) {
            values.push_back(std::stoi(token));
        }
        
        if (values.size() != 6) {
            return false;
        }
        
        // 构建地址和端口
        std::string host = std::to_string(values[0]) + "." + 
                          std::to_string(values[1]) + "." +
                          std::to_string(values[2]) + "." + 
                          std::to_string(values[3]);
        int port = values[4] * 256 + values[5];
        
        // 连接到数据端口
        dataSocket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (dataSocket_ < 0) {
            return false;
        }
        
        struct sockaddr_in dataAddr;
        memset(&dataAddr, 0, sizeof(dataAddr));
        dataAddr.sin_family = AF_INET;
        dataAddr.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &dataAddr.sin_addr);
        
        if (::connect(dataSocket_, (struct sockaddr*)&dataAddr, sizeof(dataAddr)) < 0) {
            close(dataSocket_);
            dataSocket_ = -1;
            return false;
        }
        
        return true;
    }
    
    // 关闭数据连接
    void closeDataConnection() {
        if (dataSocket_ >= 0) {
            close(dataSocket_);
            dataSocket_ = -1;
        }
        if (passiveListenSocket_ >= 0) {
            close(passiveListenSocket_);
            passiveListenSocket_ = -1;
        }
    }
    
    // 接收数据
    std::string receiveData() {
        std::string data;
        char buffer[Config::BUFFER_SIZE];
        ssize_t received;
        
        while ((received = recv(dataSocket_, buffer, sizeof(buffer), 0)) > 0) {
            data.append(buffer, received);
        }
        
        return data;
    }
    
private:
    // 连接相关
    int controlSocket_;              // 控制连接套接字
    int dataSocket_;                 // 数据连接套接字
    int passiveListenSocket_;        // 被动模式监听套接字
    bool connected_;                 // 是否已连接
    bool loggedIn_;                  // 是否已登录
    
    // 服务器信息
    std::string serverHost_;        // 服务器主机
    int serverPort_;                 // 服务器端口
    std::string username_;           // 用户名
    std::string currentDirectory_;   // 当前目录
    
    // 传输参数
    TransferType transferType_;      // 传输类型
    bool verbose_;                   // 详细输出模式
};

// 显示帮助信息
void showHelp() {
    std::cout << "FTP客户端命令:" << std::endl;
    std::cout << "  open <主机> [端口]  - 连接到FTP服务器" << std::endl;
    std::cout << "  login <用户名>      - 登录" << std::endl;
    std::cout << "  close               - 关闭连接" << std::endl;
    std::cout << "  pwd                 - 显示当前目录" << std::endl;
    std::cout << "  cd <目录>           - 改变目录" << std::endl;
    std::cout << "  ls [路径]           - 列出目录内容" << std::endl;
    std::cout << "  get <远程文件> [本地文件] - 下载文件" << std::endl;
    std::cout << "  put <本地文件> [远程文件] - 上传文件" << std::endl;
    std::cout << "  mkdir <目录名>      - 创建目录" << std::endl;
    std::cout << "  rmdir <目录名>      - 删除目录" << std::endl;
    std::cout << "  delete <文件名>     - 删除文件" << std::endl;
    std::cout << "  binary              - 设置二进制传输模式" << std::endl;
    std::cout << "  ascii               - 设置ASCII传输模式" << std::endl;
    std::cout << "  verbose             - 切换详细输出模式" << std::endl;
    std::cout << "  help                - 显示帮助信息" << std::endl;
    std::cout << "  quit/exit           - 退出程序" << std::endl;
}

// 主函数
int main(int argc, char* argv[]) {
    std::cout << "FTP客户端 v1.0" << std::endl;
    std::cout << "输入 'help' 查看命令列表" << std::endl << std::endl;
    
    FTPClient client;
    std::string line;
    
    // 命令行模式
    while (true) {
        std::cout << "ftp> ";
        std::getline(std::cin, line);
        
        if (line.empty()) continue;
        
        // 解析命令
        std::istringstream iss(line);
        std::string command;
        iss >> command;
        
        // 转换为小写
        std::transform(command.begin(), command.end(), command.begin(), ::tolower);
        
        if (command == "quit" || command == "exit") {
            client.disconnect();
            std::cout << "再见!" << std::endl;
            break;
        } else if (command == "help" || command == "?") {
            showHelp();
        } else if (command == "open") {
            std::string host;
            int port = Config::DEFAULT_CONTROL_PORT;
            
            iss >> host;
            if (host.empty()) {
                std::cout << "请指定主机名" << std::endl;
                continue;
            }
            
            iss >> port;
            client.connect(host, port);
        } else if (command == "login") {
            std::string username, password;
            
            iss >> username;
            if (username.empty()) {
                std::cout << "用户名: ";
                std::getline(std::cin, username);
            }
            
            std::cout << "密码: ";
            // 关闭回显
            system("stty echo");
            std::getline(std::cin, password);
            system("stty echo");
            std::cout << std::endl;
            
            client.login(username, password);
        } else if (command == "close") {
            client.disconnect();
            std::cout << "连接已关闭" << std::endl;
        } else if (command == "pwd") {
            client.pwd();
        } else if (command == "cd") {
            std::string path;
            iss >> path;
            if (path.empty()) {
                std::cout << "请指定目录" << std::endl;
                continue;
            }
            client.cd(path);
        } else if (command == "ls" || command == "dir") {
            std::string path;
            iss >> path;
            client.list(path);
        } else if (command == "get") {
            std::string remoteFile, localFile;
            iss >> remoteFile >> localFile;
            if (remoteFile.empty()) {
                std::cout << "请指定远程文件名" << std::endl;
                continue;
            }
            client.get(remoteFile, localFile);
        } else if (command == "put") {
            std::string localFile, remoteFile;
            iss >> localFile >> remoteFile;
            if (localFile.empty()) {
                std::cout << "请指定本地文件名" << std::endl;
                continue;
            }
            client.put(localFile, remoteFile);
        } else if (command == "mkdir") {
            std::string dirname;
            iss >> dirname;
            if (dirname.empty()) {
                std::cout << "请指定目录名" << std::endl;
                continue;
            }
            client.mkdir(dirname);
        } else if (command == "rmdir") {
            std::string dirname;
            iss >> dirname;
            if (dirname.empty()) {
                std::cout << "请指定目录名" << std::endl;
                continue;
            }
            client.rmdir(dirname);
        } else if (command == "delete" || command == "rm") {
            std::string filename;
            iss >> filename;
            if (filename.empty()) {
                std::cout << "请指定文件名" << std::endl;
                continue;
            }
            client.deleteFile(filename);
        } else if (command == "binary" || command == "bin") {
            client.setType(TransferType::IMAGE);
            std::cout << "传输模式设置为二进制" << std::endl;
        } else if (command == "ascii" || command == "asc") {
            client.setType(TransferType::ASCII);
            std::cout << "传输模式设置为ASCII" << std::endl;
        } else if (command == "verbose") {
            static bool verbose = true;
            verbose = !verbose;
            client.setVerbose(verbose);
            std::cout << "详细模式: " << (verbose ? "开" : "关") << std::endl;
        } else {
            // 尝试作为原始FTP命令执行
            client.executeCommand(line);
        }
    }
    
    return 0;
}