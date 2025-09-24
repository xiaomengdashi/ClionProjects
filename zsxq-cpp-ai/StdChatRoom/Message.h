#ifndef MESSAGE_H
#define MESSAGE_H

#include <string>
#include <ctime>

// 消息类型枚举
enum class MessageType {
    LOGIN = 1,              // 登录消息
    REGISTER = 2,           // 注册消息  
    CHAT_PUBLIC = 3,        // 群聊消息
    CHAT_PRIVATE = 4,       // 私聊消息
    LOGOUT = 5,             // 登出消息
    USER_LIST = 6,          // 用户列表请求
    RESPONSE = 7,           // 服务器响应
    FILE_TRANSFER_REQUEST = 8,  // 文件传输请求（私聊）
    FILE_TRANSFER_ACCEPT = 9,   // 接受文件传输
    FILE_TRANSFER_REJECT = 10,  // 拒绝文件传输
    FILE_UPLOAD_GROUP = 11,     // 群文件上传
    FILE_LIST_REQUEST = 12,     // 群文件列表请求
    FILE_LIST_RESPONSE = 13,    // 群文件列表响应
    FILE_DOWNLOAD_REQUEST = 14, // 文件下载请求
    FILE_DATA = 15,             // 文件数据传输
    FILE_TRANSFER_COMPLETE = 16 // 文件传输完成
};

// 消息类 - 封装所有通信消息
class Message {
public:
    MessageType type;           // 消息类型
    std::string sender;         // 发送者用户名
    std::string receiver;       // 接收者用户名（私聊时使用）
    std::string content;        // 消息内容
    std::time_t timestamp;      // 时间戳
    
    // 构造函数
    Message();
    Message(MessageType t, const std::string& s, const std::string& c);
    Message(MessageType t, const std::string& s, const std::string& r, const std::string& c);
    
    // 序列化消息为字符串（用于网络传输）
    std::string serialize() const;
    
    // 从字符串反序列化消息
    static Message deserialize(const std::string& data);
    
    // 获取格式化的时间字符串
    std::string getTimeString() const;
};

#endif // MESSAGE_H