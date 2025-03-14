#pragma once

#include <iostream>

// 策略接口
class Strategy {
public:
    virtual ~Strategy() = default;
    virtual void execute() = 0; // 纯虚函数
};

// 具体策略 A
class ConcreteStrategyA final : public Strategy {
public:
    void execute() override {
        std::cout << "执行策略 A" << std::endl;
    }
};

// 具体策略 B
class ConcreteStrategyB final : public Strategy {
public:
    void execute() override {
        std::cout << "执行策略 B" << std::endl;
    }
};

// 上下文
class Context {
private:
    Strategy* strategy; // 持有一个策略的指针
public:
    explicit Context(Strategy* strategy) : strategy(strategy) {}

    void setStrategy(Strategy* strategy) {
        this->strategy = strategy;
    }

    void executeStrategy() const
    {
        strategy->execute(); // 执行当前策略
    }
};

inline void strategy_test() {

    std::cout << "=========strategy test==========" << std::endl;
    ConcreteStrategyA strategyA;
    ConcreteStrategyB strategyB;

    Context context(&strategyA); // 使用策略 A
    context.executeStrategy(); // 输出: 执行策略 A

    context.setStrategy(&strategyB); // 切换到策略 B
    context.executeStrategy(); // 输出: 执行策略 B

    std::cout << "\n\n" << std::endl;

}