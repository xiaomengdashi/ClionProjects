#include <iostream>
#include <memory>
#include <vector>
#include <atomic>
#include <new>
#include <cstdlib>
#include <cassert>
#include <type_traits>
#include <concepts>
#include <bit>

// 内存统计类
class AllocatorStats {
public:
    static std::atomic<std::size_t> total_allocated;
    static std::atomic<std::size_t> total_deallocated;
    static std::atomic<std::size_t> current_allocated;
    static std::atomic<std::size_t> allocation_count;
    static std::atomic<std::size_t> deallocation_count;
    
    static void record_allocation(std::size_t bytes) noexcept {
        total_allocated.fetch_add(bytes, std::memory_order_relaxed);
        current_allocated.fetch_add(bytes, std::memory_order_relaxed);
        allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
    
    static void record_deallocation(std::size_t bytes) noexcept {
        total_deallocated.fetch_add(bytes, std::memory_order_relaxed);
        current_allocated.fetch_sub(bytes, std::memory_order_relaxed);
        deallocation_count.fetch_add(1, std::memory_order_relaxed);
    }
    
    static void print_stats() {
        std::cout << "=== Allocator Statistics ===" << std::endl;
        std::cout << "Total allocated: " << total_allocated.load() << " bytes" << std::endl;
        std::cout << "Total deallocated: " << total_deallocated.load() << " bytes" << std::endl;
        std::cout << "Current allocated: " << current_allocated.load() << " bytes" << std::endl;
        std::cout << "Allocation count: " << allocation_count.load() << std::endl;
        std::cout << "Deallocation count: " << deallocation_count.load() << std::endl;
    }
};

// 静态成员定义
std::atomic<std::size_t> AllocatorStats::total_allocated{0};
std::atomic<std::size_t> AllocatorStats::total_deallocated{0};
std::atomic<std::size_t> AllocatorStats::current_allocated{0};
std::atomic<std::size_t> AllocatorStats::allocation_count{0};
std::atomic<std::size_t> AllocatorStats::deallocation_count{0};

// C++20概念定义
template<typename T>
concept Allocatable = std::is_object_v<T> && !std::is_const_v<T> && !std::is_volatile_v<T>;

// 优化的分配器
template<Allocatable T>
class MyAllocator {
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using propagate_on_container_move_assignment = std::true_type;
    using is_always_equal = std::true_type;
    
    // 默认构造函数
    constexpr MyAllocator() noexcept = default;
    
    // 拷贝构造函数
    constexpr MyAllocator(const MyAllocator&) noexcept = default;
    
    // 转换构造函数
    template<Allocatable U>
    constexpr MyAllocator(const MyAllocator<U>&) noexcept {}
    
    // 移动构造函数
    constexpr MyAllocator(MyAllocator&&) noexcept = default;
    
    // 赋值运算符
    constexpr MyAllocator& operator=(const MyAllocator&) noexcept = default;
    constexpr MyAllocator& operator=(MyAllocator&&) noexcept = default;
    
    // 析构函数
    ~MyAllocator() = default;
    
    // 分配内存
    [[nodiscard]] constexpr T* allocate(size_type n) {
        if (n == 0) {
            throw std::invalid_argument("Cannot allocate zero elements");
        }
        
        // 检查溢出
        if (n > max_size()) {
            throw std::bad_array_new_length();
        }
        
        // 检查整数溢出
        const size_type bytes = n * sizeof(T);
        if (bytes / sizeof(T) != n) {
            throw std::bad_array_new_length();
        }
        
        // 使用对齐分配
        void* ptr = nullptr;
        try {
            if constexpr (alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
                ptr = std::aligned_alloc(alignof(T), bytes);
            } else {
                ptr = std::malloc(bytes);
            }
        } catch (...) {
            throw std::bad_alloc();
        }
        
        if (!ptr) {
            throw std::bad_alloc();
        }
        
        // 记录统计信息
        AllocatorStats::record_allocation(bytes);
        
        return static_cast<T*>(ptr);
    }
    
    // 释放内存
    constexpr void deallocate(T* ptr, size_type n) noexcept {
        if (!ptr) {
            // 记录错误但不抛异常（noexcept函数）
            return;
        }
        
        if (n == 0) {
            return;
        }
        
        // 记录统计信息
        AllocatorStats::record_deallocation(n * sizeof(T));
        
        std::free(ptr);
    }
    
