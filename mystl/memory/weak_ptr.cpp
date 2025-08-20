//
// Created by Kolane on 2025/3/15.
//

#ifndef MY_WEAK_PTR_CPP
#define MY_WEAK_PTR_CPP

// 前向声明
template <typename T> class my_shared_ptr;
template <typename T> class my_weak_ptr;

// RefCount类定义
template <typename T>
class RefCount {
public:
    RefCount(T* ptr = nullptr)
            : ptr_(ptr), strong_count_(1), weak_count_(0) {}

    void add_weak() { ++weak_count_; }
    
    void add_strong() { ++strong_count_; }

    int release_strong() {
        if (--strong_count_ == 0) {
            delete ptr_;
            ptr_ = nullptr;
        }
        return strong_count_;
    }

    int release_weak() {
        return --weak_count_;
    }

    int get_strong_count() const { return strong_count_; }
    int get_weak_count() const { return weak_count_; }

private:
    T* ptr_;
    int strong_count_;
    int weak_count_;
};

// my_shared_ptr类的完整定义
template<typename T>
class my_shared_ptr {
public:
    // 默认构造函数
    my_shared_ptr() : ptr_(nullptr), ref_count_(nullptr) {}

    // 构造函数，接受一个指向对象的指针
    explicit my_shared_ptr(T* ptr) : ptr_(ptr), ref_count_(new RefCount<T>(ptr)) {}

    // 从weak_ptr构造
    explicit my_shared_ptr(const my_weak_ptr<T>& weak_ptr) {
        if (!weak_ptr.expired()) {
            ptr_ = weak_ptr.ptr_;
            ref_count_ = weak_ptr.ref_count_;
            if (ref_count_) {
                ref_count_->add_strong();
            }
        } else {
            ptr_ = nullptr;
            ref_count_ = nullptr;
        }
    }

    // 拷贝构造函数
    my_shared_ptr(const my_shared_ptr& other) : ptr_(other.ptr_), ref_count_(other.ref_count_) {
        if (ref_count_) {
            ref_count_->add_strong();
        }
    }

    // 移动构造函数
    my_shared_ptr(my_shared_ptr&& other) noexcept : ptr_(other.ptr_), ref_count_(other.ref_count_) {
        other.ptr_ = nullptr;
        other.ref_count_ = nullptr;
    }

    // 拷贝赋值运算符
    my_shared_ptr& operator=(const my_shared_ptr& other) {
        if (this != &other) {
            if (ref_count_ && ref_count_->release_strong() == 0) {
                if (ref_count_->get_weak_count() == 0) {
                    delete ref_count_;
                }
            }
            ptr_ = other.ptr_;
            ref_count_ = other.ref_count_;
            if (ref_count_) {
                ref_count_->add_strong();
            }
        }
        return *this;
    }

    // 移动赋值运算符
    my_shared_ptr& operator=(my_shared_ptr&& other) noexcept {
        if (this != &other) {
            if (ref_count_ && ref_count_->release_strong() == 0) {
                if (ref_count_->get_weak_count() == 0) {
                    delete ref_count_;
                }
            }
            ptr_ = other.ptr_;
            ref_count_ = other.ref_count_;
            other.ptr_ = nullptr;
            other.ref_count_ = nullptr;
        }
        return *this;
    }

    // 析构函数
    ~my_shared_ptr() {
        if (ref_count_ && ref_count_->release_strong() == 0) {
            if (ref_count_->get_weak_count() == 0) {
                delete ref_count_;
            }
        }
    }

    // 解引用运算符
    T& operator*() const {
        return *ptr_;
    }

    // 成员访问运算符
    T* operator->() const {
        return ptr_;
    }

    // 获取原始指针
    T* get() const {
        return ptr_;
    }

    // 获取引用计数
    int use_count() const {
        return ref_count_ ? ref_count_->get_strong_count() : 0;
    }

    // 检查是否为空
    explicit operator bool() const {
        return ptr_ != nullptr;
    }

private:
    T* ptr_;
    RefCount<T>* ref_count_;
    
    friend class my_weak_ptr<T>;
};

template <typename T>
class my_weak_ptr {
private:
    T* ptr_ = nullptr;
    RefCount<T>* ref_count_ = nullptr;

