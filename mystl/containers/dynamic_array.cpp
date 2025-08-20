//
// Created by Kolane on 2025/3/14.
//

#include <iostream>
#include <stdexcept>
#include <cstdlib>

// 实现一个简单的动态数组类, 允许插入和删除元素,自动拓展
class DynamicArray {
public:
    explicit DynamicArray() : capacity_(2), size_(0),
        data_((int*)malloc(sizeof(int) * capacity_)) {
        if (data_ == nullptr) {
            std::cerr << "malloc failed" << std::endl;
            throw std::bad_alloc();
        }
    }

    ~DynamicArray() {
        free(data_);
    }

    void add(int value) {
        if (size_ == capacity_) {
            resize(capacity_ * 2);
        }
        data_[size_++] = value;
    }

    void remove(int index) {
        if (index < 0 || index >= size_) {
            std::cerr << "index out of range" << std::endl;
            return;
        }
        for (int i = index; i < size_ - 1; ++i) {
            data_[i] = data_[i + 1];
        }
        --size_;
    }

    void print() {
        for (int i = 0; i < size_; ++i) {
            std::cout << data_[i] << " ";
        }
        std::cout << std::endl;
    }

    int get(int index) {
        if (index < 0 || index >= size_) {
            std::cerr << "index out of range" << std::endl;
            return -1;
        }
        return data_[index];
    }

    void set(int index, int value) {
        if (index < 0 || index >= size_) {
            std::cerr << "index out of range" << std::endl;
            return;
        }
        data_[index] = value;
    }

    int size() {
        return size_;
    }

    int capacity() {
        return capacity_;
    }

    void clear() {
        size_ = 0;
    }

    void insert(int index, int value) {
        if (index < 0 || index > size_) {
            std::cerr << "index out of range" << std::endl;
            throw std::out_of_range("index out of range");
        }
        if (size_ == capacity_) {
            resize(capacity_ * 2);
        }
        for (int i = size_; i > index; --i) {
            data_[i] = data_[i - 1];
        }
        data_[index] = value;
        ++size_;
    }

    void erase(int index) {
        if (index < 0 || index >= size_) {
            std::cerr << "index out of range" << std::endl;
            return;
        }
        for (int i = index; i < size_ - 1; ++i) {
            data_[i] = data_[i + 1];
        }
        --size_;
    }

    void reserve(int new_capacity) {
        if (new_capacity < 0) {
            std::cerr << "new capacity must be greater than 0" << std::endl;
            return;
        }
        resize(new_capacity);
        capacity_ = new_capacity;
    }

private:
    void resize(int new_capacity) {
        if (new_capacity < 0) {
            std::cerr << "new size must be greater than 0" << std::endl;
            return;
        }
        if (new_capacity > capacity_) {
            capacity_ = new_capacity;
            data_ = (int*)realloc(data_, sizeof(int) * capacity_);
            if (data_ == nullptr) {
                std::cerr << "realloc failed" << std::endl;
                throw std::bad_alloc();
            }
        }
    }

private:
    int capacity_;
    int size_;
    int* data_;
};

// 测试函数
void test_dynamic_array() {
    std::cout << "=======test_dynamic_array========" << std::endl;
    DynamicArray arr;
    
    // 添加元素
    arr.add(1);
    arr.add(2);
    arr.add(3);
    arr.add(4);
    arr.add(5);
    
    std::cout << "After adding elements: ";
    arr.print();
    std::cout << "Size: " << arr.size() << ", Capacity: " << arr.capacity() << std::endl;
    
    // 插入元素
    arr.insert(2, 99);
    std::cout << "After inserting 99 at index 2: ";
    arr.print();
    
    // 删除元素
    arr.remove(1);
    std::cout << "After removing element at index 1: ";
    arr.print();
    
    // 获取和设置元素
    std::cout << "Element at index 0: " << arr.get(0) << std::endl;
    arr.set(0, 100);
    std::cout << "After setting index 0 to 100: ";
    arr.print();
    
    // 清空数组
    arr.clear();
    std::cout << "After clear, size: " << arr.size() << std::endl;
}

int main() {
    test_dynamic_array();
    return 0;
}