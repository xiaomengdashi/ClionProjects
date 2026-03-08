#pragma once
#include <cstddef>
#include <cassert>
#include <mutex>
#include <vector>
#include <algorithm>
#include "IMemoryPool.h"

/**
 * 线程安全的内存池包装器
 * 
 * 特性：
 * - 为任何内存池类型提供线程安全性
 * - 使用线程本地存储减少锁争用
 * - 支持全局和线程本地统计信息
 * 
 * 使用示例：
 * @code
 * // 创建线程安全的固定内存池
 * ThreadSafeMemoryPool<FixedMemoryPool<GameObject>> pool(1000);
 * 
 * // 在多线程中安全使用
 * void* ptr = pool.allocate(sizeof(GameObject));
 * if (ptr) {
 *     GameObject* obj = new(ptr) GameObject();
 *     // 使用对象...
 *     obj->~GameObject();
 *     pool.deallocate(ptr);
 * }
 * @endcode
 * 
 * @tparam PoolType 底层内存池类型，必须实现 IMemoryPool 接口
 */


template<typename PoolType>
class ThreadSafeMemoryPool :public IMemoryPool {
public:
    // 构造函数
    template<typename... Args>
    explicit ThreadSafeMemoryPool(Args&&... args)
        : m_central_pool(new PoolType(std::forward<Args>(args)...)), m_thread_local_pool_size(128) {}
    
    // 析构函数
    ~ThreadSafeMemoryPool() {
        delete m_central_pool;
        // 本地池由TLS自动释放
    }
    
    // 线程安全的内存分配
    void* allocate(size_t size) override {
        return get_local_pool()->allocate(size);
    }
    
    // 线程安全的内存释放
    void deallocate(void* ptr) override {
        get_local_pool()->deallocate(ptr);
    }
    
    // 获取统计信息（中央池+所有本地池）
    PoolStatistics get_statistics() const override {
        PoolStatistics stats = m_central_pool->get_statistics();
        std::lock_guard<std::mutex> lock(m_stats_mutex);
        for (const auto& pool : m_all_local_pools) {
            PoolStatistics s = pool->get_statistics();
            stats.total_allocations += s.total_allocations;
            stats.total_deallocations += s.total_deallocations;
            stats.current_allocations += s.current_allocations;
            stats.peak_allocations = std::max(stats.peak_allocations, s.peak_allocations);
            stats.total_bytes_allocated += s.total_bytes_allocated;
            stats.current_bytes_used += s.current_bytes_used;
            stats.peak_bytes_used = std::max(stats.peak_bytes_used, s.peak_bytes_used);
            stats.fragmentation_ratio = std::max(stats.fragmentation_ratio, s.fragmentation_ratio);
        }
        return stats;
    }
    
    // 重置内存池
    void reset() override {
        m_central_pool->reset();
        std::lock_guard<std::mutex> lock(m_stats_mutex);
        for (auto& pool : m_all_local_pools) pool->reset();
    }
    
    // 获取当前线程本地池统计信息
    PoolStatistics get_thread_local_statistics() const {
        return get_local_pool()->get_statistics();
    }
    
    // 设置线程本地池大小（仅影响新线程）
    void set_thread_local_pool_size(size_t size) {
        m_thread_local_pool_size = size;
    }

private:
    // 获取线程本地池
    PoolType* get_local_pool() const {
        thread_local PoolType* local_pool = nullptr;
        if (!local_pool) {
            std::lock_guard<std::mutex> lock(m_stats_mutex);
            local_pool = new PoolType(m_thread_local_pool_size);
            m_all_local_pools.push_back(local_pool);
            // 补货机制可扩展：本地池耗尽时从中央池批量获取
        }
        return local_pool;
    }

    PoolType* m_central_pool;
    mutable std::mutex m_stats_mutex;
    mutable std::vector<PoolType*> m_all_local_pools;
    size_t m_thread_local_pool_size;
};