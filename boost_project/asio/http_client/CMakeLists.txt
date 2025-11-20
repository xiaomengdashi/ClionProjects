cmake_minimum_required(VERSION 3.5)
project(HttpClient)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Find required packages
find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)

# Add include paths
include_directories(
    ${BOOST_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

# Source files
set(SOURCES
    src/HttpRequest.cpp
    src/HttpResponse.cpp
    src/UrlParser.cpp
    src/SslStreamWrapper.cpp
    src/HttpConnection.cpp
    src/HttpClient.cpp
    src/HttpClientFactory.cpp
)

# Create library
add_library(http_client ${SOURCES})
target_link_libraries(http_client
    ${BOOST_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    pthread
)


add_executable(test_http test/main.cpp)
target_link_libraries(test_http http_client)

add_executable(example_client_demo test/test_network.cpp)
target_link_libraries(example_client_demo http_client)
