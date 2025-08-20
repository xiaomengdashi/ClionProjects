#include <iostream>
#include <string>
#include <atomic>
#include <memory>
#include <type_traits>
#include <utility>
#include <stdexcept>

// 线程安全的引用计数控制块
template<typename T>
class ControlBlock {
public:
    explicit ControlBlock(T* ptr) : ptr_(ptr), strong_count_(1), weak_count_(0) {
        // 允许nullptr，但不允许无效指针
        // 这里我们假设传入的指针要么是nullptr，要么是有效的
    }

    // 增加强引用计数
    void add_strong_ref() noexcept {
        strong_count_.fetch_add(1, std::memory_order_relaxed);
    }

    // 减少强引用计数，返回新的计数值
    long release_strong() noexcept {
        long old_count = strong_count_.fetch_sub(1, std::memory_order_acq_rel);
        if (old_count == 1) {
            destroy_object();
            if (weak_count_.load(std::memory_order_acquire) == 0) {
                delete this;
            }
        }
        return old_count - 1;
    }

    // 增加弱引用计数
    void add_weak_ref() noexcept {
        weak_count_.fetch_add(1, std::memory_order_relaxed);
    }

    // 减少弱引用计数
    void release_weak() noexcept {
        if (weak_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            if (strong_count_.load(std::memory_order_acquire) == 0) {
                delete this;
            }
        }
    }

    // 获取强引用计数
    long get_strong_count() const noexcept {
        return strong_count_.load(std::memory_order_acquire);
    }

    // 获取弱引用计数
    long get_weak_count() const noexcept {
        return weak_count_.load(std::memory_order_acquire);
    }

    // 获取原始指针
    T* get() const noexcept {
        return ptr_;
    }

    // 尝试增加强引用计数（用于weak_ptr::lock）
    bool try_add_strong_ref() noexcept {
        long current = strong_count_.load(std::memory_order_relaxed);
        do {
            if (current == 0) return false;
        } while (!strong_count_.compare_exchange_weak(current, current + 1, 
                                                     std::memory_order_acq_rel, 
                                                     std::memory_order_relaxed));
        return true;
    }

private:
    void destroy_object() noexcept {
        delete ptr_;
        ptr_ = nullptr;
    }

    T* ptr_;
    std::atomic<long> strong_count_;
    std::atomic<long> weak_count_;
};

// 前向声明
template<typename T> class MyWeakPtr;

// 改进的shared_ptr实现
template<typename T>
class MySharedPtr {
public:
    using element_type = T;
    using weak_type = MyWeakPtr<T>;

    // 默认构造函数
    constexpr MySharedPtr() noexcept : ptr_(nullptr), control_block_(nullptr) {}
    
    // nullptr构造函数
    constexpr MySharedPtr(std::nullptr_t) noexcept : ptr_(nullptr), control_block_(nullptr) {}

    // 构造函数，接受一个指向对象的指针
    template<typename Y>
    explicit MySharedPtr(Y* ptr) : ptr_(ptr), control_block_(nullptr) {
        static_assert(std::is_convertible_v<Y*, T*>, "Y* must be convertible to T*");
        try {
            control_block_ = new ControlBlock<Y>(ptr);
        } catch (...) {
            delete ptr;
            throw;
        }
    }

    // 拷贝构造函数
    MySharedPtr(const MySharedPtr& other) noexcept 
        : ptr_(other.ptr_), control_block_(other.control_block_) {
        if (control_block_) {
            control_block_->add_strong_ref();
        }
    }

    // 不同类型的拷贝构造函数
    template<typename Y>
    MySharedPtr(const MySharedPtr<Y>& other) noexcept 
        : ptr_(other.ptr_), control_block_(other.control_block_) {
        static_assert(std::is_convertible_v<Y*, T*>, "Y* must be convertible to T*");
        if (control_block_) {
            control_block_->add_strong_ref();
        }
    }

    // 移动构造函数
    MySharedPtr(MySharedPtr&& other) noexcept 
        : ptr_(other.ptr_), control_block_(other.control_block_) {
        other.ptr_ = nullptr;
        other.control_block_ = nullptr;
    }

    // 不同类型的移动构造函数
    template<typename Y>
    MySharedPtr(MySharedPtr<Y>&& other) noexcept 
        : ptr_(other.ptr_), control_block_(other.control_block_) {
        static_assert(std::is_convertible_v<Y*, T*>, "Y* must be convertible to T*");
        other.ptr_ = nullptr;
        other.control_block_ = nullptr;
    }

    // 拷贝赋值运算符
    MySharedPtr& operator=(const MySharedPtr& other) noexcept {
        if (this != &other) {
            MySharedPtr temp(other);
            swap(temp);
        }
        return *this;
    }

    // 不同类型的拷贝赋值运算符
    template<typename Y>
    MySharedPtr& operator=(const MySharedPtr<Y>& other) noexcept {
        static_assert(std::is_convertible_v<Y*, T*>, "Y* must be convertible to T*");
        MySharedPtr temp(other);
        swap(temp);
        return *this;
    }

