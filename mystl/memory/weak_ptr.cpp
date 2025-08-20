//
// Created by Kolane on 2025/3/15.
//

#ifndef MY_WEAK_PTR_CPP
#define MY_WEAK_PTR_CPP

// 前向声明
template <typename T> class my_shared_ptr;
template <typename T> class my_weak_ptr;

#include <atomic>
#include <type_traits>
#include <exception>
#include <stdexcept>

// 线程安全的控制块，支持强引用和弱引用
template<typename T>
class ControlBlock {
public:
    explicit ControlBlock(T* ptr) noexcept : ptr_(ptr), strong_count_(1), weak_count_(1) {}

    // 增加强引用计数
    void add_strong_ref() noexcept {
        strong_count_.fetch_add(1, std::memory_order_relaxed);
    }

    // 增加弱引用计数
    void add_weak_ref() noexcept {
        weak_count_.fetch_add(1, std::memory_order_relaxed);
    }

    // 释放强引用，如果是最后一个强引用则删除对象
    void release_strong() noexcept {
        if (strong_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete ptr_;
            ptr_ = nullptr;
            release_weak(); // 释放shared_ptr持有的弱引用
        }
    }

    // 释放弱引用，如果是最后一个弱引用则删除控制块
    void release_weak() noexcept {
        if (weak_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete this;
        }
    }

    // 尝试锁定弱引用为强引用
    bool try_lock() noexcept {
        long current = strong_count_.load(std::memory_order_relaxed);
        do {
            if (current == 0) {
                return false; // 对象已被销毁
            }
        } while (!strong_count_.compare_exchange_weak(current, current + 1, 
                                                      std::memory_order_acq_rel,
                                                      std::memory_order_relaxed));
        return true;
    }

    // 获取强引用计数
    long get_strong_count() const noexcept {
        return strong_count_.load(std::memory_order_relaxed);
    }

    // 获取弱引用计数（不包括shared_ptr持有的）
    long get_weak_count() const noexcept {
        long weak = weak_count_.load(std::memory_order_relaxed);
        long strong = strong_count_.load(std::memory_order_relaxed);
        return weak - (strong > 0 ? 1 : 0);
    }

    // 获取原始指针
    T* get_ptr() const noexcept {
        return ptr_;
    }

    // 检查对象是否已过期
    bool expired() const noexcept {
        return strong_count_.load(std::memory_order_relaxed) == 0;
    }

private:
    T* ptr_;
    std::atomic<long> strong_count_;
    std::atomic<long> weak_count_;
    
    // 禁止拷贝和赋值
    ControlBlock(const ControlBlock&) = delete;
    ControlBlock& operator=(const ControlBlock&) = delete;
};

// my_shared_ptr类的完整定义
template<typename T>
class my_shared_ptr {
public:
    // 默认构造函数
    my_shared_ptr() noexcept : ptr_(nullptr), control_block_(nullptr) {}

    // 构造函数，接受一个指向对象的指针
    explicit my_shared_ptr(T* ptr) : ptr_(ptr), control_block_(new ControlBlock<T>(ptr)) {}

    // 从weak_ptr构造
    explicit my_shared_ptr(const my_weak_ptr<T>& weak_ptr) {
        if (!weak_ptr.control_block_) {
            throw std::runtime_error("Attempting to construct shared_ptr from empty weak_ptr");
        }
        if (weak_ptr.expired()) {
            throw std::runtime_error("Attempting to construct shared_ptr from expired weak_ptr");
        }
        if (weak_ptr.control_block_ && weak_ptr.control_block_->try_lock()) {
            ptr_ = weak_ptr.control_block_->get_ptr();
            control_block_ = weak_ptr.control_block_;
        } else {
            ptr_ = nullptr;
            control_block_ = nullptr;
        }
    }

    // 拷贝构造函数
    my_shared_ptr(const my_shared_ptr& other) : ptr_(other.ptr_), control_block_(other.control_block_) {
        if (control_block_) {
            control_block_->add_strong_ref();
        }
    }

    // 移动构造函数
    my_shared_ptr(my_shared_ptr&& other) noexcept : ptr_(other.ptr_), control_block_(other.control_block_) {
        other.ptr_ = nullptr;
        other.control_block_ = nullptr;
    }

