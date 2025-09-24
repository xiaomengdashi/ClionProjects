# CMakeLists.txt for Load Balancing Algorithms Study
# 负载均衡算法学习案例 CMake 配置文件

cmake_minimum_required(VERSION 3.10)
project(LoadBalancerDemo)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加编译选项
add_compile_options(-Wall -Wextra -O2)

# 查找线程库
find_package(Threads REQUIRED)

# 头文件
set(HEADERS
    Server.h
    LoadBalancer.h
    RoundRobinBalancer.h
    ConsistentHashBalancer.h
    LeastConnectionsBalancer.h
)

# 源文件
set(SOURCES
    main.cpp
)

# 创建可执行文件
add_executable(load_balancer_demo ${SOURCES} ${HEADERS})

# 链接线程库
target_link_libraries(load_balancer_demo ${CMAKE_THREAD_LIBS_INIT})

# 设置输出目录
set_target_properties(load_balancer_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 添加自定义命令
add_custom_target(run
    COMMAND load_balancer_demo
    DEPENDS load_balancer_demo
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    COMMENT "运行负载均衡算法演示"
)

# 安装规则（可选）
install(TARGETS load_balancer_demo
    RUNTIME DESTINATION bin
)