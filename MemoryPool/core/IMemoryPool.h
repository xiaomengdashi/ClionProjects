#pragma once
#include <cstddef>
#include "PoolStatistics.h"

class IMemoryPool {
public:
    virtual ~IMemoryPool() = default;
    virtual void* allocate(size_t size) = 0;
    virtual void deallocate(void* ptr) = 0;
    virtual PoolStatistics get_statistics() const = 0;
    virtual void reset() = 0;
};
