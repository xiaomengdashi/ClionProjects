#include "memory_block.h"
#include <new>
#include <algorithm>
#include <cstdlib>     // for std::malloc, std::free
#include <cstdint>     // for uintptr_t

MemoryBlock::MemoryBlock(size_t block_size, size_t block_count)
    : memory_start_(nullptr)
    , raw_memory_(nullptr)
    , block_size_(alignSize(std::max(block_size, sizeof(void*))))  // 确保块大小至少能存放一个指针
    , block_count_(block_count)
    , free_count_(block_count)
    , free_list_head_(nullptr) {
    
    // 分配整块内存（手动对齐以支持C++11）
    size_t total_size = block_size_ * block_count_;
    
    // 分配额外的内存用于对齐
    size_t alloc_size = total_size + ALIGNMENT - 1;
    raw_memory_ = static_cast<char*>(std::malloc(alloc_size));
    
    if (!raw_memory_) {
        throw std::bad_alloc();
    }
    
    // 手动对齐到ALIGNMENT字节边界
    uintptr_t raw_addr = reinterpret_cast<uintptr_t>(raw_memory_);
    uintptr_t aligned_addr = (raw_addr + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    memory_start_ = reinterpret_cast<char*>(aligned_addr);
    
    // 初始化空闲链表
    initializeFreeList();
}

MemoryBlock::~MemoryBlock() {
    if (raw_memory_) {
        std::free(raw_memory_);
        raw_memory_ = nullptr;
        memory_start_ = nullptr;
    }
}

void* MemoryBlock::allocate() {
    // 如果没有空闲块，返回nullptr
    if (free_list_head_ == nullptr) {
        return nullptr;
    }
    
    // 从空闲链表头部取出一个块
    void* allocated_block = free_list_head_;
    
    // 更新空闲链表头部为下一个空闲块
    free_list_head_ = *static_cast<void**>(free_list_head_);
    
    // 减少空闲块计数
    --free_count_;
    
    return allocated_block;
}

bool MemoryBlock::deallocate(void* ptr) {
    // 检查指针是否有效
    if (!ptr || !contains(ptr)) {
        return false;
    }
    
    // 检查指针是否对齐到块边界
    char* char_ptr = static_cast<char*>(ptr);
    size_t offset = char_ptr - memory_start_;
    if (offset % block_size_ != 0) {
        return false;  // 指针不在块边界上
    }
    
    // 检查是否重复释放（空闲块数已达到最大值）
    if (free_count_ >= block_count_) {
        return false;  // 不能释放更多块，可能是重复释放
    }
    
    // 将块添加到空闲链表头部
    *static_cast<void**>(ptr) = free_list_head_;
    free_list_head_ = ptr;
    
    // 增加空闲块计数
    ++free_count_;
    
    return true;
}

void MemoryBlock::reset() {
    // 重置空闲块计数
    free_count_ = block_count_;
    
    // 重新初始化空闲链表
    initializeFreeList();
}

bool MemoryBlock::contains(void* ptr) const {
    if (!ptr || !memory_start_) {
        return false;
    }
    
    char* char_ptr = static_cast<char*>(ptr);
    char* memory_end = memory_start_ + (block_size_ * block_count_);
    
    return (char_ptr >= memory_start_) && (char_ptr < memory_end);
}

void MemoryBlock::initializeFreeList() {
    if (!memory_start_ || block_count_ == 0) {
        free_list_head_ = nullptr;
        return;
    }
    
    // 将所有块链接成空闲链表
    // 每个块的前sizeof(void*)字节存储下一个块的地址
    char* current_block = memory_start_;
    free_list_head_ = current_block;
    
    for (size_t i = 0; i < block_count_ - 1; ++i) {
        char* next_block = current_block + block_size_;
        *reinterpret_cast<void**>(current_block) = next_block;
        current_block = next_block;
    }
    
    // 最后一个块的下一个指针设为nullptr
    *reinterpret_cast<void**>(current_block) = nullptr;
}

size_t MemoryBlock::alignSize(size_t size) {
    // 将大小对齐到ALIGNMENT字节边界
    return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}