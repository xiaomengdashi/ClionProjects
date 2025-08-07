# Adapter Pattern（适配器模式）

## 概述
适配器模式是一种结构型设计模式，它能使接口不兼容的对象能够相互合作。适配器模式通过包装一个对象，使其能够与另一个接口不兼容的对象进行交互。

## 意图
- 将一个类的接口转换成客户希望的另外一个接口
- 使得原本由于接口不兼容而不能一起工作的那些类可以一起工作

## 结构
1. **Target（目标）**：定义客户端使用的特定领域相关的接口
2. **Adaptee（被适配者）**：定义一个已经存在的接口，这个接口需要适配
3. **Adapter（适配器）**：对Adaptee的接口与Target接口进行适配

## 适用场景
- 系统需要使用现有的类，而此类的接口不符合系统的需要
- 想要建立一个可以重复使用的类，用于与一些彼此之间没有太大关联的一些类一起工作
- 需要在不修改现有代码的情况下使用第三方库

## 优点
- 可以让任何两个没有关联的类一起运行
- 提高了类的复用性
- 增加了类的透明度
- 灵活性好

## 缺点
- 过多地使用适配器，会让系统非常零乱，不易整体进行把握
- 由于Java至多继承一个类，所以至多只能适配一个适配者类

## 编译和运行
```bash
g++ -std=c++14 adapter.cpp -o adapter
./adapter
```

## 示例输出
```
=== Adapter Pattern Demo ===
Client: I can work just fine with the Target objects:
Target: The default target's behavior.

Client: The Adaptee class has a weird interface. See, I don't understand it:
Adaptee: .eetpadA eht fo roivaheb laicepS

Client: But I can work with it via the Adapter:
Adapter: (TRANSLATED) Special behavior of the Adaptee.
```