#include <iostream>
#include <string>
#include "singleton.hpp"

// 策略接口
class Strategy {
public:
    virtual ~Strategy() {}
    virtual void execute() = 0; // 纯虚函数
};

// 具体策略 A
class ConcreteStrategyA : public Strategy {
public:
    void execute() override {
        std::cout << "执行策略 A" << std::endl;
    }
};

// 具体策略 B
class ConcreteStrategyB : public Strategy {
public:
    void execute() override {
        std::cout << "执行策略 B" << std::endl;
    }
};

// 上下文
class Context {
private:
    Strategy* strategy; // 持有一个策略的指针
public:
    Context(Strategy* strategy) : strategy(strategy) {}

    void setStrategy(Strategy* strategy) {
        this->strategy = strategy;
    }

    void executeStrategy() {
        strategy->execute(); // 执行当前策略
    }
};


struct Student {
    unsigned int id;
    std::string name;

    unsigned int get_id() const {
        return id;
    }
};

#define the_student util::Singleton<Student>::instance()

int main() {
    ConcreteStrategyA strategyA;
    ConcreteStrategyB strategyB;

    Context context(&strategyA); // 使用策略 A
    context.executeStrategy(); // 输出: 执行策略 A

    context.setStrategy(&strategyB); // 切换到策略 B
    context.executeStrategy(); // 输出: 执行策略 B


    std::cout << the_student.get_id() << std::endl;
    the_student.id = 1;
    std::cout << the_student.get_id() << std::endl;
    std::cout << the_student.name << std::endl;
    the_student.name = "张三";
    std::cout << the_student.name << std::endl;



    int a = 1;
    float b = a;

    double c = 33.5e39;

    b = c;

    return 0;
}