    // 移动赋值运算符
    MySharedPtr& operator=(MySharedPtr&& other) noexcept {
        if (this != &other) {
            MySharedPtr temp(std::move(other));
            swap(temp);
        }
        return *this;
    }

    // 不同类型的移动赋值运算符
    template<typename Y>
    MySharedPtr& operator=(MySharedPtr<Y>&& other) noexcept {
        static_assert(std::is_convertible_v<Y*, T*>, "Y* must be convertible to T*");
        MySharedPtr temp(std::move(other));
        swap(temp);
        return *this;
    }

    // nullptr赋值
    MySharedPtr& operator=(std::nullptr_t) noexcept {
        reset();
        return *this;
    }

    // 析构函数
    ~MySharedPtr() {
        if (control_block_) {
            control_block_->release_strong();
        }
    }

    // 解引用运算符
    std::add_lvalue_reference_t<T> operator*() const {
        if (!ptr_) {
            throw std::runtime_error("Attempting to dereference null shared_ptr");
        }
        return *ptr_;
    }

    // 箭头运算符
    T* operator->() const {
        if (!ptr_) {
            throw std::runtime_error("Attempting to access null shared_ptr");
        }
        return ptr_;
    }

    // 获取原始指针
    T* get() const noexcept {
        return ptr_;
    }

    // 获取引用计数
    long use_count() const noexcept {
        return control_block_ ? control_block_->get_strong_count() : 0;
    }

    // 检查是否唯一
    bool unique() const noexcept {
        return use_count() == 1;
    }

    // 类型转换操作符
    explicit operator bool() const noexcept {
        return ptr_ != nullptr;
    }

    // 重置指针
    void reset() noexcept {
        MySharedPtr().swap(*this);
    }

    template<typename Y>
    void reset(Y* ptr) {
        static_assert(std::is_convertible_v<Y*, T*>, "Y* must be convertible to T*");
        MySharedPtr(ptr).swap(*this);
    }

    // 交换操作
    void swap(MySharedPtr& other) noexcept {
        std::swap(ptr_, other.ptr_);
        std::swap(control_block_, other.control_block_);
    }

private:
    template<typename Y> friend class MySharedPtr;
    template<typename Y> friend class MyWeakPtr;
    
    T* ptr_;
    ControlBlock<T>* control_block_;
    
    // 内部构造函数，用于从weak_ptr构造
    MySharedPtr(T* ptr, ControlBlock<T>* control_block) noexcept
        : ptr_(ptr), control_block_(control_block) {
        if (control_block_) {
            control_block_->add_strong_ref();
        }
    }
};

// make_shared工厂函数
template<typename T, typename... Args>
MySharedPtr<T> make_shared(Args&&... args) {
    T* ptr = nullptr;
    try {
        ptr = new T(std::forward<Args>(args)...);
        return MySharedPtr<T>(ptr);
    } catch (...) {
        delete ptr;  // 确保在异常情况下清理内存
        throw;
    }
}

// 比较运算符
template<typename T, typename Y>
bool operator==(const MySharedPtr<T>& lhs, const MySharedPtr<Y>& rhs) noexcept {
    return lhs.get() == rhs.get();
}

template<typename T>
bool operator==(const MySharedPtr<T>& lhs, std::nullptr_t) noexcept {
    return !lhs;
}

template<typename T>
bool operator==(std::nullptr_t, const MySharedPtr<T>& rhs) noexcept {
    return !rhs;
}

template<typename T, typename Y>
bool operator!=(const MySharedPtr<T>& lhs, const MySharedPtr<Y>& rhs) noexcept {
    return !(lhs == rhs);
}

template<typename T>
bool operator!=(const MySharedPtr<T>& lhs, std::nullptr_t) noexcept {
    return static_cast<bool>(lhs);
}

template<typename T>
bool operator!=(std::nullptr_t, const MySharedPtr<T>& rhs) noexcept {
    return static_cast<bool>(rhs);
}

// 测试函数
void test_myshared_ptr() {
    try {
        // 创建一个shared_ptr
        MySharedPtr<int> p1(new int(10));
        std::cout << *p1 << std::endl; // 输出: 10
        std::cout << p1.use_count() << std::endl; // 输出: 1

        // 拷贝构造
        MySharedPtr<int> p2 = p1;
        MySharedPtr<int> p3(p1);
        std::cout << p1.use_count() << std::endl; // 输出: 3
        MySharedPtr<int> p4 = std::move(p1);
        std::cout << p4.use_count() << std::endl; // 输出: 3

        // 移动构造
        MySharedPtr<int> p5;
        p5 = std::move(p2);
        std::cout << p5.use_count() << std::endl; // 输出: 3

        // 修改值
        MySharedPtr<int> s1(new int(21));
        *s1 = 21;
        std::cout << *s1 << std::endl; // 输出: 21
        
        // 测试make_shared
        auto s2 = make_shared<int>(42);
        std::cout << *s2 << std::endl; // 输出: 42
        
        // 测试比较运算符
        MySharedPtr<int> null_ptr;
        std::cout << (null_ptr == nullptr) << std::endl; // 输出: 1
        std::cout << (s2 != nullptr) << std::endl; // 输出: 1
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

int main() {
    test_myshared_ptr();
    return 0;
}