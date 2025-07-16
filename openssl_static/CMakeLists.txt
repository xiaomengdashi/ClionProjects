cmake_minimum_required(VERSION 3.16)
project(OpenSSLStaticDemo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译选项
option(BUILD_EXAMPLES "Build example programs" ON)

# Build order: ProjectA first, then ProjectB
add_subdirectory(projectA)
add_subdirectory(projectB)

# 添加示例程序
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Make ProjectB depend on ProjectA
add_dependencies(ProjectB ProjectA)