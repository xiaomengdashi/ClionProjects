# Bridge Pattern（桥接模式）

## 概述
桥接模式是一种结构型设计模式，可将一个大类或一系列紧密相关的类拆分为抽象和实现两个独立的层次结构，从而能在开发时分别使用。

## 意图
- 将抽象部分与它的实现部分分离，使它们都可以独立地变化
- 通过组合的方式建立两个类之间的联系，而不是继承

## 结构
1. **Abstraction（抽象类）**：定义抽象类的接口，维护一个指向Implementation类型对象的指针
2. **RefinedAbstraction（扩充抽象类）**：扩充由Abstraction定义的接口
3. **Implementation（实现类接口）**：定义实现类的接口，该接口不一定要与Abstraction的接口完全一致
4. **ConcreteImplementation（具体实现类）**：具体实现Implementation接口

## 适用场景
- 不希望在抽象和它的实现部分之间有一个固定的绑定关系
- 类的抽象以及它的实现都应该可以通过生成子类的方法加以扩充
- 对一个抽象的实现部分的修改应对客户不产生影响
- 想在多个对象间共享实现，但同时要求客户并不知道这一点

## 优点
- 分离抽象接口及其实现部分
- 桥接模式提高了系统的可扩充性
- 实现细节对客户透明
- 可以对客户隐藏实现细节

## 缺点
- 桥接模式的引入会增加系统的理解与设计难度
- 桥接模式要求正确识别出系统中两个独立变化的维度

## 编译和运行
```bash
g++ -std=c++14 bridge.cpp -o bridge
./bridge
```

## 示例输出
```
=== Bridge Pattern Demo ===
Abstraction: Base operation with:
ConcreteImplementationA: Here's the result on the platform A.

ExtendedAbstraction: Extended operation with:
ConcreteImplementationB: Here's the result on the platform B.
```