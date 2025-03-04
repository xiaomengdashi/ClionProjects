cmake_minimum_required(VERSION 3.12)
project(msg_queue)

set(CMAKE_CXX_STANDARD 17)

# 添加 gtest 库
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# 添加你的源文件和测试文件
add_executable(msg_queue_test
    msg_que.h
    msg_que.cpp
    test_msg_queue.cpp
)

# 链接 gtest 库
target_link_libraries(msg_queue_test ${GTEST_LIBRARIES} pthread)