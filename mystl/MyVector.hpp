#include <iostream>
#include <stdexcept>

// 迭代器类的定义
template <typename T>
class MyVectorIterator {
public:
    MyVectorIterator(T* ptr) : ptr_(ptr) {}

    T& operator*() const {
        return *ptr_;
    }

    T* operator->() const {
        return ptr_;
    }

    MyVectorIterator& operator++() {
        ++ptr_;
        return *this;
    }

    MyVectorIterator operator++(int) {
        MyVectorIterator temp = *this;
        ++ptr_;
        return temp;
    }

    bool operator==(const MyVectorIterator& other) const {
        return ptr_ == other.ptr_;
    }

    bool operator!=(const MyVectorIterator& other) const {
        return ptr_ != other.ptr_;
    }

private:
    T* ptr_;
};

template <typename T>
class MyVector {
public:
    // 默认构造函数
    MyVector() : size_(0), capacity_(0), data_(nullptr) {}

    // 构造函数，指定初始容量
    explicit MyVector(size_t capacity) : size_(0), capacity_(capacity), data_(new T[capacity]) {}

    // 拷贝构造函数
    MyVector(const MyVector& other) : size_(other.size_), capacity_(other.capacity_), data_(new T[other.capacity_]) {
        for (size_t i = 0; i < size_; ++i) {
            data_[i] = other.data_[i];
        }
    }

    // 析构函数
    ~MyVector() {
        delete[] data_;
    }

    // 拷贝赋值运算符
    MyVector& operator=(const MyVector& other) {
        if (this != &other) {
            delete[] data_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            data_ = new T[capacity_];
            for (size_t i = 0; i < size_; ++i) {
                data_[i] = other.data_[i];
            }
        }
        return *this;
    }

    // 移动构造函数
    MyVector(MyVector&& other) noexcept : size_(other.size_), capacity_(other.capacity_), data_(other.data_) {
        other.size_ = 0;
        other.capacity_ = 0;
        other.data_ = nullptr;
    }

    // 移动赋值运算符
    MyVector& operator=(MyVector&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            data_ = other.data_;
            other.size_ = 0;
            other.capacity_ = 0;
            other.data_ = nullptr;
        }
        return *this;
    }

    // 在末尾添加元素
    void push_back(const T& value) {
        if (size_ == capacity_) {
            reserve(capacity_ == 0 ? 1 : capacity_ * 2);
        }
        data_[size_++] = value;
    }

    // 删除末尾元素
    void pop_back() {
        if (size_ > 0) {
            --size_;
        }
    }

    // 获取元素数量
    size_t size() const {
        return size_;
    }

    // 获取当前容量
    size_t capacity() const {
        return capacity_;
    }

    // 访问元素
    T& operator[](size_t index) {
        if (index >= size_) {
            throw std::out_of_range("Index out of range");
        }
        return data_[index];
    }

    // 访问元素（const版本）
    const T& operator[](size_t index) const {
        if (index >= size_) {
            throw std::out_of_range("Index out of range");
        }
        return data_[index];
    }

    // 返回指向第一个元素的迭代器
    MyVectorIterator<T> begin() {
        return MyVectorIterator<T>(data_);
    }

    // 返回指向最后一个元素之后的迭代器
    MyVectorIterator<T> end() {
        return MyVectorIterator<T>(data_ + size_);
    }

private:
    size_t size_;       // 当前元素数量
    size_t capacity_;   // 当前容量
    T* data_;           // 存储元素的数组

    // 调整容量
    void reserve(size_t new_capacity) {
        if (new_capacity > capacity_) {
            T* new_data = new T[new_capacity];
            for (size_t i = 0; i < size_; ++i) {
                new_data[i] = data_[i];
            }
            delete[] data_;
            data_ = new_data;
            capacity_ = new_capacity;
        }
    }
};
