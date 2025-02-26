//
// Created by Kolane on 2025/1/10.
//

#include <iostream>
#include <mutex>
#include <stack>
#include <memory>
#include <thread>

#include "head.h"

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
};


void test_threadsafe_stack1() {
    threadsafe_stack<int> safe_stack;
    safe_stack.push(1);

    std::thread t1([&safe_stack]() {
        if (!safe_stack.empty()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            safe_stack.pop();
        }
    });

    std::thread t2([&safe_stack]() {
        if (!safe_stack.empty()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            safe_stack.pop();
        }
    });

    t1.join();
    t2.join();
}