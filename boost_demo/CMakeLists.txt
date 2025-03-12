cmake_minimum_required(VERSION 3.5)
project(boost_demo)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(runtime_output_dir "${CMAKE_BINARY_DIR}/bin")

if(WIN32)
    include_directories(
            ./
            D:\\CppBoost\\boost_1_87_0
    )
    link_directories(
            D:\\CppBoost\\boost_1_87_0\\stage\\lib
    )
    set(Boost_LIBRARIES
            libboost_system-mgw13-mt-x64-1_87.a
    )
    set(WIN32_LIBS ws2_32 mswsock )  # 添加Windows Socket库

else()
    include_directories(
            ./
            /usr/local/include/
    )
    link_directories(
            /usr/local/lib/
    )
    find_package(Boost REQUIRED COMPONENTS system)
endif()

include_directories(${Boost_INCLUDE_DIRS})    # 包含Boost库的头文件目录
add_executable(cppexp main.cpp UrlDecode.hpp)
target_link_libraries(cppexp  ${Boost_LIBRARIES}  ${WIN32_LIBS})    # 链接Boost库