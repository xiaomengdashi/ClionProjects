cmake_minimum_required(VERSION 3.20)
project(MemoryLeakDetector)

set(CMAKE_CXX_STANDARD 20)

# 设置编译器标志
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# 创建MemoryLeakDetector库
add_library(MemoryLeakDetector
    MemoryLeakDetector.cpp
    MemoryLeakDetector.h
)

# 设置包含目录
target_include_directories(MemoryLeakDetector PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# 定义编译宏
target_compile_definitions(MemoryLeakDetector PUBLIC ENABLE_MEMORY_LEAK_DETECTOR)

# simple_test 可执行文件
add_executable(simple_test
    simple_test.cpp
)

# 链接MemoryLeakDetector库到simple_test
target_link_libraries(simple_test
    PRIVATE MemoryLeakDetector
)

# test_memory_leak 可执行文件
add_executable(test_memory_leak
    test_memory_leak.cpp
)

# 链接MemoryLeakDetector库到test_memory_leak
target_link_libraries(test_memory_leak
    PRIVATE MemoryLeakDetector
)