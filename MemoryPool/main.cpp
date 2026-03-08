#include "core/FixedMemoryPool.hpp"
#include <iostream>

// 假设你有一个GameObject类，大小是固定的
struct GameObject {
    int id;
    float x, y, z;
    GameObject() : id(0), x(0), y(0), z(0) { /* 构造函数 */ }
    ~GameObject() { /* 析构函数 */ } 
};

// 创建一个能管理1000个GameObject对象的内存池
// 模板参数是对象类型，内存池会自动使用sizeof(GameObject)计算块大小
FixedMemoryPool<GameObject> object_pool(1000);

int main() {
    // 分配内存：从池中获取一块原始内存
    void* raw_mem = object_pool.allocate();

    // 使用placement new在分配的内存上构造对象
    GameObject* obj = new(raw_mem) GameObject();

    obj->id = 42;
    obj->x = 10.5f;

    // 释放内存：先调用对象的析构函数，再将内存归还给池
    obj->~GameObject(); // 显式调用析构函数
    std::cout << obj->id << std::endl;
    object_pool.deallocate(obj); // 将内存块归还给池

    return 0;
}

// 注意：FixedMemoryPool的allocate/deallocate默认不接受size参数，
// 因为它只处理固定大小的块。