    // 拷贝赋值运算符
    my_shared_ptr& operator=(const my_shared_ptr& other) {
        if (this != &other) {
            if (control_block_) {
                control_block_->release_strong();
            }
            ptr_ = other.ptr_;
            control_block_ = other.control_block_;
            if (control_block_) {
                control_block_->add_strong_ref();
            }
        }
        return *this;
    }

    // 移动赋值运算符
    my_shared_ptr& operator=(my_shared_ptr&& other) noexcept {
        if (this != &other) {
            if (control_block_) {
                control_block_->release_strong();
            }
            ptr_ = other.ptr_;
            control_block_ = other.control_block_;
            other.ptr_ = nullptr;
            other.control_block_ = nullptr;
        }
        return *this;
    }

    // 析构函数
    ~my_shared_ptr() {
        if (control_block_) {
            control_block_->release_strong();
        }
    }

    // 解引用运算符
    T& operator*() const {
        if (!ptr_) {
            throw std::runtime_error("Attempting to dereference null shared_ptr");
        }
        return *ptr_;
    }

    // 成员访问运算符
    T* operator->() const {
        if (!ptr_) {
            throw std::runtime_error("Attempting to access null shared_ptr");
        }
        return ptr_;
    }

    // 获取原始指针
    T* get() const {
        return ptr_;
    }

    // 获取引用计数
    long use_count() const {
        return control_block_ ? control_block_->get_strong_count() : 0;
    }

    // 检查是否为空
    explicit operator bool() const {
        return ptr_ != nullptr;
    }

    // 重置指针
    void reset() noexcept {
        if (control_block_) {
            control_block_->release_strong();
        }
        ptr_ = nullptr;
        control_block_ = nullptr;
    }

    // 重置为新指针
    void reset(T* ptr) {
        if (control_block_) {
            control_block_->release_strong();
        }
        try {
            control_block_ = ptr ? new ControlBlock<T>(ptr) : nullptr;
            ptr_ = ptr;
        } catch (const std::bad_alloc&) {
            delete ptr;  // 确保在异常情况下清理内存
            throw std::runtime_error("Failed to allocate control block");
        }
    }

private:
    T* ptr_;
    ControlBlock<T>* control_block_;
    
    friend class my_weak_ptr<T>;
};

template <typename T>
class my_weak_ptr {
private:
    ControlBlock<T>* control_block_ = nullptr;

    // 辅助方法：释放弱引用
    void release_weak() noexcept {
        if (control_block_) {
            control_block_->release_weak();
            control_block_ = nullptr;
        }
    }

    friend class my_shared_ptr<T>;

public:
    // 默认构造
    my_weak_ptr() = default;

    // 从shared_ptr构造
    my_weak_ptr(const my_shared_ptr<T>& other) noexcept
            : control_block_(other.control_block_) {
        if (control_block_) control_block_->add_weak_ref();
    }

    // 拷贝构造
    my_weak_ptr(const my_weak_ptr& other) noexcept
            : control_block_(other.control_block_) {
        if (control_block_) control_block_->add_weak_ref();
    }

    // 移动构造
    my_weak_ptr(my_weak_ptr&& other) noexcept
            : control_block_(other.control_block_) {
        other.control_block_ = nullptr;
    }

    // 析构函数
    ~my_weak_ptr() {
        release_weak();
    }

    // 拷贝赋值
    my_weak_ptr& operator=(const my_weak_ptr& other) noexcept {
        if (this != &other) {
            release_weak();
            control_block_ = other.control_block_;
            if (control_block_) control_block_->add_weak_ref();
        }
        return *this;
    }

    // 移动赋值
    my_weak_ptr& operator=(my_weak_ptr&& other) noexcept {
        if (this != &other) {
            release_weak();
            control_block_ = other.control_block_;
            other.control_block_ = nullptr;
        }
        return *this;
    }

    // 锁定弱引用，返回shared_ptr
    my_shared_ptr<T> lock() const noexcept {
        if (!control_block_ || !control_block_->try_lock()) {
            return my_shared_ptr<T>();
        }
        // 创建一个临时的shared_ptr来管理引用计数
        my_shared_ptr<T> result;
        result.ptr_ = control_block_->get_ptr();
        result.control_block_ = control_block_;
        // try_lock已经增加了强引用计数，所以不需要再次增加
        return result;
    }
    
    // 交换操作
    void swap(my_weak_ptr& other) noexcept {
        std::swap(control_block_, other.control_block_);
    }

    // 检查是否过期
    bool expired() const noexcept {
        return !control_block_ || control_block_->expired();
    }

