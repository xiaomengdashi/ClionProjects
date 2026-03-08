cmake_minimum_required(VERSION 3.10)
project(MemoryPoolDemo CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -Wall -Wextra")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -DNDEBUG")
endif()

# 收集头文件和源文件
file(GLOB HEADERS "*.h" "*.hpp" "core/*.h" "core/*.hpp" "utils/*.h" "utils/*.hpp")
file(GLOB SOURCES "*.cpp")

# 创建主程序可执行文件
add_executable(memorypool_demo 
    main.cpp
    ${HEADERS}
)

# 创建测试程序可执行文件
add_executable(memorypool_allocator
    tests/allocator_demo.cpp
    ${HEADERS}
)

# 创建工厂演示程序可执行文件
add_executable(memorypool_factory
    tests/factory_demo.cpp
    ${HEADERS}
)

# 创建线程安全演示程序可执行文件
add_executable(memorypool_threadsafe
    tests/threadsafe_demo.cpp
    ${HEADERS}
)

# 创建索引内存池演示程序可执行文件
add_executable(memorypool_indexed
    tests/indexed_pool_demo.cpp
    ${HEADERS}
)

# 创建NGAP消息演示程序可执行文件
add_executable(memorypool_ngap
    tests/ngap_demo.cpp
    ${HEADERS}
)

# 创建NGAP复杂示例可执行文件
add_executable(ngap_complex_example
    examples/ngap_complex_example.cpp
    ${HEADERS}
)

# 添加包含目录
target_include_directories(memorypool_demo PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)
target_include_directories(memorypool_allocator PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)
target_include_directories(memorypool_factory PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)
target_include_directories(memorypool_threadsafe PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)
target_include_directories(memorypool_indexed PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)
target_include_directories(memorypool_ngap PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/ngap
)
target_include_directories(ngap_complex_example PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/ngap
)

# 添加 pthread 支持
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(memorypool_demo PRIVATE Threads::Threads)
target_link_libraries(memorypool_allocator PRIVATE Threads::Threads)
target_link_libraries(memorypool_factory PRIVATE Threads::Threads)
target_link_libraries(memorypool_threadsafe PRIVATE Threads::Threads)
target_link_libraries(memorypool_indexed PRIVATE Threads::Threads)
target_link_libraries(memorypool_ngap PRIVATE Threads::Threads)
target_link_libraries(ngap_complex_example PRIVATE Threads::Threads)