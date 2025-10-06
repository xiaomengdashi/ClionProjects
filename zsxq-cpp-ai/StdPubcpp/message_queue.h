#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <functional>
#include <algorithm>
#include <random>
#include "message.h"
#include "subscriber.h"

/**
 * 分发策略枚举
 * BROADCAST: 广播模式 - 所有订阅者都收到消息
 * ROUND_ROBIN: 轮询模式 - 消息轮流分发给订阅者
 * RANDOM: 随机模式 - 随机选择一个订阅者
 */
enum class DistributionStrategy {
    BROADCAST,      // 广播模式
    ROUND_ROBIN,    // 轮询模式
    RANDOM         // 随机模式
};

/**
 * 消息队列类 - 核心消息队列实现
 * 支持多主题、多订阅者、多种分发策略
 */
class MessageQueue {
private:
    // 消息优先级比较器
    struct MessageComparator {
        bool operator()(const std::shared_ptr<Message>& a, const std::shared_ptr<Message>& b) {
            // 优先级高的在前，同优先级按时间戳排序
            if (a->getPriority() != b->getPriority()) {
                return a->getPriority() < b->getPriority();
            }
            return a->getTimestamp() > b->getTimestamp();
        }
    };

    // 主题订阅信息
    struct TopicSubscription {
        std::vector<std::shared_ptr<ISubscriber>> subscribers;  // 订阅者列表
        DistributionStrategy strategy;                         // 分发策略
        size_t roundRobinIndex = 0;                           // 轮询索引
    };

    // 使用优先级队列存储消息
    std::priority_queue<std::shared_ptr<Message>,
                       std::vector<std::shared_ptr<Message>>,
                       MessageComparator> messages_;

    // 主题到订阅信息的映射
    std::unordered_map<std::string, TopicSubscription> subscriptions_;

    // 线程安全相关
    mutable std::mutex queue_mutex_;           // 队列互斥锁
    mutable std::mutex subscription_mutex_;    // 订阅互斥锁
    std::condition_variable cv_;               // 条件变量

    // 工作线程相关
    std::vector<std::thread> workers_;         // 工作线程池
    std::atomic<bool> running_;                // 运行状态标志
    size_t worker_count_;                      // 工作线程数量

    // 统计信息
    std::atomic<size_t> total_messages_processed_;  // 已处理消息总数
    std::atomic<size_t> total_messages_published_;  // 已发布消息总数

    // 随机数生成器
    std::mt19937 random_generator_;
    std::uniform_int_distribution<size_t> random_dist_;

    // 处理消息的工作函数
    void workerFunction() {
        while (running_) {
            std::unique_lock<std::mutex> lock(queue_mutex_);

            // 等待消息或停止信号
            cv_.wait(lock, [this] { return !messages_.empty() || !running_; });

            if (!running_) break;

            if (!messages_.empty()) {
                // 取出优先级最高的消息
                auto message = messages_.top();
                messages_.pop();
                lock.unlock();

                // 分发消息给订阅者
                distributeMessage(message);
                total_messages_processed_++;
            }
        }
    }

    // 分发消息给订阅者
    void distributeMessage(const std::shared_ptr<Message>& message) {
        std::lock_guard<std::mutex> lock(subscription_mutex_);

        auto it = subscriptions_.find(message->getTopic());
        if (it == subscriptions_.end() || it->second.subscribers.empty()) {
            return;  // 没有订阅者
        }

        auto& subscription = it->second;

        switch (subscription.strategy) {
            case DistributionStrategy::BROADCAST:
                // 广播给所有订阅者
                for (auto& subscriber : subscription.subscribers) {
                    try {
                        subscriber->onMessage(message);
                    } catch (const std::exception& e) {
                        // 记录错误但继续处理其他订阅者
                        std::cerr << "订阅者处理消息异常: " << e.what() << std::endl;
                    }
                }
                break;

            case DistributionStrategy::ROUND_ROBIN:
                // 轮询分发
                if (!subscription.subscribers.empty()) {
                    size_t index = subscription.roundRobinIndex % subscription.subscribers.size();
                    try {
                        subscription.subscribers[index]->onMessage(message);
                    } catch (const std::exception& e) {
                        std::cerr << "订阅者处理消息异常: " << e.what() << std::endl;
                    }
                    subscription.roundRobinIndex = (subscription.roundRobinIndex + 1) % subscription.subscribers.size();
                }
                break;

            case DistributionStrategy::RANDOM:
                // 随机分发
                if (!subscription.subscribers.empty()) {
                    std::uniform_int_distribution<size_t> dist(0, subscription.subscribers.size() - 1);
                    size_t index = dist(random_generator_);
                    try {
                        subscription.subscribers[index]->onMessage(message);
                    } catch (const std::exception& e) {
                        std::cerr << "订阅者处理消息异常: " << e.what() << std::endl;
                    }
                }
                break;
        }
    }

public:
    // 构造函数
    explicit MessageQueue(size_t worker_count = 4)
        : worker_count_(worker_count),
          running_(true),
          total_messages_processed_(0),
          total_messages_published_(0),
          random_generator_(std::random_device{}()) {
        // 启动工作线程
        for (size_t i = 0; i < worker_count_; ++i) {
            workers_.emplace_back(&MessageQueue::workerFunction, this);
        }
    }

