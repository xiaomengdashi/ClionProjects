#include <iostream>
#include <memory>

// 实现接口 - 定义实现类的接口
class Implementation {
public:
    virtual ~Implementation() = default;
    virtual std::string operationImplementation() const = 0;
};

// 具体实现A
class ConcreteImplementationA : public Implementation {
public:
    std::string operationImplementation() const override {
        return "ConcreteImplementationA: Here's the result on the platform A.";
    }
};

// 具体实现B
class ConcreteImplementationB : public Implementation {
public:
    std::string operationImplementation() const override {
        return "ConcreteImplementationB: Here's the result on the platform B.";
    }
};

// 抽象类 - 定义抽象类的接口
class Abstraction {
protected:
    std::unique_ptr<Implementation> implementation_;

public:
    Abstraction(std::unique_ptr<Implementation> implementation) 
        : implementation_(std::move(implementation)) {}

    virtual ~Abstraction() = default;

    virtual std::string operation() const {
        return "Abstraction: Base operation with:\n" +
               this->implementation_->operationImplementation();
    }
};

// 扩展抽象类
class ExtendedAbstraction : public Abstraction {
public:
    ExtendedAbstraction(std::unique_ptr<Implementation> implementation) 
        : Abstraction(std::move(implementation)) {}

    std::string operation() const override {
        return "ExtendedAbstraction: Extended operation with:\n" +
               this->implementation_->operationImplementation();
    }
};

// 客户端代码
void clientCode(const Abstraction& abstraction) {
    std::cout << abstraction.operation() << std::endl;
}

int main() {
    std::cout << "=== Bridge Pattern Demo ===" << std::endl;
    
    auto implementation = std::make_unique<ConcreteImplementationA>();
    auto abstraction = std::make_unique<Abstraction>(std::move(implementation));
    clientCode(*abstraction);
    
    std::cout << std::endl;
    
    auto implementation2 = std::make_unique<ConcreteImplementationB>();
    auto extendedAbstraction = std::make_unique<ExtendedAbstraction>(std::move(implementation2));
    clientCode(*extendedAbstraction);
    
    return 0;
}