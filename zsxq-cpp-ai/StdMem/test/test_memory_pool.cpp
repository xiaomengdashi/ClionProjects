#include "../src/memory_pool.h"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <random>
#include <cassert>
#include <iomanip>

/**
 * 测试辅助类：用于验证内存池的正确性和性能
 */
class MemoryPoolTester {
public:
    /**
     * 运行所有测试
     */
    static void runAllTests() {
        std::cout << "=== 内存池测试开始 ===" << std::endl;
        
        testBasicFunctionality();
        testThreadSafety();
        testPerformance();
        testBoundaryConditions();
        
        std::cout << "=== 所有测试通过！ ===" << std::endl;
    }
    
    /**
     * 打印内存池统计信息
     */
    static void printStatistics(const MemoryPool& pool) {
        auto stats = pool.getStatistics();
        
        std::cout << "\n--- 内存池统计信息 ---" << std::endl;
        std::cout << "总内存: " << stats.total_memory << " 字节" << std::endl;
        std::cout << "已用内存: " << stats.used_memory << " 字节" << std::endl;
        std::cout << "空闲内存: " << stats.free_memory << " 字节" << std::endl;
        std::cout << "总块数: " << stats.total_blocks << std::endl;
        std::cout << "已用块数: " << stats.used_blocks << std::endl;
        std::cout << "空闲块数: " << stats.free_blocks << std::endl;
        std::cout << "分配次数: " << stats.allocation_count << std::endl;
        std::cout << "释放次数: " << stats.deallocation_count << std::endl;
        std::cout << "内存利用率: " << std::fixed << std::setprecision(2) 
                  << (stats.total_memory > 0 ? (double)stats.used_memory / stats.total_memory * 100 : 0) 
                  << "%" << std::endl;
    }

private:
    /**
     * 基本功能测试
     */
    static void testBasicFunctionality() {
        std::cout << "\n[测试1] 基本功能测试..." << std::endl;
        
        // 创建内存池：32字节块，100个块
        MemoryPool pool(32, 100);
        
        // 测试分配
        std::vector<void*> ptrs;
        for (int i = 0; i < 50; ++i) {
            void* ptr = pool.allocate(16);  // 分配16字节
            assert(ptr != nullptr);
            ptrs.push_back(ptr);
            
            // 在分配的内存中写入数据，验证可写性
            *static_cast<int*>(ptr) = i;
        }
        
        // 验证写入的数据
        for (int i = 0; i < 50; ++i) {
            assert(*static_cast<int*>(ptrs[i]) == i);
        }
        
        // 获取统计信息
        auto stats = pool.getStatistics();
        assert(stats.used_blocks == 50);
        assert(stats.free_blocks == 50);
        
        // 测试释放
        for (void* ptr : ptrs) {
            assert(pool.deallocate(ptr));
        }
        
        // 验证释放后的状态
        assert(pool.isEmpty());
        stats = pool.getStatistics();
        assert(stats.used_blocks == 0);
        assert(stats.free_blocks == 100);
        
        std::cout << "  ✓ 基本分配和释放功能正常" << std::endl;
        std::cout << "  ✓ 统计信息正确" << std::endl;
    }
    
    /**
     * 线程安全测试
     */
    static void testThreadSafety() {
        std::cout << "\n[测试2] 线程安全测试..." << std::endl;
        
        const int NUM_THREADS = 8;
        const int OPERATIONS_PER_THREAD = 1000;
        
        MemoryPool pool(64, 2000);  // 足够大的内存池
        
        std::vector<std::thread> threads;
        std::atomic<int> successful_allocations(0);
        std::atomic<int> successful_deallocations(0);
        
        // 启动多个线程进行并发分配和释放
        for (int t = 0; t < NUM_THREADS; ++t) {
            threads.emplace_back([&pool, &successful_allocations, &successful_deallocations]() {
                std::vector<void*> local_ptrs;
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> dis(1, 64);
                
                // 执行分配操作
                for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
                    void* ptr = pool.allocate(dis(gen));
                    if (ptr) {
                        local_ptrs.push_back(ptr);
                        successful_allocations.fetch_add(1);
                        
                        // 写入数据验证内存可用性
                        *static_cast<int*>(ptr) = i;
                    }
                }
                
                // 随机释放一些内存
                for (size_t i = 0; i < local_ptrs.size(); i += 2) {
                    if (pool.deallocate(local_ptrs[i])) {
                        successful_deallocations.fetch_add(1);
                    }
                }
            });
        }
        
        // 等待所有线程完成
        for (auto& t : threads) {
            t.join();
        }
        
        auto stats = pool.getStatistics();
        
