//
// Created by Kolane on 2025/3/14.
//

#include <iostream>
#include <memory>
#include <stdexcept>
#include <cstdlib>
#include <atomic>
#include <mutex>
#include <vector>
#include <cassert>
#include <type_traits>
#include <concepts>
#include <bit>
#include <new>
#include <chrono>
#include <cstring>

// C++20 概念定义
template<typename T>
concept Poolable = std::is_object_v<T> && !std::is_abstract_v<T>;

// 内存池统计信息
struct PoolStats {
    std::atomic<std::size_t> total_allocations{0};
    std::atomic<std::size_t> total_deallocations{0};
    std::atomic<std::size_t> current_usage{0};
    std::atomic<std::size_t> peak_usage{0};
    std::atomic<std::size_t> allocation_failures{0};
    
    void record_allocation() noexcept {
        total_allocations.fetch_add(1, std::memory_order_relaxed);
        auto current = current_usage.fetch_add(1, std::memory_order_relaxed) + 1;
        
        // 更新峰值使用量
        auto peak = peak_usage.load(std::memory_order_relaxed);
        while (current > peak && !peak_usage.compare_exchange_weak(peak, current, std::memory_order_relaxed)) {
            // 重试直到成功
        }
    }
    
    void record_deallocation() noexcept {
        total_deallocations.fetch_add(1, std::memory_order_relaxed);
        current_usage.fetch_sub(1, std::memory_order_relaxed);
    }
    
    void record_failure() noexcept {
        allocation_failures.fetch_add(1, std::memory_order_relaxed);
    }
    
    void print_stats() const {
        std::cout << "=== Memory Pool Statistics ===" << std::endl;
        std::cout << "Total allocations: " << total_allocations.load() << std::endl;
        std::cout << "Total deallocations: " << total_deallocations.load() << std::endl;
        std::cout << "Current usage: " << current_usage.load() << std::endl;
        std::cout << "Peak usage: " << peak_usage.load() << std::endl;
        std::cout << "Allocation failures: " << allocation_failures.load() << std::endl;
    }
};

// 线程安全的内存池
template<Poolable T>
class MemoryPool {
private:
    struct Block {
        alignas(T) char data[sizeof(T)];
        Block* next;
    };
    
    std::unique_ptr<char[]> pool_;
    Block* free_list_;
    mutable std::mutex mutex_;
    std::size_t total_blocks_;
    std::size_t block_size_;
    PoolStats stats_;
    
    // 检查指针是否属于此池
    bool is_from_pool(void* ptr) const noexcept {
        char* pool_start = pool_.get();
        char* pool_end = pool_start + total_blocks_ * sizeof(Block);
        char* ptr_char = static_cast<char*>(ptr);
        return ptr_char >= pool_start && ptr_char < pool_end;
    }
    
public:
    using value_type = T;
    using size_type = std::size_t;
    
    // 构造函数
    explicit MemoryPool(size_type total_blocks) 
        : pool_(std::make_unique<char[]>(total_blocks * sizeof(Block)))
        , free_list_(nullptr)
        , total_blocks_(total_blocks)
        , block_size_(sizeof(T)) {
        
        if (total_blocks == 0) {
            throw std::invalid_argument("Pool size cannot be zero");
        }
        
        // 初始化空闲列表
        Block* blocks = reinterpret_cast<Block*>(pool_.get());
        for (size_type i = 0; i < total_blocks - 1; ++i) {
            blocks[i].next = &blocks[i + 1];
        }
        blocks[total_blocks - 1].next = nullptr;
        free_list_ = blocks;
    }
    
    // 禁用拷贝
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
    
    // 支持移动
    MemoryPool(MemoryPool&& other) noexcept 
        : pool_(std::move(other.pool_))
        , free_list_(std::exchange(other.free_list_, nullptr))
        , total_blocks_(std::exchange(other.total_blocks_, 0))
        , block_size_(std::exchange(other.block_size_, 0))
        , stats_(std::move(other.stats_)) {
    }
    
