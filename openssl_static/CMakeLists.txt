cmake_minimum_required(VERSION 3.16)
project(OpenSSLStaticDemo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build order: ProjectA first, then ProjectB
add_subdirectory(projectA)
add_subdirectory(projectB)

# Make ProjectB depend on ProjectA
add_dependencies(ProjectB ProjectA)