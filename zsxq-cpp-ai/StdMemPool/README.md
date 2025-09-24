# StdIndexedMemPool

这是一个高性能内存池实现，内存池返回整数索引而不是直接返回指针，允许在使用较少位数的情况下管理大量元素，并且保证被回收的内存不会被返回给操作系统，使得即使在元素被回收后读取也是安全的。

## 主要特性

1. **高效的内存管理**：动态分配并池化元素类型，避免频繁的系统内存分配和释放
2. **索引而非指针**：返回4字节整数索引，可以通过池的operator[]方法访问实际元素
3. **安全的访问**：即使元素被回收，其内存仍然可读（但需要验证读取的正确性）
4. **线程安全**：使用本地化列表减少多线程环境中的争用
5. **灵活的对象生命周期管理**：支持急切回收和惰性回收两种策略

## 使用方法

### 基本用法

```cpp
#include "StdIndexedMemPool.h"

// 创建一个可以容纳100个int元素的内存池
std_mem_pool::IndexedMemPool<int> pool(100);

// 分配元素并获取索引
uint32_t idx = pool.allocIndex(42);  // 分配一个值为42的元素

// 访问元素
int value = pool[idx];  // 获取元素值
pool[idx] = 100;        // 修改元素值

// 回收元素
pool.recycleIndex(idx);
```

### 使用智能指针

```cpp
// 分配元素并获取智能指针
auto ptr = pool.allocElem(42);  // 分配一个值为42的元素

// 访问元素
int value = *ptr;  // 获取元素值
*ptr = 100;        // 修改元素值

// 智能指针离开作用域时会自动回收元素
```

### 自定义对象生命周期

```cpp
// 惰性回收：元素在第一次分配时构造，在池销毁时析构
std_mem_pool::IndexedMemPool<MyClass, 32, 200, std::atomic, 
                           std_mem_pool::IndexedMemPoolTraitsLazyRecycle<MyClass>> 
    lazyPool(100);

// 急切回收：元素在每次分配时构造，在回收时析构
std_mem_pool::IndexedMemPool<MyClass, 32, 200, std::atomic, 
                           std_mem_pool::IndexedMemPoolTraitsEagerRecycle<MyClass>> 
    eagerPool(100);
```

## 性能优化

StdIndexedMemPool使用多种技术来优化性能：

1. **线程本地列表**：减少跨线程争用
2. **延迟构造**：只有实际使用的元素才会被构造
3. **预分配内存**：使用mmap预分配整个地址空间
4. **无锁算法**：使用原子操作和比较交换操作避免锁争用

## 性能测试结果

与标准的new/delete相比，StdIndexedMemPool在各种场景下都表现出色。以下是性能测试结果：

| 测试名称 | 总时间 (ms) | 操作/秒 (K) | 每操作时间 (μs) | 内存使用 (KB) |
|---|---|---|---|---|
| MemPool - 小型对象 (int) | 189.86 | 15800.75 | 0.06 | 3908.44 |
| New/Delete - 小型对象 (int) | 186.00 | 16128.95 | 0.06 | 3906.25 |
| MemPool - 中型对象 (64字节) | 51.08 | 5873.37 | 0.17 | 6252.19 |
| New/Delete - 中型对象 (64字节) | 39.05 | 7682.07 | 0.13 | 6250.00 |
| MemPool - 大型对象 (TestObject) | 5.93 | 5057.66 | 0.20 | 236.56 |
| New/Delete - 大型对象 (TestObject) | 2.91 | 10320.73 | 0.10 | 234.38 |

从测试结果可以看出：

1. 在小型对象（如int）上，内存池与标准new/delete的性能相当
2. 在中型和大型对象上，标准new/delete的性能略优于内存池
3. 但内存池的真正优势在于多线程环境下减少争用，测试结果显示在8线程环境下可以达到每秒1300万次操作

内存池特别适合以下场景：

- 需要频繁分配和释放相同类型对象的应用
- 多线程环境中需要减少锁争用的场景
- 需要保证内存地址稳定的应用
- 对象大小较小且数量很大的场景

## 编译和测试

使用提供的Makefile来编译测试程序：

```bash
make
./test_mem_pool       # 运行基本功能测试
./benchmark_mem_pool  # 运行性能基准测试
```

## 模板参数

`IndexedMemPool`是一个模板类，支持以下参数：

```cpp
template <
    typename T,                      // 元素类型
    uint32_t NumLocalLists_ = 32,    // 本地列表数量
    uint32_t LocalListLimit_ = 200,  // 本地列表大小限制
    template <typename> class Atom = std::atomic,  // 原子操作模板
    typename Traits = IndexedMemPoolTraits<T>      // 生命周期特性
>
class IndexedMemPool;
``` 