    // 析构函数
    ~MessageQueue() {
        stop();
    }

    // 发布消息到队列
    void publish(const std::shared_ptr<Message>& message) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            messages_.push(message);
            total_messages_published_++;
        }
        cv_.notify_one();  // 唤醒一个工作线程
    }

    // 订阅主题
    void subscribe(const std::string& topic,
                  const std::shared_ptr<ISubscriber>& subscriber,
                  DistributionStrategy strategy = DistributionStrategy::BROADCAST) {
        std::lock_guard<std::mutex> lock(subscription_mutex_);

        auto it = subscriptions_.find(topic);
        if (it == subscriptions_.end()) {
            // 首次订阅该主题，创建新的订阅信息
            TopicSubscription subscription;
            subscription.strategy = strategy;
            subscription.subscribers.push_back(subscriber);
            subscriptions_[topic] = subscription;
        } else {
            // 主题已存在，检查策略是否一致
            auto& subscription = it->second;
            if (subscription.strategy != strategy) {
                std::cerr << "警告：主题 '" << topic
                          << "' 的分发策略不一致！已存在的策略将被保留。\n";
            }

            // 检查是否已经订阅
            auto sub_it = std::find_if(subscription.subscribers.begin(),
                                       subscription.subscribers.end(),
                                       [&subscriber](const std::shared_ptr<ISubscriber>& s) {
                                           return s->getId() == subscriber->getId();
                                       });

            if (sub_it == subscription.subscribers.end()) {
                subscription.subscribers.push_back(subscriber);
            }
        }
    }

    // 取消订阅
    void unsubscribe(const std::string& topic, const std::string& subscriberId) {
        std::lock_guard<std::mutex> lock(subscription_mutex_);

        auto it = subscriptions_.find(topic);
        if (it != subscriptions_.end()) {
            auto& subscribers = it->second.subscribers;
            subscribers.erase(
                std::remove_if(subscribers.begin(), subscribers.end(),
                              [&subscriberId](const std::shared_ptr<ISubscriber>& s) {
                                  return s->getId() == subscriberId;
                              }),
                subscribers.end()
            );

            // 如果没有订阅者了，删除主题
            if (subscribers.empty()) {
                subscriptions_.erase(it);
            }
        }
    }

    // 设置主题的分发策略
    void setDistributionStrategy(const std::string& topic, DistributionStrategy strategy) {
        std::lock_guard<std::mutex> lock(subscription_mutex_);
        if (subscriptions_.find(topic) != subscriptions_.end()) {
            subscriptions_[topic].strategy = strategy;
        }
    }

    // 获取队列大小
    size_t size() const {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return messages_.size();
    }

    // 获取订阅者数量
    size_t getSubscriberCount(const std::string& topic) const {
        std::lock_guard<std::mutex> lock(subscription_mutex_);
        auto it = subscriptions_.find(topic);
        return (it != subscriptions_.end()) ? it->second.subscribers.size() : 0;
    }

    // 获取统计信息
    size_t getTotalMessagesProcessed() const {
        return total_messages_processed_.load();
    }

    size_t getTotalMessagesPublished() const {
        return total_messages_published_.load();
    }

    // 清空队列
    void clear() {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        while (!messages_.empty()) {
            messages_.pop();
        }
    }

    // 停止消息队列
    void stop() {
        running_ = false;
        cv_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    // 检查是否正在运行
    bool isRunning() const {
        return running_.load();
    }
};

#endif // MESSAGE_QUEUE_H