# Common library CMakeLists.txt

# Create a static library for common components
add_library(pcap_common STATIC
    shared_memory.cpp
    packet_analyzer.cpp
)

# Include directories
target_include_directories(pcap_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PCAPPLUSPLUS_ROOT}/include
)

# Platform-specific library linking for common components
if(WIN32)
    # Windows libraries
    target_link_libraries(pcap_common
        Packet++
        Pcap++
        Common++
        wpcap
        ws2_32
        iphlpapi
    )
elseif(APPLE)
    # macOS libraries
    target_link_libraries(pcap_common
        Packet++
        Pcap++
        Common++
        pcap
        "-framework CoreFoundation"
        "-framework SystemConfiguration"
    )
else()
    # Linux libraries
    target_link_libraries(pcap_common
        Packet++
        Pcap++
        Common++
        pcap
        pthread
    )
endif()

# Compiler-specific options
if(MSVC)
    target_compile_options(pcap_common PRIVATE /W3)
    target_compile_definitions(pcap_common PRIVATE _CRT_SECURE_NO_WARNINGS)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(pcap_common PRIVATE -Wall -Wextra)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(pcap_common PRIVATE -Wall -Wno-deprecated-declarations)
endif()