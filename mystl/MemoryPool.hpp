//
// Created by Kolane on 2025/3/14.
//

#include <iostream>
#include <stack>



#pragma once


// 内存池
class MemoryPool {
public:
    MemoryPool(size_t obj_size, size_t total_size) : obj_size_(obj_size), total_size_(total_size){
        pool = (char*) malloc(obj_size_ * total_size_);
        if (pool == nullptr) {
            std::cerr << "Failed to allocate memory pool." << std::endl;
            throw std::runtime_error("Failed to allocate memory pool.");
        }

        // 初始化free_list_
        for (size_t i = 0; i < total_size_; ++i) {
            free_list_.push(pool + i * obj_size_);
        }
    }


    ~MemoryPool() {
        free(pool);
    }

    void* allocate() {
        if (free_list_.empty()) {
            std::cerr << "Memory pool is full." << std::endl;
            throw std::runtime_error("Memory pool is full.");
        }

        void *p = free_list_.top();
        free_list_.pop();
        return p;
    }

    void deallocate(void* p) {
        free_list_.push(p);
    }
private:
    size_t obj_size_;
    size_t total_size_;
    char* pool;
    std::stack<void*> free_list_;
};