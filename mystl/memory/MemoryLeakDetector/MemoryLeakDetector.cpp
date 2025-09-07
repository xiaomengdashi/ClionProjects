//
// Created by Kolane on 2025/9/7.
//

#include "MemoryLeakDetector.h"


#include "MemoryLeakDetector.h"
#include <ctime>
#include <cstdlib>
#include <iomanip>
#include <algorithm>
#include <vector>

// 标准的malloc和free函数声明
extern "C" {
void* malloc(size_t size);
void free(void* ptr);
}

// MemoryInfo构造函数实现
MemoryInfo::MemoryInfo(void* addr, size_t sz, const char* file, int ln)
        : address(addr), size(sz), filename(file), line(ln) {
    // 获取当前时间作为时间戳
    std::time_t now = std::time(nullptr);
    char buffer[100];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    timestamp = buffer;
}

// MemoryLeakDetector构造函数实现
MemoryLeakDetector::MemoryLeakDetector()
        : totalAllocated(0), totalDeallocated(0),
          allocationCount(0), deallocationCount(0),
          isEnabled(true), warningLevel(WARN_ERROR_ONLY), isCleaningUp(false) {
    // 预留一定的空间以提高性能
    allocations.reserve(1000);
}

// 获取单例实例
MemoryLeakDetector& MemoryLeakDetector::getInstance() {
    // C++11保证局部静态变量的线程安全初始化
    static MemoryLeakDetector instance;
    return instance;
}

// 析构函数，自动生成内存泄漏报告
MemoryLeakDetector::~MemoryLeakDetector() {
    // 设置清理标志，避免析构期间的警告
    isCleaningUp = true;

    // 只有在检测启用且存在未释放内存时才生成报告
    if (isEnabled && !allocations.empty()) {
        generateReport();
    }
}

// 记录内存分配
void MemoryLeakDetector::recordAllocation(void* ptr, size_t size,
                                          const char* filename, int line) {
    // 如果检测未启用或指针为空，直接返回
    if (!isEnabled || ptr == nullptr) {
        return;
    }

    // 加锁保证线程安全
    std::lock_guard<std::recursive_mutex> lock(mutex);

    // 创建内存信息记录
    MemoryInfo info(ptr, size, filename, line);

    // 检查是否重复分配（通常不应该发生）
    auto it = allocations.find(ptr);
    if (it != allocations.end() && warningLevel >= WARN_ERROR_ONLY) {
        // 重复分配是严重错误，在ERROR_ONLY级别就应该警告
        std::cerr << "Error: Detected duplicate memory allocation at address " << ptr
                  << ", there may be a memory management error!" << std::endl;
    }

    // 记录分配信息
    allocations[ptr] = info;

    // 更新统计信息
    totalAllocated += size;
    allocationCount++;
}

// 记录内存释放
void MemoryLeakDetector::recordDeallocation(void* ptr) {
    // 如果检测未启用或指针为空，直接返回
    if (!isEnabled || ptr == nullptr) {
        return;
    }

    // 加锁保证线程安全
    std::lock_guard<std::recursive_mutex> lock(mutex);

    // 查找分配记录
    auto it = allocations.find(ptr);
    if (it != allocations.end()) {
        // 更新统计信息
        totalDeallocated += it->second.size;
        deallocationCount++;

        // 删除分配记录
        allocations.erase(it);
    } else {
        // 释放未记录的内存，根据警告级别和清理状态决定是否输出警告
        if (!isCleaningUp && warningLevel == WARN_ALL) {
            // 只在非清理阶段且警告级别为WARN_ALL时才显示警告
            std::cerr << "Warning: Attempting to deallocate untracked memory address " << ptr
                      << ", this may be a double-free or deallocating unallocated memory!" << std::endl;
        }
        // 在WARN_ERROR_ONLY级别下，我们可以记录但不输出
        // 这样可以在需要时进行调试，但不会干扰正常使用
    }
}

