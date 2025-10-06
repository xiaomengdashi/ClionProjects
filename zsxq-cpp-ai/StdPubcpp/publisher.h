#ifndef PUBLISHER_H
#define PUBLISHER_H

#include <memory>
#include <atomic>
#include <sstream>
#include "message_queue.h"
#include "message.h"

/**
 * 发布者类 - 负责向消息队列发布消息
 * 提供便捷的消息发布接口
 */
class Publisher {
private:
    std::shared_ptr<MessageQueue> queue_;  // 消息队列引用
    std::string publisher_id_;             // 发布者ID
    std::atomic<size_t> message_counter_;  // 消息计数器

public:
    // 构造函数
    Publisher(std::shared_ptr<MessageQueue> queue, const std::string& publisher_id)
        : queue_(queue), publisher_id_(publisher_id), message_counter_(0) {}

    // 发布消息（自动生成消息ID）
    void publish(const std::string& topic, const std::any& data,
                Message::Priority priority = Message::Priority::NORMAL) {
        // 生成唯一消息ID
        std::stringstream ss;
        ss << publisher_id_ << "_" << message_counter_++;
        std::string message_id = ss.str();

        // 创建并发布消息
        auto message = std::make_shared<Message>(message_id, topic, data, priority);
        queue_->publish(message);
    }

    // 发布带自定义ID的消息
    void publishWithId(const std::string& message_id, const std::string& topic,
                      const std::any& data, Message::Priority priority = Message::Priority::NORMAL) {
        auto message = std::make_shared<Message>(message_id, topic, data, priority);
        queue_->publish(message);
    }

    // 批量发布消息
    void publishBatch(const std::string& topic,
                     const std::vector<std::any>& data_list,
                     Message::Priority priority = Message::Priority::NORMAL) {
        for (const auto& data : data_list) {
            publish(topic, data, priority);
        }
    }

    // 获取发布者ID
    const std::string& getId() const { return publisher_id_; }

    // 获取已发布消息数量
    size_t getPublishedCount() const { return message_counter_.load(); }
};

#endif // PUBLISHER_H