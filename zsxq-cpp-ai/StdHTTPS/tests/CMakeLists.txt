# 测试程序CMakeLists.txt
add_executable(http_parser_test http_parser_test.cpp)
target_link_libraries(http_parser_test stdhttps)

add_executable(http_message_test http_message_test.cpp)
target_link_libraries(http_message_test stdhttps)

add_executable(chunked_test chunked_test.cpp)
target_link_libraries(chunked_test stdhttps)

# 添加测试
add_test(NAME HTTPParserTest COMMAND http_parser_test)
add_test(NAME HTTPMessageTest COMMAND http_message_test)
add_test(NAME ChunkedTest COMMAND chunked_test)