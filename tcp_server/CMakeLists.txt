cmake_minimum_required(VERSION 3.22)
project(TcpServer)
set(CMAKE_CXX_STANDARD 11) # 与代码中的 C++ 标准保持一致

if(WIN32)
    include_directories(
            D:\\CppBoost\\boost_1_87_0
    )
    link_directories(
            D:\\CppBoost\\build\\lib
    )

    set(Boost_LIBRARIES
            libboost_system-mgw13-mt-x32-1_87.a
            libboost_filesystem-mt.lib
            libboost_thread-mt.lib
    )
else()
# 查找 Boost 库，要求版本 1.87.0，需要 system 和 filesystem 组件
find_package(Boost 1.87.0 COMPONENTS system filesystem thread REQUIRED)
if(Boost_FOUND)
    message(STATUS "Boost 库已找到，包含目录: ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost 库文件: ${Boost_LIBRARIES}")
else()
    message(FATAL_ERROR "未找到所需的 Boost 库")
endif()
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


add_executable(tcp_client client.cpp)
target_link_libraries(tcp_client ${Boost_LIBRARIES} pthread)