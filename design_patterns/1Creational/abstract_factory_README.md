# Abstract Factory Pattern（抽象工厂模式）

## 概述
抽象工厂模式是一种创建型设计模式，它能创建一系列相关的对象，而无需指定其具体类。抽象工厂模式提供一个创建一系列相关或相互依赖对象的接口，而无需指定它们具体的类。

## 意图
- 提供一个创建一系列相关或相互依赖对象的接口，而无需指定它们具体的类
- 系统应该独立于它的产品创建、组合和表示时
- 系统应该由多个产品系列中的一个来配置时

## 结构
1. **AbstractFactory（抽象工厂）**：声明一个创建抽象产品对象的操作接口
2. **ConcreteFactory（具体工厂）**：实现创建具体产品对象的操作
3. **AbstractProduct（抽象产品）**：为一类产品对象声明一个接口
4. **ConcreteProduct（具体产品）**：定义一个将被相应的具体工厂创建的产品对象
5. **Client（客户端）**：仅使用由AbstractFactory和AbstractProduct类声明的接口

## 适用场景
- 系统不应当依赖于产品类实例如何被创建、组合和表达的细节
- 系统中有多于一个的产品族，而每次只使用其中某一产品族
- 属于同一个产品族的产品将在一起使用
- 系统提供一个产品类的库，所有的产品以同样的接口出现

## 优点
- 分离了具体的类
- 使得易于交换产品系列
- 有利于产品的一致性
- 符合开放-封闭原则

## 缺点
- 难以支持新种类的产品
- 增加了系统的抽象性和理解难度

## 与工厂方法模式的区别
- **工厂方法模式**：针对一个产品等级结构
- **抽象工厂模式**：针对多个产品等级结构

## 编译和运行
```bash
g++ -std=c++14 abstract_factory.cpp -o abstract_factory
./abstract_factory
```

## 示例输出
```
=== Abstract Factory Pattern Demo ===
Client: Testing client code with the first factory type:
The result of the product B1.
The result of the B1 collaborating with ( The result of the product A1. )

Client: Testing the same client code with the second factory type:
The result of the product B2.
The result of the B2 collaborating with ( The result of the product A2. )
```