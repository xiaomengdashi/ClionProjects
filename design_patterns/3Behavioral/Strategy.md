策略模式（Strategy Pattern）是一种行为型设计模式，允许定义一系列算法（策略），并使得这些算法可以互相替换。策略模式让算法的变化独立于使用算法的客户。以下是 C++ 的一个简单策略模式实现示例。

### 策略模式示例

```cpp
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
```

### 解释：
1. **Strategy** 类是抽象策略类，它定义了一个 `execute` 方法，所有具体策略类都需要实现这个方法来执行特定的行为。

2. **ConcreteStrategyA**, **ConcreteStrategyB**, 和 **ConcreteStrategyC** 是具体策略类，它们实现了 `execute` 方法，分别提供不同的实现行为。

3. **Context** 类维护一个 `Strategy` 对象，并提供了 `setStrategy` 方法来动态更换策略。`executeStrategy` 方法用于调用当前策略的 `execute` 方法。

4. 客户端代码创建了一个 `Context` 对象，并通过构造函数设置初始策略。然后，使用 `setStrategy` 动态更换策略，并使用 `executeStrategy` 执行策略。

### 输出：
```
Executing strategy A
Executing strategy B
Executing strategy C
```

通过使用策略模式，代码可以在运行时灵活地改变策略，避免了大量的 `if-else` 或 `switch` 语句。这使得算法的扩展变得非常容易，只需添加新的策略类而不需要修改现有的代码。