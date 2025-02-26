cmake_minimum_required(VERSION 3.22)
project(thread_pool3)

set(CMAKE_CXX_STANDARD 20)

# 添加源文件
add_executable(thread_pool3 main.cpp)

# 添加头文件搜索路径
target_include_directories(thread_pool3 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 链接库
target_link_libraries(thread_pool3 pthread)

# 设置编译选项
target_compile_options(thread_pool3 PRIVATE -Wall -Wextra)
