#ifndef MEMORY_BLOCK_H
#define MEMORY_BLOCK_H

#include <cstddef>
#include <cstdint>

/**
 * 内存块类
 * 管理单个固定大小的内存块，维护空闲块链表
 */
class MemoryBlock {
public:
    /**
     * 构造函数
     * @param block_size 每个小块的大小（字节）
     * @param block_count 块的数量
     */
    MemoryBlock(size_t block_size, size_t block_count);
    
    /**
     * 析构函数
     * 释放所有分配的内存
     */
    ~MemoryBlock();
    
    /**
     * 分配一个内存块
     * @return 分配的内存指针，如果无可用块则返回nullptr
     */
    void* allocate();
    
    /**
     * 释放一个内存块
     * @param ptr 要释放的内存指针
     * @return 如果成功释放则返回true，否则返回false
     */
    bool deallocate(void* ptr);
    
    /**
     * 重置内存块，将所有块标记为可用
     */
    void reset();
    
    /**
     * 检查指针是否属于此内存块
     * @param ptr 要检查的指针
     * @return 如果指针属于此块则返回true
     */
    bool contains(void* ptr) const;
    
    /**
     * 获取空闲块数量
     * @return 当前可用的空闲块数量
     */
    size_t getFreeCount() const { return free_count_; }
    
    /**
     * 获取总块数量
     * @return 总的块数量
     */
    size_t getTotalCount() const { return block_count_; }
    
    /**
     * 获取块大小
     * @return 每个块的大小（字节）
     */
    size_t getBlockSize() const { return block_size_; }

private:
    // 禁用拷贝构造和赋值
    MemoryBlock(const MemoryBlock&) = delete;
    MemoryBlock& operator=(const MemoryBlock&) = delete;
    
    /**
     * 初始化空闲链表
     * 将所有块链接到空闲链表中
     */
    void initializeFreeList();
    
    char* memory_start_;        // 对齐后的内存块起始地址
    char* raw_memory_;          // 原始分配的内存地址（用于释放）
    size_t block_size_;         // 每个块的大小
    size_t block_count_;        // 总块数量
    size_t free_count_;         // 空闲块数量
    void* free_list_head_;      // 空闲链表头指针
    
    // 内存对齐常量
    static const size_t ALIGNMENT = 8;
    
    /**
     * 对齐大小到指定边界
     * @param size 要对齐的大小
     * @return 对齐后的大小
     */
    static size_t alignSize(size_t size);
};

#endif // MEMORY_BLOCK_H