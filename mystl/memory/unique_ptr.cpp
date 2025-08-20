//
// Created by Kolane on 2025/3/15.
//

#include <iostream>
#include <utility>
#include <type_traits>
#include <functional>
#include <cstdio>
#include <compare>

// 默认删除器
template<typename T>
struct DefaultDeleter {
    constexpr DefaultDeleter() noexcept = default;
    
    template<typename U>
    constexpr DefaultDeleter(const DefaultDeleter<U>&) noexcept 
        requires std::is_convertible_v<U*, T*> {}
    
    constexpr void operator()(T* ptr) const noexcept {
        static_assert(sizeof(T) > 0, "Cannot delete incomplete type");
        delete ptr;
    }
};

// 数组删除器特化
template<typename T>
struct DefaultDeleter<T[]> {
    constexpr DefaultDeleter() noexcept = default;
    
    template<typename U>
    constexpr DefaultDeleter(const DefaultDeleter<U[]>&) noexcept 
        requires std::is_convertible_v<U(*)[], T(*)[]> {}
    
    template<typename U>
    constexpr void operator()(U* ptr) const noexcept 
        requires std::is_convertible_v<U(*)[], T(*)[]> {
        static_assert(sizeof(U) > 0, "Cannot delete incomplete type");
        delete[] ptr;
    }
};

// UniquePtr主模板
template <typename T, typename Deleter = DefaultDeleter<T>>
class UniquePtr {
public:
    using element_type = T;
    using deleter_type = Deleter;
    using pointer = std::conditional_t<std::is_void_v<T>, T*, T*>;

private:
    template<typename U, typename E> friend class UniquePtr;
    pointer ptr_;
    [[no_unique_address]] deleter_type deleter_;

public:
    // 构造函数
    constexpr UniquePtr() noexcept : ptr_(nullptr), deleter_() {}
    
    constexpr UniquePtr(std::nullptr_t) noexcept : ptr_(nullptr), deleter_() {}
    
    template<typename U = T>
    explicit constexpr UniquePtr(pointer ptr) noexcept 
        requires std::is_same_v<U, T> && (!std::is_pointer_v<Deleter>)
        : ptr_(ptr), deleter_() {}
    
    template<typename U = T>
    constexpr UniquePtr(pointer ptr, const deleter_type& d) noexcept 
        requires std::is_same_v<U, T> && std::is_copy_constructible_v<Deleter>
        : ptr_(ptr), deleter_(d) {}
    
    template<typename U = T>
    constexpr UniquePtr(pointer ptr, deleter_type&& d) noexcept 
        requires std::is_same_v<U, T> && std::is_move_constructible_v<Deleter>
        : ptr_(ptr), deleter_(std::move(d)) {}

    // 禁止拷贝
    UniquePtr(const UniquePtr&) = delete;
    UniquePtr& operator=(const UniquePtr&) = delete;

    // 移动构造
    constexpr UniquePtr(UniquePtr&& other) noexcept 
        : ptr_(other.release()), deleter_(std::forward<deleter_type>(other.get_deleter())) {}
    
    // 不同类型的移动构造
    template<typename U, typename E>
    constexpr UniquePtr(UniquePtr<U, E>&& other) noexcept
        requires std::is_convertible_v<typename UniquePtr<U, E>::pointer, pointer> &&
                 (!std::is_array_v<U>) &&
                 std::is_convertible_v<E, Deleter>
        : ptr_(other.release()), deleter_(std::forward<E>(other.get_deleter())) {}

    // 移动赋值
    constexpr UniquePtr& operator=(UniquePtr&& other) noexcept {
        if (this != &other) {
            reset(other.release());
            deleter_ = std::forward<deleter_type>(other.get_deleter());
        }
        return *this;
    }
    
    // 不同类型的移动赋值
    template<typename U, typename E>
    constexpr UniquePtr& operator=(UniquePtr<U, E>&& other) noexcept 
        requires std::is_convertible_v<typename UniquePtr<U, E>::pointer, pointer> &&
                 (!std::is_array_v<U>) &&
                 std::is_assignable_v<Deleter&, E&&> {
        reset(other.release());
        deleter_ = std::forward<E>(other.get_deleter());
        return *this;
    }
    
