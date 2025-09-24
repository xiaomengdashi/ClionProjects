# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(StateMachine)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 查找源文件
file(GLOB SOURCES "*.cpp")

# 为每个源文件创建可执行文件
foreach(SOURCE_FILE ${SOURCES})
    # 获取文件名（不包含路径和扩展名）
    get_filename_component(EXECUTABLE_NAME ${SOURCE_FILE} NAME_WE)
    
    # 创建可执行文件
    add_executable(${EXECUTABLE_NAME} ${SOURCE_FILE})
    
    # 设置属性以便正确包含头文件
    target_include_directories(${EXECUTABLE_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()

# 如果只想构建特定的可执行文件，可以使用以下方式替代上面的循环：
# add_executable(game_example game_example.cpp)
# add_executable(protocol_example protocol_example.cpp)

# 编译选项（可选）
if(MSVC)
    target_compile_options(game_example PRIVATE /W4)
    target_compile_options(protocol_example PRIVATE /W4)
else()
    target_compile_options(game_example PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(protocol_example PRIVATE -Wall -Wextra -pedantic)
endif()
