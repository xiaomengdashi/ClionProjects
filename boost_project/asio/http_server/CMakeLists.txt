cmake_minimum_required(VERSION 3.5)
project(boost_demo)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")


if(WIN32)
    include_directories(
            ./include/
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
            ./include
            /usr/local/include/
    )
    link_directories(
            /usr/local/lib/
    )
    find_package(Boost REQUIRED COMPONENTS system)
endif()

# 源文件配置
file(GLOB_RECURSE SRC_FILES
        src/*.cpp
        include/*.hpp
)


include_directories(${Boost_INCLUDE_DIRS})    # 包含Boost库的头文件目录
add_executable(HttpServer ${SRC_FILES} )
target_link_libraries(HttpServer  ${Boost_LIBRARIES}  ${WIN32_LIBS})    # 链接Boost库