#ifndef SAMPLE_SUBSCRIBERS_H
#define SAMPLE_SUBSCRIBERS_H

#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <atomic>
#include "subscriber.h"

/**
 * 控制台订阅者 - 将消息输出到控制台
 */
class ConsoleSubscriber : public ISubscriber {
private:
    std::string name_;
    std::string id_;
    std::atomic<size_t> message_count_;

public:
    ConsoleSubscriber(const std::string& name, const std::string& id)
        : name_(name), id_(id), message_count_(0) {}

    // 处理接收到的消息
    void onMessage(const std::shared_ptr<Message>& message) override {
        message_count_++;

        // 获取当前时间
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);

        std::cout << "========================================\n"
                  << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "] "
                  << "订阅者: " << name_ << " (ID: " << id_ << ")\n"
                  << "消息ID: " << message->getId() << "\n"
                  << "主题: " << message->getTopic() << "\n"
                  << "优先级: " << static_cast<int>(message->getPriority()) << "\n"
                  << "消息年龄: " << message->getAgeMs() << " ms\n";

        // 尝试打印不同类型的数据
        try {
            if (message->getData().type() == typeid(std::string)) {
                std::cout << "数据: " << std::any_cast<std::string>(message->getData()) << "\n";
            } else if (message->getData().type() == typeid(int)) {
                std::cout << "数据: " << std::any_cast<int>(message->getData()) << "\n";
            } else if (message->getData().type() == typeid(double)) {
                std::cout << "数据: " << std::any_cast<double>(message->getData()) << "\n";
            } else {
                std::cout << "数据: [未知类型]\n";
            }
        } catch (const std::bad_any_cast& e) {
            std::cout << "数据: [类型转换错误]\n";
        }

        std::cout << "已处理消息数: " << message_count_.load() << "\n"
                  << "========================================\n" << std::endl;
    }

    std::string getName() const override { return name_; }
    std::string getId() const override { return id_; }

    // 获取已处理消息数量
    size_t getMessageCount() const { return message_count_.load(); }
};

/**
 * 文件订阅者 - 将消息记录到文件
 */
class FileSubscriber : public ISubscriber {
private:
    std::string name_;
    std::string id_;
    std::string filename_;
    std::ofstream file_;
    std::atomic<size_t> message_count_;
    std::mutex file_mutex_;

public:
    FileSubscriber(const std::string& name, const std::string& id, const std::string& filename)
        : name_(name), id_(id), filename_(filename), message_count_(0) {
        file_.open(filename, std::ios::app);
        if (!file_.is_open()) {
            throw std::runtime_error("无法打开文件: " + filename);
        }
    }

    ~FileSubscriber() {
        if (file_.is_open()) {
            file_.close();
        }
    }

    // 处理接收到的消息
    void onMessage(const std::shared_ptr<Message>& message) override {
        std::lock_guard<std::mutex> lock(file_mutex_);
        message_count_++;

        // 获取当前时间
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);

        file_ << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "] "
              << "Subscriber: " << name_ << " | "
              << "MessageID: " << message->getId() << " | "
              << "Topic: " << message->getTopic() << " | "
              << "Priority: " << static_cast<int>(message->getPriority()) << " | ";

        // 尝试记录不同类型的数据
        try {
            if (message->getData().type() == typeid(std::string)) {
                file_ << "Data: " << std::any_cast<std::string>(message->getData());
            } else if (message->getData().type() == typeid(int)) {
                file_ << "Data: " << std::any_cast<int>(message->getData());
            } else if (message->getData().type() == typeid(double)) {
                file_ << "Data: " << std::any_cast<double>(message->getData());
            } else {
                file_ << "Data: [Unknown Type]";
            }
        } catch (const std::bad_any_cast& e) {
            file_ << "Data: [Type Cast Error]";
        }

        file_ << std::endl;
        file_.flush();
    }

    std::string getName() const override { return name_; }
    std::string getId() const override { return id_; }

    // 获取已处理消息数量
    size_t getMessageCount() const { return message_count_.load(); }
};

