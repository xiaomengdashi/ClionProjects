//
// Created by Kolane on 2025/3/29.
//

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