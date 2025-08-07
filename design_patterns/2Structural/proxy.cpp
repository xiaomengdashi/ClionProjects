#include <iostream>
#include <memory>

// 服务接口
class Subject {
public:
    virtual ~Subject() = default;
    virtual void request() const = 0;
};

// 真实服务
class RealSubject : public Subject {
public:
    void request() const override {
        std::cout << "RealSubject: Handling request." << std::endl;
    }
};

// 代理
class Proxy : public Subject {
private:
    std::unique_ptr<RealSubject> realSubject_;

    bool checkAccess() const {
        // 一些真实的检查应该在这里进行
        std::cout << "Proxy: Checking access prior to firing a real request." << std::endl;
        return true;
    }

    void logAccess() const {
        std::cout << "Proxy: Logging the time of request." << std::endl;
    }

public:
    Proxy(std::unique_ptr<RealSubject> realSubject) 
        : realSubject_(std::move(realSubject)) {}

    void request() const override {
        if (this->checkAccess()) {
            this->realSubject_->request();
            this->logAccess();
        }
    }
};

// 客户端代码
void clientCode(const Subject& subject) {
    subject.request();
}

int main() {
    std::cout << "=== Proxy Pattern Demo ===" << std::endl;
    
    std::cout << "Client: Executing the client code with a real subject:" << std::endl;
    auto realSubject = std::make_unique<RealSubject>();
    clientCode(*realSubject);
    
    std::cout << std::endl;
    
    std::cout << "Client: Executing the same client code with a proxy:" << std::endl;
    auto proxy = std::make_unique<Proxy>(std::move(realSubject));
    clientCode(*proxy);

    return 0;
}