    // 获取最大可分配数量
    [[nodiscard]] constexpr size_type max_size() const noexcept {
        return std::numeric_limits<size_type>::max() / sizeof(T);
    }
    
    // 构造对象
    template<typename... Args>
    constexpr void construct(T* ptr, Args&&... args) 
        noexcept(std::is_nothrow_constructible_v<T, Args...>) {
        std::construct_at(ptr, std::forward<Args>(args)...);
    }
    
    // 销毁对象
    constexpr void destroy(T* ptr) noexcept(std::is_nothrow_destructible_v<T>) {
        std::destroy_at(ptr);
    }
};

// 比较运算符
template<Allocatable T, Allocatable U>
constexpr bool operator==(const MyAllocator<T>&, const MyAllocator<U>&) noexcept {
    return true; // 所有MyAllocator实例都相等
}

template<Allocatable T, Allocatable U>
constexpr bool operator!=(const MyAllocator<T>&, const MyAllocator<U>&) noexcept {
    return false;
}

// 内存池分配器（用于小对象优化）
template<Allocatable T, std::size_t PoolSize = 1024>
class PoolAllocator {
private:
    struct Block {
        alignas(T) char data[sizeof(T)];
        Block* next;
    };
    
    static inline Block* free_list_ = nullptr;
    static inline std::atomic<std::size_t> pool_usage_{0};
    static inline char pool_[PoolSize * sizeof(Block)];
    static inline std::atomic<bool> pool_initialized_{false};
    
    void initialize_pool() {
        if (!pool_initialized_.exchange(true)) {
            // 检查池大小是否有效
            if (PoolSize == 0) {
                pool_initialized_.store(false);
                throw std::runtime_error("Invalid pool size");
            }
            
            // 检查Block大小是否足够容纳T
            if (sizeof(Block) < sizeof(T)) {
                pool_initialized_.store(false);
                throw std::runtime_error("Block size too small for type T");
            }
            
            try {
                Block* blocks = reinterpret_cast<Block*>(pool_);
                for (std::size_t i = 0; i < PoolSize - 1; ++i) {
                    blocks[i].next = &blocks[i + 1];
                }
                blocks[PoolSize - 1].next = nullptr;
                free_list_ = blocks;
            } catch (...) {
                // 初始化失败，重置状态
                free_list_ = nullptr;
                pool_initialized_.store(false);
                throw;
            }
        }
    }
    
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using propagate_on_container_move_assignment = std::true_type;
    using is_always_equal = std::true_type;
    
    // rebind支持
    template<typename U>
    struct rebind {
        using other = PoolAllocator<U, PoolSize>;
    };
    
    constexpr PoolAllocator() noexcept = default;
    
    template<Allocatable U>
    constexpr PoolAllocator(const PoolAllocator<U, PoolSize>&) noexcept {}
    
    [[nodiscard]] T* allocate(size_type n) {
        if (n == 0) {
            throw std::invalid_argument("Cannot allocate zero elements");
        }
        
        if (n != 1) {
            // 对于非单个对象分配，回退到标准分配器
            try {
                return MyAllocator<T>{}.allocate(n);
            } catch (...) {
                throw std::bad_alloc();
            }
        }
        
        try {
            initialize_pool();
        } catch (...) {
            // 池初始化失败，回退到标准分配器
            try {
                return MyAllocator<T>{}.allocate(n);
            } catch (...) {
                throw std::bad_alloc();
            }
        }
        
        // 检查池是否已满
        if (pool_usage_.load(std::memory_order_relaxed) >= PoolSize) {
            try {
                return MyAllocator<T>{}.allocate(n);
            } catch (...) {
                throw std::bad_alloc();
            }
        }
        
        if (free_list_) {
            Block* block = free_list_;
            free_list_ = block->next;
            pool_usage_.fetch_add(1, std::memory_order_relaxed);
            return reinterpret_cast<T*>(block);
        }
        
        // 池已满，使用标准分配器
        try {
            return MyAllocator<T>{}.allocate(n);
        } catch (...) {
            throw std::bad_alloc();
        }
    }
    
