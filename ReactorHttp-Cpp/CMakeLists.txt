cmake_minimum_required(VERSION 3.0)
project(ReactorHttp_Cpp)

set(CMAKE_CXX_STANDARD 20)

# 添加头文件搜索路径
include_directories(${CMAKE_SOURCE_DIR}/include)

# 查找源文件
file(GLOB SOURCES "source/*.cpp")


# 创建可执行文件
add_executable(ReactorHttp_Cpp ${SOURCES}
        include/SelectDispatcher.h)

# 链接库
target_link_libraries(ReactorHttp_Cpp pthread)

# 设置编译选项
target_compile_options(ReactorHttp_Cpp PRIVATE -Wall -Wextra)