// 生成内存泄漏报告
void MemoryLeakDetector::generateReport() const {
    std::lock_guard<std::recursive_mutex> lock(mutex);

    // 计算当前使用的内存（在锁内计算，避免死锁）
    size_t currentUsage = 0;
    for (const auto& pair : allocations) {
        currentUsage += pair.second.size;
    }

    std::cout << "\n"
              << "================================================================\n"
              << "                    Memory Leak Detection Report\n"
              << "================================================================\n";

    // 输出统计信息
    std::cout << "Statistics:\n"
              << "  Total allocations: " << allocationCount << "\n"
              << "  Total deallocations: " << deallocationCount << "\n"
              << "  Total allocated memory: " << formatSize(totalAllocated) << "\n"
              << "  Total deallocated memory: " << formatSize(totalDeallocated) << "\n"
              << "  Current memory usage: " << formatSize(currentUsage) << "\n"
              << "----------------------------------------------------------------\n";

    if (allocations.empty()) {
        std::cout << "Congratulations! No memory leaks detected.\n";
    } else {
        std::cout << "Detected " << allocations.size() << " memory leaks:\n\n";

        // 将泄漏信息排序（按地址或大小）
        std::vector<std::pair<void*, MemoryInfo>> leaks(allocations.begin(), allocations.end());
        std::sort(leaks.begin(), leaks.end(),
                  [](const std::pair<void*, MemoryInfo>& a, const std::pair<void*, MemoryInfo>& b) {
                      return a.second.size > b.second.size; // 按大小降序排列
                  });

        size_t totalLeaked = 0;
        int index = 1;

        // 输出每个泄漏的详细信息
        for (const auto& leak : leaks) {
            const MemoryInfo& info = leak.second;
            std::cout << "[Leak #" << index++ << "]\n"
                      << "  Address: " << std::hex << info.address << std::dec << "\n"
                      << "  Size: " << formatSize(info.size) << " (" << info.size << " bytes)\n"
                      << "  Location: " << info.filename << ":" << info.line << "\n"
                      << "  Time: " << info.timestamp << "\n\n";
            totalLeaked += info.size;
        }

        std::cout << "----------------------------------------------------------------\n"
                  << "Total leaked memory: " << formatSize(totalLeaked) << " ("
                  << totalLeaked << " bytes)\n";
    }

    std::cout << "================================================================\n\n";
}

// 获取当前内存使用量
size_t MemoryLeakDetector::getCurrentMemoryUsage() const {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    size_t currentUsage = 0;
    for (const auto& pair : allocations) {
        currentUsage += pair.second.size;
    }
    return currentUsage;
}

// 清空所有记录
void MemoryLeakDetector::reset() {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    allocations.clear();
    totalAllocated = 0;
    totalDeallocated = 0;
    allocationCount = 0;
    deallocationCount = 0;
}

// 获取当前时间戳
std::string MemoryLeakDetector::getCurrentTimestamp() const {
    std::time_t now = std::time(nullptr);
    char buffer[100];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return std::string(buffer);
}

// 格式化内存大小显示
std::string MemoryLeakDetector::formatSize(size_t size) const {
    std::stringstream ss;

    if (size < 1024) {
        ss << size << " B";
    } else if (size < 1024 * 1024) {
        ss << std::fixed << std::setprecision(2)
           << (double)size / 1024 << " KB";
    } else if (size < 1024 * 1024 * 1024) {
        ss << std::fixed << std::setprecision(2)
           << (double)size / (1024 * 1024) << " MB";
    } else {
        ss << std::fixed << std::setprecision(2)
           << (double)size / (1024 * 1024 * 1024) << " GB";
    }

    return ss.str();
}

// 全局标志，防止在检测器自身的构造/析构过程中进行检测
static thread_local bool g_inDetectorCode = false;

// 重载的new操作符实现（带文件名和行号）
void* operator new(size_t size, const char* filename, int line) {
    // 防止递归调用
    if (g_inDetectorCode) {
        return malloc(size);
    }

    g_inDetectorCode = true;

    // 分配内存
    void* ptr = malloc(size);

    if (ptr != nullptr) {
        // 记录分配
        MemoryLeakDetector::getInstance().recordAllocation(ptr, size, filename, line);
    } else {
        g_inDetectorCode = false;
        throw std::bad_alloc();
    }

    g_inDetectorCode = false;
    return ptr;
}

// 重载的new[]操作符实现（带文件名和行号）
void* operator new[](size_t size, const char* filename, int line) {
    return operator new(size, filename, line);
}

// 重载的delete操作符实现
void operator delete(void* ptr) noexcept {
    if (ptr == nullptr) {
        return;
    }

    // 防止递归调用
    if (g_inDetectorCode) {
        free(ptr);
        return;
    }

    g_inDetectorCode = true;

    // 记录释放
    MemoryLeakDetector::getInstance().recordDeallocation(ptr);

    // 释放内存
    free(ptr);

    g_inDetectorCode = false;
}

// 重载的delete[]操作符实现
void operator delete[](void* ptr) noexcept {
    operator delete(ptr);
}

// placement delete操作符（用于placement new失败时的清理）
void operator delete(void* ptr, const char* filename, int line) noexcept {
    operator delete(ptr);
}

// placement delete[]操作符（用于placement new[]失败时的清理）
void operator delete[](void* ptr, const char* filename, int line) noexcept {
    operator delete(ptr);
}