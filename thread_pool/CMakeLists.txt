cmake_minimum_required(VERSION 3.22)
project(thread_pool)

set(CMAKE_CXX_STANDARD 20)

add_executable(thread_pool main.cpp
        threadpool.cpp
        threadpool.h)
