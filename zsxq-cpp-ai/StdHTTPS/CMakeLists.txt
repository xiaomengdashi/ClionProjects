cmake_minimum_required(VERSION 3.11)
project(StdHTTPS VERSION 1.0.0 LANGUAGES CXX)

# 设置C++11标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g -O2")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# 查找依赖库
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# 查找OpenSSL
find_package(OpenSSL REQUIRED)
if(OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
    message(STATUS "OpenSSL包含目录: ${OPENSSL_INCLUDE_DIR}")
    message(STATUS "OpenSSL库: ${OPENSSL_LIBRARIES}")
endif()

# 包含目录
include_directories(include)
include_directories(src)

# 源文件
set(SOURCES
    src/http_parser.cpp
    src/http_message.cpp
    src/chunked_encoder.cpp
    src/ssl_handler.cpp
    src/connection_pool.cpp
    src/http_server.cpp
    src/http_client.cpp
)

# 创建静态库
add_library(stdhttps STATIC ${SOURCES})
target_link_libraries(stdhttps ${OPENSSL_LIBRARIES} Threads::Threads)

# 设置库的包含目录
target_include_directories(stdhttps PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# 示例程序
add_executable(server_example examples/server_example.cpp)
target_link_libraries(server_example stdhttps)

add_executable(client_example examples/client_example.cpp)
target_link_libraries(client_example stdhttps)

# 测试程序（可选）
enable_testing()
add_subdirectory(tests EXCLUDE_FROM_ALL)

# 安装规则
install(TARGETS stdhttps server_example client_example
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)

# 创建pkg-config文件
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/stdhttps.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/stdhttps.pc"
    @ONLY
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/stdhttps.pc"
        DESTINATION lib/pkgconfig)

# 打印构建信息
message(STATUS "项目名称: ${PROJECT_NAME}")
message(STATUS "项目版本: ${PROJECT_VERSION}")
message(STATUS "构建类型: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++标准: ${CMAKE_CXX_STANDARD}")
message(STATUS "编译器: ${CMAKE_CXX_COMPILER}")