#ifndef MESSAGE_H
#define MESSAGE_H

#include <string>
#include <chrono>
#include <any>

/**
 * 消息类 - 用于在队列中传递的基本消息单元
 * 包含消息ID、主题、内容、时间戳和优先级
 */
class Message {
public:
    // 消息优先级枚举
    enum Priority {
        LOW = 0,      // 低优先级
        NORMAL = 1,   // 普通优先级
        HIGH = 2,     // 高优先级
        URGENT = 3    // 紧急优先级
    };

private:
    std::string id_;        // 消息唯一标识
    std::string topic_;     // 消息主题，用于订阅筛选
    std::any data_;        // 消息数据内容，支持任意类型
    std::chrono::steady_clock::time_point timestamp_;  // 消息创建时间戳
    Priority priority_;     // 消息优先级

public:
    // 构造函数
    Message(const std::string& id, const std::string& topic, const std::any& data, Priority priority = NORMAL)
        : id_(id), topic_(topic), data_(data), priority_(priority) {
        timestamp_ = std::chrono::steady_clock::now();
    }

    // 获取消息ID
    const std::string& getId() const { return id_; }

    // 获取消息主题
    const std::string& getTopic() const { return topic_; }

    // 获取消息数据
    const std::any& getData() const { return data_; }

    // 获取消息时间戳
    std::chrono::steady_clock::time_point getTimestamp() const { return timestamp_; }

    // 获取消息优先级
    Priority getPriority() const { return priority_; }

    // 获取消息年龄（毫秒）
    long getAgeMs() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - timestamp_).count();
    }
};

#endif // MESSAGE_H