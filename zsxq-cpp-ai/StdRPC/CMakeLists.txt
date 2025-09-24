# CMake构建配置文件

# 最低CMake版本要求
cmake_minimum_required(VERSION 3.10)

# 项目名称和版本
project(StdRPC VERSION 1.0.0 LANGUAGES CXX)

# C++标准设置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 设置编译标志
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")

# 输出目录设置
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 包含目录
include_directories(${CMAKE_SOURCE_DIR}/include)

# 线程库
find_package(Threads REQUIRED)

# StdRPC头文件库（仅头文件）
add_library(stdrpc INTERFACE)
target_include_directories(stdrpc INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(stdrpc INTERFACE Threads::Threads)

# 计算器服务器示例
add_executable(calculator_server
    examples/calculator_server.cpp
)
target_link_libraries(calculator_server stdrpc)

# 计算器客户端示例
add_executable(calculator_client
    examples/calculator_client.cpp
)
target_link_libraries(calculator_client stdrpc)

# 简单测试程序
add_executable(simple_test
    examples/simple_test.cpp
)
target_link_libraries(simple_test stdrpc)

# 安装规则
install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)

install(TARGETS calculator_server calculator_client
    RUNTIME DESTINATION bin
)

# 打印构建信息
message(STATUS "========================================")
message(STATUS "StdRPC 构建配置")
message(STATUS "========================================")
message(STATUS "CMake版本: ${CMAKE_VERSION}")
message(STATUS "编译器: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++标准: ${CMAKE_CXX_STANDARD}")
message(STATUS "构建类型: ${CMAKE_BUILD_TYPE}")
message(STATUS "安装前缀: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "========================================")