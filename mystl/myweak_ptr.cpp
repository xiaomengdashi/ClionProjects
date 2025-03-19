//
// Created by Kolane on 2025/3/15.
//



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
        return ref_count_ == nullptr || ref_count_->get_count() == 0;
    }

    // 重置指针
    void reset() {
        release_weak();
    }
};

// 需要修改的RefCount类
template <typename T>
class RefCount {
public:
    RefCount(T* ptr = nullptr)
            : ptr_(ptr), strong_count_(1), weak_count_(0) {}

    void add_strong() { ++strong_count_; }
    void add_weak() { ++weak_count_; }

    int release_strong() {
        if (--strong_count_ == 0) {
            delete ptr_;  // 仅删除对象，保留控制块
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

// 修改后的my_shared_ptr需要增加的友元声明
template <typename U>
friend class my_weak_ptr;
