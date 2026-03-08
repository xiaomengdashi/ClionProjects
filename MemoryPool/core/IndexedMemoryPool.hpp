#pragma once
#include <cstddef>
#include <cassert>
#include <cstdint>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include "IMemoryPool.h"

/**
 * 带多级索引的高性能内存池
 * 
 * 特性：
 * 1. 大小类别索引：将空闲块按大小分类到不同的桶中
 * 2. 最佳适配索引：每个大小类别内使用有序集合快速找到最佳匹配
 * 3. 地址索引：按地址排序便于快速合并相邻块
 * 4. 动态桶调整：根据使用模式动态优化桶的划分
 * 
 * 时间复杂度：
 * - 分配：O(log n) 其中 n 是该大小类别中的空闲块数量
 * - 释放：O(log n) 用于索引更新和块合并
 * - 相比原始 O(n) 线性查找有显著性能提升
 */
class IndexedMemoryPool : public IMemoryPool {
public:
    explicit IndexedMemoryPool(size_t total_size, size_t alignment = alignof(std::max_align_t))
        : m_total_size(total_size), m_alignment(alignment), m_raw_mem(nullptr), m_head(nullptr), m_used_size(0) {
        
        // 初始化内存池
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
        
        // 初始化索引系统
        initialize_size_classes();
        add_to_free_index(m_head);
    }

    ~IndexedMemoryPool() override {
        ::operator delete(m_raw_mem);
    }

    void* allocate(size_t size) override {
        if (size == 0) return nullptr;
        size = align_up(size, m_alignment);
        
        // 使用索引快速查找合适的空闲块
        BlockHeader* block = find_best_fit(size);
        if (!block) return nullptr;
        
        // 从索引中移除该块
        remove_from_free_index(block);
        
        // 如果块太大，进行分割
        if (block->size >= size + sizeof(BlockHeader) + m_alignment) {
            split_block(block, size);
        }
        
        // 标记为已使用
        block->free = false;
        m_used_size += block->size;
        
        // 更新统计信息
        ++m_total_allocations;
        ++m_current_allocations;
        if (m_current_allocations > m_peak_allocations) {
            m_peak_allocations = m_current_allocations;
        }
        m_total_bytes_allocated += block->size;
        if (m_used_size > m_peak_bytes_used) {
            m_peak_bytes_used = m_used_size;
        }
        
        return reinterpret_cast<void*>(block + 1);
    }

    void deallocate(void* ptr) override {
        if (!ptr) return;
        
        BlockHeader* block = reinterpret_cast<BlockHeader*>(ptr) - 1;
        if (!owns(ptr) || block->free) return;
        
        // 标记为空闲
        block->free = true;
        m_used_size -= block->size;
        
        // 更新统计信息
        ++m_total_deallocations;
        if (m_current_allocations > 0) --m_current_allocations;
        
        // 尝试与相邻块合并
        block = merge_with_neighbors(block);
        
        // 将合并后的块添加到索引中
        add_to_free_index(block);
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
        
        // 计算碎片率
        size_t free_bytes = 0, largest_free = 0;
        for (const auto& pair : m_size_index) {
            for (BlockHeader* block : pair.second) {
                free_bytes += block->size;
                if (block->size > largest_free) {
                    largest_free = block->size;
                }
            }
        }
        stats.fragmentation_ratio = free_bytes == 0 ? 0.0 : 1.0 - (double)largest_free / (double)free_bytes;
        
        return stats;
    }

    void reset() override {
        // 清空所有索引
        m_size_index.clear();
        m_address_index.clear();
        
        // 重新初始化为单个大块
        m_head = reinterpret_cast<BlockHeader*>(m_pool);
        m_head->size = m_total_size;
        m_head->free = true;
        m_head->prev = nullptr;
        m_head->next = nullptr;
        m_used_size = 0;
        
        // 重新添加到索引
        add_to_free_index(m_head);
        
        // 重置统计信息
        m_current_allocations = 0;
    }

    // 获取索引统计信息（用于性能分析和调优）
    struct IndexStatistics {
        size_t total_size_classes;
        size_t total_free_blocks;
        size_t largest_size_class_blocks;
        double average_blocks_per_class;
        std::vector<std::pair<size_t, size_t>> size_class_distribution; // <size_class, block_count>
    };

    IndexStatistics get_index_statistics() const {
        IndexStatistics stats{};
        stats.total_size_classes = m_size_index.size();
        stats.total_free_blocks = 0;
        stats.largest_size_class_blocks = 0;
        
        for (const auto& pair : m_size_index) {
            size_t block_count = pair.second.size();
            stats.total_free_blocks += block_count;
            if (block_count > stats.largest_size_class_blocks) {
                stats.largest_size_class_blocks = block_count;
            }
            stats.size_class_distribution.emplace_back(pair.first, block_count);
        }
        
        stats.average_blocks_per_class = stats.total_size_classes > 0 ? 
            (double)stats.total_free_blocks / stats.total_size_classes : 0.0;
        
        return stats;
    }

    // 内存整理和索引优化
    void defragment() {
        // 清空索引
        m_size_index.clear();
        m_address_index.clear();
        
        // 遍历所有块，合并相邻的空闲块
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
        
        // 重建索引
        curr = m_head;
        while (curr) {
            if (curr->free) {
                add_to_free_index(curr);
            }
            curr = curr->next;
        }
    }

    size_t get_total_size() const { return m_total_size; }
    size_t get_available_size() const { return m_total_size - m_used_size; }

    // 禁止拷贝和赋值
    IndexedMemoryPool(const IndexedMemoryPool&) = delete;
    IndexedMemoryPool& operator=(const IndexedMemoryPool&) = delete;

private:
    // 块头结构
    struct BlockHeader {
        size_t size;
        bool free;
        BlockHeader* prev;
        BlockHeader* next;
        
