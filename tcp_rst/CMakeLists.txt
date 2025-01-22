cmake_minimum_required(VERSION 3.0)
project(tcp_rst C)

set(CMAKE_C_STANDARD 11)


set(runtime_output_dir "${CMAKE_BINARY_DIR}/bin")
add_executable(tcp_rst_server server.c)

add_executable(tcp_rst_client client.c)
