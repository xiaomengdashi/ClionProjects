#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <atomic>
#include <memory>
#include <vector>
#include <chrono>

struct Message {
    int id;
    std::string content;
    
    Message(int i, const std::string& c) : id(i), content(c) {}
};

class ThreadManager {
public:
    static ThreadManager& getInstance() {
        static ThreadManager instance;
        return instance;
    }
    
    void sendMessage(const Message& msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        messages_.push(msg);
        cv_.notify_one();
    }
    
    bool receiveMessage(Message& msg) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (messages_.empty()) {
            return false;
        }
        msg = messages_.front();
        messages_.pop();
        return true;
    }
    
    void waitForMessage() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !messages_.empty() || stop_; });
    }
    
    void stop() {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
        cv_.notify_all();
    }
    
private:
    ThreadManager() : stop_(false) {}
    ~ThreadManager() = default;
    ThreadManager(const ThreadManager&) = delete;
    ThreadManager& operator=(const ThreadManager&) = delete;
    
    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<Message> messages_;
    bool stop_;
};

class Task {
public:
    virtual ~Task() = default;
    virtual void run() = 0;
    virtual void stop() = 0;
    
    void start() {
        if (!running_) {
            running_ = true;
            thread_ = std::thread(&Task::run, this);
        }
    }
    
    void join() {
        if (thread_.joinable()) {
            thread_.join();
        }
    }
    
protected:
    std::atomic<bool> running_{false};
    std::thread thread_;
};

class MyTask : public Task {
public:
    void run() override {
        std::cout << "MyTask started" << std::endl;
        int count = 0;
        while (running_ && count < 5) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            std::cout << "MyTask working... " << count++ << std::endl;
            
            // 发送消息
            ThreadManager::getInstance().sendMessage(Message(count, "Hello from MyTask"));
        }
        std::cout << "MyTask finished" << std::endl;
    }
    
    void stop() override {
        running_ = false;
    }
};

class MyTask2 : public Task {
public:
    void run() override {
        std::cout << "MyTask2 started" << std::endl;
        while (running_) {
            ThreadManager::getInstance().waitForMessage();
            
            Message msg(0, "");
            while (ThreadManager::getInstance().receiveMessage(msg)) {
                std::cout << "MyTask2 received message: ID=" << msg.id 
                         << ", Content=" << msg.content << std::endl;
            }
            
            if (!running_) break;
        }
        std::cout << "MyTask2 finished" << std::endl;
    }
    
    void stop() override {
        running_ = false;
        ThreadManager::getInstance().stop();
    }
};

void task_test() {
    std::cout << "=== Task Test ===" << std::endl;
    
    // 创建任务
    auto task1 = std::make_unique<MyTask>();
    auto task2 = std::make_unique<MyTask2>();
    
    // 启动任务
    task1->start();
    task2->start();
    
    // 等待一段时间
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // 停止任务
    task1->stop();
    task2->stop();
    
    // 等待任务完成
    task1->join();
    task2->join();
    
    std::cout << "All tasks completed" << std::endl;
}

int main() {
    task_test();
    return 0;
}