    // nullptr赋值
    constexpr UniquePtr& operator=(std::nullptr_t) noexcept {
        reset();
        return *this;
    }

    // 析构函数
    constexpr ~UniquePtr() {
        if (ptr_) {
            deleter_(ptr_);
        }
    }

    // 解引用运算符
    constexpr std::add_lvalue_reference_t<T> operator*() const {
        if (!ptr_) {
            throw std::runtime_error("Attempting to dereference null unique_ptr");
        }
        return *ptr_;
    }
    
    constexpr pointer operator->() const {
        if (!ptr_) {
            throw std::runtime_error("Attempting to access null unique_ptr");
        }
        return ptr_;
    }

    // 获取原始指针
    constexpr pointer get() const noexcept { 
        return ptr_; 
    }
    
    // 获取删除器
    constexpr deleter_type& get_deleter() noexcept { 
        return deleter_; 
    }
    
    constexpr const deleter_type& get_deleter() const noexcept { 
        return deleter_; 
    }

    // 释放所有权
    constexpr pointer release() noexcept {
        pointer temp = ptr_;
        ptr_ = nullptr;
        return temp;
    }

    // 重置指针 - 修复了原来的bug
    constexpr void reset(pointer ptr = pointer()) noexcept {
        pointer old_ptr = ptr_;
        ptr_ = ptr;
        if (old_ptr) {
            deleter_(old_ptr);
        }
    }

    // 交换操作
    constexpr void swap(UniquePtr& other) noexcept {
        using std::swap;
        swap(ptr_, other.ptr_);
        swap(deleter_, other.deleter_);
    }

    // 类型转换操作符
    constexpr explicit operator bool() const noexcept {
        return ptr_ != nullptr;
    }
};

// UniquePtr数组特化
template <typename T, typename Deleter>
class UniquePtr<T[], Deleter> {
public:
    using element_type = T;
    using deleter_type = Deleter;
    using pointer = T*;

private:
    pointer ptr_;
    [[no_unique_address]] deleter_type deleter_;

public:
    // 构造函数
    constexpr UniquePtr() noexcept : ptr_(nullptr), deleter_() {}
    
    constexpr UniquePtr(std::nullptr_t) noexcept : ptr_(nullptr), deleter_() {}
    
    template<typename U>
    explicit UniquePtr(U ptr) noexcept : ptr_(ptr), deleter_() {
        static_assert(std::is_same<U, pointer>::value || 
                     (std::is_same<U, std::nullptr_t>::value),
                     "UniquePtr<T[]> can only be constructed with T* or nullptr");
    }
    
    template<typename U>
    UniquePtr(U ptr, const deleter_type& d) noexcept 
        : ptr_(ptr), deleter_(d) {
        static_assert(std::is_same<U, pointer>::value || 
                     (std::is_same<U, std::nullptr_t>::value),
                     "UniquePtr<T[]> can only be constructed with T* or nullptr");
    }
    
    template<typename U>
    UniquePtr(U ptr, deleter_type&& d) noexcept 
        : ptr_(ptr), deleter_(std::move(d)) {
        static_assert(std::is_same<U, pointer>::value || 
                     (std::is_same<U, std::nullptr_t>::value),
                     "UniquePtr<T[]> can only be constructed with T* or nullptr");
    }

    // 禁止拷贝
    UniquePtr(const UniquePtr&) = delete;
    UniquePtr& operator=(const UniquePtr&) = delete;

    // 移动构造
    UniquePtr(UniquePtr&& other) noexcept 
        : ptr_(other.release()), deleter_(std::forward<deleter_type>(other.get_deleter())) {}

    // 移动赋值
    UniquePtr& operator=(UniquePtr&& other) noexcept {
        if (this != &other) {
            reset(other.release());
            deleter_ = std::forward<deleter_type>(other.get_deleter());
        }
        return *this;
    }
    
