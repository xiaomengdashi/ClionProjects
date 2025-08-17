# Server CMakeLists.txt

# Add executable for pcap_server
add_executable(pcap_server
    main.cpp
    pcap_server.cpp
)

# Link with common library
target_link_libraries(pcap_server
    pcap_common
)

# Include directories
target_include_directories(pcap_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/common
    ${PCAPPLUSPLUS_ROOT}/include
)

# Platform-specific library linking
if(WIN32)
    # Windows libraries
    target_link_libraries(pcap_server
        Packet++
        Pcap++
        Common++
        wpcap
        ws2_32
        iphlpapi
    )
elseif(APPLE)
    # macOS libraries
    target_link_libraries(pcap_server
        Packet++
        Pcap++
        Common++
        pcap
        "-framework CoreFoundation"
        "-framework SystemConfiguration"
    )
else()
    # Linux libraries
    target_link_libraries(pcap_server
        Packet++
        Pcap++
        Common++
        pcap
        pthread
    )
endif()

# Compiler-specific options
if(MSVC)
    target_compile_options(pcap_server PRIVATE /W3)
    target_compile_definitions(pcap_server PRIVATE _CRT_SECURE_NO_WARNINGS)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(pcap_server PRIVATE -Wall -Wextra)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(pcap_server PRIVATE -Wall -Wno-deprecated-declarations)
endif()