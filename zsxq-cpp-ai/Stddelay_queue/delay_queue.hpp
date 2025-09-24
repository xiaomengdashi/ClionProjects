#ifndef DELAY_QUEUE_HPP
#define DELAY_QUEUE_HPP

#include <queue>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>

/**
 * @brief 延迟队列模板类
 *
 * 延迟队列是一种特殊的队列，其中的元素只有在指定的延迟时间后才能被取出。
 * 适用场景：
 * - 定时任务调度
 * - 缓存过期清理
 * - 超时重试机制
 * - 消息延迟投递
 *
 * @tparam T 存储在队列中的元素类型
 */
template<typename T>
class DelayQueue {
public:
    // 使用chrono库的时间点类型
    using TimePoint = std::chrono::steady_clock::time_point;
    using Duration = std::chrono::milliseconds;

    /**
     * @brief 延迟元素包装类
     *
     * 将实际数据和过期时间封装在一起
     */
    class DelayedElement {
    public:
        /**
         * @brief 构造延迟元素
         * @param data 实际数据
         * @param expiry 过期时间点
         */
        DelayedElement(T data, TimePoint expiry)
            : data_(std::move(data)), expiry_(expiry) {}

        /**
         * @brief 获取数据
         * @return 返回存储的数据引用
         */
        T& getData() { return data_; }
        const T& getData() const { return data_; }

        /**
         * @brief 获取过期时间
         * @return 返回过期时间点
         */
        TimePoint getExpiry() const { return expiry_; }

        /**
         * @brief 检查元素是否已过期
         * @return 如果当前时间已超过过期时间，返回true
         */
        bool isExpired() const {
            return std::chrono::steady_clock::now() >= expiry_;
        }

        /**
         * @brief 获取剩余延迟时间
         * @return 返回剩余的延迟时间（毫秒），如果已过期则返回0
         */
        Duration getRemainingDelay() const {
            auto now = std::chrono::steady_clock::now();
            if (now >= expiry_) {
                return Duration(0);
            }
            return std::chrono::duration_cast<Duration>(expiry_ - now);
        }

    private:
        T data_;           // 实际存储的数据
        TimePoint expiry_; // 过期时间点
    };

    /**
     * @brief 元素比较器
     *
     * 用于优先级队列的比较，过期时间早的元素优先级更高
     */
    struct ElementComparator {
        bool operator()(const std::shared_ptr<DelayedElement>& a,
                       const std::shared_ptr<DelayedElement>& b) const {
            // 返回true表示a的优先级低于b
            // 这里我们希望过期时间早的元素优先级更高，所以使用大于号
            return a->getExpiry() > b->getExpiry();
        }
    };

    /**
     * @brief 默认构造函数
     */
    DelayQueue() : running_(true) {}

    /**
     * @brief 析构函数
     *
     * 停止队列并清理资源
     */
    ~DelayQueue() {
        shutdown();
        // 等待一小段时间，确保所有正在执行的操作完成
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    /**
     * @brief 向队列中添加元素
     *
     * @param data 要添加的数据
     * @param delay_ms 延迟时间（毫秒）
     * @return 返回添加的延迟元素的共享指针
     */
    std::shared_ptr<DelayedElement> put(T data, int64_t delay_ms) {
        auto expiry = std::chrono::steady_clock::now() +
                     std::chrono::milliseconds(delay_ms);

        auto element = std::make_shared<DelayedElement>(
            std::move(data), expiry);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(element);
        }

        // 通知可能正在等待的消费者
        cv_.notify_one();

        return element;
    }

    /**
     * @brief 向队列中添加元素（使用chrono duration）
     *
     * @param data 要添加的数据
     * @param delay 延迟时间
     * @return 返回添加的延迟元素的共享指针
     */
    template<typename Rep, typename Period>
    std::shared_ptr<DelayedElement> put(T data,
                                        std::chrono::duration<Rep, Period> delay) {
        auto delay_ms = std::chrono::duration_cast<std::chrono::milliseconds>(delay);
        return put(std::move(data), delay_ms.count());
    }

    /**
     * @brief 非阻塞地尝试获取一个已过期的元素
     *
     * @param result 如果成功获取，将数据存储在这里
     * @return 如果成功获取到元素返回true，否则返回false
     */
    bool tryTake(T& result) {
        std::lock_guard<std::mutex> lock(mutex_);

        // 检查队列是否为空
        if (queue_.empty()) {
            return false;
        }

        // 检查队首元素是否已过期
        auto element = queue_.top();
        if (!element->isExpired()) {
            return false;
        }

        // 取出已过期的元素
        result = std::move(element->getData());
        queue_.pop();

        return true;
    }

