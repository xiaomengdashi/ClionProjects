//
// Created by Kolane on 2025/3/29.
//


#include <iostream>
#include <vector>
#include <algorithm>

// 抽象观察者
class Observer {
public:
    virtual ~Observer() = default;
    virtual void update(const std::string& message) = 0;
};

// 抽象被观察者
class Subject {
public:
    virtual ~Subject() = default;
    virtual void addObserver(Observer* observer) = 0;
    virtual void removeObserver(Observer* observer) = 0;
    virtual void notifyObservers() = 0;
};

// 具体观察者
class ConcreteObserver : public Observer {
public:
    ConcreteObserver(const std::string& name) : name(name) {}

    void update(const std::string& message) override {
        std::cout << name << " received message: " << message << std::endl;
    }

private:
    std::string name;
};

// 具体被观察者
class ConcreteSubject : public Subject {
public:
    void addObserver(Observer* observer) override {
        observers.push_back(observer);
    }

    void removeObserver(Observer* observer) override {
        observers.erase(std::remove(observers.begin(), observers.end(), observer), observers.end());
    }

    void notifyObservers() override {
        for (Observer* observer : observers) {
            observer->update(message);
        }
    }

    void setMessage(const std::string& msg) {
        message = msg;
        notifyObservers();
    }

private:
    std::vector<Observer*> observers;
    std::string message;
};

int main() {
    // 创建具体被观察者
    ConcreteSubject subject;

    // 创建具体观察者
    ConcreteObserver observer1("Observer1");
    ConcreteObserver observer2("Observer2");

    // 添加观察者
    subject.addObserver(&observer1);
    subject.addObserver(&observer2);

    // 被观察者的状态改变，通知所有观察者
    subject.setMessage("New message for all observers!");

    // 移除一个观察者
    subject.removeObserver(&observer1);

    // 被观察者状态变化，通知剩下的观察者
    subject.setMessage("Another message!");

    return 0;
}