cmake_minimum_required(VERSION 3.16)
project(ProjectA VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译选项
option(BUILD_TESTS "Build unit tests" OFF)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
option(ENABLE_COVERAGE "Enable code coverage" OFF)

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Include directories
include_directories(include)

# Source files
set(SOURCES
    src/crypto_utils.cpp
    src/crypto_config.cpp
    src/crypto_logger.cpp
    src/crypto_security.cpp
)

# Create static library
add_library(ProjectA STATIC ${SOURCES})

# 设置编译器特定选项
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ProjectA PRIVATE 
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
    )
endif()

# 代码覆盖率支持
if(ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ProjectA PRIVATE --coverage)
    target_link_libraries(ProjectA PRIVATE --coverage)
endif()

# Link OpenSSL static libraries
target_link_libraries(ProjectA PRIVATE OpenSSL::SSL OpenSSL::Crypto)

# Set output directory
set_target_properties(ProjectA PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# 单元测试
if(BUILD_TESTS)
    find_package(GTest REQUIRED)
    enable_testing()
    
    add_executable(crypto_tests tests/test_crypto_utils.cpp)
    target_link_libraries(crypto_tests 
        PRIVATE 
        ProjectA 
        GTest::gtest 
        GTest::gtest_main
    )
    
    add_test(NAME CryptoUtilsTests COMMAND crypto_tests)
    
    set_target_properties(crypto_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    )
    
    # 安全功能测试
    add_executable(security_tests tests/test_crypto_security.cpp)
    target_link_libraries(security_tests 
        PRIVATE 
        ProjectA 
        GTest::gtest 
        GTest::gtest_main
    )
    
    add_test(NAME CryptoSecurityTests COMMAND security_tests)
    
    set_target_properties(security_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    )
endif()

# 性能基准测试
if(BUILD_BENCHMARKS)
    find_package(benchmark REQUIRED)
    
    add_executable(crypto_benchmark benchmarks/crypto_benchmark.cpp)
    target_link_libraries(crypto_benchmark 
        PRIVATE 
        ProjectA 
        benchmark::benchmark
    )
    
    set_target_properties(crypto_benchmark PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/benchmarks
    )
endif()

# Install library
install(TARGETS ProjectA
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# 生成pkg-config文件
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/projecta.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/projecta.pc"
    @ONLY
)