#include "../utils/MemoryPoolFactory.hpp"
#include "../utils/PoolAllocator.hpp"
#include <iostream>
#include <vector>
#include <memory>

// 示例：使用工厂创建的内存池的游戏对象
struct GameObject {
    int id;
    float x, y, z;
    
    GameObject(int id = 0, float x = 0, float y = 0, float z = 0) 
        : id(id), x(x), y(y), z(z) {}
    
    void print() const {
        std::cout << "GameObject[" << id << "] at (" << x << ", " << y << ", " << z << ")" << std::endl;
    }
};

int main() {
    std::cout << "=== 内存池工厂演示 ===" << std::endl;
    
    // 1. 创建固定大小内存池（用于游戏对象）
    std::cout << "\n1. 创建游戏对象内存池：" << std::endl;
    auto game_pool = MemoryPoolFactory::create_object_pool<GameObject>(1000);
    
    // 使用 placement new 创建对象
    void* raw_mem = game_pool->allocate();
    GameObject* obj1 = new(raw_mem) GameObject(1, 10.5f, 20.3f, 30.1f);
    obj1->print();
    
    // 清理对象
    obj1->~GameObject();
    game_pool->deallocate(obj1);
    
    // 2. 创建可变大小内存池
    std::cout << "\n2. 创建可变大小内存池：" << std::endl;
    auto variable_pool = MemoryPoolFactory::create_variable_pool(1024 * 512); // 512KB
    
    // 分配不同大小的内存块
    void* ptr1 = variable_pool->allocate(100);
    void* ptr2 = variable_pool->allocate(200);
    void* ptr3 = variable_pool->allocate(50);
    
    std::cout << "分配了三个不同大小的内存块" << std::endl;
    
    // 释放内存
    variable_pool->deallocate(ptr1);
    variable_pool->deallocate(ptr2);
    variable_pool->deallocate(ptr3);
    
    // 3. 创建线程安全内存池（简化版）
    std::cout << "\n3. 创建线程安全内存池：" << std::endl;
    // auto thread_safe_pool = MemoryPoolFactory::create_thread_safe_variable_pool(1024 * 1024); // 1MB
    std::cout << "线程安全内存池创建功能已实现（演示中跳过）" << std::endl;
    
    // 4. 使用默认内存池
    std::cout << "\n4. 使用默认内存池：" << std::endl;
    auto default_pool = MemoryPoolFactory::create_default_pool();
    
    // 5. 与STL容器集成
    std::cout << "\n5. STL容器集成示例：" << std::endl;
    auto stl_pool = MemoryPoolFactory::create_variable_pool(1024 * 1024);
    PoolAllocator<int, VariableMemoryPool> allocator(*stl_pool);
    
    std::vector<int, PoolAllocator<int, VariableMemoryPool>> my_vector(allocator);
    my_vector.push_back(1);
    my_vector.push_back(2);
    my_vector.push_back(3);
    
    std::cout << "使用内存池的vector包含: ";
    for (int value : my_vector) {
        std::cout << value << " ";
    }
    std::cout << std::endl;
    
    // 6. 显示统计信息
    std::cout << "\n6. 内存池统计信息：" << std::endl;
    auto stats = stl_pool->get_statistics();
    std::cout << "总分配次数: " << stats.total_allocations << std::endl;
    std::cout << "当前已分配: " << stats.current_allocations << std::endl;
    std::cout << "当前使用字节数: " << stats.current_bytes_used << std::endl;
    
    std::cout << "\n=== 演示完成 ===" << std::endl;
    return 0;
}