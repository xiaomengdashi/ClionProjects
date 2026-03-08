#pragma once
#include <memory>
#include <utility>
#include "../core/FixedMemoryPool.hpp"
#include "../core/VariableMemoryPool.hpp"
#include "../core/ThreadSafeMemoryPool.hpp"

/**
 * 内存池工厂类
 * 提供统一的内存池创建接口，简化不同类型内存池的实例化过程
 */
class MemoryPoolFactory {
public:
    /**
     * 创建固定大小内存池
     * @tparam T 对象类型
     * @param block_count 块的数量
     * @param alignment 内存对齐要求（默认为T的对齐要求）
     * @return 固定大小内存池的智能指针
     */
    template<typename T>
    static std::unique_ptr<FixedMemoryPool<T>> 
    create_fixed_pool(size_t block_count, size_t alignment = alignof(T)) {
        return std::make_unique<FixedMemoryPool<T>>(block_count, alignment);
    }
    
    /**
     * 创建可变大小内存池
     * @param total_size 内存池总大小（字节）
     * @param alignment 内存对齐要求（默认为最大对齐）
     * @return 可变大小内存池的智能指针
     */
    static std::unique_ptr<VariableMemoryPool> 
    create_variable_pool(size_t total_size, size_t alignment = alignof(std::max_align_t)) {
        return std::make_unique<VariableMemoryPool>(total_size, alignment);
    }
    
    /**
     * 创建线程安全内存池
     * @tparam PoolType 底层内存池类型
     * @tparam Args 构造参数类型
     * @param args 传递给底层内存池的构造参数
     * @return 线程安全内存池的智能指针
     */
    template<typename PoolType, typename... Args>
    static std::unique_ptr<ThreadSafeMemoryPool<PoolType>> 
    create_thread_safe_pool(Args&&... args) {
        return std::make_unique<ThreadSafeMemoryPool<PoolType>>(std::forward<Args>(args)...);
    }

    // 便利方法：创建常用配置的内存池
    
    /**
     * 创建默认的小对象内存池（适用于游戏对象等）
     * @tparam T 对象类型
     * @param object_count 对象数量
     * @return 固定大小内存池的智能指针
     */
    template<typename T>
    static std::unique_ptr<FixedMemoryPool<T>>
    create_object_pool(size_t object_count) {
        return create_fixed_pool<T>(object_count, alignof(T));
    }
    
    /**
     * 创建默认的通用内存池（1MB大小）
     * @return 可变大小内存池的智能指针
     */
    static std::unique_ptr<VariableMemoryPool>
    create_default_pool() {
        return create_variable_pool(1024 * 1024); // 1MB
    }
    
    /**
     * 创建线程安全的固定大小内存池
     * @tparam T 对象类型
     * @param block_count 块的数量
     * @param alignment 内存对齐要求
     * @return 线程安全固定大小内存池的智能指针
     */
    template<typename T>
    static std::unique_ptr<ThreadSafeMemoryPool<FixedMemoryPool<T>>>
    create_thread_safe_fixed_pool(size_t block_count, size_t alignment = alignof(T)) {
        return create_thread_safe_pool<FixedMemoryPool<T>>(block_count, alignment);
    }
    
    /**
     * 创建线程安全的可变大小内存池
     * @param total_size 内存池总大小
     * @param alignment 内存对齐要求
     * @return 线程安全可变大小内存池的智能指针
     */
    static std::unique_ptr<ThreadSafeMemoryPool<VariableMemoryPool>>
    create_thread_safe_variable_pool(size_t total_size, size_t alignment = alignof(std::max_align_t)) {
        return create_thread_safe_pool<VariableMemoryPool>(total_size, alignment);
    }

private:
    // 禁止实例化，这是一个纯静态工厂类
    MemoryPoolFactory() = delete;
    ~MemoryPoolFactory() = delete;
    MemoryPoolFactory(const MemoryPoolFactory&) = delete;
    MemoryPoolFactory& operator=(const MemoryPoolFactory&) = delete;
};