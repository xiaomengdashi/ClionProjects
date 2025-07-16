cmake_minimum_required(VERSION 3.16)
project(ProjectB)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# OpenSSL is already linked in ProjectA static library
# No need to find OpenSSL separately

# Add ProjectA library path
set(PROJECT_A_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../projectA)

# Include ProjectA headers
include_directories(${PROJECT_A_DIR}/include)

# Source files
set(SOURCES
    src/main.cpp
)

# Create executable
add_executable(ProjectB ${SOURCES})

# Link ProjectA static library only
target_link_libraries(ProjectB PRIVATE 
    ProjectA
)

# Set output directory
set_target_properties(ProjectB PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Add dependency on ProjectA (if building together)
if(TARGET ProjectA)
    add_dependencies(ProjectB ProjectA)
endif()