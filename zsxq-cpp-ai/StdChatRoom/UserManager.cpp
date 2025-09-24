#include "UserManager.h"
#include <fstream>
#include <iostream>
#include <algorithm>

// 构造函数
UserManager::UserManager(const std::string& filePath) : dataFilePath(filePath) {
    loadFromFile(); // 启动时加载用户数据
}

// 析构函数
UserManager::~UserManager() {
    saveToFile(); // 关闭时保存用户数据
}

// 用户注册
bool UserManager::registerUser(const std::string& username, const std::string& password, 
                              const std::string& email) {
    std::lock_guard<std::mutex> lock(usersMutex);
    
    // 检查用户名是否已存在
    if (users.find(username) != users.end()) {
        return false; // 用户名已存在
    }
    
    // 创建新用户
    User newUser(username, password, email);
    users[username] = newUser;
    
    // 立即保存到文件
    saveToFile();
    
    std::cout << "[用户管理] 新用户注册: " << username << std::endl;
    return true;
}

// 用户登录
bool UserManager::loginUser(const std::string& username, const std::string& password, int socketFd) {
    std::lock_guard<std::mutex> usersLock(usersMutex);
    std::lock_guard<std::mutex> onlineLock(onlineUsersMutex);
    
    // 检查用户是否存在
    auto userIt = users.find(username);
    if (userIt == users.end()) {
        return false; // 用户不存在
    }
    
    // 验证密码
    if (!userIt->second.verifyPassword(password)) {
        return false; // 密码错误
    }
    
    // 检查是否已经在线
    if (onlineUsers.find(username) != onlineUsers.end()) {
        return false; // 用户已在线
    }
    
    // 更新用户状态
    userIt->second.status = UserStatus::ONLINE;
    userIt->second.socketFd = socketFd;
    userIt->second.lastLoginTime = std::time(nullptr);
    
    // 添加到在线用户列表
    onlineUsers[username] = socketFd;
    
    std::cout << "[用户管理] 用户登录: " << username << " (socket: " << socketFd << ")" << std::endl;
    return true;
}

// 用户登出（通过用户名）
void UserManager::logoutUser(const std::string& username) {
    std::lock_guard<std::mutex> usersLock(usersMutex);
    std::lock_guard<std::mutex> onlineLock(onlineUsersMutex);
    
    // 更新用户状态
    auto userIt = users.find(username);
    if (userIt != users.end()) {
        userIt->second.status = UserStatus::OFFLINE;
        userIt->second.socketFd = -1;
    }
    
    // 从在线用户列表中移除
    onlineUsers.erase(username);
    
    std::cout << "[用户管理] 用户登出: " << username << std::endl;
}

// 用户登出（通过socket fd）
void UserManager::logoutUser(int socketFd) {
    std::lock_guard<std::mutex> usersLock(usersMutex);
    std::lock_guard<std::mutex> onlineLock(onlineUsersMutex);
    
    // 查找对应的用户名
    std::string username;
    for (const auto& pair : onlineUsers) {
        if (pair.second == socketFd) {
            username = pair.first;
            break;
        }
    }
    
    if (!username.empty()) {
        // 更新用户状态
        auto userIt = users.find(username);
        if (userIt != users.end()) {
            userIt->second.status = UserStatus::OFFLINE;
            userIt->second.socketFd = -1;
        }
        
        // 从在线用户列表中移除
        onlineUsers.erase(username);
        
        std::cout << "[用户管理] 用户登出: " << username << " (socket: " << socketFd << ")" << std::endl;
    }
}

// 检查用户名是否存在
bool UserManager::userExists(const std::string& username) const {
    std::lock_guard<std::mutex> lock(usersMutex);
    return users.find(username) != users.end();
}

// 检查用户是否在线
bool UserManager::isUserOnline(const std::string& username) const {
    std::lock_guard<std::mutex> lock(onlineUsersMutex);
    return onlineUsers.find(username) != onlineUsers.end();
}

// 根据socket fd获取用户名
std::string UserManager::getUsernameBySocket(int socketFd) const {
    std::lock_guard<std::mutex> lock(onlineUsersMutex);
    
    for (const auto& pair : onlineUsers) {
        if (pair.second == socketFd) {
            return pair.first;
        }
    }
    return ""; // 未找到
}

// 根据用户名获取socket fd
int UserManager::getSocketByUsername(const std::string& username) const {
    std::lock_guard<std::mutex> lock(onlineUsersMutex);
    
    auto it = onlineUsers.find(username);
    if (it != onlineUsers.end()) {
        return it->second;
    }
    return -1; // 未找到
}

// 获取在线用户列表
std::vector<std::string> UserManager::getOnlineUserList() const {
    std::lock_guard<std::mutex> lock(onlineUsersMutex);
    
    std::vector<std::string> userList;
    for (const auto& pair : onlineUsers) {
        userList.push_back(pair.first);
    }
    
    return userList;
}

// 获取用户信息
User UserManager::getUserInfo(const std::string& username) const {
    std::lock_guard<std::mutex> lock(usersMutex);
    
    auto it = users.find(username);
    if (it != users.end()) {
        return it->second;
    }
    
    return User(); // 返回默认用户对象
}

// 保存数据到文件
bool UserManager::saveToFile() const {
    std::ofstream file(dataFilePath, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "[用户管理] 无法打开文件进行写入: " << dataFilePath << std::endl;
        return false;
    }
    
    for (const auto& pair : users) {
        file << pair.second.serialize() << std::endl;
    }
    
    file.close();
    std::cout << "[用户管理] 用户数据已保存到文件: " << dataFilePath << std::endl;
    return true;
}

// 从文件加载数据
bool UserManager::loadFromFile() {
    std::ifstream file(dataFilePath);
    if (!file.is_open()) {
        std::cout << "[用户管理] 用户数据文件不存在，将创建新文件: " << dataFilePath << std::endl;
        return true; // 文件不存在是正常情况，第一次运行时
    }
    
    std::string line;
    int loadedCount = 0;
    
    while (std::getline(file, line)) {
        if (!line.empty()) {
            try {
                User user = User::deserialize(trim(line));
                if (!user.username.empty()) {
                    users[user.username] = user;
                    loadedCount++;
                }
            } catch (const std::exception& e) {
                std::cerr << "[用户管理] 解析用户数据失败: " << line << std::endl;
            }
        }
    }
    
    file.close();
    std::cout << "[用户管理] 从文件加载了 " << loadedCount << " 个用户数据" << std::endl;
    return true;
}

// 获取在线用户数量
size_t UserManager::getOnlineUserCount() const {
    std::lock_guard<std::mutex> lock(onlineUsersMutex);
    return onlineUsers.size();
}

// 内部辅助函数 - 去除字符串首尾空白
std::string UserManager::trim(const std::string& str) const {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}