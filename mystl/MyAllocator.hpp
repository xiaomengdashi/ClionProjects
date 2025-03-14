#pragma once


#include <iostream>
#include <memory>


template<typename T>
class MyAllocator {
public:
    using value_type = T;

    MyAllocator() noexcept = default;

    template<typename U>
    explicit MyAllocator(const MyAllocator<U>&) noexcept {}

    T* allocate(std::size_t n) {
        if (n > std::size_t(-1) / sizeof(T))
            throw std::bad_alloc();
        if (auto p = static_cast<T*>(std::malloc(n * sizeof(T))))
            return p;
        throw std::bad_alloc();
    }

    void deallocate(T* p, std::size_t) noexcept {
        std::free(p);
    }
};

template<typename T, typename U>
bool operator==(const MyAllocator<T>&, const MyAllocator<U>&) noexcept {
    return true;
}

template<typename T, typename U>
bool operator!=(const MyAllocator<T>&, const MyAllocator<U>&) noexcept {
    return false;
}

