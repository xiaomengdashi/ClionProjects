状态模式（State Pattern）是一种行为型设计模式，它允许对象在内部状态改变时改变其行为。状态模式将行为的变化封装到状态对象中，使得上下文（Context）对象的行为随着状态的变化而变化。以下是一个简单的 C++ 实现示例：

### 状态模式示例

```cpp
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
```

### 解释：
1. **State** 类是抽象状态类，定义了一个 `handleRequest` 方法。所有具体的状态类都会继承该类并实现具体的行为。

2. **ConcreteStateA** 和 **ConcreteStateB** 是具体状态类，分别实现了 `handleRequest` 方法，表现不同的行为。

3. **Context** 类维护对当前状态的引用，并委托状态对象处理请求。`setState` 方法允许在运行时更换状态。

4. 客户端代码在 `main` 函数中创建了一个 `Context` 对象，并设置了初始状态。然后通过 `request` 方法调用当前状态的 `handleRequest`，并在后续更改状态。

### 输出：
```
Handling request in ConcreteStateA.
Handling request in ConcreteStateB.
```

这个实现展示了如何使用状态模式根据对象的状态动态改变其行为。