    // 辅助方法：释放弱引用
    void release_weak() {
        if (ref_count_ && ref_count_->release_weak() == 0) {
            delete ref_count_;
        }
        ptr_ = nullptr;
        ref_count_ = nullptr;
    }

    friend class my_shared_ptr<T>;

public:
    // 默认构造
    my_weak_ptr() = default;

    // 从shared_ptr构造
    my_weak_ptr(const my_shared_ptr<T>& other) noexcept
            : ptr_(other.ptr_), ref_count_(other.ref_count_) {
        if (ref_count_) ref_count_->add_weak();
    }

    // 拷贝构造
    my_weak_ptr(const my_weak_ptr& other) noexcept
            : ptr_(other.ptr_), ref_count_(other.ref_count_) {
        if (ref_count_) ref_count_->add_weak();
    }

    // 移动构造
    my_weak_ptr(my_weak_ptr&& other) noexcept
            : ptr_(other.ptr_), ref_count_(other.ref_count_) {
        other.ptr_ = nullptr;
        other.ref_count_ = nullptr;
    }

    // 析构函数
    ~my_weak_ptr() {
        release_weak();
    }

    // 拷贝赋值
    my_weak_ptr& operator=(const my_weak_ptr& other) noexcept {
        if (this != &other) {
            release_weak();
            ptr_ = other.ptr_;
            ref_count_ = other.ref_count_;
            if (ref_count_) ref_count_->add_weak();
        }
        return *this;
    }

    // 移动赋值
    my_weak_ptr& operator=(my_weak_ptr&& other) noexcept {
        if (this != &other) {
            release_weak();
            ptr_ = other.ptr_;
            ref_count_ = other.ref_count_;
            other.ptr_ = nullptr;
            other.ref_count_ = nullptr;
        }
        return *this;
    }

    // 转换为shared_ptr
    my_shared_ptr<T> lock() const {
        if (expired()) return my_shared_ptr<T>();
        return my_shared_ptr<T>(*this);
    }

    // 检查是否过期
    bool expired() const {
        return ref_count_ == nullptr || ref_count_->get_strong_count() == 0;
    }

    // 重置指针
    void reset() {
        release_weak();
    }
};

#include <iostream>
#include <memory>

// 测试weak_ptr功能
void test_weak_ptr() {
    std::cout << "=======Test Weak Ptr=======" << std::endl;
    
    // 测试基本功能
    {
        my_shared_ptr<int> sp1(new int(42));
        std::cout << "shared_ptr created with value: " << *sp1 << std::endl;
        
        my_weak_ptr<int> wp1(sp1);
        std::cout << "weak_ptr created from shared_ptr" << std::endl;
        std::cout << "weak_ptr expired: " << wp1.expired() << std::endl;
        
        // 通过weak_ptr获取shared_ptr
        auto sp2 = wp1.lock();
        if (sp2) {
            std::cout << "locked weak_ptr, value: " << *sp2 << std::endl;
        }
        
        // 测试拷贝构造
        my_weak_ptr<int> wp2(wp1);
        std::cout << "weak_ptr copied" << std::endl;
        
        // 测试赋值
        my_weak_ptr<int> wp3;
        wp3 = wp1;
        std::cout << "weak_ptr assigned" << std::endl;
    }
    
    std::cout << "shared_ptr destroyed, testing expiration..." << std::endl;
    
    // 测试过期检测
    my_weak_ptr<int> wp_expired;
    {
        my_shared_ptr<int> sp_temp(new int(100));
        wp_expired = my_weak_ptr<int>(sp_temp);
        std::cout << "weak_ptr expired before destruction: " << wp_expired.expired() << std::endl;
    }
    std::cout << "weak_ptr expired after destruction: " << wp_expired.expired() << std::endl;
    
    // 测试lock返回空指针
    auto locked = wp_expired.lock();
    if (!locked) {
        std::cout << "lock() returned empty shared_ptr for expired weak_ptr" << std::endl;
    }
    
    // 测试reset
    my_shared_ptr<int> sp3(new int(200));
    my_weak_ptr<int> wp4(sp3);
    std::cout << "Before reset - expired: " << wp4.expired() << std::endl;
    wp4.reset();
    std::cout << "After reset - expired: " << wp4.expired() << std::endl;
}

int main() {
    test_weak_ptr();
    return 0;
}

#endif // MY_WEAK_PTR_CPP
