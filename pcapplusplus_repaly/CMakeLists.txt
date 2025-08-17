cmake_minimum_required(VERSION 3.10)
project(send_pcap)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform-specific compiler detection
if(WIN32)
    message(STATUS "Configuring for Windows")
    if(MSVC)
        message(STATUS "Using MSVC compiler")
    elseif(MINGW)
        message(STATUS "Using MinGW compiler")
    endif()
elseif(APPLE)
    message(STATUS "Configuring for macOS")
    message(STATUS "Using Clang compiler")
elseif(UNIX)
    message(STATUS "Configuring for Linux")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(STATUS "Using GCC compiler")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(STATUS "Using Clang compiler")
    endif()
endif()

# Find PcapPlusPlus
# Allow user to specify PCAPPLUSPLUS_ROOT via environment variable or CMake variable
if(NOT DEFINED PCAPPLUSPLUS_ROOT)
    if(DEFINED ENV{PCAPPLUSPLUS_ROOT})
        set(PCAPPLUSPLUS_ROOT $ENV{PCAPPLUSPLUS_ROOT})
    elseif(WIN32)
        # Default path for Windows
        set(PCAPPLUSPLUS_ROOT "C:/PcapPlusPlus")
    elseif(APPLE)
        # Default path for macOS with Homebrew
        set(PCAPPLUSPLUS_ROOT "/opt/homebrew/opt/pcapplusplus")
    elseif(UNIX)
        # Default path for Linux
        set(PCAPPLUSPLUS_ROOT "/usr/local")
    else()
        message(FATAL_ERROR "PCAPPLUSPLUS_ROOT not specified. Please set it via -DPCAPPLUSPLUS_ROOT=<path> or environment variable")
    endif()
endif()

message(STATUS "Using PcapPlusPlus from: ${PCAPPLUSPLUS_ROOT}")

# Include directories
include_directories(${PCAPPLUSPLUS_ROOT}/include)

# Link directories
if(WIN32)
    link_directories(${PCAPPLUSPLUS_ROOT}/lib)
else()
    link_directories(${PCAPPLUSPLUS_ROOT}/lib)
endif()

# Add subdirectories
add_subdirectory(src)
add_subdirectory(test)