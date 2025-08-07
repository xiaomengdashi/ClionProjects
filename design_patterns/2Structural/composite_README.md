# Composite Pattern（组合模式）

## 概述
组合模式是一种结构型设计模式，你可以使用它将对象组合成树状结构，并且能像使用独立对象一样使用它们。组合模式让客户端能够统一对待单个对象和对象组合。

## 意图
- 将对象组合成树形结构以表示"部分-整体"的层次结构
- 使得用户对单个对象和组合对象的使用具有一致性

## 结构
1. **Component（组件）**：为组合中的对象声明接口，在适当情况下，实现所有类共有接口的默认行为
2. **Leaf（叶子）**：在组合中表示叶子节点对象，叶子节点没有子节点
3. **Composite（组合）**：定义有枝节点行为，用来存储子部件，在Component接口中实现与子部件有关的操作

## 适用场景
- 想表示对象的部分-整体层次结构
- 希望用户忽略组合对象与单个对象的不同，用户将统一地使用组合结构中的所有对象
- 动态地增加或减少组件

## 优点
- 高层模块调用简单
- 节点自由增加
- 符合开放封闭原则

## 缺点
- 在使用组合模式时，其叶子和树枝的声明都是实现类，而不是接口，违反了依赖倒置原则
- 设计较复杂，客户端需要花更多时间理清类之间的层次关系

## 编译和运行
```bash
g++ -std=c++14 composite.cpp -o composite
./composite
```

## 示例输出
```
=== Composite Pattern Demo ===
Client: I've got a simple component:
RESULT: Leaf

Client: Now I've got a composite tree:
RESULT: Branch(Branch(Leaf+Leaf)+Branch(Leaf))

Client: I don't need to check the components classes even when managing the tree:
RESULT: Branch(Branch(Leaf+Leaf)+Branch(Leaf)+Leaf)
```