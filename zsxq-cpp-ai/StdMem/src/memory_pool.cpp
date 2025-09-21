#include "memory_pool.h"
#include <algorithm>
#include <stdexcept>

MemoryPool::MemoryPool(size_t block_size, size_t initial_block_count, size_t max_blocks)
    : block_size_(std::max(block_size, sizeof(void*)))  // 确保块大小至少能存放一个指针
    , initial_block_count_(initial_block_count)
    , max_blocks_(max_blocks)
    , blocks_per_expansion_(std::max(initial_block_count / 4, size_t(64)))  // 扩展时的块数量
    , allocation_count_(0)
    , deallocation_count_(0) {
    
    if (initial_block_count == 0) {
        throw std::invalid_argument("初始块数量不能为0");
    }
    
    // 创建初始内存块
    blocks_.reserve(8);  // 预留空间，减少重新分配
    
    try {
        // C++11兼容：使用new创建unique_ptr
        std::unique_ptr<MemoryBlock> initial_block(new MemoryBlock(block_size_, initial_block_count));
        blocks_.push_back(std::move(initial_block));
    } catch (const std::exception& e) {
        throw std::runtime_error("创建初始内存块失败");
    }
}

MemoryPool::~MemoryPool() {
    // unique_ptr会自动清理所有MemoryBlock
}

void* MemoryPool::allocate(size_t size) {
    // 检查请求的大小是否超过块大小
    if (size > block_size_) {
        return nullptr;  // 不支持超过块大小的分配
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 尝试从现有块中分配
    for (auto& block : blocks_) {
        void* ptr = block->allocate();
        if (ptr) {
            allocation_count_.fetch_add(1, std::memory_order_relaxed);
            return ptr;
        }
    }
    
    // 如果所有块都满了，尝试扩展内存池
    if (max_blocks_ == 0 || blocks_.size() < max_blocks_) {
        if (expandPool(blocks_per_expansion_)) {
            // 从新创建的块中分配
            void* ptr = blocks_.back()->allocate();
            if (ptr) {
                allocation_count_.fetch_add(1, std::memory_order_relaxed);
                return ptr;
            }
        }
    }
    
    // 分配失败
    return nullptr;
}

bool MemoryPool::deallocate(void* ptr) {
    if (!ptr) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 查找包含该指针的内存块
    MemoryBlock* target_block = findBlockContaining(ptr);
    if (!target_block) {
        return false;  // 指针不属于此内存池
    }
    
    // 释放内存
    bool success = target_block->deallocate(ptr);
    if (success) {
        deallocation_count_.fetch_add(1, std::memory_order_relaxed);
    }
    
    return success;
}

void MemoryPool::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 重置所有内存块
    for (auto& block : blocks_) {
        block->reset();
    }
    
    // 重置统计信息
    allocation_count_.store(0, std::memory_order_relaxed);
    deallocation_count_.store(0, std::memory_order_relaxed);
}

MemoryPool::Statistics MemoryPool::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Statistics stats = {};
    
    for (const auto& block : blocks_) {
        size_t block_total = block->getTotalCount();
        size_t block_free = block->getFreeCount();
        size_t block_used = block_total - block_free;
        
        stats.total_blocks += block_total;
        stats.free_blocks += block_free;
        stats.used_blocks += block_used;
    }
    
    stats.total_memory = stats.total_blocks * block_size_;
    stats.used_memory = stats.used_blocks * block_size_;
    stats.free_memory = stats.free_blocks * block_size_;
    stats.allocation_count = allocation_count_.load(std::memory_order_relaxed);
    stats.deallocation_count = deallocation_count_.load(std::memory_order_relaxed);
    
    return stats;
}

bool MemoryPool::isEmpty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& block : blocks_) {
        if (block->getFreeCount() != block->getTotalCount()) {
            return false;  // 有已分配的块
        }
    }
    
    return true;
}

bool MemoryPool::preallocate(size_t count) {
    if (count == 0) {
        return true;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 计算需要多少个新的MemoryBlock
    size_t current_capacity = 0;
    for (const auto& block : blocks_) {
        current_capacity += block->getFreeCount();
    }
    
    if (current_capacity >= count) {
        return true;  // 已经有足够的空闲块
    }
    
    size_t needed_blocks = count - current_capacity;
    return expandPool(needed_blocks);
}

bool MemoryPool::expandPool(size_t additional_blocks) {
    // 检查是否超过最大块数限制
    if (max_blocks_ > 0) {
        size_t current_total_blocks = 0;
        for (const auto& block : blocks_) {
            current_total_blocks += block->getTotalCount();
        }
        
        if (current_total_blocks + additional_blocks > max_blocks_) {
            additional_blocks = max_blocks_ - current_total_blocks;
            if (additional_blocks == 0) {
                return false;  // 已达到最大限制
            }
        }
    }
    
    try {
        // C++11兼容：使用new创建unique_ptr
        std::unique_ptr<MemoryBlock> new_block(new MemoryBlock(block_size_, additional_blocks));
        blocks_.push_back(std::move(new_block));
        return true;
    } catch (const std::exception& e) {
        return false;  // 分配失败
    }
}

MemoryBlock* MemoryPool::findBlockContaining(void* ptr) {
    for (auto& block : blocks_) {
        if (block->contains(ptr)) {
            return block.get();
        }
    }
    return nullptr;
}