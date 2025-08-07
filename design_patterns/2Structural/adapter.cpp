#include <iostream>
#include <memory>

// 目标接口 - 客户端期望的接口
class Target {
public:
    virtual ~Target() = default;
    virtual std::string request() const {
        return "Target: The default target's behavior.";
    }
};

// 被适配者 - 需要适配的现有类
class Adaptee {
public:
    std::string specificRequest() const {
        return ".eetpadA eht fo roivaheb laicepS";
    }
};

// 适配器 - 使Adaptee兼容Target接口
class Adapter : public Target {
private:
    std::unique_ptr<Adaptee> adaptee_;

public:
    Adapter(std::unique_ptr<Adaptee> adaptee) : adaptee_(std::move(adaptee)) {}

    std::string request() const override {
        std::string toReverse = this->adaptee_->specificRequest();
        std::reverse(toReverse.begin(), toReverse.end());
        return "Adapter: (TRANSLATED) " + toReverse;
    }
};

// 客户端代码
void clientCode(const Target* target) {
    std::cout << target->request() << std::endl;
}

int main() {
    std::cout << "=== Adapter Pattern Demo ===" << std::endl;
    
    std::cout << "Client: I can work just fine with the Target objects:" << std::endl;
    auto target = std::make_unique<Target>();
    clientCode(target.get());
    
    std::cout << std::endl;
    
    auto adaptee = std::make_unique<Adaptee>();
    std::cout << "Client: The Adaptee class has a weird interface. See, I don't understand it:" << std::endl;
    std::cout << "Adaptee: " << adaptee->specificRequest() << std::endl;
    
    std::cout << std::endl;
    
    std::cout << "Client: But I can work with it via the Adapter:" << std::endl;
    auto adapter = std::make_unique<Adapter>(std::move(adaptee));
    clientCode(adapter.get());
    
    return 0;
}