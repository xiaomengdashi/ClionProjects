// factory_pattern.hpp
#pragma once

#include <iostream>
#include <memory>

// 产品基类
class Product {
public:
    virtual void operation() const = 0;
    virtual ~Product() = default;
};

// 具体产品 A
class ConcreteProductA : public Product {
public:
    void operation() const override {
        std::cout << "ConcreteProductA operation" << std::endl;
    }
};

// 具体产品 B
class ConcreteProductB : public Product {
public:
    void operation() const override {
        std::cout << "ConcreteProductB operation" << std::endl;
    }
};

// 工厂基类
class Factory {
public:
    virtual std::unique_ptr<Product> createProduct() const = 0;
    virtual ~Factory() = default;
};

// 具体工厂 A，用于创建具体产品 A
class ConcreteFactoryA : public Factory {
public:
    std::unique_ptr<Product> createProduct() const override {
        return std::make_unique<ConcreteProductA>();
    }
};

// 具体工厂 B，用于创建具体产品 B
class ConcreteFactoryB : public Factory {
public:
    std::unique_ptr<Product> createProduct() const override {
        return std::make_unique<ConcreteProductB>();
    }
};

// 测试工厂模式的函数
inline void factory_test() {
    std::cout << "=========strategy test==========" << std::endl;

    // 使用具体工厂 A 创建产品
    ConcreteFactoryA factoryA;
    auto productA = factoryA.createProduct();
    productA->operation();

    // 使用具体工厂 B 创建产品
    ConcreteFactoryB factoryB;
    auto productB = factoryB.createProduct();
    productB->operation();

    std::cout << "\n" << std::endl;
}