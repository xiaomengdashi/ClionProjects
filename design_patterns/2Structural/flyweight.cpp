#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

// 享元接口
class Flyweight {
public:
    virtual ~Flyweight() = default;
    virtual void operation(const std::string& extrinsicState) const = 0;
};

// 具体享元
class ConcreteFlyweight : public Flyweight {
private:
    std::string intrinsicState_;

public:
    ConcreteFlyweight(const std::string& intrinsicState) 
        : intrinsicState_(intrinsicState) {}

    void operation(const std::string& extrinsicState) const override {
        std::cout << "ConcreteFlyweight: Intrinsic State = " << intrinsicState_ 
                  << ", Extrinsic State = " << extrinsicState << std::endl;
    }
};

// 享元工厂
class FlyweightFactory {
private:
    std::unordered_map<std::string, std::shared_ptr<Flyweight>> flyweights_;

public:
    std::shared_ptr<Flyweight> getFlyweight(const std::string& sharedState) {
        if (flyweights_.find(sharedState) == flyweights_.end()) {
            std::cout << "FlyweightFactory: Can't find a flyweight, creating new one." << std::endl;
            flyweights_[sharedState] = std::make_shared<ConcreteFlyweight>(sharedState);
        } else {
            std::cout << "FlyweightFactory: Reusing existing flyweight." << std::endl;
        }
        return flyweights_[sharedState];
    }

    void listFlyweights() const {
        size_t count = flyweights_.size();
        std::cout << "\nFlyweightFactory: I have " << count << " flyweights:" << std::endl;
        for (const auto& pair : flyweights_) {
            std::cout << pair.first << std::endl;
        }
    }
};

// 上下文类
class Context {
private:
    std::shared_ptr<Flyweight> flyweight_;
    std::string uniqueState_;

public:
    Context(std::shared_ptr<Flyweight> flyweight, const std::string& uniqueState)
        : flyweight_(flyweight), uniqueState_(uniqueState) {}

    void operation() const {
        flyweight_->operation(uniqueState_);
    }
};

// 客户端代码
void addCarToPoliceDatabase(FlyweightFactory& ff, const std::string& plates,
                           const std::string& owner, const std::string& brand,
                           const std::string& model, const std::string& color) {
    std::cout << "\nClient: Adding a car to database." << std::endl;
    auto flyweight = ff.getFlyweight(brand + model + color);
    // 客户端代码存储或计算外部状态，并将其传递给享元的方法
    Context context(flyweight, plates + owner);
    context.operation();
}

int main() {
    std::cout << "=== Flyweight Pattern Demo ===" << std::endl;
    
    FlyweightFactory factory;

    addCarToPoliceDatabase(factory, "CL234IR", "James Doe", "BMW", "M5", "red");
    addCarToPoliceDatabase(factory, "CL234IR", "James Doe", "BMW", "X1", "red");
    addCarToPoliceDatabase(factory, "CL234IR", "James Doe", "BMW", "M5", "red");
    addCarToPoliceDatabase(factory, "CL234IR", "James Doe", "BMW", "X6", "blue");

    factory.listFlyweights();

    return 0;
}