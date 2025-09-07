#define ENABLE_MEMORY_LEAK_DETECTION

#include "MemoryLeakDetector.h"
#include <iostream>

int main() {
    std::cout << "=== Simple Memory Leak Detection Test ===\n\n";

    // 设置警告级别为ERROR_ONLY，减少无关警告
    // 可选值：WARN_NONE(0), WARN_ERROR_ONLY(1), WARN_ALL(2)
    MemoryLeakDetector::getInstance().setWarningLevel(
            MemoryLeakDetector::WARN_ERROR_ONLY);

    // 场景1：正确的内存管理
    std::cout << "1. Correct memory allocation and deallocation:\n";
    int* goodPtr = new int(42);
    std::cout << "   Allocated an integer with value: " << *goodPtr << std::endl;
    delete goodPtr;
    std::cout << "   Memory deallocated\n\n";

    // 场景2：内存泄漏
    std::cout << "2. Intentional memory leak:\n";
    int* leakPtr1 = new int(100);
    std::cout << "   Allocated an integer (value 100), but not deallocating\n";

    auto* leakPtr2 = new double[10];
    std::cout << "   Allocated an array of 10 doubles, but not deallocating\n\n";

    // 场景3：更多泄漏
    std::cout << "3. String allocation:\n";
    char* str = new char[100];
    strcpy(str, "This string will leak!");
    std::cout << "   Allocated string: " << str << std::endl;
    std::cout << "   Intentionally not deallocating this string\n\n";

    // 显示统计信息
    auto& detector = MemoryLeakDetector::getInstance();
    std::cout << "=== Memory Usage Statistics ===\n";
    std::cout << "Total allocations: " << detector.getAllocationCount() << std::endl;
    std::cout << "Total deallocations: " << detector.getDeallocationCount() << std::endl;
    std::cout << "Current leaked memory: " << detector.getCurrentMemoryUsage() << " bytes\n\n";

    std::cout << "Program will exit and automatically generate a detailed memory leak report...\n";

    return 0;
}