//
// Created by Kolane on 2025/3/29.
//

#include <iostream>
#include <string>
#include <memory>


// 定义产品类（Computer）
class Computer {
private:
    std::string CPU_;
    std::string RAM_;
    std::string storage_;

public:
    void setCPU(const std::string& cpu) {
        CPU_= cpu;
    }

    void setRAM(const std::string& ram) {
        RAM_ = ram;
    }

    void setStorage(const std::string& storage) {
        this->storage_ = storage;
    }

    void show() const {
        std::cout << "Computer [CPU=" << CPU_ << ", RAM=" << RAM_ << ", Storage=" << storage_ << "]\n";
    }
};

// 定义抽象生成器类（ComputerBuilder）
class ComputerBuilder {
protected:
    std::unique_ptr<Computer> computer_;

public:
    virtual ~ComputerBuilder() = default;

    void createNewComputer() {
        computer_ = std::make_unique<Computer>();  // 正确初始化
    }

    std::unique_ptr<Computer> getComputer() {
        return std::move(computer_);
    }

    virtual void buildCPU() = 0;
    virtual void buildRAM() = 0;
    virtual void buildStorage() = 0;
};

// 定义具体生成器类（GamingComputerBuilder 和 OfficeComputerBuilder）
class GamingComputerBuilder : public ComputerBuilder {
public:
    void buildCPU() override {
        computer_->setCPU("Intel i9");
    }

    void buildRAM() override {
        computer_->setRAM("32GB DDR4");
    }

    void buildStorage() override {
        computer_->setStorage("1TB SSD");
    }
};

class OfficeComputerBuilder : public ComputerBuilder {
public:
    void buildCPU() override {
        computer_->setCPU("Intel i5");
    }

    void buildRAM() override {
        computer_->setRAM("16GB DDR4");
    }

    void buildStorage() override {
        computer_->setStorage("500GB HDD");
    }
};

// 定义指挥者类（Director）
class Director {
private:
    ComputerBuilder* builder;

public:
    Director(ComputerBuilder* builder) : builder(builder) {}

    void constructComputer() {
        builder->createNewComputer();
        builder->buildCPU();
        builder->buildRAM();
        builder->buildStorage();
    }
};

// 客户端代码（Client）
int main() {
    // 构建游戏电脑
    std::unique_ptr<ComputerBuilder> gamingBuilder = std::make_unique<GamingComputerBuilder>();
    Director director(gamingBuilder.get());

    director.constructComputer();
    std::unique_ptr<Computer> gamingComputer = gamingBuilder->getComputer();
    gamingComputer->show();

    // 构建办公电脑
    std::unique_ptr<ComputerBuilder> officeBuilder = std::make_unique<OfficeComputerBuilder>();
    director = Director(officeBuilder.get());

    director.constructComputer();
    std::unique_ptr<Computer> officeComputer = officeBuilder->getComputer();
    officeComputer->show();

    return 0;
}
