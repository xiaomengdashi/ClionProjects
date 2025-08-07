#include <iostream>
#include <memory>

// 子系统类1
class Subsystem1 {
public:
    std::string operation1() const {
        return "Subsystem1: Ready!\n";
    }

    std::string operationN() const {
        return "Subsystem1: Go!\n";
    }
};

// 子系统类2
class Subsystem2 {
public:
    std::string operation1() const {
        return "Subsystem2: Get ready!\n";
    }

    std::string operationZ() const {
        return "Subsystem2: Fire!\n";
    }
};

// 外观类
class Facade {
protected:
    std::unique_ptr<Subsystem1> subsystem1_;
    std::unique_ptr<Subsystem2> subsystem2_;

public:
    Facade(std::unique_ptr<Subsystem1> subsystem1 = nullptr,
           std::unique_ptr<Subsystem2> subsystem2 = nullptr) {
        this->subsystem1_ = subsystem1 ? std::move(subsystem1) : std::make_unique<Subsystem1>();
        this->subsystem2_ = subsystem2 ? std::move(subsystem2) : std::make_unique<Subsystem2>();
    }

    std::string operation() {
        std::string result = "Facade initializes subsystems:\n";
        result += this->subsystem1_->operation1();
        result += this->subsystem2_->operation1();
        result += "Facade orders subsystems to perform the action:\n";
        result += this->subsystem1_->operationN();
        result += this->subsystem2_->operationZ();
        return result;
    }
};

// 客户端代码
void clientCode(Facade* facade) {
    std::cout << facade->operation();
}

int main() {
    std::cout << "=== Facade Pattern Demo ===" << std::endl;
    
    auto subsystem1 = std::make_unique<Subsystem1>();
    auto subsystem2 = std::make_unique<Subsystem2>();
    auto facade = std::make_unique<Facade>(std::move(subsystem1), std::move(subsystem2));
    
    clientCode(facade.get());

    return 0;
}