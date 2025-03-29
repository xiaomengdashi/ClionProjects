//
// Created by Kolane on 2025/3/29.
//

#include <iostream>
#include <vector>

// 迭代器接口
class Iterator {
public:
    virtual ~Iterator() {}
    virtual bool hasNext() = 0;  // 判断是否还有下一个元素
    virtual int next() = 0;      // 返回下一个元素
};

// 集合接口
class Aggregate {
public:
    virtual ~Aggregate() {}
    virtual Iterator* createIterator() = 0;  // 创建一个迭代器
};

// 具体迭代器类
class ConcreteIterator : public Iterator {
private:
    std::vector<int> collection;  // 被遍历的集合
    size_t currentPosition = 0;   // 当前遍历的位置

public:
    ConcreteIterator(const std::vector<int>& items) : collection(items) {}

    bool hasNext() override {
        return currentPosition < collection.size();
    }

    int next() override {
        return collection[currentPosition++];
    }
};

// 具体集合类
class ConcreteAggregate : public Aggregate {
private:
    std::vector<int> items;  // 存储集合的元素

public:
    ConcreteAggregate(const std::vector<int>& items) : items(items) {}

    Iterator* createIterator() override {
        return new ConcreteIterator(items);
    }
};

// 客户端代码
int main() {
    // 创建一个具体集合
    std::vector<int> items = {1, 2, 3, 4, 5};
    ConcreteAggregate aggregate(items);

    // 创建一个迭代器
    Iterator* iterator = aggregate.createIterator();

    // 使用迭代器遍历集合
    while (iterator->hasNext()) {
        std::cout << iterator->next() << " ";
    }

    delete iterator;
    return 0;
}