cmake_minimum_required(VERSION 3.22)
project(untitled CXX)

set(CMAKE_CXX_STANDARD_REQUIRED    11)

add_executable(untitled
        main.cpp
        singleton.hpp
        string.hpp
        count_down_latch.hpp)
