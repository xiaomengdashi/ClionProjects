#include <iostream>

template<typename T>
class RefCount {
public:
    RefCount(T* ptr = nullptr) : ptr_(ptr), count_(ptr ? 1 : 0) {}

    void addRef() {
        ++count_;
    }

    int release() {
        return --count_;
    }

    T* get() const {
        return ptr_;
    }

    int get_count() {
        return count_;
    }

private:
    T* ptr_;
    int count_;
};

template<typename T>
class my_shared_ptr {
public:
    // 默认构造函数
    my_shared_ptr() : ptr_(nullptr), ref_count_(nullptr) {}

    // 构造函数，接受一个指向对象的指针
    explicit my_shared_ptr(T* ptr) : ptr_(ptr), ref_count_(new RefCount<T>(ptr)) {}

    // 拷贝构造函数
    my_shared_ptr(const my_shared_ptr& other) : ptr_(other.ptr_), ref_count_(other.ref_count_) {
        if (ref_count_) {
            ref_count_->addRef();
        }
    }

    // 拷贝赋值运算符
    my_shared_ptr& operator=(const my_shared_ptr& other) {
        if (this != &other) {
            if (ref_count_ && ref_count_->release() == 0) {
                delete ptr_;
                delete ref_count_;
            }
            ptr_ = other.ptr_;
            ref_count_ = other.ref_count_;
            if (ref_count_) {
                ref_count_->addRef();
            }
        }
        return *this;
    }

    // 析构函数
    ~my_shared_ptr() {
        if (ref_count_ && ref_count_->release() == 0) {
            delete ptr_;
            delete ref_count_;
        }
    }

    // 解引用运算符
    T& operator*() const {
        return *ptr_;
    }

    // 箭头运算符
    T* operator->() const {
        return ptr_;
    }

    // 获取原始指针
    T* get() const {
        return ptr_;
    }

    // 获取引用计数
    int use_count() const {
        return ref_count_ ? ref_count_->get_count() : 0;
    }

private:
    T* ptr_;
    RefCount<T>* ref_count_;
};
