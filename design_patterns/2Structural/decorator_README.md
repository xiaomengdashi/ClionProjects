# Decorator Pattern（装饰器模式）

## 概述
装饰器模式是一种结构型设计模式，允许你通过将对象放入包含行为的特殊封装对象中来为原对象绑定新的行为。装饰器模式能够在不改变对象结构的情况下，动态地给一个对象添加一些额外的职责。

## 意图
- 动态地给一个对象添加一些额外的职责
- 就增加功能来说，装饰器模式相比生成子类更为灵活

## 结构
1. **Component（组件）**：定义一个对象接口，可以给这些对象动态地添加职责
2. **ConcreteComponent（具体组件）**：定义一个对象，可以给这个对象添加一些职责
3. **Decorator（装饰器）**：维持一个指向Component对象的指针，并定义一个与Component接口一致的接口
4. **ConcreteDecorator（具体装饰器）**：向组件添加职责

## 适用场景
- 在不影响其他对象的情况下，以动态、透明的方式给单个对象添加职责
- 处理那些可以撤消的职责
- 当不能采用生成子类的方法进行扩充时

## 优点
- 装饰器模式与继承关系的目的都是要扩展对象的功能，但是装饰器模式可以提供比继承更多的灵活性
- 可以通过一种动态的方式来扩展一个对象的功能
- 通过使用不同的具体装饰类以及这些装饰类的排列组合，可以创造出很多不同行为的组合
- 具体组件类与具体装饰类可以独立变化

## 缺点
- 使用装饰器模式进行系统设计时将产生很多小对象
- 装饰器模式比继承更加易于出错，排错也很困难

## 编译和运行
```bash
g++ -std=c++14 decorator.cpp -o decorator
./decorator
```

## 示例输出
```
=== Decorator Pattern Demo ===
Client: I've got a simple component:
RESULT: ConcreteComponent

Client: Now I've got a decorated component:
RESULT: ConcreteDecoratorB(ConcreteDecoratorA(ConcreteComponent))
```