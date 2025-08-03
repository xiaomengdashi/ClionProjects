cmake_minimum_required(VERSION 3.10)
project(5gc_amf_system CXX)

set(CMAKE_CXX_STANDARD 17)

# 包含目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# 定义源文件
set(AMF_SOURCES
    src/AmfSm.cpp
    src/SbiMessage.cpp
    src/SbiMessageFactory.cpp
    src/HttpServer.cpp
    src/UeContext.cpp
    src/AmfConfiguration.cpp
    src/N1N2Interface.cpp
    src/NfManagement.cpp
)

# HTTP AMF服务器 - 核心AMF程序
add_executable(amf_http_server amf_http_server.cpp ${AMF_SOURCES})
target_include_directories(amf_http_server PRIVATE include)