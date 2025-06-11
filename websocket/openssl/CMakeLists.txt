cmake_minimum_required(VERSION 3.10)
project(openssl_ws_server)

set(CMAKE_CXX_STANDARD 11)

if(UNIX)
    find_package(OpenSSL REQUIRED)
elseif(APPLE)
    set(OPENSSL_ROOT_DIR "/opt/homebrew/opt/openssl@3")
    set(OPENSSL_INCLUDE_DIR "/opt/homebrew/opt/openssl@3/include")
    set(OPENSSL_LIBRARIES "/opt/homebrew/opt/openssl@3/lib/libssl.dylib;/opt/homebrew/opt/openssl@3/lib/libcrypto.dylib")
    link_directories("/opt/homebrew/opt/openssl@3/lib")
endif()


include_directories(${OPENSSL_INCLUDE_DIR})

add_executable(openssl_ws_server server.cpp)
target_link_libraries(openssl_ws_server ${OPENSSL_LIBRARIES})
add_executable(openssl_ws_client client.cpp)
target_link_libraries(openssl_ws_client ${OPENSSL_LIBRARIES})