    // nullptr赋值
    UniquePtr& operator=(std::nullptr_t) noexcept {
        reset();
        return *this;
    }

    // 析构函数
    ~UniquePtr() {
        if (ptr_) {
            deleter_(ptr_);
        }
    }

    // 数组访问运算符
    T& operator[](std::size_t i) const {
        if (!ptr_) {
            throw std::runtime_error("Attempting to access null unique_ptr array");
        }
        return ptr_[i];
    }

    // 获取原始指针
    pointer get() const noexcept { 
        return ptr_; 
    }
    
    // 获取删除器
    deleter_type& get_deleter() noexcept { 
        return deleter_; 
    }
    
    const deleter_type& get_deleter() const noexcept { 
        return deleter_; 
    }

    // 释放所有权
    pointer release() noexcept {
        pointer temp = ptr_;
        ptr_ = nullptr;
        return temp;
    }

    // 重置指针
    template<typename U>
    void reset(U ptr) noexcept {
        static_assert(std::is_same<U, pointer>::value || 
                     std::is_same<U, std::nullptr_t>::value,
                     "UniquePtr<T[]> can only be reset with T* or nullptr");
        pointer old_ptr = ptr_;
        ptr_ = ptr;
        if (old_ptr) {
            deleter_(old_ptr);
        }
    }
    
    void reset(std::nullptr_t = nullptr) noexcept {
        pointer old_ptr = ptr_;
        ptr_ = nullptr;
        if (old_ptr) {
            deleter_(old_ptr);
        }
    }

    // 交换操作
    void swap(UniquePtr& other) noexcept {
        using std::swap;
        swap(ptr_, other.ptr_);
        swap(deleter_, other.deleter_);
    }

    // 类型转换操作符
    explicit operator bool() const noexcept {
        return ptr_ != nullptr;
    }
};

// C++20 三路比较运算符
template<typename T1, typename D1, typename T2, typename D2>
std::strong_ordering operator<=>(const UniquePtr<T1, D1>& x, const UniquePtr<T2, D2>& y) {
    using CT = std::common_type_t<typename UniquePtr<T1, D1>::pointer,
                                  typename UniquePtr<T2, D2>::pointer>;
    return std::compare_three_way{}(x.get(), y.get());
}

template<typename T, typename D>
std::strong_ordering operator<=>(const UniquePtr<T, D>& x, std::nullptr_t) {
    return std::compare_three_way{}(x.get(), static_cast<typename UniquePtr<T, D>::pointer>(nullptr));
}

// 相等比较运算符
template<typename T1, typename D1, typename T2, typename D2>
constexpr bool operator==(const UniquePtr<T1, D1>& x, const UniquePtr<T2, D2>& y) noexcept {
    return x.get() == y.get();
}

template<typename T, typename D>
constexpr bool operator==(const UniquePtr<T, D>& x, std::nullptr_t) noexcept {
    return !x;
}

// C++20 make_unique工厂函数
template<typename T, typename... Args>
constexpr UniquePtr<T> make_unique(Args&&... args) 
    requires (!std::is_array_v<T>) {
    try {
        return UniquePtr<T>(new T(std::forward<Args>(args)...));
    } catch (const std::bad_alloc&) {
        throw std::runtime_error("Failed to allocate memory for unique_ptr");
    }
}

template<typename T>
constexpr UniquePtr<T> make_unique(std::size_t size) 
    requires (std::is_unbounded_array_v<T>) {
    using U = std::remove_extent_t<T>;
    if (size == 0) {
        throw std::invalid_argument("Cannot create array with size 0");
    }
    try {
        return UniquePtr<T>(new U[size]());
    } catch (const std::bad_alloc&) {
        throw std::runtime_error("Failed to allocate memory for unique_ptr array");
    }
}

// 禁止有界数组的make_unique - 通过SFINAE实现
template<typename T, typename... Args>
std::enable_if_t<std::is_bounded_array_v<T>, void> make_unique(Args&&...) = delete;

