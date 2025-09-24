/**
 * @file test_mem_pool.cpp
 * @brief 测试StdIndexedMemPool的功能和性能
 */

#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <cassert>
#include <string>
#include "StdIndexedMemPool.h"

// 测试用的简单类
class TestObject {
public:
    TestObject() : value_(0) {
        std::cout << "TestObject默认构造: " << this << std::endl;
    }
    
    explicit TestObject(int value) : value_(value) {
        std::cout << "TestObject带参数构造: " << this << ", value = " << value_ << std::endl;
    }
    
    ~TestObject() {
        std::cout << "TestObject析构: " << this << ", value = " << value_ << std::endl;
    }
    
    int getValue() const { return value_; }
    void setValue(int value) { value_ = value; }

private:
    int value_;
};

// 测试基本分配和回收功能
void testBasicAllocation() {
    std::cout << "\n=== 测试基本分配和回收 ===" << std::endl;
    
    // 创建内存池，容量为100
    std_mem_pool::IndexedMemPool<TestObject> pool(100);
    
    // 分配对象
    uint32_t idx1 = pool.allocIndex();
    uint32_t idx2 = pool.allocIndex(42);
    
    // 验证分配成功
    assert(idx1 != 0);
    assert(idx2 != 0);
    assert(idx1 != idx2);
    
    // 访问对象
    TestObject& obj1 = pool[idx1];
    TestObject& obj2 = pool[idx2];
    
    std::cout << "obj1.getValue() = " << obj1.getValue() << std::endl;
    std::cout << "obj2.getValue() = " << obj2.getValue() << std::endl;
    
    // 修改对象
    obj1.setValue(100);
    std::cout << "修改后 obj1.getValue() = " << obj1.getValue() << std::endl;
    
    // 回收对象
    pool.recycleIndex(idx1);
    pool.recycleIndex(idx2);
    
    // 再次分配，应该重用之前回收的对象
    uint32_t idx3 = pool.allocIndex(200);
    uint32_t idx4 = pool.allocIndex(300);
    
    // 验证重用
    std::cout << "重用后 obj3.getValue() = " << pool[idx3].getValue() << std::endl;
    std::cout << "重用后 obj4.getValue() = " << pool[idx4].getValue() << std::endl;
    
    // 测试是否已分配
    assert(pool.isAllocated(idx3));
    assert(pool.isAllocated(idx4));
    
    // 回收对象
    pool.recycleIndex(idx3);
    pool.recycleIndex(idx4);
}

// 测试智能指针功能
void testUniquePtr() {
    std::cout << "\n=== 测试智能指针功能 ===" << std::endl;
    
    std_mem_pool::IndexedMemPool<TestObject> pool(100);
    
    {
        // 使用智能指针分配
        auto ptr1 = pool.allocElem();
        auto ptr2 = pool.allocElem(42);
        
        // 验证分配成功
        assert(ptr1 != nullptr);
        assert(ptr2 != nullptr);
        
        // 访问对象
        ptr1->setValue(500);
        std::cout << "ptr1->getValue() = " << ptr1->getValue() << std::endl;
        std::cout << "ptr2->getValue() = " << ptr2->getValue() << std::endl;
        
        // 智能指针离开作用域时会自动回收对象
    }
    
    std::cout << "智能指针已离开作用域，对象已自动回收" << std::endl;
    
    // 分配新对象，应该重用之前回收的对象
    auto ptr3 = pool.allocElem(600);
    std::cout << "重用后 ptr3->getValue() = " << ptr3->getValue() << std::endl;
}

// 测试多线程分配和回收
void testMultithreading() {
    std::cout << "\n=== 测试多线程分配和回收 ===" << std::endl;
    
    const int numThreads = 4;
    const int allocsPerThread = 1000;
    
    std_mem_pool::IndexedMemPool<int> pool(numThreads * allocsPerThread);
    
    std::vector<std::thread> threads;
    
    auto threadFunc = [&pool, allocsPerThread](int threadId) {
        std::vector<uint32_t> indices;
        
        // 分配对象
        for (int i = 0; i < allocsPerThread; ++i) {
            uint32_t idx = pool.allocIndex(threadId * 10000 + i);
            if (idx != 0) {
                indices.push_back(idx);
            }
        }
        
        // 验证对象值
        for (uint32_t idx : indices) {
            assert(pool[idx] / 10000 == threadId);
        }
        
        // 回收一半对象
        for (size_t i = 0; i < indices.size() / 2; ++i) {
            pool.recycleIndex(indices[i]);
        }
        
        // 分配更多对象
        for (int i = 0; i < allocsPerThread / 2; ++i) {
            pool.allocIndex(threadId * 20000 + i);
        }
        
        // 回收剩余对象
        for (size_t i = indices.size() / 2; i < indices.size(); ++i) {
            pool.recycleIndex(indices[i]);
        }
    };
    
    // 启动线程
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(threadFunc, i);
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    std::cout << "多线程测试完成，耗时: " << duration << " 毫秒" << std::endl;
    std::cout << "每秒分配和回收次数: " 
              << (numThreads * allocsPerThread * 1.5 * 1000) / duration 
              << std::endl;
}

