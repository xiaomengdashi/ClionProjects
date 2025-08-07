# Facade Pattern（外观模式）

## 概述
外观模式是一种结构型设计模式，能为程序库、框架或其他复杂类集提供一个简单的接口。外观模式为子系统中的一组接口提供一个一致的界面，定义一个高层接口，这个接口使得这一子系统更加容易使用。

## 意图
- 为子系统中的一组接口提供一个一致的界面
- 定义一个高层接口，这个接口使得这一子系统更加容易使用

## 结构
1. **Facade（外观）**：知道哪些子系统类负责处理请求，将客户的请求代理给适当的子系统对象
2. **Subsystem classes（子系统类）**：实现子系统的功能，处理由Facade对象指派的任务

## 适用场景
- 当你要为一个复杂子系统提供一个简单接口时
- 客户程序与抽象类的实现部分之间存在着很大的依赖性
- 当你需要构建一个层次结构的子系统时

## 优点
- 减少系统相互依赖
- 提高灵活性
- 提高了安全性

## 缺点
- 不符合开闭原则，如果要改东西很麻烦，继承重写都不合适

## 编译和运行
```bash
g++ -std=c++14 facade.cpp -o facade
./facade
```

## 示例输出
```
=== Facade Pattern Demo ===
Facade initializes subsystems:
Subsystem1: Ready!
Subsystem2: Get ready!
Facade orders subsystems to perform the action:
Subsystem1: Go!
Subsystem2: Fire!
```