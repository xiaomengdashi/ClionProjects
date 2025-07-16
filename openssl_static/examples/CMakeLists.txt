cmake_minimum_required(VERSION 3.16)

# 示例程序
add_executable(crypto_examples crypto_examples.cpp)

# 添加包含目录
target_include_directories(crypto_examples PRIVATE 
    ${CMAKE_SOURCE_DIR}/projectA/include
)

# 链接ProjectA库
target_link_libraries(crypto_examples PRIVATE ProjectA)

# 设置输出目录
set_target_properties(crypto_examples PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
)

# 安全功能示例程序
add_executable(security_examples security_examples.cpp)

# 添加包含目录
target_include_directories(security_examples PRIVATE 
    ${CMAKE_SOURCE_DIR}/projectA/include
)

# 链接ProjectA库
target_link_libraries(security_examples PRIVATE ProjectA)

# 设置输出目录
set_target_properties(security_examples PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
)