    /**
     * @brief 阻塞地获取一个元素
     *
     * 如果队列中没有已过期的元素，将阻塞等待直到有元素过期
     *
     * @param result 用于存储获取到的数据
     * @return 如果成功获取返回true，如果队列已关闭返回false
     */
    bool take(T& result) {
        std::unique_lock<std::mutex> lock(mutex_);

        while (running_) {
            // 队列为空时等待
            if (queue_.empty()) {
                cv_.wait(lock);
                continue;
            }

            auto element = queue_.top();
            auto delay = element->getRemainingDelay();

            // 如果队首元素已过期，立即返回
            if (delay.count() <= 0) {
                result = std::move(element->getData());
                queue_.pop();
                return true;
            }

            // 等待直到队首元素过期或有新元素加入
            cv_.wait_for(lock, delay);
        }

        return false;  // 队列已关闭
    }

    /**
     * @brief 带超时的阻塞获取
     *
     * @param result 用于存储获取到的数据
     * @param timeout_ms 最大等待时间（毫秒）
     * @return 如果成功获取返回true，超时或队列关闭返回false
     */
    bool take(T& result, int64_t timeout_ms) {
        auto timeout_point = std::chrono::steady_clock::now() +
                            std::chrono::milliseconds(timeout_ms);

        std::unique_lock<std::mutex> lock(mutex_);

        while (running_) {
            auto now = std::chrono::steady_clock::now();
            if (now >= timeout_point) {
                return false;  // 超时
            }

            if (queue_.empty()) {
                // 等待直到超时或有新元素
                if (cv_.wait_until(lock, timeout_point) == std::cv_status::timeout) {
                    return false;
                }
                continue;
            }

            auto element = queue_.top();
            auto element_delay = element->getRemainingDelay();

            // 如果元素已过期，立即返回
            if (element_delay.count() <= 0) {
                result = std::move(element->getData());
                queue_.pop();
                return true;
            }

            // 计算实际等待时间：取元素过期时间和超时时间的较早时间点
            auto wait_time = (element->getExpiry() < timeout_point)
                ? element->getExpiry()
                : timeout_point;

            if (cv_.wait_until(lock, wait_time) == std::cv_status::timeout) {
                // 再次检查是否有元素过期
                if (!queue_.empty() && queue_.top()->isExpired()) {
                    element = queue_.top();
                    result = std::move(element->getData());
                    queue_.pop();
                    return true;
                }
                return false;
            }
        }

        return false;
    }

    /**
     * @brief 获取队列中的元素数量
     *
     * @return 返回队列中的元素总数（包括未过期的）
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    /**
     * @brief 检查队列是否为空
     *
     * @return 如果队列为空返回true
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    /**
     * @brief 清空队列中的所有元素
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!queue_.empty()) {
            queue_.pop();
        }
    }

    /**
     * @brief 获取队首元素的剩余延迟时间
     *
     * @return 返回队首元素的剩余延迟时间（毫秒），队列为空时返回-1
     */
    int64_t peekDelay() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return -1;
        }
        return queue_.top()->getRemainingDelay().count();
    }

    /**
     * @brief 关闭队列
     *
     * 停止队列操作，唤醒所有等待的线程
     */
    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_) {
                return;
            }
            running_ = false;
        }
        cv_.notify_all();
    }

    /**
     * @brief 检查队列是否正在运行
     *
     * @return 如果队列正在运行返回true
     */
    bool isRunning() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return running_;
    }

    /**
     * @brief 批量获取所有已过期的元素
     *
     * @param max_count 最多获取的元素数量，0表示不限制
     * @return 返回所有已过期元素的vector
     */
    std::vector<T> drainExpired(size_t max_count = 0) {
        std::vector<T> result;
        bool need_notify = false;

        {
            std::lock_guard<std::mutex> lock(mutex_);

            size_t count = 0;
            while (!queue_.empty() && (max_count == 0 || count < max_count)) {
                auto element = queue_.top();
                if (!element->isExpired()) {
                    break;
                }

                result.push_back(std::move(element->getData()));
                queue_.pop();
                ++count;
                need_notify = true;
            }
        }

        // 如果取出了元素，通知可能在等待的线程
        if (need_notify) {
            cv_.notify_all();
        }

        return result;
    }

private:
    // 使用优先级队列存储延迟元素，以过期时间作为优先级
    std::priority_queue<
        std::shared_ptr<DelayedElement>,
        std::vector<std::shared_ptr<DelayedElement>>,
        ElementComparator
    > queue_;

    mutable std::mutex mutex_;              // 保护队列的互斥锁
    std::condition_variable cv_;            // 用于线程间同步的条件变量
    std::atomic<bool> running_;             // 队列运行状态标志
};

#endif // DELAY_QUEUE_HPP