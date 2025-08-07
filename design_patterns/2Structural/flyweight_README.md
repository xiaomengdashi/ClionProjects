# Flyweight Pattern（享元模式）

## 概述
享元模式是一种结构型设计模式，它摒弃了在每个对象中保存所有数据的方式，通过共享多个对象所共有的相同状态，让你能在有限的内存容量中载入更多对象。享元模式通过共享技术实现相同或相似对象的重用。

## 意图
- 运用共享技术有效地支持大量细粒度的对象
- 减少创建对象的数量，以减少内存占用和提高性能

## 结构
1. **Flyweight（享元）**：声明一个接口，通过这个接口flyweight可以接受并作用于外部状态
2. **ConcreteFlyweight（具体享元）**：实现Flyweight接口，并为内部状态增加存储空间
3. **FlyweightFactory（享元工厂）**：创建并管理flyweight对象，确保合理地共享flyweight
4. **Context（上下文）**：包含享元的外部状态

## 适用场景
- 一个应用程序使用了大量的对象
- 完全由于使用大量的对象，造成很大的存储开销
- 对象的大多数状态都可变为外部状态
- 如果删除对象的外部状态，那么可以用相对较少的共享对象取代很多组对象

## 优点
- 大大减少对象的创建，降低系统的内存，使效率提高
- 减少内存之外的其他计算开销

## 缺点
- 提高了系统的复杂度，需要分离出外部状态和内部状态
- 而且外部状态具有固有化的性质，不应该随着内部状态的变化而变化

## 编译和运行
```bash
g++ -std=c++14 flyweight.cpp -o flyweight
./flyweight
```

## 示例输出
```
=== Flyweight Pattern Demo ===

Client: Adding a car to database.
FlyweightFactory: Can't find a flyweight, creating new one.
ConcreteFlyweight: Intrinsic State = BMWMred, Extrinsic State = CL234IRJames Doe

Client: Adding a car to database.
FlyweightFactory: Can't find a flyweight, creating new one.
ConcreteFlyweight: Intrinsic State = BMWX1red, Extrinsic State = CL234IRJames Doe

Client: Adding a car to database.
FlyweightFactory: Reusing existing flyweight.
ConcreteFlyweight: Intrinsic State = BMWMred, Extrinsic State = CL234IRJames Doe

Client: Adding a car to database.
FlyweightFactory: Can't find a flyweight, creating new one.
ConcreteFlyweight: Intrinsic State = BMWX6blue, Extrinsic State = CL234IRJames Doe

FlyweightFactory: I have 3 flyweights:
BMWMred
BMWX1red
BMWX6blue
```