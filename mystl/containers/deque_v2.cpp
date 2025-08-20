#include <iostream>
#include <vector>
#include <memory>
#include <stdexcept>
#include <array>


// 基于分块存储的双端队列
template <typename T>
class Deque {
private:
    static const size_t CHUNK_SIZE = 512;  // 每个块的大小
    using Chunk = std::array<T, CHUNK_SIZE>;

    std::vector<std::unique_ptr<Chunk>> chunks;  // 块容器
    size_t front_chunk;    // 首元素所在块索引
    size_t front_offset;   // 首元素在块内的偏移
    size_t back_chunk;     // 末元素所在块索引
    size_t back_offset;    // 末元素在块内的偏移
    size_t element_count;  // 元素总数

    // 前向分配新块
    void expand_front() {
        // 在当前位置插入新块
        chunks.insert(chunks.begin() + front_chunk, std::make_unique<Chunk>());
        // front_chunk保持指向新插入块的索引
        front_offset = CHUNK_SIZE - 1;  // 重置偏移
    }

    // 后向分配新块
    void expand_back() {
        chunks.insert(chunks.begin() + back_chunk + 1, std::make_unique<Chunk>());
        back_chunk = back_chunk + 1;
        back_offset = 0;
    }

public:
    // 迭代器定义
    class Iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = T;
        using pointer = T*;
        using reference = T&;
        using difference_type = std::ptrdiff_t;
    private:
        size_t chunk_idx;
        size_t elem_idx;
        Deque* parent;

    public:
        Iterator(Deque* p, size_t c, size_t e)
                : parent(p), chunk_idx(c), elem_idx(e) {}

        reference operator*() const {
            return (*parent->chunks[chunk_idx])[elem_idx];
        }

        pointer operator->() const {
            return &(**this);
        }

        Iterator& operator++() {
            if (++elem_idx == CHUNK_SIZE) {
                ++chunk_idx;
                elem_idx = 0;
            }
            return *this;
        }

        Iterator operator++(int) {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const Iterator& other) const {
            return chunk_idx == other.chunk_idx &&
                   elem_idx == other.elem_idx &&
                   parent == other.parent;
        }

        bool operator!=(const Iterator& other) const {
            return !(*this == other);
        }

        reference operator[](difference_type n) {
            return (*parent)[chunk_idx * CHUNK_SIZE + elem_idx + n];
        }
    };

    Deque() : front_chunk(0), front_offset(CHUNK_SIZE/2),
              back_chunk(0), back_offset(CHUNK_SIZE/2), element_count(0) {
        chunks.push_back(std::make_unique<Chunk>());
    }

    // 拷贝构造函数
    Deque(const Deque& other) : Deque() {
        for (const auto& item : other) {
            push_back(item);
        }
    }

    // 移动构造函数
    Deque(Deque&& other) noexcept {
        // 移动资源...
    }

    ~Deque() = default;

    bool empty() const { return element_count == 0; }
    size_t size() const { return element_count; }

    void push_front(const T& value) {
        if (front_offset == 0) {
            expand_front();
        }
        (*chunks[front_chunk])[--front_offset] = value;
        ++element_count;
    }

    void push_back(const T& value) {
        if (back_offset == CHUNK_SIZE) {
            expand_back();
        }
        (*chunks[back_chunk])[back_offset++] = value;
        ++element_count;
    }

    void pop_front() {
        if (empty()) throw std::out_of_range("Deque is empty");
        if (++front_offset == CHUNK_SIZE) {
            front_offset = 0;
            ++front_chunk;
        }
        --element_count;
    }

    void pop_back() {
        if (empty()) throw std::out_of_range("Deque is empty");
        if (back_offset-- == 0) {
            --back_chunk;
            back_offset = CHUNK_SIZE - 1;
        }
        --element_count;
    }

    T& operator[](size_t index) {
        const size_t global_idx = front_offset + index;
        const size_t chunk = front_chunk + global_idx / CHUNK_SIZE;
        const size_t offset = global_idx % CHUNK_SIZE;
        return (*chunks[chunk])[offset];
    }

    Iterator begin() { return Iterator(this, front_chunk, front_offset); }
    Iterator end() { return Iterator(this, back_chunk, back_offset); }
};

// 使用示例
int main() {
    Deque<int> dq;
    for (int i = 0; i < 1000; ++i) {
       // dq.push_back(i);
       dq.push_front(-i);
        std::cout << dq[i] << " ";
    }

    std::cout << "Deque size: " << dq.size() << "\n";
    std::cout << "First element: " << dq[0] << "\n";
    std::cout << "Last element: " << dq[dq.size()-1] << "\n";

    // 使用迭代器遍历
    for (auto it = dq.begin(); it != dq.end(); ++it) {
        // 处理元素...
    }

    return 0;
}