// C++20 make_unique_for_overwrite
template<typename T>
constexpr UniquePtr<T> make_unique_for_overwrite() 
    requires (!std::is_array_v<T>) {
    return UniquePtr<T>(new T);
}

template<typename T>
constexpr UniquePtr<T> make_unique_for_overwrite(std::size_t size) 
    requires (std::is_unbounded_array_v<T>) {
    using U = std::remove_extent_t<T>;
    return UniquePtr<T>(new U[size]);
}

// 全局swap函数
template<typename T, typename D>
void swap(UniquePtr<T, D>& x, UniquePtr<T, D>& y) noexcept {
    x.swap(y);
}

// 自定义删除器示例
struct FileDeleter {
    void operator()(std::FILE* f) const {
        if (f) {
            std::fclose(f);
            std::cout << "File closed\n";
        }
    }
};

class Student {
public:
    Student(int id, std::string name)
            : id_(id), name_(std::move(name)) {
        std::cout << "Student " << id_ << " created\n";
    }
    
    ~Student() {
        std::cout << "Student " << id_ << " destroyed\n";
    }

    void print() const {
        std::cout << "ID: " << id_
                  << ", Name: " << name_ << "\n";
    }

private:
    int id_;
    std::string name_;
};

// 测试用的简单类
struct Person {
    int id;
    std::string name;
    
    Person() : id(0), name("") {
        std::cout << "Person default created\n";
    }
    
    Person(int i, const std::string& n) : id(i), name(n) {
        std::cout << "Person " << id << " (" << name << ") created\n";
    }
    
    ~Person() {
        std::cout << "Person " << id << " (" << name << ") destroyed\n";
    }
};

// 测试函数
void test_unique_ptr() {
    try {
        std::cout << "=== Testing UniquePtr ===" << std::endl;
        
        // 测试基本功能
        UniquePtr<Person> p1 = make_unique<Person>(1, "Alice");
        std::cout << "ID: " << p1->id << ", Name: " << p1->name << std::endl;

        // 测试移动语义
        UniquePtr<Person> p2 = std::move(p1);
        if (!p1) {
            std::cout << "p1 is empty after move\n";
        }
        std::cout << "ID: " << p2->id << ", Name: " << p2->name << std::endl;
        
        // 测试make_unique_for_overwrite
        auto p4 = make_unique_for_overwrite<Person>();
        p4->id = 4;
        p4->name = "David";
        std::cout << "ID: " << p4->id << ", Name: " << p4->name << std::endl;
        
        // 测试数组
        auto arr = make_unique<int[]>(5);
        for (int i = 0; i < 5; ++i) {
            arr[i] = i * 2;
        }
        std::cout << "Array: ";
        for (int i = 0; i < 5; ++i) {
            std::cout << arr[i] << " ";
        }
        std::cout << std::endl;
        
        // 测试比较运算符
        UniquePtr<int> null_ptr;
        auto int_ptr = make_unique<int>(42);
        std::cout << "null_ptr == nullptr: " << (null_ptr == nullptr) << std::endl;
        std::cout << "int_ptr != nullptr: " << (int_ptr != nullptr) << std::endl;
        
        // 测试reset和release
        auto temp_ptr = make_unique<int>(100);
        int* raw_ptr = temp_ptr.release();
        std::cout << "Released value: " << *raw_ptr << std::endl;
        delete raw_ptr; // 手动删除
        
        // 测试swap
        auto ptr1 = make_unique<int>(10);
        auto ptr2 = make_unique<int>(20);
        std::cout << "Before swap: ptr1=" << *ptr1 << ", ptr2=" << *ptr2 << std::endl;
        ptr1.swap(ptr2);
        std::cout << "After swap: ptr1=" << *ptr1 << ", ptr2=" << *ptr2 << std::endl;
        
        // 测试自定义删除器
        {
            UniquePtr<std::FILE, FileDeleter> file_ptr(std::fopen("test.txt", "w"));
            if (file_ptr) {
                std::fprintf(file_ptr.get(), "Hello, World!\n");
            }
        } // 文件在这里自动关闭
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

int main() {
    test_unique_ptr();
    return 0;
}
