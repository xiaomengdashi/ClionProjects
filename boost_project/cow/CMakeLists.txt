cmake_minimum_required(VERSION 3.15)
project(cow VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

# ── Boost ─────────────────────────────────────────────────────────────────────
if(WIN32)
    include_directories(D:\\CppBoost\\boost_1_87_0)
    link_directories(D:\\CppBoost\\boost_1_87_0\\stage\\lib)
    set(Boost_LIBRARIES libboost_system-mgw13-mt-x64-1_87.a
                        libboost_program_options-mgw13-mt-x64-1_87.a)
    set(PLATFORM_LIBS ws2_32 mswsock)
else()
    find_package(Boost REQUIRED COMPONENTS system)
    set(PLATFORM_LIBS pthread)
endif()

# ── 目标 ──────────────────────────────────────────────────────────────────────
include_directories(include ${Boost_INCLUDE_DIRS})

add_executable(cow
    src/main.cpp
    src/Daemon.cpp
    src/Server.cpp
    src/IpcClient.cpp
)

target_link_libraries(cow
    ${Boost_LIBRARIES}
    ${PLATFORM_LIBS}
)

# 可选：安装到 /usr/local/bin
install(TARGETS cow DESTINATION bin)
