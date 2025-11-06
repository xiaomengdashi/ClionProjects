cmake_minimum_required(VERSION 3.10)
project(ZeroMQ_Examples)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find ZeroMQ using pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)

# Include directories (使用系统已安装的cppzmq头文件)
include_directories(${ZMQ_INCLUDE_DIRS})
link_directories(${ZMQ_LIBRARY_DIRS})

# 请求-应答模式
add_executable(req_rep_server examples/01_request_reply/server.cpp)
target_link_libraries(req_rep_server ${ZMQ_LIBRARIES})

add_executable(req_rep_client examples/01_request_reply/client.cpp)
target_link_libraries(req_rep_client ${ZMQ_LIBRARIES})

# 发布-订阅模式
add_executable(pub_sub_pub examples/02_pub_sub/publisher.cpp)
target_link_libraries(pub_sub_pub ${ZMQ_LIBRARIES})

add_executable(pub_sub_sub examples/02_pub_sub/subscriber.cpp)
target_link_libraries(pub_sub_sub ${ZMQ_LIBRARIES})

# 推送-拉取模式
add_executable(push_pull_push examples/03_push_pull/pusher.cpp)
target_link_libraries(push_pull_push ${ZMQ_LIBRARIES})

add_executable(push_pull_pull examples/03_push_pull/puller.cpp)
target_link_libraries(push_pull_pull ${ZMQ_LIBRARIES})

# 路由-经销模式
add_executable(router_dealer_router examples/04_router_dealer/router.cpp)
target_link_libraries(router_dealer_router ${ZMQ_LIBRARIES})

add_executable(router_dealer_dealer examples/04_router_dealer/dealer.cpp)
target_link_libraries(router_dealer_dealer ${ZMQ_LIBRARIES})

# IPC传输方式（进程间通信）
add_executable(ipc_server examples/05_ipc/server.cpp)
target_link_libraries(ipc_server ${ZMQ_LIBRARIES})

add_executable(ipc_client examples/05_ipc/client.cpp)
target_link_libraries(ipc_client ${ZMQ_LIBRARIES})

# INPROC传输方式（线程间通信）
add_executable(inproc_demo examples/06_inproc/main.cpp)
target_link_libraries(inproc_demo ${ZMQ_LIBRARIES} pthread)
