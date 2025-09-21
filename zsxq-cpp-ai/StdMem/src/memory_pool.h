#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include "memory_block.h"
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>

/**
 * 线程安全的内存池类
 * 管理多个不同大小的内存块，提供高效的内存分配和释放
 */
class MemoryPool {
public:
    /**
     * 内存池统计信息结构体
     */
    struct Statistics {
        size_t total_memory;        // 总内存大小（字节）
        size_t used_memory;         // 已使用内存大小（字节）
        size_t free_memory;         // 空闲内存大小（字节）
        size_t total_blocks;        // 总块数量
        size_t used_blocks;         // 已使用块数量
        size_t free_blocks;         // 空闲块数量
        size_t allocation_count;    // 分配次数统计
        size_t deallocation_count;  // 释放次数统计
    };
    
    /**
     * 构造函数
     * @param block_size 每个小块的大小（字节）
     * @param initial_block_count 初始块数量
     * @param max_blocks 最大块数量（0表示无限制）
     */
    explicit MemoryPool(size_t block_size, 
                       size_t initial_block_count = 1024,
                       size_t max_blocks = 0);
    
    /**
     * 析构函数
     */
    ~MemoryPool();
    
    /**
     * 分配指定大小的内存
     * @param size 需要分配的内存大小
     * @return 分配的内存指针，失败返回nullptr
     */
    void* allocate(size_t size);
    
    /**
     * 释放内存
     * @param ptr 要释放的内存指针
     * @return 成功返回true，失败返回false
     */
    bool deallocate(void* ptr);
    
    /**
     * 重置内存池
     * 释放所有已分配的内存，重置为初始状态
     */
    void reset();
    
    /**
     * 获取内存池统计信息
     * @return 统计信息结构体
     */
    Statistics getStatistics() const;
    
    /**
     * 检查内存池是否为空（无已分配内存）
     * @return 如果为空返回true
     */
    bool isEmpty() const;
    
    /**
     * 获取支持的最大块大小
     * @return 最大块大小（字节）
     */
    size_t getMaxBlockSize() const { return block_size_; }
    
    /**
     * 预分配指定数量的块
     * @param count 要预分配的块数量
     * @return 成功返回true
     */
    bool preallocate(size_t count);

private:
    // 禁用拷贝构造和赋值
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
    
    /**
     * 扩展内存池（添加新的MemoryBlock）
     * @param additional_blocks 要添加的块数量
     * @return 成功返回true
     */
    bool expandPool(size_t additional_blocks);
    
    /**
     * 查找包含指定指针的内存块
     * @param ptr 要查找的指针
     * @return 包含该指针的MemoryBlock指针，未找到返回nullptr
     */
    MemoryBlock* findBlockContaining(void* ptr);
    
    mutable std::mutex mutex_;                    // 保护内存池的互斥锁
    std::vector<std::unique_ptr<MemoryBlock>> blocks_;  // 内存块集合
    
    size_t block_size_;                          // 每个小块的大小
    size_t initial_block_count_;                 // 初始块数量
    size_t max_blocks_;                          // 最大块数限制（0为无限制）
    size_t blocks_per_expansion_;                // 每次扩展的块数量
    
    // 统计信息（原子操作保证线程安全）
    mutable std::atomic<size_t> allocation_count_;    // 分配次数
    mutable std::atomic<size_t> deallocation_count_;  // 释放次数
};

#endif // MEMORY_POOL_H