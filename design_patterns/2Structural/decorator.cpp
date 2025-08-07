#include <iostream>
#include <memory>

// 组件接口
class Component {
public:
    virtual ~Component() = default;
    virtual std::string operation() const = 0;
};

// 具体组件
class ConcreteComponent : public Component {
public:
    std::string operation() const override {
        return "ConcreteComponent";
    }
};

// 装饰器基类
class BaseDecorator : public Component {
protected:
    std::unique_ptr<Component> component_;

public:
    BaseDecorator(std::unique_ptr<Component> component) 
        : component_(std::move(component)) {}

    std::string operation() const override {
        return this->component_->operation();
    }
};

// 具体装饰器A
class ConcreteDecoratorA : public BaseDecorator {
public:
    ConcreteDecoratorA(std::unique_ptr<Component> component) 
        : BaseDecorator(std::move(component)) {}

    std::string operation() const override {
        return "ConcreteDecoratorA(" + BaseDecorator::operation() + ")";
    }
};

// 具体装饰器B
class ConcreteDecoratorB : public BaseDecorator {
public:
    ConcreteDecoratorB(std::unique_ptr<Component> component) 
        : BaseDecorator(std::move(component)) {}

    std::string operation() const override {
        return "ConcreteDecoratorB(" + BaseDecorator::operation() + ")";
    }
};

// 客户端代码
void clientCode(Component* component) {
    std::cout << "RESULT: " << component->operation() << std::endl;
}

int main() {
    std::cout << "=== Decorator Pattern Demo ===" << std::endl;
    
    // 简单组件
    auto simple = std::make_unique<ConcreteComponent>();
    std::cout << "Client: I've got a simple component:" << std::endl;
    clientCode(simple.get());
    std::cout << std::endl;

    // 装饰后的组件
    std::cout << "Client: Now I've got a decorated component:" << std::endl;
    auto decorator1 = std::make_unique<ConcreteDecoratorA>(std::move(simple));
    auto decorator2 = std::make_unique<ConcreteDecoratorB>(std::move(decorator1));
    clientCode(decorator2.get());

    return 0;
}