//
// Created by Kolane on 2025/3/14.
//

#include <iostream>
#include <stack>
#include <stdexcept>
#include <cstdlib>

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

// 测试函数
void test_memory_pool() {
    std::cout << "=======test_memory_pool========" << std::endl;
    MemoryPool pool(sizeof(int), 10);
    
    // 分配一些内存
    void* ptr1 = pool.allocate();
    void* ptr2 = pool.allocate();
    void* ptr3 = pool.allocate();
    
    std::cout << "Allocated 3 blocks" << std::endl;
    
    // 释放内存
    pool.deallocate(ptr2);
    std::cout << "Deallocated 1 block" << std::endl;
    
    // 再次分配
    void* ptr4 = pool.allocate();
    std::cout << "Allocated 1 more block" << std::endl;
    
    // 清理
    pool.deallocate(ptr1);
    pool.deallocate(ptr3);
    pool.deallocate(ptr4);
    std::cout << "Test completed" << std::endl;
}

int main() {
    test_memory_pool();
    return 0;
}