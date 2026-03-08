#pragma once
#include <cstddef>



struct PoolStatistics {
    size_t total_allocations;      // 总分配次数
    size_t total_deallocations;    // 总释放次数
    size_t current_allocations;    // 当前分配数量
    size_t peak_allocations;       // 峰值分配数量
    size_t total_bytes_allocated;  // 总分配字节数
    size_t current_bytes_used;     // 当前使用字节数
    size_t peak_bytes_used;        // 峰值使用字节数
    double fragmentation_ratio;    // 碎片率
};