//
// Created by Kolane on 2025/1/10.
//

#include <iostream>
#include <mutex>
#include <stack>
#include <memory>
#include <thread>
#include <chrono>

struct empty_stack : std::exception
{
    const char* what() const noexcept override
    {
        return "Stack is empty!";
    }
};

template<typename T>
class threadsafe_stack
{
private:
    std::stack<T> data;
    mutable std::mutex m;
public:
    threadsafe_stack() = default;
    threadsafe_stack(const threadsafe_stack& other)
    {
        std::lock_guard<std::mutex> lock(other.m);
        //①在构造函数的函数体（constructor body）内进行复制操作
        data = other.data;
    }
    threadsafe_stack& operator=(const threadsafe_stack&) = delete;

    void push(T new_value)
    {
        std::lock_guard<std::mutex> lock(m);
        data.push(std::move(new_value));
    }

    std::shared_ptr<T> pop()
    {
        std::lock_guard<std::mutex> lock(m);
        //②试图弹出前检查是否为空栈
        if (data.empty()) throw empty_stack();
        //③改动栈容器前设置返回值
        std::shared_ptr<T> const res(std::make_shared<T>(data.top()));
        data.pop();
        return res;
    }

    void pop(T& value)
    {
        std::lock_guard<std::mutex> lock(m);
        if (data.empty()) throw empty_stack();
        value = data.top();
        data.pop();
    }
    
    bool empty() const
    {
        std::lock_guard<std::mutex> lock(m);
        return data.empty();
    }
    
    size_t size() const
    {
        std::lock_guard<std::mutex> lock(m);
        return data.size();
    }
};

void test_threadsafe_stack1() {
    std::cout << "=======Test Threadsafe Stack 1=======" << std::endl;
    threadsafe_stack<int> safe_stack;
    safe_stack.push(1);
    safe_stack.push(2);
    safe_stack.push(3);
    
    std::cout << "Initial stack size: " << safe_stack.size() << std::endl;

    std::thread t1([&safe_stack]() {
        try {
            if (!safe_stack.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                auto value = safe_stack.pop();
                std::cout << "Thread 1 popped: " << *value << std::endl;
            }
        } catch (const empty_stack& e) {
            std::cout << "Thread 1 caught exception: " << e.what() << std::endl;
        }
    });

    std::thread t2([&safe_stack]() {
        try {
            if (!safe_stack.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                auto value = safe_stack.pop();
                std::cout << "Thread 2 popped: " << *value << std::endl;
            }
        } catch (const empty_stack& e) {
            std::cout << "Thread 2 caught exception: " << e.what() << std::endl;
        }
    });

    t1.join();
    t2.join();
    
    std::cout << "Final stack size: " << safe_stack.size() << std::endl;
}

void test_threadsafe_stack2() {
    std::cout << "=======Test Threadsafe Stack 2=======" << std::endl;
    threadsafe_stack<std::string> safe_stack;
    
    // 测试拷贝构造函数
    safe_stack.push("Hello");
    safe_stack.push("World");
    
    threadsafe_stack<std::string> copied_stack(safe_stack);
    std::cout << "Original stack size: " << safe_stack.size() << std::endl;
    std::cout << "Copied stack size: " << copied_stack.size() << std::endl;
    
    // 测试异常处理
    threadsafe_stack<int> empty_test_stack;
    try {
        empty_test_stack.pop();
    } catch (const empty_stack& e) {
        std::cout << "Caught expected exception: " << e.what() << std::endl;
    }
    
    // 测试pop到引用
    std::string value;
    if (!copied_stack.empty()) {
        copied_stack.pop(value);
        std::cout << "Popped value: " << value << std::endl;
    }
}

void test_concurrent_access() {
    std::cout << "=======Test Concurrent Access=======" << std::endl;
    threadsafe_stack<int> safe_stack;
    
    // 生产者线程
    std::thread producer([&safe_stack]() {
        for (int i = 0; i < 10; ++i) {
            safe_stack.push(i);
            std::cout << "Produced: " << i << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });
    
    // 消费者线程
    std::thread consumer([&safe_stack]() {
        int consumed_count = 0;
        while (consumed_count < 10) {
            try {
                if (!safe_stack.empty()) {
                    auto value = safe_stack.pop();
                    std::cout << "Consumed: " << *value << std::endl;
                    consumed_count++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
            } catch (const empty_stack& e) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    std::cout << "Final stack size: " << safe_stack.size() << std::endl;
}

int main() {
    test_threadsafe_stack1();
    std::cout << std::endl;
    
    test_threadsafe_stack2();
    std::cout << std::endl;
    
    test_concurrent_access();
    
    return 0;
}