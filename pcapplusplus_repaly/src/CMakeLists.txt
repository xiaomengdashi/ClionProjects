# Source directory CMakeLists.txt

# Add executable for send_pcap
add_executable(send_pcap send_pcap.cpp)

# Platform-specific library linking
if(WIN32)
    # Windows libraries
    target_link_libraries(send_pcap
        Packet++
        Pcap++
        Common++
        wpcap
        ws2_32
        iphlpapi
    )
elseif(APPLE)
    # macOS libraries
    target_link_libraries(send_pcap
        Packet++
        Pcap++
        Common++
        pcap
        "-framework CoreFoundation"
        "-framework SystemConfiguration"
    )
else()
    # Linux libraries
    target_link_libraries(send_pcap
        Packet++
        Pcap++
        Common++
        pcap
        pthread
    )
endif()

# Compiler-specific options
if(MSVC)
    target_compile_options(send_pcap PRIVATE /W3)
    target_compile_definitions(send_pcap PRIVATE _CRT_SECURE_NO_WARNINGS)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(send_pcap PRIVATE -Wall -Wextra)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(send_pcap PRIVATE -Wall -Wno-deprecated-declarations)
endif()