#pragma once

#include <iostream>


// 单例类
class Singleton {
private:
    // 私有构造函数，防止外部实例化
    Singleton() = default;
    // 私有拷贝构造函数，防止拷贝
    Singleton(const Singleton&) = delete;
    // 私有赋值运算符，防止赋值
    Singleton& operator=(const Singleton&) = delete;
public:
    // 静态方法，获取单例实例
    static Singleton& getInstance() {
        static Singleton instance;
        return instance;
    }
    // 示例方法
    void doSomething() {
        std::cout << "Singleton is doing something." << std::endl;
    }

    bool operator==(const Singleton& singleton) const
    {
        return &singleton == this;
    }
};

// 单例测试函数
inline void singleton_test() {
    std::cout << "=========singleton test==========" << std::endl;
    Singleton& singleton1 = Singleton::getInstance();
    Singleton& singleton2 = Singleton::getInstance();

    // 验证两个实例是否相同
    if (singleton1 == singleton2) {
        std::cout << "Both instances are the same." << std::endl;
    } else {
        std::cout << "Instances are different." << std::endl;
    }

    // 调用单例方法
    singleton1.doSomething();

    std::cout << "\n\n" << std::endl;
}