#pragma once
#include <iostream>
#include <thread>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <vector>
#include <atomic>

// 消息结构体
struct Message {
    int id;                     // 消息号
    std::vector<char> content;  // 消息内容（使用vector自动管理内存）

    Message(const int id, const char* data, const size_t len)
        : id(id), content(data, data + len) {}
};

// 前置声明 Task 类
class Task;

// 线程管理器（单例）
class ThreadManager {
public:
    static ThreadManager& getInstance();
    void registerTask(const std::shared_ptr<Task>& task);
    void unregisterTask(int id);
    void sendMessage(int receiverId, const Message& msg);

private:
    ThreadManager() = default;
    std::unordered_map<int, std::shared_ptr<Task>> tasks_;
    std::mutex mutex_;
};

// 任务基类
class Task : public std::enable_shared_from_this<Task> {
public:
    explicit Task(const int id) : id_(id), running_(false) {}
    virtual ~Task() { stop(); }

    void start() {
        running_ = true;
        thread_ = std::thread(&Task::runThread, this);
    }

    void stop() {
        if (running_) {
            running_ = false;
            cv_.notify_one();  // 唤醒等待的线程
            if (thread_.joinable()) {
                thread_.join();
            }
        }
    }

    int getId() const { return id_; }

    void sendMessage(const Message& msg) {
        {
            std::lock_guard lock(mutex_);
            queue_.push(msg);
        }
        cv_.notify_one();  // 通知有消息到达
    }

    // 新增方法，用于向其他任务发送消息
    static void sendMessageTo(int receiverId, const Message& msg) {
        ThreadManager::getInstance().sendMessage(receiverId, msg);
    }

protected:
    virtual void processMessage(const Message& msg) = 0;

private:
    void runThread() {
        // 注册到线程管理器
        ThreadManager::getInstance().registerTask(shared_from_this());

        while (running_) {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this] { return !queue_.empty() || !running_; });

            if (!running_) break;

            // 取出消息处理
            auto msg = queue_.front();
            queue_.pop();
            lock.unlock();  // 提前释放锁

            processMessage(msg);
        }

        // 注销
        ThreadManager::getInstance().unregisterTask(id_);
    }

    int id_;
    std::atomic<bool> running_;
    std::thread thread_;
    std::queue<Message> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

// 实现 ThreadManager 类的成员函数
inline ThreadManager& ThreadManager::getInstance() {
    static ThreadManager instance;
    return instance;
}

inline void ThreadManager::registerTask(const std::shared_ptr<Task>& task) {
    std::lock_guard lock(mutex_);
    tasks_[task->getId()] = task;
}

inline void ThreadManager::unregisterTask(const int id) {
    std::lock_guard lock(mutex_);
    tasks_.erase(id);
    std::cout << "Task " << id << " unregistered." << std::endl;
}

inline void ThreadManager::sendMessage(const int receiverId, const Message& msg) {
    std::lock_guard lock(mutex_);
    if (const auto it = tasks_.find(receiverId); it != tasks_.end()) {
        it->second->sendMessage(msg);
    } else {
        std::cerr << "Task " << receiverId << " not found!" << std::endl;
    }
}

// 示例任务实现
class MyTask final : public Task {
public:
    explicit MyTask(const int id) : Task(id) {}

protected:
    void processMessage(const Message& msg) override {
        std::cout << "Task " << getId() << " received message #" << msg.id
                  << ", content: " << msg.content.data() << std::endl;
    }
};

// 实例任务实现2
class MyTask2 final : public Task {
public:
    explicit MyTask2(const int id) : Task(id) {}
protected:
    void processMessage(const Message& msg) override {
        std::cout << "Task2 " << getId() << " received message #" << msg.id
            << ", content: " << msg.content.data() << std::endl;
    }
};

int task_test() {
    // 创建任务数组
    std::vector<std::shared_ptr<Task>> tasks = {
        std::make_shared<MyTask>(1),
        std::make_shared<MyTask2>(2)
    };

    // 启动所有任务
    for (const auto& task : tasks) {
        task->start();
    }

    // 等待一段时间确保任务启动
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // task1 调用发送接口给 task2 发送消息
    tasks[0]->sendMessageTo(2, Message(100, "Hello from Task1", 16));
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // task2 调用发送接口给 task1 发送消息
    tasks[1]->sendMessageTo(1, Message(200, "Hi from Task2", 13));

    // 等待一段时间确保消息处理
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 停止所有任务
    for (const auto& task : tasks) {
        task->stop();
    }

    return 0;
}