    MemoryPool& operator=(MemoryPool&& other) noexcept {
        if (this != &other) {
            pool_ = std::move(other.pool_);
            free_list_ = std::exchange(other.free_list_, nullptr);
            total_blocks_ = std::exchange(other.total_blocks_, 0);
            block_size_ = std::exchange(other.block_size_, 0);
            stats_ = std::move(other.stats_);
        }
        return *this;
    }
    
    // 析构函数
    ~MemoryPool() = default;
    
    // 分配内存
    [[nodiscard]] T* allocate() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!free_list_) {
            stats_.record_failure();
            throw std::bad_alloc();
        }
        
        Block* block = free_list_;
        free_list_ = block->next;
        
        stats_.record_allocation();
        return reinterpret_cast<T*>(block);
    }
    
    // 尝试分配内存（不抛异常）
    [[nodiscard]] T* try_allocate() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!free_list_) {
            stats_.record_failure();
            return nullptr;
        }
        
        Block* block = free_list_;
        free_list_ = block->next;
        
        stats_.record_allocation();
        return reinterpret_cast<T*>(block);
    }
    
    // 释放内存
    void deallocate(T* ptr) noexcept {
        if (!ptr) return;
        
        assert(is_from_pool(ptr) && "Pointer not from this pool");
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        Block* block = reinterpret_cast<Block*>(ptr);
        block->next = free_list_;
        free_list_ = block;
        
        stats_.record_deallocation();
    }
    
    // 构造对象
    template<typename... Args>
    [[nodiscard]] T* construct(Args&&... args) {
        T* ptr = allocate();
        try {
            new(ptr) T(std::forward<Args>(args)...);
            return ptr;
        } catch (...) {
            deallocate(ptr);
            throw;
        }
    }
    
    // 销毁对象
    void destroy(T* ptr) noexcept {
        if (!ptr) return;
        
        if constexpr (!std::is_trivially_destructible_v<T>) {
            ptr->~T();
        }
        deallocate(ptr);
    }
    
    // 获取可用块数量
    [[nodiscard]] size_type available() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        size_type count = 0;
        Block* current = free_list_;
        while (current) {
            ++count;
            current = current->next;
        }
        return count;
    }
    
    // 获取总块数量
    [[nodiscard]] constexpr size_type capacity() const noexcept {
        return total_blocks_;
    }
    
    // 获取已使用块数量
    [[nodiscard]] size_type used() const noexcept {
        return capacity() - available();
    }
    
    // 检查是否为空
    [[nodiscard]] bool empty() const noexcept {
        return used() == 0;
    }
    
    // 检查是否已满
    [[nodiscard]] bool full() const noexcept {
        return available() == 0;
    }
    
    // 获取统计信息
    [[nodiscard]] const PoolStats& get_stats() const noexcept {
        return stats_;
    }
    
    // 重置统计信息
    void reset_stats() noexcept {
        stats_ = PoolStats{};
    }
};

// 固定大小内存池（用于特定大小的对象）
template<std::size_t BlockSize, std::size_t BlockCount>
class FixedSizePool {
private:
    struct Block {
        alignas(std::max_align_t) char data[BlockSize];
        Block* next;
    };
    
    std::unique_ptr<Block[]> pool_;
    Block* free_list_;
    mutable std::mutex mutex_;
    PoolStats stats_;
    
public:
    explicit FixedSizePool() 
        : pool_(std::make_unique<Block[]>(BlockCount))
        , free_list_(nullptr) {
        
        // 初始化空闲列表
        for (std::size_t i = 0; i < BlockCount - 1; ++i) {
            pool_[i].next = &pool_[i + 1];
        }
        pool_[BlockCount - 1].next = nullptr;
        free_list_ = pool_.get();
    }
    
