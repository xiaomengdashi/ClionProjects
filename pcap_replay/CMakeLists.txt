cmake_minimum_required(VERSION 3.16)
project(pcap_replay)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 libpcap
find_path(PCAP_INCLUDE_DIR pcap.h)
find_library(PCAP_LIBRARY pcap)

if(NOT PCAP_INCLUDE_DIR OR NOT PCAP_LIBRARY)
    message(FATAL_ERROR "libpcap not found. Please install libpcap development package.")
endif()

# 包含目录
include_directories(${PCAP_INCLUDE_DIR})
include_directories(include)

# 源文件
set(COMMON_SOURCES
    src/pcap_parser.cpp
    src/packet_modifier.cpp
    src/network_utils.cpp
)

# 可执行文件
add_executable(pcap_server
    src/server.cpp
    ${COMMON_SOURCES}
)

add_executable(pcap_client
    src/client.cpp
    ${COMMON_SOURCES}
)

add_executable(pcap_replayer
    src/replayer.cpp
    ${COMMON_SOURCES}
)

# 链接库
target_link_libraries(pcap_server ${PCAP_LIBRARY})
target_link_libraries(pcap_client ${PCAP_LIBRARY})
target_link_libraries(pcap_replayer ${PCAP_LIBRARY})

# 编译选项
target_compile_options(pcap_server PRIVATE -Wall -Wextra)
target_compile_options(pcap_client PRIVATE -Wall -Wextra)
target_compile_options(pcap_replayer PRIVATE -Wall -Wextra)