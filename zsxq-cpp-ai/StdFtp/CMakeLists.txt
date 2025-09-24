# FTP服务器和客户端项目的CMake构建文件
cmake_minimum_required(VERSION 3.10)
project(StdFTP VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 添加包含目录
include_directories(${CMAKE_SOURCE_DIR}/include)

# 查找必要的库
find_package(Threads REQUIRED)

# 公共源文件
set(COMMON_SOURCES
    common/ftp_protocol.cpp
    common/ftp_session.cpp
)

# 服务器源文件
set(SERVER_SOURCES
    server/ftp_server.cpp
    ${COMMON_SOURCES}
)

# 客户端源文件
set(CLIENT_SOURCES
    client/ftp_client.cpp
    common/ftp_protocol.cpp
)

# 构建FTP服务器
add_executable(ftp_server ${SERVER_SOURCES})
target_link_libraries(ftp_server 
    Threads::Threads
)

# 构建FTP客户端
add_executable(ftp_client ${CLIENT_SOURCES})
target_link_libraries(ftp_client 
    Threads::Threads
)

# 安装规则
install(TARGETS ftp_server ftp_client
    RUNTIME DESTINATION bin
)

# 创建FTP根目录
install(DIRECTORY DESTINATION /tmp/ftp
    DIRECTORY_PERMISSIONS OWNER_WRITE OWNER_READ OWNER_EXECUTE
                         GROUP_WRITE GROUP_READ GROUP_EXECUTE
                         WORLD_READ WORLD_EXECUTE
)

# 添加测试（可选）
enable_testing()

# 打印配置信息
message(STATUS "")
message(STATUS "========================================")
message(STATUS "FTP服务器和客户端项目配置:")
message(STATUS "========================================")
message(STATUS "CMake版本: ${CMAKE_VERSION}")
message(STATUS "项目版本: ${PROJECT_VERSION}")
message(STATUS "C++标准: C++${CMAKE_CXX_STANDARD}")
message(STATUS "构建类型: ${CMAKE_BUILD_TYPE}")
message(STATUS "编译器: ${CMAKE_CXX_COMPILER}")
message(STATUS "编译选项: ${CMAKE_CXX_FLAGS}")
message(STATUS "输出目录: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "========================================")
message(STATUS "")