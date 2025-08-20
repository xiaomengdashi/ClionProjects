//
// Created by Kolane on 2025/3/13.
//

#include <iostream>
#include <initializer_list>
#include <stdexcept>

template <typename T>
class List {
private:
    struct Node {
        T data;
        Node* prev;
        Node* next;

        Node(const T& val = T(), Node* p = nullptr, Node* n = nullptr)
                : data(val), prev(p), next(n) {}
    };

    Node* dummy;  // 哨兵节点
    size_t _size;

public:
    class iterator {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        explicit iterator(Node* p = nullptr) : current(p) {}

        reference operator*() const { return current->data; }
        pointer operator->() const { return &(current->data); }

        iterator& operator++() {
            current = current->next;
            return *this;
        }

        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        iterator& operator--() {
            current = current->prev;
            return *this;
        }

        iterator operator--(int) {
            iterator tmp = *this;
            --(*this);
            return tmp;
        }

        bool operator==(const iterator& rhs) const { return current == rhs.current; }
        bool operator!=(const iterator& rhs) const { return !(*this == rhs); }

    private:
        Node* current;
        friend class List;
    };

    List() : dummy(new Node()), _size(0) {
        dummy->prev = dummy;
        dummy->next = dummy;
    }

    List(std::initializer_list<T> init) : List() {
        for (const auto& val : init) {
            push_back(val);
        }
    }

    ~List() {
        clear();
        delete dummy;
    }

    List(const List& other) : List() {
        for (const auto& val : other) {
            push_back(val);
        }
    }

    List& operator=(const List& other) {
        if (this != &other) {
            List tmp(other);
            std::swap(dummy, tmp.dummy);
            std::swap(_size, tmp._size);
        }
        return *this;
    }

    // 容量相关
    bool empty() const { return _size == 0; }
    size_t size() const { return _size; }

    // 元素访问
    T& front() {
        if (empty()) throw std::out_of_range("List is empty");
        return dummy->next->data;
    }

    T& back() {
        if (empty()) throw std::out_of_range("List is empty");
        return dummy->prev->data;
    }

    // 修改操作
    void push_front(const T& value) { insert(begin(), value); }
    void push_back(const T& value) { insert(end(), value); }

    void pop_front() {
        if (empty()) throw std::out_of_range("List is empty");
        erase(begin());
    }

    void pop_back() {
        if (empty()) throw std::out_of_range("List is empty");
        erase(--end());
    }

    iterator insert(iterator pos, const T& value) {
        Node* p = pos.current;
        Node* newNode = new Node(value, p->prev, p);
        p->prev->next = newNode;
        p->prev = newNode;
        ++_size;
        return iterator(newNode);
    }

    iterator erase(iterator pos) {
        if (pos == end()) return pos;

        Node* p = pos.current;
        iterator retVal(p->next);
        p->prev->next = p->next;
        p->next->prev = p->prev;
        delete p;
        --_size;
        return retVal;
    }

    void clear() {
        while (!empty()) {
            pop_front();
        }
    }

    // 迭代器
    iterator begin() { return iterator(dummy->next); }
    iterator end() { return iterator(dummy); }
};

// 测试用例
int main() {
    List<int> lst = {1, 2, 3, 4, 5};

    // 测试迭代和修改
    for (auto& n : lst) {
        std::cout << n << " ";
    }
    std::cout << "\n";

    lst.push_front(0);
    lst.push_back(6);

    std::cout << "Modified list: ";
    for (auto& n : lst) {
        std::cout << n << " ";
    }
    std::cout << "\n";

    // 测试删除
    lst.pop_front();
    lst.pop_back();

    // 测试插入和删除
    auto it = lst.begin();
    ++it;
    ++it;
    it = lst.insert(it, 99);
    it = lst.erase(it);

    std::cout << "Modified list: ";
    for (auto& n : lst) {
        std::cout << n << " ";
    }
    std::cout << "\n";

    // 测试异常
    try {
        List<int> emptyList;
        emptyList.pop_back();
    } catch (const std::exception& e) {
        std::cout << "Exception caught: " << e.what() << "\n";
    }

    return 0;
}