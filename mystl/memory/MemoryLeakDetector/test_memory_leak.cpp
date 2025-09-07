//
// Created by Kolane on 2025/9/8.
//

// 启用内存泄漏检测
#define ENABLE_MEMORY_LEAK_DETECTION

#include <iostream>
#include <vector>
#include <memory>
#include <cstring>

// 然后包含内存泄漏检测器
#include "MemoryLeakDetector.h"

// 测试类，用于演示对象的内存泄漏
class TestClass {
private:
    int* data;      // 内部动态分配的数据
    size_t size;    // 数据大小

public:
    // 构造函数，分配内存
    TestClass(size_t n = 10) : size(n) {
        std::cout << "TestClass constructor: Allocating " << size << " integers\n";
        data = new int[size];
        for (size_t i = 0; i < size; ++i) {
            data[i] = i;
        }
    }

    // 析构函数，释放内存
    ~TestClass() {
        std::cout << "TestClass destructor: Releasing memory\n";
        delete[] data;
    }

    // 获取数据
    int getValue(size_t index) const {
        return (index < size) ? data[index] : -1;
    }
};

// 测试函数1：正常的内存分配和释放
void testNormalAllocation() {
    std::cout << "\n=== Test 1: Normal Memory Allocation and Deallocation ===\n";

    // 分配单个整数
    int* p1 = new int(42);
    std::cout << "Allocated an integer with value: " << *p1 << std::endl;
    delete p1;

    // 分配整数数组
    int* p2 = new int[100];
    for (int i = 0; i < 100; ++i) {
        p2[i] = i * 2;
    }
    std::cout << "Allocated an array of 100 integers\n";
    delete[] p2;

    // 分配字符串
    char* str = new char[50];
    strcpy(str, "Hello, Memory Leak Detector!");
    std::cout << "Allocated string: " << str << std::endl;
    delete[] str;

    std::cout << "Test 1 completed: All memory correctly deallocated\n";
}

// 测试函数2：故意制造内存泄漏
void testMemoryLeak() {
    std::cout << "\n=== Test 2: Intentional Memory Leak ===\n";

    // 泄漏1：分配但不释放
    int* leak1 = new int(100);
    std::cout << "Allocated an integer (value 100), but intentionally not deallocating\n";

    // 泄漏2：分配数组但不释放
    double* leak2 = new double[50];
    for (int i = 0; i < 50; ++i) {
        leak2[i] = i * 3.14;
    }
    std::cout << "Allocated an array of 50 doubles, but intentionally not deallocating\n";

    // 泄漏3：分配对象但不释放
    TestClass* leak3 = new TestClass(20);
    std::cout << "Allocated a TestClass object, but intentionally not deallocating\n";

    // 泄漏4：分配字符串但不释放
    char* leak4 = new char[256];
    strcpy(leak4, "This is a string that will leak");
    std::cout << "Allocated a string: " << leak4 << ", but intentionally not deallocating\n";

    std::cout << "Test 2 completed: Intentionally leaked 4 memory blocks\n";
}

// 测试函数3：对象数组的分配和释放
void testObjectArray() {
    std::cout << "\n=== Test 3: Object Array Allocation and Deallocation ===\n";

    // 正确的对象数组分配和释放
    TestClass* objArray = new TestClass[5];
    std::cout << "Allocated an array of 5 TestClass objects\n";

    // 使用对象
    for (int i = 0; i < 5; ++i) {
        std::cout << "Object " << i << " first value: " << objArray[i].getValue(0) << std::endl;
    }

    // 正确释放
    delete[] objArray;
    std::cout << "Object array deallocated\n";

    // 故意泄漏一个对象数组
    TestClass* leakedArray = new TestClass[3];
    std::cout << "Allocated an array of 3 TestClass objects, but intentionally not deallocating\n";

    std::cout << "Test 3 completed\n";
}

