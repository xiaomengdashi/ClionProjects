//
// Created by Kolane on 2025/3/29.
//

#include <iostream>
#include <memory>

// 抽象状态类
class State {
public:
    virtual ~State() {}
    virtual void handleRequest() = 0;
};

// 具体状态类A
class ConcreteStateA : public State {
public:
    void handleRequest() override {
        std::cout << "Handling request in ConcreteStateA." << std::endl;
    }
};

// 具体状态类B
class ConcreteStateB : public State {
public:
    void handleRequest() override {
        std::cout << "Handling request in ConcreteStateB." << std::endl;
    }
};

// 上下文类
class Context {
private:
    std::unique_ptr<State> state;

public:
    // 构造函数，初始化时设置状态
    Context(std::unique_ptr<State> initialState) : state(std::move(initialState)) {}

    // 设置新的状态
    void setState(std::unique_ptr<State> newState) {
        state = std::move(newState);
    }

    // 执行当前状态的请求处理
    void request() {
        state->handleRequest();
    }
};

// 客户端代码
int main() {
    // 创建上下文对象并设置初始状态
    Context context(std::make_unique<ConcreteStateA>());

    // 执行请求，状态A会处理
    context.request();

    // 更改状态为B
    context.setState(std::make_unique<ConcreteStateB>());

    // 执行请求，状态B会处理
    context.request();

    return 0;
}
