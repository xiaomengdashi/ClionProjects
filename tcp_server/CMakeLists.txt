cmake_minimum_required(VERSION 3.30)
project(TcpServer)
set(CMAKE_CXX_STANDARD 11) # 与代码中的 C++ 标准保持一致

# 查找 Boost 库，要求版本 1.87.0，需要 system 和 filesystem 组件
find_package(Boost 1.87.0 COMPONENTS system filesystem thread REQUIRED)
if(Boost_FOUND)
    message(STATUS "Boost 库已找到，包含目录: ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost 库文件: ${Boost_LIBRARIES}")
else()
    message(FATAL_ERROR "未找到所需的 Boost 库")
endif()

# 包含 Boost 库的头文件目录
include_directories(${Boost_INCLUDE_DIRS})

# 添加可执行文件
add_executable(tcp_server main.cpp
        CServer.cpp
        CServer.h
        CSession.cpp
        CSession.h
        const.h)

# 链接 Boost 库和 pthread 库到可执行文件
target_link_libraries(tcp_server ${Boost_LIBRARIES} pthread)


add_executable(client client.cpp)
target_link_libraries(client ${Boost_LIBRARIES} pthread)