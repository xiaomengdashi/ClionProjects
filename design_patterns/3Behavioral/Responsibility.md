责任链模式（Chain of Responsibility）是一种行为设计模式，允许多个对象有机会处理请求，从而避免请求的发送者与接收者之间的耦合关系。该模式将多个处理对象连接成一条链，并沿着这条链传递请求，直到有对象处理该请求为止。

下面是一个简单的责任链模式的代码实现，模拟一个审批流程，审批人可以是经理、主任和总经理，每个审批人依次处理不同级别的请求：


### 责任链模式 C++ 实现

```cpp
#include <iostream>
#include <memory>
#include <string>

// 请求类
class Request {
public:
    explicit Request(double amount) : amount(amount) {}
    double getAmount() const { return amount; }

private:
    double amount;
};

// 抽象审批人类
class Approver {
public:
    virtual ~Approver() = default;
    virtual void approve(std::shared_ptr<Request> request) = 0;
    void setNextApprover(std::shared_ptr<Approver> approver) {
        nextApprover = approver;
    }

protected:
    std::shared_ptr<Approver> nextApprover;
};

// 经理类
class Manager : public Approver {
public:
    explicit Manager(const std::string& name) : name(name) {}
    
    void approve(std::shared_ptr<Request> request) override {
        if (request->getAmount() <= 1000) {
            std::cout << name << "（经理）批准了请求：" << request->getAmount() << "元\n";
        } else if (nextApprover) {
            std::cout << name << "（经理）无法批准请求：" << request->getAmount() << "元，转交给下一个审批人\n";
            nextApprover->approve(request);
        }
    }

private:
    std::string name;
};

// 主任类
class Director : public Approver {
public:
    explicit Director(const std::string& name) : name(name) {}
    
    void approve(std::shared_ptr<Request> request) override {
        if (request->getAmount() <= 5000) {
            std::cout << name << "（主任）批准了请求：" << request->getAmount() << "元\n";
        } else if (nextApprover) {
            std::cout << name << "（主任）无法批准请求：" << request->getAmount() << "元，转交给下一个审批人\n";
            nextApprover->approve(request);
        }
    }

private:
    std::string name;
};

// 总经理类
class CEO : public Approver {
public:
    explicit CEO(const std::string& name) : name(name) {}
    
    void approve(std::shared_ptr<Request> request) override {
        if (request->getAmount() > 5000) {
            std::cout << name << "（总经理）批准了请求：" << request->getAmount() << "元\n";
        } else {
            std::cout << name << "（总经理）无法批准请求：" << request->getAmount() << "元，没必要\n";
        }
    }

private:
    std::string name;
};

// 主函数
int main() {
    // 创建请求
    auto request = std::make_shared<Request>(4500);

    // 创建审批人
    auto manager = std::make_shared<Manager>("李经理");
    auto director = std::make_shared<Director>("王主任");
    auto ceo = std::make_shared<CEO>("张总经理");

    // 设置责任链
    manager->setNextApprover(director);
    director->setNextApprover(ceo);

    // 经理开始处理请求
    manager->approve(request);

    return 0;
}
```

### 代码解析：
1. **`Request` 类**：表示一个请求，包含一个金额属性（`amount`），用于传递给审批人。
2. **`Approver` 类**：这是一个抽象类，定义了审批人对象的基本结构，包含了 `setNextApprover` 方法来设置下一个处理请求的对象，以及一个纯虚函数 `approve`，需要由子类实现具体的审批逻辑。
3. **`Manager`, `Director`, `CEO` 类**：继承自 `Approver` 类，实现了 `approve` 方法，分别处理不同级别的请求。经理处理金额小于等于 1000 的请求，主任处理金额小于等于 5000 的请求，总经理处理金额大于 5000 的请求。
4. **责任链的建立**：在 `main` 函数中创建了审批人（经理、主任和总经理），并设置了责任链。请求从经理开始处理，如果经理无法处理，则将请求转交给主任，再转交给总经理。

### 输出结果：
```
李经理（经理）无法批准请求：4500元，转交给下一个审批人
王主任（主任）批准了请求：4500元
```

### 总结：
- 责任链模式通过将多个处理对象连接成一条链，并沿着这条链传递请求，避免了请求者与处理者之间的直接耦合。
- 每个审批人根据自己的处理条件决定是否处理请求，如果不能处理，则将请求传递给链中的下一个审批人。
- 使用 `shared_ptr` 来管理内存，确保资源的自动管理。