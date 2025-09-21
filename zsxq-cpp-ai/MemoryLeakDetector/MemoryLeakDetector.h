//
// Created by Kolane on 2025/9/7.
//

#pragma once

#include <iostream>
#include <unordered_map>
#include <mutex>
#include <string>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm>


// 内存分配信息结构体，用于记录每次内存分配的详细信息
struct MemoryInfo {
    void* address;          // 分配的内存地址
    size_t size;           // 分配的内存大小（字节）
    std::string filename;   // 分配发生的源文件名
    int line;              // 分配发生的行号
    std::string timestamp;  // 分配时间戳

    // 构造函数
    MemoryInfo(void* addr = nullptr, size_t sz = 0,
               const char* file = "", int ln = 0);
};

// 内存泄漏检测器类，采用单例模式设计
class MemoryLeakDetector {
public:
    // 警告级别控制
    enum WarningLevel {
        WARN_NONE = 0,      // 不显示任何警告
        WARN_ERROR_ONLY = 1, // 仅显示错误（如双重释放）
        WARN_ALL = 2        // 显示所有警告
    };

private:
    // 存储所有内存分配记录的哈希表，键为内存地址，值为内存信息
    std::unordered_map<void*, MemoryInfo> allocations;

    // 递归互斥锁，确保多线程环境下的线程安全，并允许同一线程多次获取锁
    mutable std::recursive_mutex mutex;

    // 统计信息
    size_t totalAllocated;      // 总分配内存大小
    size_t totalDeallocated;    // 总释放内存大小
    size_t allocationCount;     // 分配次数
    size_t deallocationCount;   // 释放次数

    // 是否启用检测的标志
    bool isEnabled;

    WarningLevel warningLevel;

    // 是否正在清理（程序退出时）
    bool isCleaningUp;

    // 私有构造函数，防止外部实例化
    MemoryLeakDetector();

    // 禁用拷贝构造和赋值操作
    MemoryLeakDetector(const MemoryLeakDetector&) = delete;
    MemoryLeakDetector& operator=(const MemoryLeakDetector&) = delete;

public:
    // 获取单例实例的静态方法
    static MemoryLeakDetector& getInstance();

    // 析构函数，在程序结束时自动生成内存泄漏报告
    ~MemoryLeakDetector();

    // 记录内存分配
    void recordAllocation(void* ptr, size_t size,
                          const char* filename = "unknown",
                          int line = 0);

    // 记录内存释放
    void recordDeallocation(void* ptr);

    // 生成内存泄漏报告
    void generateReport() const;

    // 启用/禁用内存泄漏检测
    void enable() { isEnabled = true; }
    void disable() { isEnabled = false; }
    bool enabled() const { return isEnabled; }

    // 设置警告级别
    void setWarningLevel(WarningLevel level) { warningLevel = level; }
    WarningLevel getWarningLevel() const { return warningLevel; }

    // 获取当前内存使用统计
    size_t getCurrentMemoryUsage() const;
    size_t getTotalAllocated() const { return totalAllocated; }
    size_t getTotalDeallocated() const { return totalDeallocated; }
    size_t getAllocationCount() const { return allocationCount; }
    size_t getDeallocationCount() const { return deallocationCount; }

    // 清空所有记录（用于测试）
    void reset();

private:
    // 获取当前时间戳字符串
    std::string getCurrentTimestamp() const;

    // 格式化内存大小显示（例如：1024B -> 1KB）
    std::string formatSize(size_t size) const;
};

// 重载的new操作符，带有文件名和行号信息
void* operator new(size_t size, const char* filename, int line);
void* operator new[](size_t size, const char* filename, int line);

// 重载的delete操作符
void operator delete(void* ptr) noexcept;
void operator delete[](void* ptr) noexcept;

// 特殊的delete操作符，用于placement new失败时的清理
void operator delete(void* ptr, const char* filename, int line) noexcept;
void operator delete[](void* ptr, const char* filename, int line) noexcept;

// 宏定义，用于自动插入文件名和行号信息
#ifdef ENABLE_MEMORY_LEAK_DETECTION
#define new new(__FILE__, __LINE__)

    // 手动启用/禁用检测的宏
    #define MEMORY_LEAK_DETECTOR_ENABLE() \
        MemoryLeakDetector::getInstance().enable()

    #define MEMORY_LEAK_DETECTOR_DISABLE() \
        MemoryLeakDetector::getInstance().disable()

    // 生成报告的宏
    #define MEMORY_LEAK_DETECTOR_REPORT() \
        MemoryLeakDetector::getInstance().generateReport()

    // 重置检测器的宏
    #define MEMORY_LEAK_DETECTOR_RESET() \
        MemoryLeakDetector::getInstance().reset()

    // 设置警告级别的宏
    #define MEMORY_LEAK_DETECTOR_SET_WARNING_LEVEL(level) \
        MemoryLeakDetector::getInstance().setWarningLevel(level)
#else
// 如果未启用检测，这些宏什么都不做
#define MEMORY_LEAK_DETECTOR_ENABLE()
#define MEMORY_LEAK_DETECTOR_DISABLE()
#define MEMORY_LEAK_DETECTOR_REPORT()
#define MEMORY_LEAK_DETECTOR_RESET()
#define MEMORY_LEAK_DETECTOR_SET_WARNING_LEVEL(level)
#endif




