#include <iostream>
#include <stdexcept>
#include <iterator>

// ���ڻ��������˫�˶���
template <typename T>
class Deque {
private:
    T* buffer;               // �ڲ�������
    size_t capacity;         // ����������
    size_t front_idx;        // ͷ������
    size_t back_idx;         // β������
    size_t count;            // ��ǰԪ������

    // ��������
    void resize(size_t new_capacity) {
        T* new_buffer = new T[new_capacity];
        // ��������Ԫ��
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
    // ���캯��
    Deque(size_t initial_capacity = 8)
            : capacity(initial_capacity), front_idx(0), back_idx(0), count(0) {
        buffer = new T[capacity];
    }

    // ��������
    ~Deque() {
        delete[] buffer;
    }

    // ���ø��ƹ��캯���͸�ֵ�������Ϊ�˼�࣬�ɸ�����Ҫʵ�֣�
    Deque(const Deque& other) = delete;
    Deque& operator=(const Deque& other) = delete;

    // ����Ƿ�Ϊ��
    bool empty() const {
        return count == 0;
    }

    // ��ȡ��С
    size_t size() const {
        return count;
    }

    // ��ǰ�����Ԫ��
    void push_front(const T& value) {
        if (count == capacity) {
            resize(capacity * 2);
        }
        front_idx = (front_idx == 0) ? capacity - 1 : front_idx - 1;
        buffer[front_idx] = value;
        ++count;
    }

    // �ں������Ԫ��
    void push_back(const T& value) {
        if (count == capacity) {
            resize(capacity * 2);
        }
        buffer[back_idx] = value;
        back_idx = (back_idx + 1) % capacity;
        ++count;
    }

    // ��ǰ��ɾ��Ԫ��
    void pop_front() {
        if (empty()) {
            throw std::out_of_range("Deque is empty");
        }
        front_idx = (front_idx + 1) % capacity;
        --count;
    }

    // �Ӻ���ɾ��Ԫ��
    void pop_back() {
        if (empty()) {
            throw std::out_of_range("Deque is empty");
        }
        back_idx = (back_idx == 0) ? capacity - 1 : back_idx - 1;
        --count;
    }

    // ��ȡǰ��Ԫ��
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

    // ��ȡ���Ԫ��
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

    // �������ඨ��
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

        // �����ò���
        reference operator*() const {
            size_t real_idx = (deque_ptr->front_idx + pos) % deque_ptr->capacity;
            return deque_ptr->buffer[real_idx];
        }

        pointer operator->() const {
            size_t real_idx = (deque_ptr->front_idx + pos) % deque_ptr->capacity;
            return &(deque_ptr->buffer[real_idx]);
        }

        // ǰ�õ���
        Iterator& operator++() {
            ++pos;
            return *this;
        }

        // ���õ���
        Iterator operator++(int) {
            Iterator temp = *this;
            ++pos;
            return temp;
        }

        // ǰ�õݼ�
        Iterator& operator--() {
            --pos;
            return *this;
        }

        // ���õݼ�
        Iterator operator--(int) {
            Iterator temp = *this;
            --pos;
            return temp;
        }

        // �Ƚϲ���
        bool operator==(const Iterator& other) const {
            return (deque_ptr == other.deque_ptr) && (pos == other.pos);
        }

        bool operator!=(const Iterator& other) const {
            return !(*this == other);
        }
    };

    // ��ȡ begin ������
    Iterator begin() {
        return Iterator(this, 0);
    }

    // ��ȡ end ������
    Iterator end() {
        return Iterator(this, count);
    }
};

// ʹ��ʾ��
int main() {
    Deque<std::string> dq;

    // �ں������Ԫ��
    dq.push_back("Apple");
    dq.push_back("Banana");
    dq.push_back("Cherry");

    // ��ǰ�����Ԫ��
    dq.push_front("Date");
    dq.push_front("Elderberry");

    // ��ʾ���д�С
    std::cout << "Deque ��С: " << dq.size() << std::endl;

    // ʹ�õ��������б���
    std::cout << "Deque Ԫ��: ";
    for (auto it = dq.begin(); it != dq.end(); ++it) {
        std::cout << *it << " ";
    }
    std::cout << std::endl;

    // ����ǰ�˺ͺ��Ԫ��
    std::cout << "ǰ��Ԫ��: " << dq.front() << std::endl;
    std::cout << "���Ԫ��: " << dq.back() << std::endl;

    // ɾ��Ԫ��
    dq.pop_front();
    dq.pop_back();

    // �ٴα���
    std::cout << "ɾ��Ԫ�غ�� Deque: ";
    for (auto it = dq.begin(); it != dq.end(); ++it) {
        std::cout << *it << " ";
    }
    std::cout << std::endl;

    return 0;
}