// 测试函数4：复杂的内存分配模式
void testComplexPattern() {
    std::cout << "\n=== Test 4: Complex Memory Allocation Pattern ===\n";

    // 使用容器存储动态分配的指针
    std::vector<int*> pointers;

    // 分配多个内存块
    for (int i = 0; i < 10; ++i) {
        pointers.push_back(new int(i * 10));
    }
    std::cout << "Allocated 10 integers through vector\n";

    // 释放一半的内存
    for (int i = 0; i < 5; ++i) {
        delete pointers[i];
        pointers[i] = nullptr;
    }
    std::cout << "Deallocated first 5 integers\n";

    // 剩余的5个将会泄漏
    std::cout << "Remaining 5 integers will leak\n";

    // 嵌套分配
    int*** nested = new int**[3];
    for (int i = 0; i < 3; ++i) {
        nested[i] = new int*[4];
        for (int j = 0; j < 4; ++j) {
            nested[i][j] = new int(i * 10 + j);
        }
    }
    std::cout << "Created a 3x4 nested dynamic array\n";

    // 只释放部分嵌套数组
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 4; ++j) {
            delete nested[i][j];
        }
        delete[] nested[i];
    }
    // 故意不释放第3行和最外层数组，造成泄漏
    std::cout << "Partially deallocated nested array, intentionally leaking some parts\n";

    std::cout << "Test 4 completed\n";
}

// 测试函数5：测试检测器的开关功能
void testDetectorControl() {
    std::cout << "\n=== Test 5: Detector Control Functionality ===\n";

    // 暂时禁用检测器
    MEMORY_LEAK_DETECTOR_DISABLE();
    std::cout << "Detector disabled\n";

    // 这些分配不会被记录
    int* untracked1 = new int(999);
    int* untracked2 = new int[50];
    std::cout << "Allocated some memory that won't be tracked\n";

    // 重新启用检测器
    MEMORY_LEAK_DETECTOR_ENABLE();
    std::cout << "Detector re-enabled\n";

    // 这些分配会被记录
    int* tracked = new int(888);
    std::cout << "Allocated memory that will be tracked\n";

    // 释放被跟踪的内存
    delete tracked;

    // 释放未跟踪的内存（不会在报告中显示）
    delete untracked1;
    delete[] untracked2;

    std::cout << "Test 5 completed\n";
}

// 主函数
int main() {
    std::cout << "========================================\n";
    std::cout << "     Memory Leak Detector Test Program\n";
    std::cout << "========================================\n";

    // 设置警告级别为ERROR_ONLY，只显示严重错误
    // 这样可以避免STL容器和系统库产生的大量无关警告
    MemoryLeakDetector::getInstance().setWarningLevel(
            MemoryLeakDetector::WARN_ERROR_ONLY);

    // 在开始测试前显示初始状态
    std::cout << "\nInitial state:\n";
    std::cout << "Current memory usage: " << MemoryLeakDetector::getInstance().getCurrentMemoryUsage() << " bytes\n";

    // 执行各项测试
    testNormalAllocation();     // 测试正常分配和释放
    testMemoryLeak();           // 测试内存泄漏
    testObjectArray();          // 测试对象数组
    testComplexPattern();       // 测试复杂模式
    testDetectorControl();      // 测试检测器控制

    // 显示最终统计
    std::cout << "\n========================================\n";
    std::cout << "Test completed, statistics:\n";
    auto& detector = MemoryLeakDetector::getInstance();
    std::cout << "Total allocations: " << detector.getAllocationCount() << "\n";
    std::cout << "Total deallocations: " << detector.getDeallocationCount() << "\n";
    std::cout << "Total allocated memory: " << detector.getTotalAllocated() << " bytes\n";
    std::cout << "Total deallocated memory: " << detector.getTotalDeallocated() << " bytes\n";
    std::cout << "Current memory usage: " << detector.getCurrentMemoryUsage() << " bytes\n";

    // 手动生成报告（析构函数也会自动生成）
    std::cout << "\nManually generating memory leak report:\n";
    MEMORY_LEAK_DETECTOR_REPORT();

    std::cout << "\nProgram will exit, destructor will generate report again...\n";

    return 0;
}