/**
 * 过滤订阅者 - 只处理特定条件的消息
 */
class FilterSubscriber : public ISubscriber {
private:
    std::string name_;
    std::string id_;
    std::function<bool(const std::shared_ptr<Message>&)> filter_;  // 过滤函数
    std::shared_ptr<ISubscriber> delegate_;  // 委托订阅者
    std::atomic<size_t> filtered_count_;     // 过滤掉的消息数
    std::atomic<size_t> processed_count_;    // 处理的消息数

public:
    FilterSubscriber(const std::string& name, const std::string& id,
                    std::function<bool(const std::shared_ptr<Message>&)> filter,
                    std::shared_ptr<ISubscriber> delegate)
        : name_(name), id_(id), filter_(filter), delegate_(delegate),
          filtered_count_(0), processed_count_(0) {}

    // 处理接收到的消息
    void onMessage(const std::shared_ptr<Message>& message) override {
        if (filter_(message)) {
            delegate_->onMessage(message);
            processed_count_++;
        } else {
            filtered_count_++;
        }
    }

    std::string getName() const override { return name_; }
    std::string getId() const override { return id_; }

    // 获取统计信息
    size_t getFilteredCount() const { return filtered_count_.load(); }
    size_t getProcessedCount() const { return processed_count_.load(); }
};

/**
 * 统计订阅者 - 收集消息统计信息
 */
class StatisticsSubscriber : public ISubscriber {
private:
    std::string name_;
    std::string id_;

    // 统计信息
    std::atomic<size_t> total_messages_;
    std::atomic<size_t> low_priority_count_;
    std::atomic<size_t> normal_priority_count_;
    std::atomic<size_t> high_priority_count_;
    std::atomic<size_t> urgent_priority_count_;
    std::atomic<long> total_latency_ms_;

    std::unordered_map<std::string, size_t> topic_counts_;  // 主题计数
    mutable std::mutex stats_mutex_;  // 使用mutable以在const方法中使用

public:
    StatisticsSubscriber(const std::string& name, const std::string& id)
        : name_(name), id_(id), total_messages_(0),
          low_priority_count_(0), normal_priority_count_(0),
          high_priority_count_(0), urgent_priority_count_(0),
          total_latency_ms_(0) {}

    // 处理接收到的消息
    void onMessage(const std::shared_ptr<Message>& message) override {
        total_messages_++;

        // 更新优先级统计
        switch (message->getPriority()) {
            case Message::Priority::LOW:
                low_priority_count_++;
                break;
            case Message::Priority::NORMAL:
                normal_priority_count_++;
                break;
            case Message::Priority::HIGH:
                high_priority_count_++;
                break;
            case Message::Priority::URGENT:
                urgent_priority_count_++;
                break;
        }

        // 更新延迟统计
        total_latency_ms_ += message->getAgeMs();

        // 更新主题统计
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            topic_counts_[message->getTopic()]++;
        }
    }

    std::string getName() const override { return name_; }
    std::string getId() const override { return id_; }

    // 打印统计报告
    void printStatistics() const {
        std::cout << "\n===== 消息统计报告 =====\n"
                  << "订阅者: " << name_ << " (ID: " << id_ << ")\n"
                  << "总消息数: " << total_messages_.load() << "\n"
                  << "优先级分布:\n"
                  << "  - 低优先级: " << low_priority_count_.load() << "\n"
                  << "  - 普通优先级: " << normal_priority_count_.load() << "\n"
                  << "  - 高优先级: " << high_priority_count_.load() << "\n"
                  << "  - 紧急优先级: " << urgent_priority_count_.load() << "\n";

        if (total_messages_ > 0) {
            std::cout << "平均延迟: "
                      << (total_latency_ms_.load() / total_messages_.load())
                      << " ms\n";
        }

        std::cout << "主题分布:\n";
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            for (const auto& [topic, count] : topic_counts_) {
                std::cout << "  - " << topic << ": " << count << "\n";
            }
        }

        std::cout << "========================\n" << std::endl;
    }
};

#endif // SAMPLE_SUBSCRIBERS_H