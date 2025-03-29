中介者模式（Mediator Pattern）是用来减少对象之间的直接交互，避免了多个对象之间的复杂耦合关系。通过一个中介者对象来协调多个对象之间的交互，达到松耦合的效果。

以下是一个实现中介者模式的示例代码：

### 中介者模式示例代码

```cpp

#include <iostream>
#include <string>
#include <vector>

// 抽象中介者
class Mediator;

// 抽象的同事类
class Colleague {
protected:
Mediator* mediator;

public:
explicit Colleague(Mediator* m) : mediator(m) {}
virtual ~Colleague() = default;

virtual void sendMessage(const std::string& message) = 0;
virtual void receiveMessage(const std::string& message) = 0;
};

// 抽象中介者
class Mediator {
public:
virtual void sendMessage(Colleague* sender, const std::string& message) = 0;
virtual void setColleague1(Colleague* c1) = 0;
virtual void setColleague2(Colleague* c2) = 0;
};

// 具体的中介者
class ConcreteMediator : public Mediator {
private:
Colleague* colleague1;
Colleague* colleague2;

public:
ConcreteMediator(Colleague* c1, Colleague* c2) : colleague1(c1), colleague2(c2) {}

void setColleague1(Colleague* c1) override {
colleague1 = c1;
}

void setColleague2(Colleague* c2) override {
colleague2 = c2;
}

void sendMessage(Colleague* sender, const std::string& message) override {
if (sender == colleague1) {
colleague2->receiveMessage(message);
} else {
colleague1->receiveMessage(message);
}
}
};

// 具体的同事类
class ConcreteColleague1 : public Colleague {
public:
explicit ConcreteColleague1(Mediator* m) : Colleague(m) {}

void sendMessage(const std::string& message) override {
std::cout << "ConcreteColleague1 sending message: " << message << std::endl;
mediator->sendMessage(this, message);
}

void receiveMessage(const std::string& message) override {
std::cout << "ConcreteColleague1 received message: " << message << std::endl;
}
};

// 具体的同事类
class ConcreteColleague2 : public Colleague {
public:
explicit ConcreteColleague2(Mediator* m) : Colleague(m) {}

void sendMessage(const std::string& message) override {
std::cout << "ConcreteColleague2 sending message: " << message << std::endl;
mediator->sendMessage(this, message);
}

void receiveMessage(const std::string& message) override {
std::cout << "ConcreteColleague2 received message: " << message << std::endl;
}
};

int main() {
// 创建中介者对象
auto* mediator = new ConcreteMediator(nullptr, nullptr);;

// 创建同事类对象，并传入中介者
auto* colleague1 = new ConcreteColleague1(mediator);
auto* colleague2 = new ConcreteColleague2(mediator);

// 中介者对象设置对象
mediator->setColleague1(colleague1);
mediator->setColleague2(colleague2);

// 启动通信
colleague1->sendMessage("Hello from Colleague1!");
colleague2->sendMessage("Hi from Colleague2!");

delete colleague1;
delete colleague2;
delete mediator;

return 0;
}
```

### 代码解释：
1. **Mediator（中介者）**：这是一个抽象类，用来定义 `sendMessage` 的接口，所有的具体中介者都需要实现这个接口。

2. **Colleague（同事类）**：这是一个抽象的同事类，每个具体的同事类都需要实现 `sendMessage` 和 `receiveMessage`，并且知道中介者对象。

3. **ConcreteMediator（具体中介者）**：这个类实现了 `Mediator` 接口，负责协调 `ConcreteColleague1` 和 `ConcreteColleague2` 之间的通信。它会在 `sendMessage` 方法中判断是哪个同事类发送了消息，并通知另一个同事类。

4. **ConcreteColleague1 和 ConcreteColleague2（具体同事类）**：这些是具体的同事类，实现了消息的发送和接收。在发送消息时，它们调用中介者的 `sendMessage` 方法，而不是直接与其他同事类交互。

### 输出：

```
ConcreteColleague1 sending message: Hello from Colleague1!
ConcreteColleague2 received message: Hello from Colleague1!
ConcreteColleague2 sending message: Hi from Colleague2!
ConcreteColleague1 received message: Hi from Colleague2!
```

### 总结：
- 中介者模式通过引入一个中介者对象来减少同事类之间的直接依赖。所有的交互都由中介者进行协调，从而达到松耦合的效果。
