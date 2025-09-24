#ifndef USER_H
#define USER_H

#include <string>
#include <ctime>

// 用户状态枚举
enum class UserStatus {
    OFFLINE = 0,        // 离线
    ONLINE = 1,         // 在线
    BUSY = 2            // 忙碌
};

// 用户类 - 存储用户信息
class User {
public:
    std::string username;       // 用户名
    std::string password;       // 密码（加密存储）
    std::string email;          // 邮箱
    std::time_t registerTime;   // 注册时间
    std::time_t lastLoginTime;  // 最后登录时间
    UserStatus status;          // 当前状态
    int socketFd;              // socket文件描述符（运行时使用）
    
    // 构造函数
    User();
    User(const std::string& name, const std::string& pwd);
    User(const std::string& name, const std::string& pwd, const std::string& mail);
    
    // 验证密码
    bool verifyPassword(const std::string& pwd) const;
    
    // 设置密码（自动加密）
    void setPassword(const std::string& pwd);
    
    // 序列化用户数据为字符串（用于文件存储）
    std::string serialize() const;
    
    // 从字符串反序列化用户数据
    static User deserialize(const std::string& data);
    
    // 获取显示用的用户信息
    std::string getDisplayInfo() const;

private:
    // 密码加密函数（简单哈希）
    std::string encryptPassword(const std::string& pwd) const;
};

#endif // USER_H