    void deallocate(T* ptr, size_type n) noexcept {
        if (!ptr) {
            return;
        }
        
        if (n == 0 || n != 1) {
            try {
                MyAllocator<T>{}.deallocate(ptr, n);
            } catch (...) {
                // 忽略deallocate中的异常（noexcept函数）
            }
            return;
        }
        
        // 检查是否来自池
        char* pool_start = pool_;
        char* pool_end = pool_ + sizeof(pool_);
        char* ptr_char = reinterpret_cast<char*>(ptr);
        
        if (ptr_char >= pool_start && ptr_char < pool_end) {
            // 检查指针对齐
            if ((ptr_char - pool_start) % sizeof(Block) != 0) {
                // 指针未正确对齐，可能是错误的指针
                return;
            }
            
            Block* block = reinterpret_cast<Block*>(ptr);
            block->next = free_list_;
            free_list_ = block;
            pool_usage_.fetch_sub(1, std::memory_order_relaxed);
        } else {
            try {
                MyAllocator<T>{}.deallocate(ptr, n);
            } catch (...) {
                // 忽略deallocate中的异常（noexcept函数）
            }
        }
    }
    
    static std::size_t get_pool_usage() {
        return pool_usage_.load();
    }
};

// 测试用的简单类
class TestObject {
public:
    int value;
    double data;
    
    TestObject(int v = 0, double d = 0.0) : value(v), data(d) {
        std::cout << "TestObject(" << value << ", " << data << ") constructed" << std::endl;
    }
    
    ~TestObject() {
        std::cout << "TestObject(" << value << ", " << data << ") destructed" << std::endl;
    }
    
    TestObject(const TestObject& other) : value(other.value), data(other.data) {
        std::cout << "TestObject copied: (" << value << ", " << data << ")" << std::endl;
    }
    
    TestObject& operator=(const TestObject& other) {
        if (this != &other) {
            value = other.value;
            data = other.data;
            std::cout << "TestObject assigned: (" << value << ", " << data << ")" << std::endl;
        }
        return *this;
    }
};

// 测试函数
void test_myallocator() {
    try {
        std::cout << "=== Testing MyAllocator ===" << std::endl;
        
        // 基本功能测试
        {
            std::cout << "\n--- Basic functionality test ---" << std::endl;
            std::vector<int, MyAllocator<int>> vec;
            
            std::cout << "Adding elements to vector..." << std::endl;
            for (int i = 0; i < 10; ++i) {
                vec.push_back(i * i);
            }
            
            std::cout << "Vector contents: ";
            for (const auto& val : vec) {
                std::cout << val << " ";
            }
            std::cout << std::endl;
            
            std::cout << "Vector size: " << vec.size() << std::endl;
            std::cout << "Vector capacity: " << vec.capacity() << std::endl;
        }
        
        // 自定义对象测试
        {
            std::cout << "\n--- Custom object test ---" << std::endl;
            std::vector<TestObject, MyAllocator<TestObject>> obj_vec;
            
            obj_vec.emplace_back(1, 1.1);
            obj_vec.emplace_back(2, 2.2);
            obj_vec.emplace_back(3, 3.3);
            
            std::cout << "Custom objects created and stored in vector" << std::endl;
        }
        
        // 内存池分配器测试
        {
            std::cout << "\n--- Pool allocator test ---" << std::endl;
            std::vector<int, PoolAllocator<int, 100>> pool_vec;
            
            std::cout << "Initial pool usage: " << PoolAllocator<int, 100>::get_pool_usage() << std::endl;
            
            for (int i = 0; i < 50; ++i) {
                pool_vec.push_back(i);
            }
            
            std::cout << "Pool usage after adding 50 elements: " << PoolAllocator<int, 100>::get_pool_usage() << std::endl;
            std::cout << "Pool vector size: " << pool_vec.size() << std::endl;
        }
        
        // 大对象分配测试
        {
            std::cout << "\n--- Large object allocation test ---" << std::endl;
            MyAllocator<double> alloc;
            
            constexpr std::size_t large_size = 10000;
            double* large_array = alloc.allocate(large_size);
            
            // 初始化数组
            for (std::size_t i = 0; i < large_size; ++i) {
                alloc.construct(large_array + i, static_cast<double>(i) * 0.1);
            }
            
            std::cout << "Large array allocated and initialized" << std::endl;
            std::cout << "First 10 elements: ";
            for (std::size_t i = 0; i < 10; ++i) {
                std::cout << large_array[i] << " ";
            }
            std::cout << std::endl;
            
            // 清理
            for (std::size_t i = 0; i < large_size; ++i) {
                alloc.destroy(large_array + i);
            }
            alloc.deallocate(large_array, large_size);
            
            std::cout << "Large array deallocated" << std::endl;
        }
        
        // 打印统计信息
        std::cout << std::endl;
        AllocatorStats::print_stats();
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

int main() {
    test_myallocator();
    return 0;
}