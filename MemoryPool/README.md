# C++ 高性能内存池系统

一个现代化的 C++ 内存池库，提供多种内存管理策略和优化方案，专为高性能应用设计。

## 🚀 主要特性

### 多种内存池实现
- **FixedMemoryPool**: 固定大小块内存池，适合频繁分配相同大小对象
- **VariableMemoryPool**: 可变大小内存池，支持任意大小内存分配
- **ThreadSafeMemoryPool**: 线程安全内存池，支持多线程并发访问
- **IndexedMemoryPool**: 多级索引内存池，提供 O(log n) 查找性能

### 性能优化
- **多级索引**: 对于大型内存池，索引系统提供高达 **200x** 的性能提升
- **智能合并**: 自动合并相邻空闲块，减少内存碎片
- **缓存友好**: 优化的数据结构布局，提高缓存命中率
- **零拷贝**: 高效的内存管理，避免不必要的数据拷贝

### STL 兼容
- **PoolAllocator**: STL 兼容的分配器，可直接用于 std::vector, std::map 等容器
- **类型安全**: 模板化设计，编译期类型检查
- **标准接口**: 完全符合 C++ 分配器要求

## 📊 性能对比

### 多级索引 vs 传统链表查找

| 分配大小 | 传统内存池 | 索引内存池 | 性能提升 |
|---------|-----------|-----------|----------|
| 16 字节  | 78 ms    | 1 ms      | **78x**  |
| 256 字节 | 237 ms   | 1 ms      | **237x** |
| 1024 字节| 242 ms   | 1 ms      | **242x** |

> 测试条件：10MB 内存池，10000 次分配操作

## 🏗️ 快速开始

### 编译项目

```bash
mkdir build && cd build
cmake ..
make
```

### 基本使用

#### 1. 固定大小内存池
```cpp
#include "core/FixedMemoryPool.hpp"

// 创建可存储1000个int的内存池
FixedMemoryPool<int> pool(1000);

// 分配对象
int* ptr = pool.allocate();
*ptr = 42;

// 释放对象
pool.deallocate(ptr);
```

#### 2. 可变大小内存池
```cpp
#include "core/VariableMemoryPool.hpp"

// 创建1MB的可变大小内存池
VariableMemoryPool pool(1024 * 1024);

// 分配不同大小的内存
void* ptr1 = pool.allocate(64);
void* ptr2 = pool.allocate(128);

// 释放内存
pool.deallocate(ptr1);
pool.deallocate(ptr2);
```

#### 3. 高性能索引内存池
```cpp
#include "core/IndexedMemoryPool.hpp"

// 创建带索引的高性能内存池
IndexedMemoryPool pool(10 * 1024 * 1024); // 10MB

// 快速分配 - O(log n) 性能
void* ptr = pool.allocate(1024);

// 快速释放和合并
pool.deallocate(ptr);

// 获取索引统计信息
auto stats = pool.get_index_statistics();
std::cout << "大小类别数: " << stats.total_size_classes << std::endl;
```

#### 4. STL容器集成
```cpp
#include "utils/PoolAllocator.hpp"
#include "core/VariableMemoryPool.hpp"

// 创建内存池
auto pool = std::make_shared<VariableMemoryPool>(1024 * 1024);

// 创建使用内存池的STL容器
PoolAllocator<int, VariableMemoryPool> allocator(*pool);
std::vector<int, PoolAllocator<int, VariableMemoryPool>> vec(allocator);

// 正常使用STL容器，内存自动通过池分配
vec.push_back(1);
vec.push_back(2);
vec.push_back(3);
```

### 线程安全使用
```cpp
#include "core/ThreadSafeMemoryPool.hpp"

// 创建线程安全的内存池
ThreadSafeMemoryPool pool(1024 * 1024);

// 多线程环境下安全使用
std::thread t1([&pool]() {
    void* ptr = pool.allocate(256);
    // ... 使用内存 ...
    pool.deallocate(ptr);
});

std::thread t2([&pool]() {
    void* ptr = pool.allocate(512);
    // ... 使用内存 ...
    pool.deallocate(ptr);
});
```

### 5G NGAP消息处理
```cpp
#include "tests/ngap_demo.cpp"

// 创建NGAP专用内存池
NgapMemoryPool ngap_pool(10 * 1024 * 1024);

// 构建 Initial Context Setup Request
NgapMessageBuilder builder(ngap_pool, NGAP_LARGE);

// UE标识符
uint64_t amf_ue_id = 0x123456789ABCDEF0;
uint64_t ran_ue_id = 0x0FEDCBA987654321;

// GUAMI
uint8_t guami[16] = {0x46, 0x00, 0x07, 0x00, 0x00, 0x01, /*...*/};

// PDU Session资源
std::vector<PduSessionResource> pdu_sessions;
// ... 填充PDU Session数据 ...

// 使用流式接口构建消息
auto [msg_ptr, msg_size] = builder
    .add_ue_ids(amf_ue_id, ran_ue_id)
    .add_guami(guami)
    .add_pdu_session_list(pdu_sessions)
    .finalize();

if (msg_ptr) {
    // 发送到网络层
    send_to_network(msg_ptr, msg_size);
    
    // 释放消息内存
    ngap_pool.deallocate_message(msg_ptr);
}
```

## 🧪 运行演示程序

