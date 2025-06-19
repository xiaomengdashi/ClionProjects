cmake_minimum_required(VERSION 3.12)
project(grpc_test)


# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


set(_GRPC_GRPCPP gRPC::grpc++)
set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
set(_REFLECTION gRPC::grpc++_reflection)


find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)



file(GLOB PROTO_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)


add_executable(grpc_echo_server
    grpc_echo_server.cpp
    ${PROTO_SRCS}
)

add_executable(grpc_echo_client
    grpc_echo_client.cpp
    ${PROTO_SRCS}
)

target_link_libraries(grpc_echo_server
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF}
)

target_link_libraries(grpc_echo_client
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF}
)