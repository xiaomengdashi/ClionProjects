cmake_minimum_required(VERSION 3.22)
project(mystl)

set(CMAKE_CXX_STANDARD 17)

add_executable(mystl
        main.cpp
        myshared_ptr.hpp
        MyVector.hpp
        lock.hpp
        stack.hpp
        mutex.hpp
        algorithm.hpp
        list.hpp
)
