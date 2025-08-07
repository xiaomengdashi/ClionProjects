#include <iostream>
#include <memory>

// 抽象产品A
class AbstractProductA {
public:
    virtual ~AbstractProductA() = default;
    virtual std::string usefulFunctionA() const = 0;
};

// 具体产品A1
class ConcreteProductA1 : public AbstractProductA {
public:
    std::string usefulFunctionA() const override {
        return "The result of the product A1.";
    }
};

// 具体产品A2
class ConcreteProductA2 : public AbstractProductA {
public:
    std::string usefulFunctionA() const override {
        return "The result of the product A2.";
    }
};

// 抽象产品B
class AbstractProductB {
public:
    virtual ~AbstractProductB() = default;
    virtual std::string usefulFunctionB() const = 0;
    virtual std::string anotherUsefulFunctionB(const AbstractProductA& collaborator) const = 0;
};

// 具体产品B1
class ConcreteProductB1 : public AbstractProductB {
public:
    std::string usefulFunctionB() const override {
        return "The result of the product B1.";
    }

    std::string anotherUsefulFunctionB(const AbstractProductA& collaborator) const override {
        const std::string result = collaborator.usefulFunctionA();
        return "The result of the B1 collaborating with ( " + result + " )";
    }
};

// 具体产品B2
class ConcreteProductB2 : public AbstractProductB {
public:
    std::string usefulFunctionB() const override {
        return "The result of the product B2.";
    }

    std::string anotherUsefulFunctionB(const AbstractProductA& collaborator) const override {
        const std::string result = collaborator.usefulFunctionA();
        return "The result of the B2 collaborating with ( " + result + " )";
    }
};

// 抽象工厂
class AbstractFactory {
public:
    virtual ~AbstractFactory() = default;
    virtual std::unique_ptr<AbstractProductA> createProductA() const = 0;
    virtual std::unique_ptr<AbstractProductB> createProductB() const = 0;
};

// 具体工厂1
class ConcreteFactory1 : public AbstractFactory {
public:
    std::unique_ptr<AbstractProductA> createProductA() const override {
        return std::make_unique<ConcreteProductA1>();
    }

    std::unique_ptr<AbstractProductB> createProductB() const override {
        return std::make_unique<ConcreteProductB1>();
    }
};

// 具体工厂2
class ConcreteFactory2 : public AbstractFactory {
public:
    std::unique_ptr<AbstractProductA> createProductA() const override {
        return std::make_unique<ConcreteProductA2>();
    }

    std::unique_ptr<AbstractProductB> createProductB() const override {
        return std::make_unique<ConcreteProductB2>();
    }
};

// 客户端代码
void clientCode(const AbstractFactory& factory) {
    const auto productA = factory.createProductA();
    const auto productB = factory.createProductB();
    
    std::cout << productB->usefulFunctionB() << std::endl;
    std::cout << productB->anotherUsefulFunctionB(*productA) << std::endl;
}

int main() {
    std::cout << "=== Abstract Factory Pattern Demo ===" << std::endl;
    
    std::cout << "Client: Testing client code with the first factory type:" << std::endl;
    auto factory1 = std::make_unique<ConcreteFactory1>();
    clientCode(*factory1);
    
    std::cout << std::endl;
    
    std::cout << "Client: Testing the same client code with the second factory type:" << std::endl;
    auto factory2 = std::make_unique<ConcreteFactory2>();
    clientCode(*factory2);

    return 0;
}