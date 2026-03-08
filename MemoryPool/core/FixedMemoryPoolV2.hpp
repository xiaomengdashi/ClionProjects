#pragma once
#include <cstddef>
#include <cassert>
#include <cstdint>
#include <vector>
#include "IMemoryPool.h"

// 固定大小对象的高性能内存池 V2
// 使用独立链表管理空闲块，支持任意大小的类型
// T: 对象类型，无大小限制

template <typename T>
class FixedMemoryPoolV2 : public IMemoryPool {
public:
    explicit FixedMemoryPoolV2(size_t block_count, size_t alignment = alignof(T))
        : m_block_count(block_count), m_alignment(alignment), m_pool(nullptr), m_used_blocks(0) {
        
        // 分配内存池
        size_t total_size = block_count * sizeof(T) + alignment;
        m_raw_mem = ::operator new(total_size);
        
        // 对齐起始地址
        uintptr_t raw_addr = reinterpret_cast<uintptr_t>(m_raw_mem);
        uintptr_t aligned_addr = (raw_addr + alignment - 1) & ~(alignment - 1);
        m_pool = reinterpret_cast<void*>(aligned_addr);
        
        // 初始化空闲块索引列表
        // 所有块初始都是空闲的，索引从0到block_count-1
        m_free_indices.reserve(block_count);
        for (size_t i = 0; i < block_count; ++i) {
            m_free_indices.push_back(i);
        }
    }

    ~FixedMemoryPoolV2() override {
        ::operator delete(m_raw_mem);
    }

    void* allocate(size_t size = sizeof(T)) override {
        assert(size <= sizeof(T) && "Requested size exceeds block size");
        
        if (m_free_indices.empty()) {
            return nullptr; // 无可用块
        }
        
        // 获取一个空闲块的索引
        size_t block_index = m_free_indices.back();
        m_free_indices.pop_back();
        
        ++m_used_blocks;
        
        // 更新统计信息
        ++m_total_allocations;
        m_total_bytes_allocated += sizeof(T);
        if (m_used_blocks > m_peak_allocations) {
            m_peak_allocations = m_used_blocks;
        }
        size_t current_bytes = m_used_blocks * sizeof(T);
        if (current_bytes > m_peak_bytes_used) {
            m_peak_bytes_used = current_bytes;
        }
        
        // 计算块地址
        void* block_ptr = reinterpret_cast<char*>(m_pool) + block_index * sizeof(T);
        return block_ptr;
    }

    void deallocate(void* ptr) override {
        if (!owns(ptr)) return; // 非池内指针忽略
        
        // 计算块索引
        uintptr_t pool_start = reinterpret_cast<uintptr_t>(m_pool);
        uintptr_t p = reinterpret_cast<uintptr_t>(ptr);
        size_t block_index = (p - pool_start) / sizeof(T);
        
        // 将块索引加回空闲列表
        m_free_indices.push_back(block_index);
        --m_used_blocks;
        
        // 更新统计信息
        ++m_total_deallocations;
    }

    PoolStatistics get_statistics() const override {
        PoolStatistics stats{};
        stats.total_allocations = m_total_allocations;
        stats.total_deallocations = m_total_deallocations;
        stats.current_allocations = m_used_blocks;
        stats.peak_allocations = m_peak_allocations;
        stats.total_bytes_allocated = m_total_bytes_allocated;
        stats.current_bytes_used = m_used_blocks * sizeof(T);
        stats.peak_bytes_used = m_peak_bytes_used;
        // 固定池碎片率为0（无碎片）
        stats.fragmentation_ratio = 0.0;
        return stats;
    }

    void reset() override {
        // 重新初始化空闲块索引列表
        m_free_indices.clear();
        m_free_indices.reserve(m_block_count);
        for (size_t i = 0; i < m_block_count; ++i) {
            m_free_indices.push_back(i);
        }
        m_used_blocks = 0;
        
        // 重置统计信息（但保留历史峰值和总计数）
        // 只重置当前状态，不清除历史统计
    }

    static constexpr size_t get_block_size() { return sizeof(T); }
    size_t get_block_count() const { return m_block_count; }
    size_t get_available_blocks() const { return m_block_count - m_used_blocks; }

    bool owns(void* ptr) const {
        uintptr_t pool_start = reinterpret_cast<uintptr_t>(m_pool);
        uintptr_t pool_end = pool_start + m_block_count * sizeof(T);
        uintptr_t p = reinterpret_cast<uintptr_t>(ptr);
        return p >= pool_start && p < pool_end && ((p - pool_start) % sizeof(T) == 0);
    }

    // 禁止拷贝和赋值
    FixedMemoryPoolV2(const FixedMemoryPoolV2&) = delete;
    FixedMemoryPoolV2& operator=(const FixedMemoryPoolV2&) = delete;

private:
    size_t m_block_count;
    size_t m_alignment;
    void* m_raw_mem;                    // 原始分配地址（用于释放）
    void* m_pool;                       // 对齐后的池起始地址
    std::vector<size_t> m_free_indices; // 空闲块索引列表
    size_t m_used_blocks;
    
    // 统计信息
    size_t m_total_allocations = 0;
    size_t m_total_deallocations = 0;
    size_t m_peak_allocations = 0;
    size_t m_total_bytes_allocated = 0;
    size_t m_peak_bytes_used = 0;
};