### 编译所有演示程序
```bash
make memorypool_demo       # 基本演示
make memorypool_allocator  # STL分配器演示
make memorypool_factory    # 工厂模式演示
make memorypool_threadsafe # 线程安全演示
make memorypool_indexed    # 索引内存池性能测试
make memorypool_ngap       # NGAP消息演示
make ngap_complex_example  # 复杂NGAP消息示例
```

### 运行性能测试
```bash
# 运行索引内存池性能对比测试
./memorypool_indexed

# 运行STL分配器演示
./memorypool_allocator

# 运行线程安全测试
./memorypool_threadsafe

# 运行NGAP消息演示
./memorypool_ngap

# 运行复杂NGAP消息示例
./ngap_complex_example
```

## 📁 项目结构

```
MemoryPool/
├── core/                          # 核心内存池实现
│   ├── IMemoryPool.h             # 内存池接口
│   ├── FixedMemoryPool.hpp       # 固定大小内存池
│   ├── VariableMemoryPool.hpp    # 可变大小内存池
│   ├── ThreadSafeMemoryPool.hpp  # 线程安全内存池
│   ├── IndexedMemoryPool.hpp     # 多级索引内存池
│   └── PoolStatistics.h          # 统计信息结构
├── utils/                         # 工具类
│   ├── PoolAllocator.hpp         # STL兼容分配器
│   └── MemoryPoolFactory.hpp     # 内存池工厂
├── tests/                         # 测试和演示程序
│   ├── allocator_demo.cpp        # STL分配器演示
│   ├── factory_demo.cpp          # 工厂模式演示
│   ├── threadsafe_demo.cpp       # 线程安全演示
│   ├── indexed_pool_demo.cpp     # 索引内存池性能测试
│   └── ngap_demo.cpp             # NGAP消息演示
├── examples/                      # 实际应用示例
│   └── ngap_complex_example.cpp  # 复杂NGAP消息实例
├── docs/                          # 文档
│   ├── IndexedMemoryPool_Design.md # 索引内存池设计文档
│   └── NGAP_Usage_Guide.md       # NGAP内存池使用指南
├── CMakeLists.txt                 # 构建配置
└── README.md                      # 项目说明
```

## 🔧 高级特性

### 内存统计和监控
```cpp
// 获取详细的内存使用统计
auto stats = pool.get_statistics();
std::cout << "总分配次数: " << stats.total_allocations << std::endl;
std::cout << "当前内存使用: " << stats.current_bytes_used << std::endl;
std::cout << "碎片率: " << (stats.fragmentation_ratio * 100) << "%" << std::endl;
```

### 内存整理和优化
```cpp
// 手动触发内存整理，合并碎片
pool.defragment();

// 重置内存池（清空所有分配）
pool.reset();
```

### 索引性能分析
```cpp
// 获取索引系统的详细统计信息
auto index_stats = pool.get_index_statistics();
std::cout << "大小类别数: " << index_stats.total_size_classes << std::endl;
std::cout << "空闲块总数: " << index_stats.total_free_blocks << std::endl;
std::cout << "平均每类块数: " << index_stats.average_blocks_per_class << std::endl;
```

## 🎯 适用场景

### 高性能应用
- **游戏引擎**: 实时对象创建/销毁，减少GC压力
- **服务器应用**: 高并发请求处理，内存池复用
- **数据库系统**: 缓存池管理，减少系统调用
- **网络服务**: 连接池和缓冲区管理

### 嵌入式系统
- **实时系统**: 确定性内存分配，避免系统调用
- **内存受限环境**: 精确控制内存使用
- **高可靠性应用**: 减少内存分配失败风险

### 科学计算
- **数值计算**: 大量临时对象的高效管理
- **图像处理**: 大块连续内存的快速分配
- **机器学习**: 张量和矩阵的内存池管理

### 5G/电信领域
- **5G AMF**: NGAP消息的高效处理和缓存
- **5G gNB**: 无线接入网中的消息池管理
- **协议栈**: TLV结构和变长数组的优化分配
- **实时通信**: 低延迟消息处理

## 🛠️ 编译要求

- **C++17** 或更高版本
- **CMake 3.10** 或更高版本
- 支持的编译器：
  - GCC 7+
  - Clang 6+
  - MSVC 2017+

## 📈 性能优化建议

### 选择合适的内存池类型
- **频繁分配相同大小**: 使用 `FixedMemoryPool`
- **大小变化较大**: 使用 `IndexedMemoryPool`
- **多线程环境**: 使用 `ThreadSafeMemoryPool`
- **普通应用**: 使用 `VariableMemoryPool`

### 内存池大小配置
- **小内存池 (< 1MB)**: 简单实现即可
- **中等内存池 (1-10MB)**: 考虑使用索引优化
- **大内存池 (> 10MB)**: 强烈推荐索引内存池

### 碎片化管理
- 定期检查碎片率，超过30%时考虑整理
- 合理设置初始内存池大小，避免频繁扩展
- 使用对象池模式减少碎片产生

## 🤝 贡献指南

欢迎提交 Issue 和 Pull Request！

### 开发环境设置
```bash
git clone <repository-url>
cd MemoryPool
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make
```

### 运行测试
```bash
# 运行所有测试
make test

# 运行特定测试
./memorypool_indexed
```

## 📄 许可证

本项目采用 MIT 许可证，详见 LICENSE 文件。

## 🙏 致谢

感谢所有为这个项目做出贡献的开发者！

---

**高性能，低延迟，为现代 C++ 应用而生。** 🚀