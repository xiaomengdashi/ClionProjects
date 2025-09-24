#ifndef USER_MANAGER_H
#define USER_MANAGER_H

#include "User.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>

// 用户管理类 - 负责用户注册、登录、数据持久化
class UserManager {
private:
    std::unordered_map<std::string, User> users;           // 内存中的用户数据
    std::unordered_map<std::string, int> onlineUsers;      // 在线用户映射（用户名->socket fd）
    std::string dataFilePath;                              // 用户数据文件路径
    mutable std::mutex usersMutex;                         // 用户数据互斥锁
    mutable std::mutex onlineUsersMutex;                   // 在线用户互斥锁

public:
    // 构造函数
    UserManager(const std::string& filePath = "users.dat");
    
    // 析构函数
    ~UserManager();
    
    // 用户注册
    bool registerUser(const std::string& username, const std::string& password, 
                     const std::string& email = "");
    
    // 用户登录
    bool loginUser(const std::string& username, const std::string& password, int socketFd);
    
    // 用户登出
    void logoutUser(const std::string& username);
    void logoutUser(int socketFd);
    
    // 检查用户名是否存在
    bool userExists(const std::string& username) const;
    
    // 检查用户是否在线
    bool isUserOnline(const std::string& username) const;
    
    // 根据socket fd获取用户名
    std::string getUsernameBySocket(int socketFd) const;
    
    // 根据用户名获取socket fd
    int getSocketByUsername(const std::string& username) const;
    
    // 获取在线用户列表
    std::vector<std::string> getOnlineUserList() const;
    
    // 获取用户信息
    User getUserInfo(const std::string& username) const;
    
    // 数据持久化
    bool saveToFile() const;
    bool loadFromFile();
    
    // 获取在线用户数量
    size_t getOnlineUserCount() const;

private:
    // 内部辅助函数
    std::string trim(const std::string& str) const;
};

#endif // USER_MANAGER_H