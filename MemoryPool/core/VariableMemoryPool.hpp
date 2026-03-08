#pragma once
#include <cstddef>
#include <cassert>
#include <cstdint>
#include "IMemoryPool.h"

// 可变大小内存池，支持分配/释放任意大小内存块
class VariableMemoryPool : public IMemoryPool {
public:
    explicit VariableMemoryPool(size_t total_size, size_t alignment = alignof(std::max_align_t))
        : m_total_size(total_size), m_alignment(alignment), m_raw_mem(nullptr), m_head(nullptr), m_used_size(0) {
        size_t alloc_size = total_size + alignment;
        m_raw_mem = ::operator new(alloc_size);
        uintptr_t raw_addr = reinterpret_cast<uintptr_t>(m_raw_mem);
        uintptr_t aligned_addr = (raw_addr + alignment - 1) & ~(alignment - 1);
        m_pool = reinterpret_cast<void*>(aligned_addr);
        // 初始化单个大空闲块
        m_head = reinterpret_cast<BlockHeader*>(m_pool);
        m_head->size = total_size;
        m_head->free = true;
        m_head->prev = nullptr;
        m_head->next = nullptr;
    }

    ~VariableMemoryPool() override {
        ::operator delete(m_raw_mem);
    }

    void* allocate(size_t size) override {
        if (size == 0) return nullptr;
        size = align_up(size, m_alignment);
        BlockHeader* curr = m_head;
        while (curr) {
            if (curr->free && curr->size >= size) {
                // 若剩余空间足够分割，拆分块
                if (curr->size >= size + sizeof(BlockHeader) + m_alignment) {
                    BlockHeader* next = reinterpret_cast<BlockHeader*>(reinterpret_cast<char*>(curr + 1) + size);
                    next->size = curr->size - size - sizeof(BlockHeader);
                    next->free = true;
                    next->prev = curr;
                    next->next = curr->next;
                    if (curr->next) curr->next->prev = next;
                    curr->next = next;
                    curr->size = size;
                }
                curr->free = false;
                m_used_size += curr->size;
                // 统计更新
                ++m_total_allocations;
                ++m_current_allocations;
                if (m_current_allocations > m_peak_allocations) m_peak_allocations = m_current_allocations;
                m_total_bytes_allocated += curr->size;
                if (m_used_size > m_peak_bytes_used) m_peak_bytes_used = m_used_size;
                return reinterpret_cast<void*>(curr + 1);
            }
            curr = curr->next;
        }
        return nullptr; // 无足够空间
    }

    void deallocate(void* ptr) override {
        if (!ptr) return;
        BlockHeader* block = reinterpret_cast<BlockHeader*>(ptr) - 1;
        if (!owns(ptr)) return;
        if (block->free) return; // 已经是空闲
        block->free = true;
        m_used_size -= block->size;
        // 统计更新
        ++m_total_deallocations;
        if (m_current_allocations > 0) --m_current_allocations;
        // 尝试与前后空闲块合并
        merge(block);
    }

    PoolStatistics get_statistics() const override {
        PoolStatistics stats{};
        stats.total_allocations = m_total_allocations;
        stats.total_deallocations = m_total_deallocations;
        stats.current_allocations = m_current_allocations;
        stats.peak_allocations = m_peak_allocations;
        stats.total_bytes_allocated = m_total_bytes_allocated;
        stats.current_bytes_used = m_used_size;
        stats.peak_bytes_used = m_peak_bytes_used;
        // 计算碎片率：总空闲块字节数 / (总空闲字节数)
        size_t free_bytes = 0, largest_free = 0;
        BlockHeader* curr = m_head;
        while (curr) {
            if (curr->free) {
                free_bytes += curr->size;
                if (curr->size > largest_free) largest_free = curr->size;
            }
            curr = curr->next;
        }
        stats.fragmentation_ratio = free_bytes == 0 ? 0.0 : 1.0 - (double)largest_free / (double)free_bytes;
        return stats;
    }

    void reset() override {
        m_head = reinterpret_cast<BlockHeader*>(m_pool);
        m_head->size = m_total_size;
        m_head->free = true;
        m_head->prev = nullptr;
        m_head->next = nullptr;
        m_used_size = 0;
        // 只重置当前状态，保留历史统计信息用于性能分析
        m_current_allocations = 0;
        // 注意：不重置峰值和总计数据，这些对于性能分析很重要
    }

    size_t get_total_size() const { return m_total_size; }
    size_t get_available_size() const { return m_total_size - m_used_size; }

    // 内存整理（合并相邻空闲块）
    void defragment() {
        BlockHeader* curr = m_head;
        while (curr && curr->next) {
            if (curr->free && curr->next->free) {
                BlockHeader* next = curr->next;
                curr->size += sizeof(BlockHeader) + next->size;
                curr->next = next->next;
                if (next->next) next->next->prev = curr;
            } else {
                curr = curr->next;
            }
        }
    }

    // 禁止拷贝和赋值
    VariableMemoryPool(const VariableMemoryPool&) = delete;
    VariableMemoryPool& operator=(const VariableMemoryPool&) = delete;

private:
    // 统计相关成员
    size_t m_total_allocations = 0;      // 总分配次数
    size_t m_total_deallocations = 0;    // 总释放次数
    size_t m_current_allocations = 0;    // 当前分配块数
    size_t m_peak_allocations = 0;       // 峰值分配块数
    size_t m_total_bytes_allocated = 0;  // 总分配字节数
    size_t m_peak_bytes_used = 0;        // 峰值使用字节数

    struct BlockHeader {
        size_t size;
        bool free;
        BlockHeader* prev;
        BlockHeader* next;
    };

    static size_t align_up(size_t n, size_t align) {
        return (n + align - 1) & ~(align - 1);
    }

    bool owns(void* ptr) const {
        uintptr_t pool_start = reinterpret_cast<uintptr_t>(m_pool);
        uintptr_t pool_end = pool_start + m_total_size;
        uintptr_t p = reinterpret_cast<uintptr_t>(ptr);
        return p >= pool_start && p < pool_end;
    }

    void merge(BlockHeader* block) {
        // 向前合并
        if (block->prev && block->prev->free) {
            block->prev->size += sizeof(BlockHeader) + block->size;
            block->prev->next = block->next;
            if (block->next) block->next->prev = block->prev;
            block = block->prev;
        }
        // 向后合并
        if (block->next && block->next->free) {
            block->size += sizeof(BlockHeader) + block->next->size;
            block->next = block->next->next;
            if (block->next) block->next->prev = block;
        }
    }

    size_t m_total_size;
    size_t m_alignment;
    void* m_raw_mem;   // 原始分配地址
    void* m_pool;      // 对齐后池起始地址
    BlockHeader* m_head; // 块链表头
    size_t m_used_size;
};