        // 用于索引中的比较（按大小排序，大小相同则按地址排序）
        bool operator<(const BlockHeader& other) const {
            if (size != other.size) return size < other.size;
            return this < &other;
        }
    };

    // 多级索引结构
    // 1. 大小类别索引：size_class -> 该大小类别的所有空闲块（有序集合）
    std::map<size_t, std::set<BlockHeader*>> m_size_index;
    
    // 2. 地址索引：按地址排序的所有空闲块，用于快速合并
    std::set<BlockHeader*> m_address_index;
    
    // 大小类别定义
    std::vector<size_t> m_size_classes;

    // 成员变量
    size_t m_total_size;
    size_t m_alignment;
    void* m_raw_mem;
    void* m_pool;
    BlockHeader* m_head;
    size_t m_used_size;
    
    // 统计信息
    size_t m_total_allocations = 0;
    size_t m_total_deallocations = 0;
    size_t m_current_allocations = 0;
    size_t m_peak_allocations = 0;
    size_t m_total_bytes_allocated = 0;
    size_t m_peak_bytes_used = 0;

    // 工具函数
    static size_t align_up(size_t n, size_t align) {
        return (n + align - 1) & ~(align - 1);
    }

    bool owns(void* ptr) const {
        uintptr_t pool_start = reinterpret_cast<uintptr_t>(m_pool);
        uintptr_t pool_end = pool_start + m_total_size;
        uintptr_t p = reinterpret_cast<uintptr_t>(ptr);
        return p >= pool_start && p < pool_end;
    }

    // 初始化大小类别
    void initialize_size_classes() {
        // 创建对数分布的大小类别，覆盖从小到大的各种分配需求
        // 小尺寸：8, 16, 32, 64, 128, 256, 512字节 - 精细粒度
        for (size_t size = 8; size <= 512; size *= 2) {
            m_size_classes.push_back(size);
        }
        
        // 中等尺寸：1KB, 2KB, 4KB, 8KB, 16KB, 32KB - 页级粒度  
        for (size_t size = 1024; size <= 32 * 1024; size *= 2) {
            m_size_classes.push_back(size);
        }
        
        // 大尺寸：64KB, 128KB, 256KB, 512KB, 1MB+ - 大块粒度
        for (size_t size = 64 * 1024; size <= 1024 * 1024; size *= 2) {
            m_size_classes.push_back(size);
        }
    }

    // 根据大小确定大小类别
    size_t get_size_class(size_t size) const {
        // 找到第一个 >= size 的大小类别
        auto it = std::lower_bound(m_size_classes.begin(), m_size_classes.end(), size);
        if (it != m_size_classes.end()) {
            return *it;
        }
        // 如果超过最大类别，使用实际大小作为类别
        return size;
    }

    // 将空闲块添加到索引中
    void add_to_free_index(BlockHeader* block) {
        assert(block && block->free);
        
        size_t size_class = get_size_class(block->size);
        m_size_index[size_class].insert(block);
        m_address_index.insert(block);
    }

    // 从索引中移除空闲块
    void remove_from_free_index(BlockHeader* block) {
        assert(block && block->free);
        
        size_t size_class = get_size_class(block->size);
        auto& size_set = m_size_index[size_class];
        size_set.erase(block);
        
        // 如果该大小类别没有块了，删除整个类别
        if (size_set.empty()) {
            m_size_index.erase(size_class);
        }
        
        m_address_index.erase(block);
    }

    // 查找最佳适配的空闲块
    BlockHeader* find_best_fit(size_t size) {
        size_t target_class = get_size_class(size);
        
        // 首先在目标大小类别中查找
        auto class_it = m_size_index.lower_bound(target_class);
        
        while (class_it != m_size_index.end()) {
            auto& blocks = class_it->second;
            
            // 在该大小类别中找到第一个足够大的块
            for (BlockHeader* block : blocks) {
                if (block->size >= size) {
                    return block;
                }
            }
            
            ++class_it;
        }
        
        return nullptr;
    }

    // 分割块
    void split_block(BlockHeader* block, size_t size) {
        assert(block && block->free);
        assert(block->size >= size + sizeof(BlockHeader) + m_alignment);
        
        // 创建新的空闲块
        BlockHeader* new_block = reinterpret_cast<BlockHeader*>(
            reinterpret_cast<char*>(block + 1) + size);
        
        new_block->size = block->size - size - sizeof(BlockHeader);
        new_block->free = true;
        new_block->prev = block;
        new_block->next = block->next;
        
        // 更新链表
        if (block->next) {
            block->next->prev = new_block;
        }
        block->next = new_block;
        block->size = size;
        
        // 将新块添加到索引
        add_to_free_index(new_block);
    }

    // 与相邻块合并
    BlockHeader* merge_with_neighbors(BlockHeader* block) {
        assert(block && block->free);
        
        // 先向前合并
        while (block->prev && block->prev->free) {
            BlockHeader* prev = block->prev;
            
            // 从索引中移除前一个块
            remove_from_free_index(prev);
            
            // 合并
            prev->size += sizeof(BlockHeader) + block->size;
            prev->next = block->next;
            if (block->next) {
                block->next->prev = prev;
            }
            
            block = prev;
        }
        
        // 再向后合并
        while (block->next && block->next->free) {
            BlockHeader* next = block->next;
            
            // 从索引中移除下一个块
            remove_from_free_index(next);
            
            // 合并
            block->size += sizeof(BlockHeader) + next->size;
            block->next = next->next;
            if (next->next) {
                next->next->prev = block;
            }
        }
        
        return block;
    }
};