    [[nodiscard]] void* allocate() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!free_list_) {
            stats_.record_failure();
            throw std::bad_alloc();
        }
        
        Block* block = free_list_;
        free_list_ = block->next;
        
        stats_.record_allocation();
        return block->data;
    }
    
    void deallocate(void* ptr) noexcept {
        if (!ptr) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        Block* block = reinterpret_cast<Block*>(static_cast<char*>(ptr) - offsetof(Block, data));
        block->next = free_list_;
        free_list_ = block;
        
        stats_.record_deallocation();
    }
    
    [[nodiscard]] constexpr std::size_t block_size() const noexcept {
        return BlockSize;
    }
    
    [[nodiscard]] constexpr std::size_t capacity() const noexcept {
        return BlockCount;
    }
    
    [[nodiscard]] const PoolStats& get_stats() const noexcept {
        return stats_;
    }
};

// 测试用的复杂对象
class TestObject {
public:
    int id;
    double value;
    char name[32];  // 使用固定大小数组而不是std::string
    
    TestObject(int i = 0, double v = 0.0, const char* n = "default") 
        : id(i), value(v) {
        std::strncpy(name, n, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        std::cout << "TestObject(" << id << ", " << value << ", \"" << name << "\") constructed" << std::endl;
    }
    
    ~TestObject() {
        std::cout << "TestObject(" << id << ", " << value << ", \"" << name << "\") destructed" << std::endl;
    }
    
    TestObject(const TestObject& other) 
        : id(other.id), value(other.value) {
        std::strncpy(name, other.name, sizeof(name));
        std::cout << "TestObject copied: (" << id << ", " << value << ", \"" << name << "\")" << std::endl;
    }
    
    TestObject& operator=(const TestObject& other) {
        if (this != &other) {
            id = other.id;
            value = other.value;
            std::strncpy(name, other.name, sizeof(name));
            std::cout << "TestObject assigned: (" << id << ", " << value << ", \"" << name << "\")" << std::endl;
        }
        return *this;
    }
};

// 简单对象（用于性能测试）
struct SimpleObject {
    int data[4];
    