// 测试惰性回收和急切回收特性
void testRecycleTraits() {
    std::cout << "\n=== 测试惰性回收和急切回收特性 ===" << std::endl;
    
    std::cout << "== 惰性回收 ==" << std::endl;
    {
        std_mem_pool::IndexedMemPool<TestObject, 32, 200, std::atomic, 
                                    std_mem_pool::IndexedMemPoolTraitsLazyRecycle<TestObject>> 
            lazyPool(10);
        
        uint32_t idx = lazyPool.allocIndex();
        lazyPool[idx].setValue(42);
        
        // 回收对象，不会调用析构函数
        std::cout << "回收对象（惰性回收模式）:" << std::endl;
        lazyPool.recycleIndex(idx);
        
        // 重新分配，不会调用构造函数
        std::cout << "重新分配（惰性回收模式）:" << std::endl;
        uint32_t newIdx = lazyPool.allocIndex();
        std::cout << "重用后的值: " << lazyPool[newIdx].getValue() << std::endl;
        
        // 池销毁时会调用析构函数
        std::cout << "池销毁（惰性回收模式）:" << std::endl;
    }
    
    std::cout << "== 急切回收 ==" << std::endl;
    {
        std_mem_pool::IndexedMemPool<TestObject, 32, 200, std::atomic, 
                                    std_mem_pool::IndexedMemPoolTraitsEagerRecycle<TestObject>> 
            eagerPool(10);
        
        uint32_t idx = eagerPool.allocIndex(42);
        
        // 回收对象，会调用析构函数
        std::cout << "回收对象（急切回收模式）:" << std::endl;
        eagerPool.recycleIndex(idx);
        
        // 重新分配，会调用构造函数
        std::cout << "重新分配（急切回收模式）:" << std::endl;
        uint32_t newIdx = eagerPool.allocIndex(100);
        std::cout << "新分配的值: " << eagerPool[newIdx].getValue() << std::endl;
        
        // 池销毁时不需要再次调用析构函数
        std::cout << "池销毁（急切回收模式）:" << std::endl;
    }
}

// 测试内存池容量
void testCapacity() {
    std::cout << "\n=== 测试内存池容量 ===" << std::endl;
    
    const uint32_t requestedCapacity = 1000;
    std_mem_pool::IndexedMemPool<int> pool(requestedCapacity);
    
    std::cout << "请求的容量: " << requestedCapacity << std::endl;
    std::cout << "实际容量: " << pool.capacity() << std::endl;
    
    // 分配直到满或者到达上限
    std::vector<uint32_t> indices;
    uint32_t idx = 0;
    int allocCount = 0;
    const int maxAllocs = requestedCapacity * 2; // 设置一个合理的上限
    
    while ((idx = pool.allocIndex(allocCount)) != 0 && allocCount < maxAllocs) {
        indices.push_back(idx);
        allocCount++;
    }
    
    std::cout << "成功分配的对象数量: " << allocCount << std::endl;
    assert(allocCount >= requestedCapacity);
    
    // 回收所有对象
    for (uint32_t i : indices) {
        pool.recycleIndex(i);
    }
    indices.clear();
    
    // 再次分配，但只测试到请求的容量
    allocCount = 0;
    for (int i = 0; i < requestedCapacity; ++i) {
        idx = pool.allocIndex(i);
        if (idx != 0) {
            allocCount++;
            pool.recycleIndex(idx);
        } else {
            break;
        }
    }
    
    std::cout << "回收后再次成功分配的对象数量: " << allocCount << std::endl;
    assert(allocCount == requestedCapacity);
}

// 测试定位元素
void testLocateElem() {
    std::cout << "\n=== 测试定位元素 ===" << std::endl;
    
    std_mem_pool::IndexedMemPool<TestObject> pool(100);
    
    uint32_t idx = pool.allocIndex(42);
    TestObject* objPtr = &pool[idx];
    
    uint32_t foundIdx = pool.locateElem(objPtr);
    std::cout << "分配的索引: " << idx << std::endl;
    std::cout << "定位的索引: " << foundIdx << std::endl;
    
    assert(idx == foundIdx);
    assert(pool.locateElem(nullptr) == 0);
    
    pool.recycleIndex(idx);
}

int main() {
    std::cout << "=== StdIndexedMemPool 测试程序 ===" << std::endl;
    
    testBasicAllocation();
    testUniquePtr();
    testMultithreading();
    testRecycleTraits();
    testCapacity();
    testLocateElem();
    
    std::cout << "\n所有测试完成！" << std::endl;
    return 0;
} 