cmake_minimum_required(VERSION 3.0)
project(boost_demo)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)




set(runtime_output_dir "${CMAKE_BINARY_DIR}/bin")




include_directories(
        D:\\CppBoost\\boost_1_87_0
)

link_directories(
        D:\\CppBoost\\boost_1_87_0\\stage\\lib
)

add_executable(main main.cpp)
