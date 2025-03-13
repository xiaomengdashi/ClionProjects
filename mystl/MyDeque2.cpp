#include <iostream>
#include <vector>
#include <memory>
#include <stdexcept>
#include <array>


// ���ڷֿ�洢��˫�˶���
template <typename T>
class Deque {
private:
    static const size_t CHUNK_SIZE = 512;  // ÿ����Ĵ�С
    using Chunk = std::array<T, CHUNK_SIZE>;

    std::vector<std::unique_ptr<Chunk>> chunks;  // ������
    size_t front_chunk;    // ��Ԫ�����ڿ�����
    size_t front_offset;   // ��Ԫ���ڿ��ڵ�ƫ��
    size_t back_chunk;     // ĩԪ�����ڿ�����
    size_t back_offset;    // ĩԪ���ڿ��ڵ�ƫ��
    size_t element_count;  // Ԫ������

    // ǰ������¿�
    void expand_front() {
        // �ڵ�ǰλ�ò����¿�
        chunks.insert(chunks.begin() + front_chunk, std::make_unique<Chunk>());
        // front_chunk����ָ���²���������
        front_offset = CHUNK_SIZE - 1;  // ����ƫ��
    }

    // ��������¿�
    void expand_back() {
        chunks.insert(chunks.begin() + back_chunk + 1, std::make_unique<Chunk>());
        back_chunk = back_chunk + 1;
        back_offset = 0;
    }

public:
    // ����������
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

    // �������캯��
    Deque(const Deque& other) : Deque() {
        for (const auto& item : other) {
            push_back(item);
        }
    }

    // �ƶ����캯��
    Deque(Deque&& other) noexcept {
        // �ƶ���Դ...
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

// ʹ��ʾ��
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

    // ʹ�õ���������
    for (auto it = dq.begin(); it != dq.end(); ++it) {
        // ����Ԫ��...
    }

    return 0;
}