        std::cout << "  ✓ 成功分配: " << successful_allocations.load() << " 次" << std::endl;
        std::cout << "  ✓ 成功释放: " << successful_deallocations.load() << " 次" << std::endl;
        std::cout << "  ✓ 当前使用块数: " << stats.used_blocks << std::endl;
        std::cout << "  ✓ 线程安全测试通过" << std::endl;
    }
    
    /**
     * 性能测试
     */
    static void testPerformance() {
        std::cout << "\n[测试3] 性能测试..." << std::endl;
        
        const int NUM_ALLOCATIONS = 100000;
        const size_t BLOCK_SIZE = 32;
        
        // 测试内存池分配性能
        {
            MemoryPool pool(BLOCK_SIZE, NUM_ALLOCATIONS);
            std::vector<void*> ptrs;
            ptrs.reserve(NUM_ALLOCATIONS);
            
            auto start = std::chrono::high_resolution_clock::now();
            
            // 分配测试
            for (int i = 0; i < NUM_ALLOCATIONS; ++i) {
                void* ptr = pool.allocate(BLOCK_SIZE);
                if (ptr) {
                    ptrs.push_back(ptr);
                }
            }
            
            auto alloc_end = std::chrono::high_resolution_clock::now();
            
            // 释放测试
            for (void* ptr : ptrs) {
                pool.deallocate(ptr);
            }
            
            auto dealloc_end = std::chrono::high_resolution_clock::now();
            
            auto alloc_time = std::chrono::duration_cast<std::chrono::microseconds>(alloc_end - start);
            auto dealloc_time = std::chrono::duration_cast<std::chrono::microseconds>(dealloc_end - alloc_end);
            
            std::cout << "  ✓ 内存池分配 " << NUM_ALLOCATIONS << " 次耗时: " 
                      << alloc_time.count() << " 微秒" << std::endl;
            std::cout << "  ✓ 内存池释放 " << NUM_ALLOCATIONS << " 次耗时: " 
                      << dealloc_time.count() << " 微秒" << std::endl;
        }
        
        // 对比标准malloc/free性能
        {
            std::vector<void*> ptrs;
            ptrs.reserve(NUM_ALLOCATIONS);
            
            auto start = std::chrono::high_resolution_clock::now();
            
            // malloc分配测试
            for (int i = 0; i < NUM_ALLOCATIONS; ++i) {
                void* ptr = std::malloc(BLOCK_SIZE);
                if (ptr) {
                    ptrs.push_back(ptr);
                }
            }
            
            auto alloc_end = std::chrono::high_resolution_clock::now();
            
            // free释放测试
            for (void* ptr : ptrs) {
                std::free(ptr);
            }
            
            auto dealloc_end = std::chrono::high_resolution_clock::now();
            
            auto alloc_time = std::chrono::duration_cast<std::chrono::microseconds>(alloc_end - start);
            auto dealloc_time = std::chrono::duration_cast<std::chrono::microseconds>(dealloc_end - alloc_end);
            
            std::cout << "  ✓ 标准malloc分配 " << NUM_ALLOCATIONS << " 次耗时: " 
                      << alloc_time.count() << " 微秒" << std::endl;
            std::cout << "  ✓ 标准free释放 " << NUM_ALLOCATIONS << " 次耗时: " 
                      << dealloc_time.count() << " 微秒" << std::endl;
        }
    }
    
    /**
     * 边界条件测试
     */
    static void testBoundaryConditions() {
        std::cout << "\n[测试4] 边界条件测试..." << std::endl;
        
        // 测试小内存池
        {
            MemoryPool small_pool(16, 2);  // 只有2个块
            
            void* ptr1 = small_pool.allocate(8);
            void* ptr2 = small_pool.allocate(8);
            void* ptr3 = small_pool.allocate(8);  // 这个应该触发扩展
            
            assert(ptr1 != nullptr);
            assert(ptr2 != nullptr);
            assert(ptr3 != nullptr);  // 应该通过扩展成功分配
            
            assert(small_pool.deallocate(ptr1));
            assert(small_pool.deallocate(ptr2));
            assert(small_pool.deallocate(ptr3));
            
            std::cout << "  ✓ 小内存池扩展功能正常" << std::endl;
        }
        
        // 测试无效指针释放
        {
            MemoryPool pool(32, 10);
            
            // 尝试释放nullptr
            assert(!pool.deallocate(nullptr));
            
            // 尝试释放无效指针
            int dummy_var;
            assert(!pool.deallocate(&dummy_var));
            
            std::cout << "  ✓ 无效指针释放处理正确" << std::endl;
        }
        
        // 测试超大块分配
        {
            MemoryPool pool(32, 10);
            
            // 尝试分配超过块大小的内存
            void* ptr = pool.allocate(100);  // 超过32字节
            assert(ptr == nullptr);  // 应该失败
            
            std::cout << "  ✓ 超大块分配正确拒绝" << std::endl;
        }
        
        // 测试重复释放
        {
            MemoryPool pool(32, 10);
            
            void* ptr = pool.allocate(16);
            assert(ptr != nullptr);
            
            assert(pool.deallocate(ptr));   // 第一次释放应该成功
            assert(!pool.deallocate(ptr));  // 第二次释放应该失败
            
            std::cout << "  ✓ 重复释放检测正常" << std::endl;
        }
    }
};

/**
 * 演示程序：展示内存池的基本使用方法
 */
void demonstrateUsage() {
    std::cout << "\n=== 内存池使用演示 ===" << std::endl;
    
    // 创建一个内存池：64字节块，初始100个块
    MemoryPool pool(64, 100);
    
    std::cout << "创建内存池：块大小64字节，初始100个块" << std::endl;
    
    // 分配一些内存
    std::vector<void*> ptrs;
    for (int i = 0; i < 10; ++i) {
        void* ptr = pool.allocate(32);  // 分配32字节
        if (ptr) {
            ptrs.push_back(ptr);
            // 写入一些数据
            *static_cast<int*>(ptr) = i * 100;
        }
    }
    
    std::cout << "分配了 " << ptrs.size() << " 个内存块" << std::endl;
    MemoryPoolTester::printStatistics(pool);
    
    // 释放部分内存
    for (size_t i = 0; i < ptrs.size(); i += 2) {
        pool.deallocate(ptrs[i]);
    }
    
    std::cout << "\n释放了一半的内存块" << std::endl;
    MemoryPoolTester::printStatistics(pool);
    
    // 重置内存池
    pool.reset();
    std::cout << "\n重置内存池后" << std::endl;
    MemoryPoolTester::printStatistics(pool);
}

/**
 * 主函数
 */
int main() {
    try {
        // 运行所有测试
        MemoryPoolTester::runAllTests();
        
        // 运行演示程序
        demonstrateUsage();
        
        std::cout << "\n程序执行完成！" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}