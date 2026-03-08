# 内存池工厂类使用指南

## 概述

`MemoryPoolFactory` 是一个静态工厂类，提供了简洁统一的接口来创建不同类型的内存池。它简化了内存池的实例化过程，并提供了一些便利方法来创建常用配置的内存池。

## 核心功能

### 1. 创建固定大小内存池

```cpp
// 基本创建方法
auto pool = MemoryPoolFactory::create_fixed_pool<64>(1000);  // 64字节块，1000个

// 为特定对象类型创建内存池
auto game_pool = MemoryPoolFactory::create_object_pool<GameObject>(500);
```

### 2. 创建可变大小内存池

```cpp
// 创建512KB的可变大小内存池
auto pool = MemoryPoolFactory::create_variable_pool(512 * 1024);

// 创建默认1MB内存池
auto default_pool = MemoryPoolFactory::create_default_pool();
```

### 3. 创建线程安全内存池

```cpp
// 线程安全的固定大小内存池
auto ts_fixed = MemoryPoolFactory::create_thread_safe_fixed_pool<64>(1000);

// 线程安全的可变大小内存池
auto ts_variable = MemoryPoolFactory::create_thread_safe_variable_pool(1024 * 1024);
```

## 使用示例

### 游戏对象内存池

```cpp
#include "utils/MemoryPoolFactory.hpp"

struct GameObject {
    int id;
    float x, y, z;
    GameObject(int id, float x, float y, float z) : id(id), x(x), y(y), z(z) {}
};

// 创建能容纳1000个游戏对象的内存池
auto game_pool = MemoryPoolFactory::create_object_pool<GameObject>(1000);

// 分配并构造对象
void* raw_mem = game_pool->allocate();
GameObject* obj = new(raw_mem) GameObject(1, 10.0f, 20.0f, 30.0f);

// 使用对象...

// 清理
obj->~GameObject();
game_pool->deallocate(obj);
```

### STL容器集成

```cpp
#include "utils/MemoryPoolFactory.hpp"
#include "utils/PoolAllocator.hpp"
#include <vector>

// 创建内存池
auto pool = MemoryPoolFactory::create_variable_pool(1024 * 1024);

// 创建使用内存池的STL分配器
PoolAllocator<int, VariableMemoryPool> allocator(*pool);

// 创建使用内存池的vector
std::vector<int, PoolAllocator<int, VariableMemoryPool>> my_vector(allocator);

// 正常使用vector
my_vector.push_back(1);
my_vector.push_back(2);
my_vector.push_back(3);
```

### 性能监控

```cpp
auto pool = MemoryPoolFactory::create_variable_pool(1024 * 1024);

// 进行一些内存分配操作...
void* ptr1 = pool->allocate(100);
void* ptr2 = pool->allocate(200);

// 获取统计信息
auto stats = pool->get_statistics();
std::cout << "总分配次数: " << stats.total_allocations << std::endl;
std::cout << "当前使用字节数: " << stats.current_bytes_used << std::endl;
std::cout << "峰值使用字节数: " << stats.peak_bytes_used << std::endl;

// 清理
pool->deallocate(ptr1);
pool->deallocate(ptr2);
```

## API 参考

### 核心创建方法

- `create_fixed_pool<BlockSize>(block_count, alignment)` - 创建固定大小内存池
- `create_variable_pool(total_size, alignment)` - 创建可变大小内存池
- `create_thread_safe_pool<PoolType>(args...)` - 创建线程安全内存池

### 便利方法

- `create_object_pool<T>(object_count)` - 为特定类型创建对象池
- `create_default_pool()` - 创建默认1MB内存池
- `create_thread_safe_fixed_pool<BlockSize>(block_count, alignment)` - 线程安全固定池
- `create_thread_safe_variable_pool(total_size, alignment)` - 线程安全可变池

## 注意事项

1. 所有工厂方法返回 `std::unique_ptr`，自动管理内存池生命周期
2. 内存池对象需要在其分配的内存被释放之前保持有效
3. 线程安全内存池适用于多线程环境，但会有额外的开销
4. 固定大小内存池性能最优，但只能分配固定大小的块
5. 可变大小内存池更灵活，但可能产生内存碎片

## 编译要求

- C++17 或更高版本
- 需要包含相关头文件路径
- 链接 pthread 库（用于线程安全内存池）
