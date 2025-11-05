cmake_minimum_required(VERSION 3.10)
project(dpdk_arp_handler C)

# 设置C标准
set(CMAKE_C_STANDARD 99)

# 查找DPDK包
find_package(PkgConfig REQUIRED)
pkg_check_modules(DPDK REQUIRED libdpdk)

# 包含目录
include_directories(${DPDK_INCLUDE_DIRS})
message(STATUS "DPDK libraries: ${DPDK_LIBRARIES}")
message(STATUS "DPDK library version: ${DPDK_VERSION}")
message(STATUS "DPDK library directories: ${DPDK_LIBRARY_DIRS}")
message(STATUS "DPDK include directories: ${DPDK_INCLUDE_DIRS}")

# 编译选项
add_compile_options(${DPDK_CFLAGS_OTHER})

# 创建静态库
add_library(arp_handler STATIC arp_handler.c)

# 链接DPDK库
target_link_libraries(arp_handler ${DPDK_LIBRARIES})

# 创建示例程序
add_executable(example example.c)
target_link_libraries(example arp_handler ${DPDK_LIBRARIES})