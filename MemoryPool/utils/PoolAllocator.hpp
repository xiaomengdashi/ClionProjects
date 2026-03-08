#pragma once
#include <cstddef>
#include <utility>
#include <new>

// STL兼容的内存池适配器
template<typename T, typename PoolType>
class PoolAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    template<typename U>
    struct rebind { using other = PoolAllocator<U, PoolType>; };

    // 构造函数
    explicit PoolAllocator(PoolType& pool) noexcept : m_pool(&pool) {}

    // 拷贝构造函数
    template<typename U>
    PoolAllocator(const PoolAllocator<U, PoolType>& other) noexcept : m_pool(other.get_pool()) {}

    pointer allocate(size_type n) {
        void* p = m_pool->allocate(n * sizeof(T));
        if (!p) throw std::bad_alloc();
        return static_cast<pointer>(p);
    }
    void deallocate(pointer p, size_type /*n*/) {
        m_pool->deallocate(p);
    }

    template<typename... Args>
    void construct(pointer p, Args&&... args) {
        ::new ((void*)p) T(std::forward<Args>(args)...);
    }
    void destroy(pointer p) { p->~T(); }

    PoolType* get_pool() const noexcept { return m_pool; }

    // STL要求的比较
    template<typename U>
    bool operator==(const PoolAllocator<U, PoolType>& other) const noexcept { return m_pool == other.get_pool(); }
    template<typename U>
    bool operator!=(const PoolAllocator<U, PoolType>& other) const noexcept { return m_pool != other.get_pool(); }

private:
    PoolType* m_pool;
};