    // 获取强引用计数
    long use_count() const noexcept {
        return control_block_ ? control_block_->get_strong_count() : 0;
    }

    // 重置指针
    void reset() noexcept {
        release_weak();
    }
};

#include <iostream>
#include <memory>

// 用于测试循环引用的节点类
struct Node {
    int value;
    my_shared_ptr<Node> next;
    my_weak_ptr<Node> parent; // 使用weak_ptr避免循环引用
    
    Node(int val) : value(val) {
        std::cout << "Node " << value << " created" << std::endl;
    }
    
    ~Node() {
        std::cout << "Node " << value << " destroyed" << std::endl;
    }
};

// 测试weak_ptr功能
void test_weak_ptr() {
    try {
        std::cout << "=== Testing weak_ptr ===" << std::endl;
        
        // 基本功能测试
        {
            std::cout << "\n--- Basic functionality ---" << std::endl;
            my_shared_ptr<int> sp1(new int(42));
            std::cout << "sp1 use_count: " << sp1.use_count() << std::endl;
            
            // 创建weak_ptr
            my_weak_ptr<int> wp1(sp1);
            std::cout << "wp1 expired: " << wp1.expired() << std::endl;
            std::cout << "wp1 use_count: " << wp1.use_count() << std::endl;
            std::cout << "sp1 use_count after creating wp1: " << sp1.use_count() << std::endl;
            
            // 从weak_ptr锁定获取shared_ptr
            my_shared_ptr<int> sp2 = wp1.lock();
            if (sp2) {
                std::cout << "Locked value: " << *sp2 << std::endl;
                std::cout << "sp1 use_count after lock: " << sp1.use_count() << std::endl;
            }
            
            // 重置sp1
            sp1.reset();
            std::cout << "sp1 use_count after reset: " << sp1.use_count() << std::endl;
            std::cout << "sp2 use_count: " << sp2.use_count() << std::endl;
            
            // 重置sp2
            sp2.reset();
            std::cout << "wp1 expired after all shared_ptr reset: " << wp1.expired() << std::endl;
            
            // 尝试从过期的weak_ptr锁定
            my_shared_ptr<int> sp3 = wp1.lock();
            if (!sp3) {
                std::cout << "Cannot lock expired weak_ptr" << std::endl;
            }
        }
        
        // 循环引用测试
        {
            std::cout << "\n--- Circular reference test ---" << std::endl;
            my_shared_ptr<Node> parent(new Node(1));
            my_shared_ptr<Node> child1(new Node(2));
            my_shared_ptr<Node> child2(new Node(3));
            
            // 建立父子关系
            parent->next = child1;
            child1->parent = parent; // 使用weak_ptr避免循环引用
            child1->next = child2;
            child2->parent = child1;
            
            std::cout << "parent use_count: " << parent.use_count() << std::endl;
            std::cout << "child1 use_count: " << child1.use_count() << std::endl;
            std::cout << "child2 use_count: " << child2.use_count() << std::endl;
            
            // 验证weak_ptr可以访问父节点
            if (auto locked_parent = child1->parent.lock()) {
                std::cout << "child1's parent value: " << locked_parent->value << std::endl;
            }
            
            // 重置parent，观察是否正确释放
            parent.reset();
            std::cout << "After parent reset, child1's parent expired: " << child1->parent.expired() << std::endl;
            
            // child1和child2会在作用域结束时自动释放
        }
        
        // 拷贝和移动测试
        {
            std::cout << "\n--- Copy and move test ---" << std::endl;
            my_shared_ptr<int> sp(new int(100));
            my_weak_ptr<int> wp1(sp);
            
            // 拷贝构造
            my_weak_ptr<int> wp2(wp1);
            std::cout << "wp1 use_count: " << wp1.use_count() << std::endl;
            std::cout << "wp2 use_count: " << wp2.use_count() << std::endl;
            
            // 移动构造
            my_weak_ptr<int> wp3(std::move(wp1));
            std::cout << "wp1 expired after move: " << wp1.expired() << std::endl;
            std::cout << "wp3 use_count: " << wp3.use_count() << std::endl;
            
            // 交换测试
            wp1.swap(wp2);
            std::cout << "After swap, wp1 use_count: " << wp1.use_count() << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

int main() {
    test_weak_ptr();
    return 0;
}

#endif // MY_WEAK_PTR_CPP