    SimpleObject(int val = 0) {
        for (int& d : data) {
            d = val;
        }
    }
};

// 测试函数
void test_memory_pool() {
    try {
        std::cout << "=== Testing Memory Pool ===" << std::endl;
        
        // 基本功能测试
        {
            std::cout << "\n--- Basic functionality test ---" << std::endl;
            MemoryPool<int> pool(10);
            
            std::cout << "Pool capacity: " << pool.capacity() << std::endl;
            std::cout << "Pool available: " << pool.available() << std::endl;
            std::cout << "Pool empty: " << std::boolalpha << pool.empty() << std::endl;
            
            // 分配一些内存
            std::vector<int*> ptrs;
            for (int i = 0; i < 5; ++i) {
                int* ptr = pool.allocate();
                *ptr = i * 10;
                ptrs.push_back(ptr);
                std::cout << "Allocated block " << i << ", value: " << *ptr << std::endl;
            }
            
            std::cout << "Pool used: " << pool.used() << std::endl;
            std::cout << "Pool available: " << pool.available() << std::endl;
            
            // 释放一些内存
            pool.deallocate(ptrs[2]);
            ptrs[2] = nullptr;
            std::cout << "Deallocated block 2" << std::endl;
            std::cout << "Pool used: " << pool.used() << std::endl;
            
            // 再次分配
            int* new_ptr = pool.allocate();
            *new_ptr = 999;
            std::cout << "Reallocated block, value: " << *new_ptr << std::endl;
            
            // 清理
            for (int* ptr : ptrs) {
                if (ptr) pool.deallocate(ptr);
            }
            pool.deallocate(new_ptr);
            
            std::cout << "Pool empty after cleanup: " << std::boolalpha << pool.empty() << std::endl;
        }
        
        // 对象构造/销毁测试
        {
            std::cout << "\n--- Object construction/destruction test ---" << std::endl;
            MemoryPool<TestObject> obj_pool(5);
            
            // 构造对象
            std::vector<TestObject*> objects;
            objects.push_back(obj_pool.construct(1, 1.1, "first"));
            objects.push_back(obj_pool.construct(2, 2.2, "second"));
            objects.push_back(obj_pool.construct(3, 3.3, "third"));
            
            std::cout << "Objects constructed in pool" << std::endl;
            
            // 访问对象
            for (const auto* obj : objects) {
                std::cout << "Object: id=" << obj->id << ", value=" << obj->value 
                         << ", name=\"" << obj->name << "\"" << std::endl;
            }
            
            // 销毁对象
            for (auto* obj : objects) {
                obj_pool.destroy(obj);
            }
            
            std::cout << "Objects destroyed" << std::endl;
        }
        
        // 性能测试
        {
            std::cout << "\n--- Performance test ---" << std::endl;
            constexpr std::size_t test_size = 1000;
            MemoryPool<SimpleObject> perf_pool(test_size);
            
            auto start = std::chrono::high_resolution_clock::now();
            
            // 分配所有对象
            std::vector<SimpleObject*> perf_objects;
            perf_objects.reserve(test_size);
            
            for (std::size_t i = 0; i < test_size; ++i) {
                perf_objects.push_back(perf_pool.construct(static_cast<int>(i)));
            }
            
            auto mid = std::chrono::high_resolution_clock::now();
            
            // 释放所有对象
            for (auto* obj : perf_objects) {
                perf_pool.destroy(obj);
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            
            auto alloc_time = std::chrono::duration_cast<std::chrono::microseconds>(mid - start);
            auto dealloc_time = std::chrono::duration_cast<std::chrono::microseconds>(end - mid);
            
            std::cout << "Allocated " << test_size << " objects in " << alloc_time.count() << " μs" << std::endl;
            std::cout << "Deallocated " << test_size << " objects in " << dealloc_time.count() << " μs" << std::endl;
            
            perf_pool.get_stats().print_stats();
        }
        
        // 固定大小池测试
        {
            std::cout << "\n--- Fixed size pool test ---" << std::endl;
            FixedSizePool<64, 100> fixed_pool;
            
            std::cout << "Fixed pool block size: " << fixed_pool.block_size() << " bytes" << std::endl;
            std::cout << "Fixed pool capacity: " << fixed_pool.capacity() << " blocks" << std::endl;
            
            // 分配一些块
            std::vector<void*> fixed_ptrs;
            for (int i = 0; i < 10; ++i) {
                void* ptr = fixed_pool.allocate();
                fixed_ptrs.push_back(ptr);
                
                // 写入一些数据
                *static_cast<int*>(ptr) = i * 100;
            }
            
            std::cout << "Allocated 10 blocks from fixed pool" << std::endl;
            
            // 读取数据
            for (std::size_t i = 0; i < fixed_ptrs.size(); ++i) {
                int value = *static_cast<int*>(fixed_ptrs[i]);
                std::cout << "Block " << i << " value: " << value << std::endl;
            }
            
            // 清理
            for (void* ptr : fixed_ptrs) {
                fixed_pool.deallocate(ptr);
            }
            
            std::cout << "Fixed pool test completed" << std::endl;
            fixed_pool.get_stats().print_stats();
        }
        
        // 异常处理测试
        {
            std::cout << "\n--- Exception handling test ---" << std::endl;
            MemoryPool<int> small_pool(2);
            
            // 分配所有可用块
            int* ptr1 = small_pool.allocate();
            int* ptr2 = small_pool.allocate();
            
            std::cout << "Pool is now full" << std::endl;
            
            // 尝试分配更多（应该抛异常）
             try {
                 [[maybe_unused]] int* ptr3 = small_pool.allocate();
                 std::cout << "ERROR: Should have thrown exception!" << std::endl;
             } catch (const std::bad_alloc& e) {
                 std::cout << "Caught expected exception: " << e.what() << std::endl;
             }
            
            // 尝试非抛异常版本
            int* ptr3 = small_pool.try_allocate();
            if (!ptr3) {
                std::cout << "try_allocate correctly returned nullptr" << std::endl;
            }
            
            // 清理
            small_pool.deallocate(ptr1);
            small_pool.deallocate(ptr2);
            
            small_pool.get_stats().print_stats();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

int main() {
    test_memory_pool();
    return 0;
}