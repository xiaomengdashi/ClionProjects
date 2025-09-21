# CMake最低版本要求
cmake_minimum_required(VERSION 3.10)

# 项目名称和版本
project(ThreadSafeMemoryPool VERSION 1.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # GCC或Clang编译器选项
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2 -g")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -DDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # MSVC编译器选项
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
endif()

# 查找线程库
find_package(Threads REQUIRED)

# 定义源文件
set(MEMORY_POOL_SOURCES
    src/memory_block.cpp
    src/memory_pool.cpp
)

# 定义头文件
set(MEMORY_POOL_HEADERS
    src/memory_block.h
    src/memory_pool.h
)

# 创建静态库
add_library(memory_pool_lib STATIC ${MEMORY_POOL_SOURCES})

# 设置包含目录
target_include_directories(memory_pool_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# 链接线程库
target_link_libraries(memory_pool_lib PUBLIC Threads::Threads)

# 创建测试可执行文件
add_executable(test_memory_pool test/test_memory_pool.cpp)

# 链接内存池库
target_link_libraries(test_memory_pool memory_pool_lib)

# 添加测试目标
enable_testing()
add_test(NAME memory_pool_test COMMAND test_memory_pool)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 打印构建信息
message(STATUS "构建类型: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++标准: ${CMAKE_CXX_STANDARD}")
message(STATUS "编译器: ${CMAKE_CXX_COMPILER_ID}")