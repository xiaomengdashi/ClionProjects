#include "User.h"
#include <sstream>
#include <iomanip>
#include <functional>

// 默认构造函数
User::User() : registerTime(std::time(nullptr)), lastLoginTime(0),
               status(UserStatus::OFFLINE), socketFd(-1) {
}

// 构造函数 - 用户名和密码
User::User(const std::string& name, const std::string& pwd) 
    : username(name), registerTime(std::time(nullptr)), lastLoginTime(0),
      status(UserStatus::OFFLINE), socketFd(-1) {
    setPassword(pwd);
}

// 构造函数 - 用户名、密码和邮箱
User::User(const std::string& name, const std::string& pwd, const std::string& mail)
    : username(name), email(mail), registerTime(std::time(nullptr)), lastLoginTime(0),
      status(UserStatus::OFFLINE), socketFd(-1) {
    setPassword(pwd);
}

// 验证密码
bool User::verifyPassword(const std::string& pwd) const {
    return password == encryptPassword(pwd);
}

// 设置密码（自动加密）
void User::setPassword(const std::string& pwd) {
    password = encryptPassword(pwd);
}

// 序列化用户数据为字符串（用于文件存储）
// 格式: username|password|email|registerTime|lastLoginTime
std::string User::serialize() const {
    std::ostringstream oss;
    oss << username << "|" 
        << password << "|" 
        << email << "|" 
        << registerTime << "|" 
        << lastLoginTime;
    return oss.str();
}

// 从字符串反序列化用户数据
User User::deserialize(const std::string& data) {
    User user;
    std::istringstream iss(data);
    std::string token;
    
    // 解析用户名
    if (std::getline(iss, token, '|')) {
        user.username = token;
    }
    
    // 解析密码（已加密）
    if (std::getline(iss, token, '|')) {
        user.password = token;
    }
    
    // 解析邮箱
    if (std::getline(iss, token, '|')) {
        user.email = token;
    }
    
    // 解析注册时间
    if (std::getline(iss, token, '|')) {
        user.registerTime = std::stoll(token);
    }
    
    // 解析最后登录时间
    if (std::getline(iss, token, '|')) {
        user.lastLoginTime = std::stoll(token);
    }
    
    user.status = UserStatus::OFFLINE;
    user.socketFd = -1;
    
    return user;
}

// 获取显示用的用户信息
std::string User::getDisplayInfo() const {
    std::ostringstream oss;
    std::tm* tm_reg = std::localtime(&registerTime);
    std::tm* tm_login = std::localtime(&lastLoginTime);
    
    oss << "用户名: " << username << "\n";
    oss << "邮箱: " << (email.empty() ? "未设置" : email) << "\n";
    oss << "注册时间: " << std::put_time(tm_reg, "%Y-%m-%d %H:%M:%S") << "\n";
    
    if (lastLoginTime > 0) {
        oss << "最后登录: " << std::put_time(tm_login, "%Y-%m-%d %H:%M:%S") << "\n";
    } else {
        oss << "最后登录: 从未登录\n";
    }
    
    oss << "状态: ";
    switch (status) {
        case UserStatus::ONLINE:
            oss << "在线";
            break;
        case UserStatus::OFFLINE:
            oss << "离线";
            break;
        case UserStatus::BUSY:
            oss << "忙碌";
            break;
    }
    
    return oss.str();
}

// 密码加密函数（简单哈希）
std::string User::encryptPassword(const std::string& pwd) const {
    // 使用简单的hash函数进行密码加密
    // 在实际应用中，应该使用更安全的加密算法如bcrypt
    std::hash<std::string> hasher;
    size_t hashValue = hasher(pwd + "salt_key_chatroom"); // 添加盐值
    
    std::ostringstream oss;
    oss << std::hex << hashValue;
    return oss.str();
}