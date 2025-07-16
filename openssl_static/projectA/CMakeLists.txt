cmake_minimum_required(VERSION 3.16)
project(ProjectA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Include directories
include_directories(include)

# Source files
set(SOURCES
    src/crypto_utils.cpp
)

# Create static library
add_library(ProjectA STATIC ${SOURCES})

# Link OpenSSL static libraries
target_link_libraries(ProjectA PRIVATE OpenSSL::SSL OpenSSL::Crypto)

# Set output directory
set_target_properties(ProjectA PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# Install library
install(TARGETS ProjectA
    ARCHIVE DESTINATION lib
)

# Install headers
install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)