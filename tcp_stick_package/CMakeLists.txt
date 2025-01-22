cmake_minimum_required(VERSION 3.22)
project(tcp_stick_package)

set(CMAKE_CXX_STANDARD 20)

add_executable(tcp_stick_package main.cpp
        socket.h
        socket.c)
