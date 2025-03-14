cmake_minimum_required(VERSION 3.30)
project(design_patterns)
set(CMAKE_CXX_STANDARD 17)


find_package(Boost 1.87.0 COMPONENTS system filesystem REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})


add_executable(design_patterns
        main.cpp
        singleton.hpp
        strategy.hpp
        factory_pattern.hpp
)

# 链接 Boost 库到可执行文件
target_link_libraries(design_patterns ${Boost_LIBRARIES})
