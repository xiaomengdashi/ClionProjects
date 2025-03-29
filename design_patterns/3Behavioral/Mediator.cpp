//
// Created by Kolane on 2025/3/29.
//

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
