#include "../core/IndexedMemoryPool.hpp"
#include "../core/VariableMemoryPool.hpp"
#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <iomanip>

// 性能测试框架
class PerformanceTest {
public:
    static void run_allocation_benchmark() {
        std::cout << "=== 内存分配性能对比测试 ===\n\n";
        
        const size_t pool_size = 10 * 1024 * 1024; // 10MB
        const size_t test_iterations = 10000;
        
        // 测试不同大小的分配请求
        std::vector<size_t> test_sizes = {16, 64, 256, 1024, 4096, 16384};
        
        for (size_t size : test_sizes) {
            std::cout << "测试分配大小: " << size << " 字节\n";
            
            // 测试原始内存池
            auto original_time = test_original_pool(pool_size, size, test_iterations);
            
            // 测试索引内存池
            auto indexed_time = test_indexed_pool(pool_size, size, test_iterations);
            
            // 计算性能提升
            double improvement = (double)original_time / indexed_time;
            
            std::cout << "  原始内存池: " << original_time << " ms\n";
            std::cout << "  索引内存池: " << indexed_time << " ms\n";
            std::cout << "  性能提升: " << std::fixed << std::setprecision(2) 
                      << improvement << "x\n\n";
        }
    }
    
    static void run_fragmentation_test() {
        std::cout << "=== 内存碎片化测试 ===\n\n";
        
        const size_t pool_size = 1024 * 1024; // 1MB
        IndexedMemoryPool pool(pool_size);
        
        // 分配各种大小的块
        std::vector<void*> ptrs;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> size_dist(16, 1024);
        
        // 分配阶段
        std::cout << "分配随机大小的内存块...\n";
        while (ptrs.size() < 500) {
            size_t size = size_dist(gen);
            void* ptr = pool.allocate(size);
            if (ptr) {
                ptrs.push_back(ptr);
            } else {
                break;
            }
        }
        
        auto stats = pool.get_statistics();
        std::cout << "分配了 " << ptrs.size() << " 个块\n";
        std::cout << "已使用内存: " << stats.current_bytes_used << " 字节\n";
        std::cout << "碎片率: " << std::fixed << std::setprecision(2) 
                  << (stats.fragmentation_ratio * 100) << "%\n\n";
        
        // 随机释放一半的块
        std::cout << "随机释放50%的块...\n";
        std::shuffle(ptrs.begin(), ptrs.end(), gen);
        for (size_t i = 0; i < ptrs.size() / 2; ++i) {
            pool.deallocate(ptrs[i]);
        }
        ptrs.erase(ptrs.begin(), ptrs.begin() + ptrs.size() / 2);
        
        stats = pool.get_statistics();
        std::cout << "剩余 " << ptrs.size() << " 个块\n";
        std::cout << "已使用内存: " << stats.current_bytes_used << " 字节\n";
        std::cout << "碎片率: " << std::fixed << std::setprecision(2) 
                  << (stats.fragmentation_ratio * 100) << "%\n\n";
        
        // 显示索引统计
        auto index_stats = pool.get_index_statistics();
        std::cout << "索引统计:\n";
        std::cout << "  大小类别数: " << index_stats.total_size_classes << "\n";
        std::cout << "  空闲块总数: " << index_stats.total_free_blocks << "\n";
        std::cout << "  平均每类块数: " << std::fixed << std::setprecision(1) 
                  << index_stats.average_blocks_per_class << "\n\n";
        
        // 清理
        for (void* ptr : ptrs) {
            pool.deallocate(ptr);
        }
    }
    
    static void run_index_efficiency_test() {
        std::cout << "=== 索引效率测试 ===\n\n";
        
        IndexedMemoryPool pool(10 * 1024 * 1024); // 10MB
        
        // 创建大量碎片
        std::vector<void*> ptrs;
        for (size_t i = 0; i < 1000; ++i) {
            void* ptr = pool.allocate(64 + (i % 7) * 8); // 创建不同大小的块
            if (ptr) ptrs.push_back(ptr);
        }
        
        // 释放每3个块中的1个，创建碎片
        for (size_t i = 0; i < ptrs.size(); i += 3) {
            pool.deallocate(ptrs[i]);
            ptrs[i] = nullptr;
        }
        
        auto index_stats = pool.get_index_statistics();
        std::cout << "创建碎片后的索引状态:\n";
        std::cout << "  大小类别数: " << index_stats.total_size_classes << "\n";
        std::cout << "  空闲块总数: " << index_stats.total_free_blocks << "\n";
        
        // 测试分配性能
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<void*> new_ptrs;
        for (size_t i = 0; i < 100; ++i) {
            void* ptr = pool.allocate(128); // 分配中等大小的块
            if (ptr) new_ptrs.push_back(ptr);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "在碎片化环境中分配100个块耗时: " << duration.count() << " 微秒\n";
        std::cout << "平均每次分配: " << std::fixed << std::setprecision(2) 
                  << (double)duration.count() / 100 << " 微秒\n\n";
        
        // 显示大小类别分布
        std::cout << "大小类别分布:\n";
        for (const auto& pair : index_stats.size_class_distribution) {
            if (pair.second > 0) {
                std::cout << "  " << pair.first << " 字节类别: " << pair.second << " 个块\n";
            }
        }
        
        // 清理
        for (void* ptr : ptrs) {
            if (ptr) pool.deallocate(ptr);
        }
        for (void* ptr : new_ptrs) {
            pool.deallocate(ptr);
        }
    }

private:
    static long long test_original_pool(size_t pool_size, size_t alloc_size, size_t iterations) {
        VariableMemoryPool pool(pool_size);
        std::vector<void*> ptrs;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // 分配测试
        for (size_t i = 0; i < iterations; ++i) {
            void* ptr = pool.allocate(alloc_size);
            if (ptr) {
                ptrs.push_back(ptr);
            }
            
            // 每100次分配后释放一些，创建碎片
            if (i % 100 == 99 && ptrs.size() > 50) {
                for (size_t j = 0; j < 10 && !ptrs.empty(); ++j) {
                    pool.deallocate(ptrs.back());
                    ptrs.pop_back();
                }
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        
        // 清理
        for (void* ptr : ptrs) {
            pool.deallocate(ptr);
        }
        
        return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    }
    
    static long long test_indexed_pool(size_t pool_size, size_t alloc_size, size_t iterations) {
        IndexedMemoryPool pool(pool_size);
        std::vector<void*> ptrs;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // 分配测试
        for (size_t i = 0; i < iterations; ++i) {
            void* ptr = pool.allocate(alloc_size);
            if (ptr) {
                ptrs.push_back(ptr);
            }
            
            // 每100次分配后释放一些，创建碎片
            if (i % 100 == 99 && ptrs.size() > 50) {
                for (size_t j = 0; j < 10 && !ptrs.empty(); ++j) {
                    pool.deallocate(ptrs.back());
                    ptrs.pop_back();
                }
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        
        // 清理
        for (void* ptr : ptrs) {
            pool.deallocate(ptr);
        }
        
        return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    }
};

int main() {
    std::cout << "多级索引内存池性能测试\n";
    std::cout << "========================\n\n";
    
    try {
        // 运行各种性能测试
        PerformanceTest::run_allocation_benchmark();
        PerformanceTest::run_fragmentation_test();
        PerformanceTest::run_index_efficiency_test();
        
        std::cout << "所有测试完成！\n";
        
    } catch (const std::exception& e) {
        std::cerr << "测试过程中发生错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}