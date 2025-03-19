//
// Created by Kolane on 2025/3/15.
//

#include <iostream>

template <typename T>
class UniquePtr {
private:
    T* ptr_ = nullptr;

public:
    // 默认构造
    explicit UniquePtr(T* ptr = nullptr) : ptr_(ptr) {}

    // 禁止拷贝
    UniquePtr(const UniquePtr&) = delete;
    UniquePtr& operator=(const UniquePtr&) = delete;

    // 移动构造
    UniquePtr(UniquePtr&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }

    // 移动赋值
    UniquePtr& operator=(UniquePtr&& other) noexcept {
        if (this != &other) {
            delete ptr_;
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    // 析构函数
    ~UniquePtr() {
        delete ptr_;
    }

    // 解引用运算符
    T& operator*() const { return *ptr_; }
    T* operator->() const { return ptr_; }

    // 获取原始指针
    T* get() const { return ptr_; }

    // 释放所有权
    T* release() {
        T* temp = ptr_;
        ptr_ = nullptr;
        return temp;
    }

    // 重置指针
    void reset(T* ptr = nullptr) {
        delete ptr_;
        ptr_ = ptr;
    }

    // 交换操作
    void swap(UniquePtr& other) {
        std::swap(ptr_, other.ptr_);
    }

    // 类型转换操作符
    explicit operator bool() const {
        return ptr_ != nullptr;
    }
};

class Student {
public:
    Student(int id, std::string name)
            : id_(id), name_(std::move(name)) {}

    void print() const {
        std::cout << "ID: " << id_
                  << ", Name: " << name_ << "\n";
    }

private:
    int id_;
    std::string name_;
};

int main() {
    UniquePtr<Student> p1(new Student(1, "Alice"));
    p1->print(); // 输出：ID: 1, Name: Alice

    UniquePtr<Student> p2 = std::move(p1); // 所有权转移
    if (!p1) {
        std::cout << "p1 is empty\n"; // 会被执行
    }

    p2.reset(new Student(2, "Bob")); // 释放原对象，创建新对象
    p2->print(); // 输出：ID: 2, Name: Bob

    Student* raw_ptr = p2.release(); // 放弃所有权
    delete raw_ptr; // 需要手动释放

    return 0;
}
