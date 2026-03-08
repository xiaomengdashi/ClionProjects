# ThreadSafeMemoryPool 使用指南

## 概述

`ThreadSafeMemoryPool` 是一个模板包装器，为任何内存池类型提供线程安全性。它使用线程本地存储（TLS）来减少锁竞争，提供高性能的多线程内存分配。

## 核心特性

### 1. 线程安全
- 多线程并发访问无竞争条件
- 使用互斥锁保护关键数据结构
- 线程本地池减少锁争用

### 2. 高性能
- 每个线程维护独立的本地内存池
- 最小化跨线程同步开销
- 批量分配策略（可扩展）

### 3. 灵活性
- 支持任何实现 `IMemoryPool` 接口的底层池类型
- 可包装 `FixedMemoryPool`、`VariableMemoryPool` 等
- 统一的接口，透明的线程安全

## 基本用法

### 创建线程安全的固定内存池

```cpp
#include "core/ThreadSafeMemoryPool.hpp"
#include "core/FixedMemoryPool.hpp"

struct GameObject {
    int id;
    float x, y, z;
    char data[48]; // 总共64字节
};

// 创建线程安全的固定大小内存池
ThreadSafeMemoryPool<FixedMemoryPool<GameObject>> safe_pool(1000);

// 在任何线程中安全使用
void* ptr = safe_pool.allocate(sizeof(GameObject));
if (ptr) {
    GameObject* obj = new(ptr) GameObject{42, 1.0f, 2.0f, 3.0f};
    
    // 使用对象...
    
    // 清理
    obj->~GameObject();
    safe_pool.deallocate(ptr);
}
```

### 使用工厂模式创建

```cpp
#include "utils/MemoryPoolFactory.hpp"

// 创建线程安全的固定内存池
auto ts_pool = MemoryPoolFactory::create_thread_safe_fixed_pool<GameObject>(500);

// 创建线程安全的可变内存池
auto ts_var_pool = MemoryPoolFactory::create_thread_safe_variable_pool(1024 * 1024);
```

## 多线程使用示例

### 基础多线程分配

```cpp
void worker_thread(ThreadSafeMemoryPool<FixedMemoryPool<GameObject>>& pool, int thread_id) {
    std::vector<GameObject*> objects;
    
    // 分配对象
    for (int i = 0; i < 100; ++i) {
        void* ptr = pool.allocate(sizeof(GameObject));
        if (ptr) {
            GameObject* obj = new(ptr) GameObject{thread_id * 100 + i, i, i, i};
            objects.push_back(obj);
        }
    }
    
    // 使用对象...
    
    // 清理
    for (GameObject* obj : objects) {
        obj->~GameObject();
        pool.deallocate(obj);
    }
}

// 启动多个线程
ThreadSafeMemoryPool<FixedMemoryPool<GameObject>> pool(10000);
std::vector<std::thread> threads;

for (int i = 0; i < 8; ++i) {
    threads.emplace_back(worker_thread, std::ref(pool), i);
}

for (auto& t : threads) {
    t.join();
}
```

## 高级功能

### 1. 线程本地统计

```cpp
// 获取当前线程的统计信息
auto local_stats = safe_pool.get_thread_local_statistics();
std::cout << "当前线程分配次数: " << local_stats.total_allocations << std::endl;

// 获取全局统计信息（所有线程）
auto global_stats = safe_pool.get_statistics();
std::cout << "全局分配次数: " << global_stats.total_allocations << std::endl;
```

### 2. 配置线程本地池大小

```cpp
// 设置新线程的本地池大小
safe_pool.set_thread_local_pool_size(256);

// 注意：只影响之后创建的线程，不影响已存在的线程
```

### 3. 重置内存池

```cpp
// 重置所有内存池（中央池 + 所有线程本地池）
safe_pool.reset();
```

## 性能特征

### 优势
1. **高并发性能**：每线程独立池减少锁竞争
2. **缓存友好**：线程本地数据提高缓存命中率
3. **可扩展性**：性能随线程数量线性扩展

### 注意事项
1. **内存开销**：每个线程维护独立的本地池
2. **延迟初始化**：线程首次访问时创建本地池
3. **生命周期管理**：本地池随线程结束自动清理

## 实际应用场景

### 1. 游戏引擎
- 多线程游戏对象管理
- 粒子系统内存分配
- 音频/图形资源管理

### 2. 服务器应用
- 高并发请求处理
- 连接池管理
- 缓存系统

### 3. 科学计算
- 并行数据处理
- 矩阵运算中间结果
- 大规模模拟

## 最佳实践

### 1. 选择合适的池大小
```cpp
// 根据预期负载选择池大小
ThreadSafeMemoryPool<FixedMemoryPool<T>> pool(expected_concurrent_objects * num_threads);
```

### 2. 配置线程本地池
```cpp
// 根据单线程峰值使用量设置
pool.set_thread_local_pool_size(peak_objects_per_thread);
```

### 3. 监控性能
```cpp
// 定期检查统计信息
auto stats = pool.get_statistics();
if (stats.fragmentation_ratio > 0.3) {
    // 考虑调整池配置
}
```

### 4. 异常安全
```cpp
void safe_allocation() {
    void* ptr = pool.allocate(sizeof(T));
    if (!ptr) {
        throw std::bad_alloc();
    }
    
    try {
        T* obj = new(ptr) T();
        // 使用对象...
        obj->~T();
        pool.deallocate(ptr);
    } catch (...) {
        pool.deallocate(ptr); // 确保内存释放
        throw;
    }
}
```

## 注意事项

1. **编译要求**：需要 C++14 或更高版本（用于 `std::make_unique`）
2. **链接要求**：需要链接 pthread 库 (`-pthread`)
3. **平台支持**：依赖平台的线程本地存储支持
4. **内存对齐**：继承底层池的对齐要求
5. **调试支持**：在调试模式下可能性能有所下降

## 示例程序

完整的使用示例请参考 `threadsafe_demo.cpp` 文件，包含：
- 基础使用方法
- 多线程压力测试
- 工厂模式创建
- 线程本地统计
- 性能基准测试

运行示例：
```bash
g++ -std=c++14 -pthread -I. threadsafe_demo.cpp -o threadsafe_demo
./threadsafe_demo
```