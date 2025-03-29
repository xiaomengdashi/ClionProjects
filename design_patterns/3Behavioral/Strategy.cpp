#include <iostream>
#include <memory>

// 抽象策略类
class Strategy {
public:
    virtual ~Strategy() {}
    virtual void execute() const = 0;
};

// 具体策略类A
class ConcreteStrategyA : public Strategy {
public:
    void execute() const override {
        std::cout << "Executing strategy A" << std::endl;
    }
};

// 具体策略类B
class ConcreteStrategyB : public Strategy {
public:
    void execute() const override {
        std::cout << "Executing strategy B" << std::endl;
    }
};

// 具体策略类C
class ConcreteStrategyC : public Strategy {
public:
    void execute() const override {
        std::cout << "Executing strategy C" << std::endl;
    }
};

// 上下文类
class Context {
private:
    std::unique_ptr<Strategy> strategy;  // 持有一个策略对象

public:
    // 构造函数，初始化时设置策略
    Context(std::unique_ptr<Strategy> initialStrategy) : strategy(std::move(initialStrategy)) {}

    // 设置新的策略
    void setStrategy(std::unique_ptr<Strategy> newStrategy) {
        strategy = std::move(newStrategy);
    }

    // 执行策略
    void executeStrategy() const {
        strategy->execute();
    }
};

// 客户端代码
int main() {
    // 创建上下文对象并设置初始策略
    Context context(std::make_unique<ConcreteStrategyA>());

    // 执行当前策略A
    context.executeStrategy();

    // 更改策略为B
    context.setStrategy(std::make_unique<ConcreteStrategyB>());

    // 执行当前策略B
    context.executeStrategy();

    // 更改策略为C
    context.setStrategy(std::make_unique<ConcreteStrategyC>());

    // 执行当前策略C
    context.executeStrategy();

    return 0;
}