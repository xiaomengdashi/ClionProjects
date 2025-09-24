#include "Message.h"
#include <sstream>
#include <iomanip>

// 默认构造函数
Message::Message() : type(MessageType::CHAT_PUBLIC), timestamp(std::time(nullptr)) {
}

// 构造函数 - 用于群聊消息
Message::Message(MessageType t, const std::string& s, const std::string& c) 
    : type(t), sender(s), content(c), timestamp(std::time(nullptr)) {
}

// 构造函数 - 用于私聊消息
Message::Message(MessageType t, const std::string& s, const std::string& r, const std::string& c)
    : type(t), sender(s), receiver(r), content(c), timestamp(std::time(nullptr)) {
}

// 序列化消息为字符串（用于网络传输）
// 格式: type|sender|receiver|content|timestamp
std::string Message::serialize() const {
    std::ostringstream oss;
    oss << static_cast<int>(type) << "|" 
        << sender << "|" 
        << receiver << "|" 
        << content << "|" 
        << timestamp;
    return oss.str();
}

// 从字符串反序列化消息
Message Message::deserialize(const std::string& data) {
    Message msg;
    std::istringstream iss(data);
    std::string token;
    
    // 解析消息类型
    if (std::getline(iss, token, '|')) {
        msg.type = static_cast<MessageType>(std::stoi(token));
    }
    
    // 解析发送者
    if (std::getline(iss, token, '|')) {
        msg.sender = token;
    }
    
    // 解析接收者
    if (std::getline(iss, token, '|')) {
        msg.receiver = token;
    }
    
    // 解析消息内容和时间戳
    // 由于content可能包含分隔符'|'，我们需要特殊处理
    // 先获取剩余的字符串，然后从末尾找最后一个'|'作为时间戳分隔符
    std::string remaining;
    std::getline(iss, remaining);
    
    // 查找最后一个'|'来分离content和timestamp
    size_t lastPipePos = remaining.find_last_of('|');
    if (lastPipePos != std::string::npos) {
        msg.content = remaining.substr(0, lastPipePos);
        std::string timestampStr = remaining.substr(lastPipePos + 1);
        try {
            msg.timestamp = std::stoll(timestampStr);
        } catch (const std::exception& e) {
            // 如果时间戳解析失败，使用当前时间
            msg.timestamp = std::time(nullptr);
        }
    } else {
        // 如果没有找到时间戳分隔符，将整个剩余部分作为content
        msg.content = remaining;
        msg.timestamp = std::time(nullptr);
    }
    
    return msg;
}

// 获取格式化的时间字符串
std::string Message::getTimeString() const {
    std::tm* tm_time = std::localtime(&timestamp);
    std::ostringstream oss;
    oss << std::put_time(tm_time, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}