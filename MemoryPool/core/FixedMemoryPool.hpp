#pragma once
#include <cstddef>
#include <cassert>
#include <cstdint>
#include "IMemoryPool.h"

// 固定大小对象的高性能内存池
// 支持自定义块数和对齐
template <typename T>
class FixedMemoryPool : public IMemoryPool {
public:
    explicit FixedMemoryPool(size_t block_count, size_t alignment = alignof(std::max_align_t))
        : m_block_count(block_count), m_pool(nullptr), m_free_list(nullptr), m_used_blocks(0) {
        assert(sizeof(T) >= sizeof(void*));
        size_t total_size = block_count * sizeof(T) + alignment;
        m_raw_mem = ::operator new(total_size);
        // 对齐起始地址
        uintptr_t raw_addr = reinterpret_cast<uintptr_t>(m_raw_mem);
        uintptr_t aligned_addr = (raw_addr + alignment - 1) & ~(alignment - 1);
        m_pool = reinterpret_cast<void*>(aligned_addr);
        // 初始化 free list
        m_free_list = reinterpret_cast<Node*>(m_pool);
        Node* curr = m_free_list;
        for (size_t i = 1; i < block_count; ++i) {
            Node* next = reinterpret_cast<Node*>(reinterpret_cast<char*>(m_pool) + i * sizeof(T));
            curr->next = next;
            curr = next;
        }
        curr->next = nullptr;
    }

    ~FixedMemoryPool() override {
        ::operator delete(m_raw_mem);
    }

    void* allocate(size_t size = sizeof(T)) override {
        assert(size <= sizeof(T) && "Requested size exceeds block size");
        if (!m_free_list) return nullptr;
        Node* node = m_free_list;
        m_free_list = node->next;
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
        
        return reinterpret_cast<void*>(node);
    }

    void deallocate(void* ptr) override {
        if (!owns(ptr)) return; // 非池内指针忽略
        Node* node = reinterpret_cast<Node*>(ptr);
        node->next = m_free_list;
        m_free_list = node;
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
        // 1. 重新构建空闲链表，恢复到初始状态
        m_free_list = reinterpret_cast<Node*>(m_pool);
        Node* curr = m_free_list;
        for (size_t i = 1; i < m_block_count; ++i) {
            Node* next = reinterpret_cast<Node*>(reinterpret_cast<char*>(m_pool) + i * sizeof(T));
            curr->next = next;
            curr = next;
        }
        curr->next = nullptr;
        
        // 2. 重置当前运行状态
        m_used_blocks = 0;
        
        // 3. 保留历史统计信息用于长期性能分析：
        // - m_total_allocations (总分配次数)
        // - m_total_deallocations (总释放次数) 
        // - m_peak_allocations (峰值分配数)
        // - m_total_bytes_allocated (总分配字节数)
        // - m_peak_bytes_used (峰值使用字节数)
        // 
        // 这些历史数据对于容量规划和性能监控很重要，不应重置
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
    FixedMemoryPool(const FixedMemoryPool&) = delete;
    FixedMemoryPool& operator=(const FixedMemoryPool&) = delete;

private:
    struct Node { Node* next; };
    size_t m_block_count;
    void* m_raw_mem;   // 原始分配地址（用于释放）
    void* m_pool;      // 对齐后的池起始地址
    Node* m_free_list; // 空闲链表头
    size_t m_used_blocks;
    size_t m_total_allocations = 0;
    size_t m_total_deallocations = 0;
    size_t m_peak_allocations = 0;
    size_t m_total_bytes_allocated = 0;
    size_t m_peak_bytes_used = 0;
};
