cmake_minimum_required(VERSION 3.10)
project(VideoChat)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加编译选项，明确指定 OpenSSL API 兼容性
add_definitions(-DOPENSSL_API_COMPAT=0x30000000L) # 兼容 OpenSSL 3.0.x

# 添加external目录下的库
include_directories(external/cpp-httplib)
include_directories(external/websocketpp)
include_directories(external/json-develop/include)

# 查找必要的库
find_package(Threads REQUIRED)
find_package(OpenSSL 3.0.0 REQUIRED) # 指定最低版本要求
find_package(Boost COMPONENTS system thread REQUIRED)

# 打印 OpenSSL 版本信息（用于调试）
message(STATUS "OpenSSL Version: ${OPENSSL_VERSION}")
message(STATUS "OpenSSL Include Dir: ${OPENSSL_INCLUDE_DIR}")
message(STATUS "OpenSSL Libraries: ${OPENSSL_LIBRARIES}")

# 设置源文件
set(SERVER_SOURCES
    src/server/main.cpp
    src/server/ChatServer.cpp
    src/server/RoomManager.cpp
    src/server/WebSocketHandler.cpp
)

# 创建可执行文件
add_executable(chat_server ${SERVER_SOURCES})

# 链接库（移除重复项）
target_link_libraries(chat_server
    ${CMAKE_THREAD_LIBS_INIT}
    ${OPENSSL_LIBRARIES}
    ${Boost_LIBRARIES}
    uuid
    stdc++fs  # 为旧版本GCC添加filesystem库支持
)

# 设置包含目录
target_include_directories(chat_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/server
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common
    ${OPENSSL_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
)

# 添加编译选项（可选）
if(CMAKE_COMPILER_IS_GNUCXX)
    target_compile_options(chat_server PRIVATE -Wall -Wextra -pedantic)
endif()
