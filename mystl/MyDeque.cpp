#include <iostream>
#include <stdexcept>
#include <iterator>

// 基于环形数组的双端队列
template <typename T>
class Deque {
private:
    T* buffer;               // 内部缓冲区
    size_t capacity;         // 缓冲区容量
    size_t front_idx;        // 头部索引
    size_t back_idx;         // 尾部索引
    size_t count;            // 当前元素数量

    // 调整容量
    void resize(size_t new_capacity) {
        T* new_buffer = new T[new_capacity];
        // 重新排列元素
        for (size_t i = 0; i < count; ++i) {
            new_buffer[i] = buffer[(front_idx + i) % capacity];
        }
        delete[] buffer;
        buffer = new_buffer;
        capacity = new_capacity;
        front_idx = 0;
        back_idx = count;
    }

public:
    // 构造函数
    Deque(size_t initial_capacity = 8)
            : capacity(initial_capacity), front_idx(0), back_idx(0), count(0) {
        buffer = new T[capacity];
    }

    // 析构函数
    ~Deque() {
        delete[] buffer;
    }

    // 禁用复制构造函数和赋值运算符（为了简洁，可根据需要实现）
    Deque(const Deque& other) = delete;
    Deque& operator=(const Deque& other) = delete;

    // 检查是否为空
    bool empty() const {
        return count == 0;
    }

    // 获取大小
    size_t size() const {
        return count;
    }

    // 在前面插入元素
    void push_front(const T& value) {
        if (count == capacity) {
            resize(capacity * 2);
        }
        front_idx = (front_idx == 0) ? capacity - 1 : front_idx - 1;
        buffer[front_idx] = value;
        ++count;
    }

    // 在后面插入元素
    void push_back(const T& value) {
        if (count == capacity) {
            resize(capacity * 2);
        }
        buffer[back_idx] = value;
        back_idx = (back_idx + 1) % capacity;
        ++count;
    }

    // 从前面删除元素
    void pop_front() {
        if (empty()) {
            throw std::out_of_range("Deque is empty");
        }
        front_idx = (front_idx + 1) % capacity;
        --count;
    }

    // 从后面删除元素
    void pop_back() {
        if (empty()) {
            throw std::out_of_range("Deque is empty");
        }
        back_idx = (back_idx == 0) ? capacity - 1 : back_idx - 1;
        --count;
    }

    // 获取前端元素
    T& front() {
        if (empty()) {
            throw std::out_of_range("Deque is empty");
        }
        return buffer[front_idx];
    }

    const T& front() const {
        if (empty()) {
            throw std::out_of_range("Deque is empty");
        }
        return buffer[front_idx];
    }

    // 获取后端元素
    T& back() {
        if (empty()) {
            throw std::out_of_range("Deque is empty");
        }
        size_t last_idx = (back_idx == 0) ? capacity - 1 : back_idx - 1;
        return buffer[last_idx];
    }

    const T& back() const {
        if (empty()) {
            throw std::out_of_range("Deque is empty");
        }
        size_t last_idx = (back_idx == 0) ? capacity - 1 : back_idx - 1;
        return buffer[last_idx];
    }

    // 迭代器类定义
    class Iterator {
    private:
        Deque<T>* deque_ptr;
        size_t pos;

    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using pointer           = T*;
        using reference         = T&;

        Iterator(Deque<T>* deque, size_t position)
                : deque_ptr(deque), pos(position) {}

        // 解引用操作
        reference operator*() const {
            size_t real_idx = (deque_ptr->front_idx + pos) % deque_ptr->capacity;
            return deque_ptr->buffer[real_idx];
        }

        pointer operator->() const {
            size_t real_idx = (deque_ptr->front_idx + pos) % deque_ptr->capacity;
            return &(deque_ptr->buffer[real_idx]);
        }

        // 前置递增
        Iterator& operator++() {
            ++pos;
            return *this;
        }

        // 后置递增
        Iterator operator++(int) {
            Iterator temp = *this;
            ++pos;
            return temp;
        }

        // 前置递减
        Iterator& operator--() {
            --pos;
            return *this;
        }

        // 后置递减
        Iterator operator--(int) {
            Iterator temp = *this;
            --pos;
            return temp;
        }

        // 比较操作
        bool operator==(const Iterator& other) const {
            return (deque_ptr == other.deque_ptr) && (pos == other.pos);
        }

        bool operator!=(const Iterator& other) const {
            return !(*this == other);
        }
    };

    // 获取 begin 迭代器
    Iterator begin() {
        return Iterator(this, 0);
    }

    // 获取 end 迭代器
    Iterator end() {
        return Iterator(this, count);
    }
};

// 使用示例
int main() {
    Deque<std::string> dq;

    // 在后面插入元素
    dq.push_back("Apple");
    dq.push_back("Banana");
    dq.push_back("Cherry");

    // 在前面插入元素
    dq.push_front("Date");
    dq.push_front("Elderberry");

    // 显示队列大小
    std::cout << "Deque 大小: " << dq.size() << std::endl;

    // 使用迭代器进行遍历
    std::cout << "Deque 元素: ";
    for (auto it = dq.begin(); it != dq.end(); ++it) {
        std::cout << *it << " ";
    }
    std::cout << std::endl;

    // 访问前端和后端元素
    std::cout << "前端元素: " << dq.front() << std::endl;
    std::cout << "后端元素: " << dq.back() << std::endl;

    // 删除元素
    dq.pop_front();
    dq.pop_back();

    // 再次遍历
    std::cout << "删除元素后的 Deque: ";
    for (auto it = dq.begin(); it != dq.end(); ++it) {
        std::cout << *it << " ";
    }
    std::cout << std::endl;

    return 0;
}