/**
 * @file benchmark_mem_pool.cpp
 * @brief 深度测试StdIndexedMemPool的功能完整性和性能，并与标准new/delete进行对比
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <thread>
#include <random>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <memory>
#include <mutex>
#include <functional>
#include <cassert>
#include <atomic>
#include "StdIndexedMemPool.h"

// 测试类定义
class TestObject {
public:
    TestObject() : value_(0), data_(nullptr), allocated_(false) {
        allocated_ = true;
    }
    
    explicit TestObject(int value) : value_(value), allocated_(false) {
        // 分配一些内存，模拟真实对象的内存占用
        data_ = new char[value_ % 1024 + 1];
        allocated_ = true;
    }
    
    ~TestObject() {
        if (allocated_ && data_) {
            delete[] data_;
            data_ = nullptr;
        }
        allocated_ = false;
    }
    
    // 不允许复制，以确保我们能追踪每个对象的生命周期
    TestObject(const TestObject&) = delete;
    TestObject& operator=(const TestObject&) = delete;
    
    // 允许移动
    TestObject(TestObject&& other) noexcept
        : value_(other.value_), data_(other.data_), allocated_(other.allocated_) {
        other.data_ = nullptr;
        other.allocated_ = false;
    }
    
    TestObject& operator=(TestObject&& other) noexcept {
        if (this != &other) {
            if (allocated_ && data_) {
                delete[] data_;
            }
            value_ = other.value_;
            data_ = other.data_;
            allocated_ = other.allocated_;
            other.data_ = nullptr;
            other.allocated_ = false;
        }
        return *this;
    }
    
    int getValue() const { return value_; }
    void setValue(int value) { value_ = value; }
    
    // 验证对象是否有效
    bool isValid() const { return allocated_; }

private:
    int value_;
    char* data_;
    bool allocated_;
};

// 用于性能测试的简单结构体
struct PerfTestData {
    int data[16]; // 64字节（在大多数系统上）
    
    PerfTestData() {
        for (int i = 0; i < 16; i++) {
            data[i] = i;
        }
    }
    
    explicit PerfTestData(int val) {
        for (int i = 0; i < 16; i++) {
            data[i] = val + i;
        }
    }
};

// 计数器类，用于测试对象生命周期
struct CounterObject {
    static std::atomic<int> constructCount;
    static std::atomic<int> destructCount;
    
    int value;
    
    CounterObject() : value(0) {
        constructCount.fetch_add(1, std::memory_order_relaxed);
    }
    
    explicit CounterObject(int v) : value(v) {
        constructCount.fetch_add(1, std::memory_order_relaxed);
    }
    
    ~CounterObject() {
        destructCount.fetch_add(1, std::memory_order_relaxed);
    }
    
    static void resetCounters() {
        constructCount.store(0, std::memory_order_relaxed);
        destructCount.store(0, std::memory_order_relaxed);
    }
};

std::atomic<int> CounterObject::constructCount{0};
std::atomic<int> CounterObject::destructCount{0};

// 记录时间的辅助类
class Timer {
public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}
    
    double elapsedMs() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(now - start_).count();
    }
    
    void reset() {
        start_ = std::chrono::high_resolution_clock::now();
    }
    
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;
};

// 测试结果统计
struct BenchmarkResult {
    std::string name;
    double totalTimeMs;
    double opsPerSecond;
    double avgTimePerOpUs;
    size_t memoryUsage;
    
    void print() const {
        std::cout << std::setw(30) << std::left << name
                  << std::setw(15) << std::fixed << std::setprecision(2) << totalTimeMs << " ms"
                  << std::setw(15) << std::fixed << std::setprecision(2) << opsPerSecond / 1000 << " K op/s"
                  << std::setw(15) << std::fixed << std::setprecision(2) << avgTimePerOpUs << " μs/op"
                  << std::setw(15) << std::fixed << std::setprecision(2) << memoryUsage / 1024.0 << " KB"
                  << std::endl;
    }
};

// ==================== 功能测试 ====================

// 测试随机分配和释放
void testRandomAllocFree() {
    std::cout << "\n=== 测试随机分配和释放 ===" << std::endl;
    
    const uint32_t capacity = 10000;
    std_mem_pool::IndexedMemPool<TestObject> pool(capacity);
    
    std::vector<uint32_t> indices;
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // 分配一些对象
    for (uint32_t i = 0; i < capacity / 2; ++i) {
        uint32_t idx = pool.allocIndex(i);
        if (idx != 0) {
            indices.push_back(idx);
            assert(pool[idx].getValue() == i);
        }
    }
    
    std::cout << "初始分配: " << indices.size() << " 个对象" << std::endl;
    
    // 随机释放一半
    std::shuffle(indices.begin(), indices.end(), gen);
    size_t halfSize = indices.size() / 2;
    
    for (size_t i = 0; i < halfSize; ++i) {
        pool.recycleIndex(indices[i]);
    }
    
    indices.erase(indices.begin(), indices.begin() + halfSize);
    std::cout << "随机释放后剩余: " << indices.size() << " 个对象" << std::endl;
    
    // 再分配一些对象
    int newAllocCount = 0;
    for (uint32_t i = 0; i < capacity; ++i) {
        uint32_t idx = pool.allocIndex(i + 1000);
        if (idx != 0) {
            indices.push_back(idx);
            newAllocCount++;
        }
        
        // 只尝试分配一定数量，避免无限循环
        if (newAllocCount >= static_cast<int>(halfSize * 2)) {
            break;
        }
    }
    
    std::cout << "再次分配: " << newAllocCount << " 个对象" << std::endl;
    std::cout << "当前总对象数: " << indices.size() << std::endl;
    
    // 释放所有对象
    for (uint32_t idx : indices) {
        pool.recycleIndex(idx);
    }
    
    std::cout << "所有对象已释放" << std::endl;
}

// 测试极限容量情况
void testCapacityLimits() {
    std::cout << "\n=== 测试极限容量情况 ===" << std::endl;
    
    // 小容量测试
    {
        std_mem_pool::IndexedMemPool<int> smallPool(1);
        uint32_t idx1 = smallPool.allocIndex(42);
        assert(idx1 != 0);
        assert(smallPool[idx1] == 42);
        
        uint32_t idx2 = smallPool.allocIndex(43);
        if (idx2 != 0) {
            std::cout << "注意：即使请求容量为1，由于内部逻辑，实际可能分配更多元素" << std::endl;
            smallPool.recycleIndex(idx2);
        }
        
        smallPool.recycleIndex(idx1);
        std::cout << "小容量测试通过" << std::endl;
    }
    
    // 大容量测试（但不要太大，避免内存问题）
    {
        const uint32_t largeCapacity = 100000;
        std_mem_pool::IndexedMemPool<uint8_t> largePool(largeCapacity);
        
        std::vector<uint32_t> indices;
        uint32_t maxAllocated = 0;
        
        // 一直分配直到失败
        for (uint32_t i = 0; i < largeCapacity * 2; ++i) {
            uint32_t idx = largePool.allocIndex(static_cast<uint8_t>(i % 256));
            if (idx == 0) {
                break;
            }
            indices.push_back(idx);
            maxAllocated = i + 1;
            
            if (i % 10000 == 0 && i > 0) {
                std::cout << "已分配 " << i << " 个元素" << std::endl;
            }
        }
        
        std::cout << "最大成功分配: " << maxAllocated << " 个元素" << std::endl;
        std::cout << "请求容量: " << largeCapacity << std::endl;
        
        // 释放所有元素
        for (uint32_t idx : indices) {
            largePool.recycleIndex(idx);
        }
        
        std::cout << "大容量测试通过" << std::endl;
    }
}

// 测试不同的对象生命周期策略
void testLifecycleStrategies() {
    std::cout << "\n=== 测试不同的对象生命周期策略 ===" << std::endl;
    
    // 测试惰性回收策略
    {
        std::cout << "-- 测试惰性回收策略 --" << std::endl;
        CounterObject::resetCounters();
        
        {
            std_mem_pool::IndexedMemPool<CounterObject, 32, 200, std::atomic, 
                                       std_mem_pool::IndexedMemPoolTraitsLazyRecycle<CounterObject>> 
                lazyPool(10);
            
            uint32_t idx1 = lazyPool.allocIndex();
            uint32_t idx2 = lazyPool.allocIndex();
            
            std::cout << "分配后构造计数: " << CounterObject::constructCount << std::endl;
            std::cout << "分配后析构计数: " << CounterObject::destructCount << std::endl;
            
            lazyPool.recycleIndex(idx1);
            std::cout << "回收一个对象后析构计数: " << CounterObject::destructCount << std::endl;
            
            uint32_t idx3 = lazyPool.allocIndex();
            std::cout << "再次分配后构造计数: " << CounterObject::constructCount << std::endl;
            
            lazyPool.recycleIndex(idx2);
            lazyPool.recycleIndex(idx3);
        }
        
        std::cout << "池销毁后构造计数: " << CounterObject::constructCount << std::endl;
        std::cout << "池销毁后析构计数: " << CounterObject::destructCount << std::endl;
    }
    
    // 测试急切回收策略
    {
        std::cout << "\n-- 测试急切回收策略 --" << std::endl;
        CounterObject::resetCounters();
        
        {
            std_mem_pool::IndexedMemPool<CounterObject, 32, 200, std::atomic,
                                       std_mem_pool::IndexedMemPoolTraitsEagerRecycle<CounterObject>>
                eagerPool(10);
            
            uint32_t idx1 = eagerPool.allocIndex();
            uint32_t idx2 = eagerPool.allocIndex();
            
            std::cout << "分配后构造计数: " << CounterObject::constructCount << std::endl;
            std::cout << "分配后析构计数: " << CounterObject::destructCount << std::endl;
            
            eagerPool.recycleIndex(idx1);
            std::cout << "回收一个对象后析构计数: " << CounterObject::destructCount << std::endl;
            
            uint32_t idx3 = eagerPool.allocIndex();
            std::cout << "再次分配后构造计数: " << CounterObject::constructCount << std::endl;
            
            eagerPool.recycleIndex(idx2);
            eagerPool.recycleIndex(idx3);
        }
        
        std::cout << "池销毁后构造计数: " << CounterObject::constructCount << std::endl;
        std::cout << "池销毁后析构计数: " << CounterObject::destructCount << std::endl;
    }
}

// 测试智能指针功能与资源管理
void testSmartPointerManagement() {
    std::cout << "\n=== 测试智能指针功能与资源管理 ===" << std::endl;
    
    std_mem_pool::IndexedMemPool<TestObject> pool(100);
    
    // 定义智能指针的类型
    using PoolPtr = std_mem_pool::IndexedMemPool<TestObject>::UniquePtr;
    std::vector<PoolPtr> pointers;
    
    // 分配一些智能指针
    for (int i = 0; i < 50; ++i) {
        auto ptr = pool.allocElem(i + 100);
        if (ptr != nullptr) {
            assert(ptr->getValue() == i + 100);
            pointers.push_back(std::move(ptr));
        }
    }
    
    std::cout << "分配了 " << pointers.size() << " 个智能指针管理的对象" << std::endl;
    
    // 随机丢弃一半的指针，应该自动回收
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(pointers.begin(), pointers.end(), gen);
    
    size_t halfSize = pointers.size() / 2;
    // 不使用resize，因为unique_ptr不能默认构造
    pointers.erase(pointers.begin(), pointers.begin() + halfSize);
    
    std::cout << "丢弃一半后剩余 " << pointers.size() << " 个智能指针" << std::endl;
    
    // 再分配一些智能指针，应该能重用已回收的内存
    int newAllocCount = 0;
    for (int i = 0; i < 50; ++i) {
        auto ptr = pool.allocElem(i + 1000);
        if (ptr != nullptr) {
            assert(ptr->getValue() == i + 1000);
            pointers.push_back(std::move(ptr));
            newAllocCount++;
        }
    }
    
    std::cout << "再次分配了 " << newAllocCount << " 个智能指针管理的对象" << std::endl;
    std::cout << "当前总共有 " << pointers.size() << " 个智能指针" << std::endl;
    
    // 清空所有指针，应该自动回收所有资源
    pointers.clear();
    std::cout << "已清空所有智能指针" << std::endl;
    
    // 确认可以再次分配
    auto ptr = pool.allocElem(42);
    assert(ptr != nullptr);
    assert(ptr->getValue() == 42);
    std::cout << "智能指针测试通过" << std::endl;
}

// ==================== 并发测试 ====================

// 测试多线程高并发访问
void testHighConcurrency() {
    std::cout << "\n=== 测试多线程高并发访问 ===" << std::endl;
    
    const int numThreads = 8;
    const int opsPerThread = 100000;
    const uint32_t poolCapacity = numThreads * opsPerThread / 10; // 故意设置较小容量，增加竞争
    
    std_mem_pool::IndexedMemPool<int> pool(poolCapacity);
    std::atomic<int> totalAllocations{0};
    std::atomic<int> totalRecycles{0};
    std::atomic<int> failedAllocations{0};
    
    auto threadFunc = [&](int threadId) {
        std::vector<uint32_t> localIndices;
        std::mt19937 gen(threadId); // 确定性随机，但每个线程不同
        std::uniform_int_distribution<> alloc_or_free(0, 100);
        
        for (int i = 0; i < opsPerThread; ++i) {
            int op = alloc_or_free(gen);
            
            if (op < 60 || localIndices.empty()) { // 60%的几率分配
                uint32_t idx = pool.allocIndex(threadId * 1000000 + i);
                if (idx != 0) {
                    localIndices.push_back(idx);
                    totalAllocations++;
                } else {
                    failedAllocations++;
                }
            } else { // 40%的几率释放
                size_t index = gen() % localIndices.size();
                uint32_t idx = localIndices[index];
                pool.recycleIndex(idx);
                totalRecycles++;
                localIndices[index] = localIndices.back();
                localIndices.pop_back();
            }
        }
        
        // 清理所有本地分配的内存
        for (uint32_t idx : localIndices) {
            pool.recycleIndex(idx);
            totalRecycles++;
        }
    };
    
    Timer timer;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(threadFunc, i);
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    double elapsed = timer.elapsedMs();
    double totalOps = totalAllocations + totalRecycles;
    double opsPerSecond = totalOps / (elapsed / 1000.0);
    
    std::cout << "线程数: " << numThreads << std::endl;
    std::cout << "每线程操作数: " << opsPerThread << std::endl;
    std::cout << "总分配次数: " << totalAllocations << std::endl;
    std::cout << "总回收次数: " << totalRecycles << std::endl;
    std::cout << "分配失败次数: " << failedAllocations << std::endl;
    std::cout << "总操作数: " << totalOps << std::endl;
    std::cout << "总时间: " << elapsed << " ms" << std::endl;
    std::cout << "每秒操作数: " << opsPerSecond << " op/s" << std::endl;
}

// 测试并发竞争情况下的一致性
void testConcurrencyConsistency() {
    std::cout << "\n=== 测试并发竞争情况下的一致性 ===" << std::endl;
    
    const int numThreads = 4;
    const int itemsPerThread = 1000;
    const int iterations = 5;
    
    std_mem_pool::IndexedMemPool<std::atomic<int>> pool(numThreads * itemsPerThread);
    std::atomic<bool> startFlag{false};
    std::atomic<int> readyThreads{0};
    
    for (int iter = 0; iter < iterations; ++iter) {
        std::cout << "迭代 " << (iter + 1) << "/" << iterations << std::endl;
        
        std::vector<uint32_t> allIndices;
        std::mutex indicesMutex;
        
        auto threadFunc = [&](int threadId) {
            std::vector<uint32_t> localIndices;
            
            // 第一阶段：每个线程分配自己的一组对象
            for (int i = 0; i < itemsPerThread; ++i) {
                uint32_t idx = pool.allocIndex(0); // 初始值设为0
                if (idx != 0) {
                    pool[idx].store(threadId * 10000 + i, std::memory_order_relaxed);
                    localIndices.push_back(idx);
                }
            }
            
            // 同步所有线程
            readyThreads.fetch_add(1, std::memory_order_acq_rel);
            while (readyThreads.load(std::memory_order_acquire) < numThreads) {
                std::this_thread::yield();
            }
            
            // 等待开始信号
            while (!startFlag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            
            // 第二阶段：随机释放一半对象
            std::random_device rd;
            std::mt19937 gen(rd());
            std::shuffle(localIndices.begin(), localIndices.end(), gen);
            
            size_t halfSize = localIndices.size() / 2;
            for (size_t i = 0; i < halfSize; ++i) {
                pool.recycleIndex(localIndices[i]);
            }
            
            localIndices.erase(localIndices.begin(), localIndices.begin() + halfSize);
            
            // 第三阶段：尝试分配一些新对象
            for (int i = 0; i < itemsPerThread / 2; ++i) {
                uint32_t idx = pool.allocIndex(0);
                if (idx != 0) {
                    // 设置新值以标记这是第二轮分配的
                    pool[idx].store(threadId * 10000 + i + 50000, std::memory_order_relaxed);
                    localIndices.push_back(idx);
                }
            }
            
            // 保存所有索引以进行验证
            {
                std::lock_guard<std::mutex> lock(indicesMutex);
                allIndices.insert(allIndices.end(), localIndices.begin(), localIndices.end());
            }
        };
        
        // 重置同步标志
        readyThreads.store(0, std::memory_order_relaxed);
        startFlag.store(false, std::memory_order_relaxed);
        
        // 启动线程
        std::vector<std::thread> threads;
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back(threadFunc, i);
        }
        
        // 等待所有线程准备好
        while (readyThreads.load(std::memory_order_acquire) < numThreads) {
            std::this_thread::yield();
        }
        
        // 发出开始信号
        startFlag.store(true, std::memory_order_release);
        
        // 等待所有线程完成
        for (auto& thread : threads) {
            thread.join();
        }
        
        // 验证一致性
        std::cout << "验证分配的 " << allIndices.size() << " 个对象..." << std::endl;
        
        std::sort(allIndices.begin(), allIndices.end());
        auto it = std::unique(allIndices.begin(), allIndices.end());
        bool duplicatesFound = (it != allIndices.end());
        
        if (duplicatesFound) {
            std::cout << "错误：发现重复索引！" << std::endl;
            assert(false);
        } else {
            std::cout << "验证通过：没有重复索引" << std::endl;
        }
        
        // 回收所有索引，准备下一轮
        for (uint32_t idx : allIndices) {
            pool.recycleIndex(idx);
        }
    }
    
    std::cout << "并发一致性测试通过" << std::endl;
}

// ==================== 性能测试 ====================

// 测试内存池与普通new/delete的性能对比
template<typename T>
BenchmarkResult benchmarkPoolVsNewDelete(const std::string& name, int numOperations) {
    std::cout << "\n=== 性能测试：" << name << " ===" << std::endl;
    
    BenchmarkResult result;
    result.name = name;
    
    Timer timer;
    
    // 根据测试类型执行不同操作
    if (name.find("MemPool") != std::string::npos) {
        // 内存池测试
        std_mem_pool::IndexedMemPool<T> pool(numOperations);
        
        std::vector<uint32_t> indices;
        indices.reserve(numOperations);
        
        // 连续分配
        for (int i = 0; i < numOperations; ++i) {
            uint32_t idx = pool.allocIndex(i);
            if (idx != 0) {
                indices.push_back(idx);
            }
        }
        
        // 连续释放
        for (uint32_t idx : indices) {
            pool.recycleIndex(idx);
        }
        
        // 交替分配释放
        uint32_t idx = 0;
        for (int i = 0; i < numOperations; ++i) {
            idx = pool.allocIndex(i);
            if (idx != 0) {
                pool.recycleIndex(idx);
            }
        }
        
        // 估算内存使用（很粗略的估计）
        result.memoryUsage = sizeof(pool) + sizeof(T) * numOperations;
        
    } else if (name.find("New/Delete") != std::string::npos) {
        // new/delete测试
        std::vector<T*> pointers;
        pointers.reserve(numOperations);
        
        // 连续分配
        for (int i = 0; i < numOperations; ++i) {
            if constexpr (std::is_constructible_v<T, int>) {
                pointers.push_back(new T(i));
            } else {
                pointers.push_back(new T());
            }
        }
        
        // 连续释放
        for (T* ptr : pointers) {
            delete ptr;
        }
        pointers.clear();
        
        // 交替分配释放
        for (int i = 0; i < numOperations; ++i) {
            T* ptr = nullptr;
            if constexpr (std::is_constructible_v<T, int>) {
                ptr = new T(i);
            } else {
                ptr = new T();
            }
            delete ptr;
        }
        
        // 估算内存使用（很粗略的估计）
        result.memoryUsage = sizeof(T) * numOperations;
    }
    
    result.totalTimeMs = timer.elapsedMs();
    result.opsPerSecond = (numOperations * 3) / (result.totalTimeMs / 1000.0);
    result.avgTimePerOpUs = (result.totalTimeMs * 1000.0) / (numOperations * 3);
    
    return result;
}

// 性能测试套件
void runPerformanceTests() {
    std::cout << "\n==================== 性能测试 ====================" << std::endl;
    
    std::vector<BenchmarkResult> results;
    
    // 测试不同大小的分配
    const int smallOps = 1000000;  // 一百万次操作
    const int mediumOps = 100000;  // 十万次操作
    const int largeOps = 10000;    // 一万次操作
    
    // 小型对象测试
    results.push_back(benchmarkPoolVsNewDelete<int>("MemPool - 小型对象 (int)", smallOps));
    results.push_back(benchmarkPoolVsNewDelete<int>("New/Delete - 小型对象 (int)", smallOps));
    
    // 中型对象测试
    results.push_back(benchmarkPoolVsNewDelete<PerfTestData>("MemPool - 中型对象 (64字节)", mediumOps));
    results.push_back(benchmarkPoolVsNewDelete<PerfTestData>("New/Delete - 中型对象 (64字节)", mediumOps));
    
    // 大型对象测试
    results.push_back(benchmarkPoolVsNewDelete<TestObject>("MemPool - 大型对象 (TestObject)", largeOps));
    results.push_back(benchmarkPoolVsNewDelete<TestObject>("New/Delete - 大型对象 (TestObject)", largeOps));
    
    // 打印结果表格
    std::cout << "\n========== 性能测试结果 ==========" << std::endl;
    std::cout << std::setw(30) << std::left << "测试名称"
              << std::setw(15) << "总时间 (ms)"
              << std::setw(15) << "操作/秒 (K)"
              << std::setw(15) << "每操作时间 (μs)"
              << std::setw(15) << "内存使用 (KB)"
              << std::endl;
    std::cout << std::string(90, '-') << std::endl;
    
    for (const auto& result : results) {
        result.print();
    }
}

// ==================== 主函数 ====================

int main() {
    std::cout << "=======================================================" << std::endl;
    std::cout << "     StdIndexedMemPool 深度测试与性能基准测试程序     " << std::endl;
    std::cout << "=======================================================" << std::endl;
    
    // 功能测试
    testRandomAllocFree();
    testCapacityLimits();
    testLifecycleStrategies();
    testSmartPointerManagement();
    
    // 并发测试
    testHighConcurrency();
    testConcurrencyConsistency();
    
    // 性能测试
    runPerformanceTests();
    
    std::cout << "\n所有测试完成！" << std::endl;
    return 0;
} 