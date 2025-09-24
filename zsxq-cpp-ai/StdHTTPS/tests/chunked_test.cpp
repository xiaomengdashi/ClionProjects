/**
 * @file chunked_test.cpp
 * @brief Chunked编码测试程序
 */

#include <iostream>
#include <cassert>
#include <string>
#include "chunked_encoder.h"

using namespace stdhttps;

void test_chunked_encoding() {
    std::cout << "测试Chunked编码..." << std::endl;
    
    ChunkedEncoder encoder;
    
    // 测试单个chunk编码
    std::string chunk1 = encoder.encode_chunk("Hello");
    assert(chunk1 == "5\r\nHello\r\n");
    
    std::string chunk2 = encoder.encode_chunk(", World!");
    assert(chunk2 == "8\r\n, World!\r\n");
    
    // 测试最后chunk
    std::string final_chunk = encoder.encode_final_chunk();
    assert(final_chunk == "0\r\n\r\n");
    
    std::cout << "Chunked编码测试通过！" << std::endl;
}

void test_chunked_decoding() {
    std::cout << "测试Chunked解码..." << std::endl;
    
    ChunkedDecoder decoder;
    
    std::string chunked_data = 
        "5\r\n"
        "Hello\r\n"
        "8\r\n"
        ", World!\r\n"
        "0\r\n"
        "\r\n";
    
    int result = decoder.decode(chunked_data);
    assert(result > 0);
    assert(decoder.is_complete());
    assert(decoder.get_decoded_data() == "Hello, World!");
    
    std::cout << "Chunked解码测试通过！" << std::endl;
}

void test_chunked_utils() {
    std::cout << "测试Chunked工具函数..." << std::endl;
    
    std::string original = "This is a test message for chunked encoding.";
    
    // 编码
    std::string encoded = ChunkedUtils::encode(original, 10);
    
    // 解码
    std::string decoded;
    bool success = ChunkedUtils::decode(encoded, decoded);
    
    assert(success == true);
    assert(decoded == original);
    
    std::cout << "Chunked工具函数测试通过！" << std::endl;
}

void test_stream_encoder() {
    std::cout << "测试流式Chunked编码器..." << std::endl;
    
    std::string result;
    StreamChunkedEncoder encoder([&result](const std::string& data) {
        result += data;
    }, 5);
    
    encoder.write("Hello");
    encoder.write(", Wor");
    encoder.write("ld!");
    encoder.finish();
    
    // 解码验证结果
    std::string decoded;
    bool success = ChunkedUtils::decode(result, decoded);
    assert(success == true);
    assert(decoded == "Hello, World!");
    
    std::cout << "流式Chunked编码器测试通过！" << std::endl;
}

int main() {
    std::cout << "运行Chunked编码测试..." << std::endl;
    
    try {
        test_chunked_encoding();
        test_chunked_decoding();
        test_chunked_utils();
        test_stream_encoder();
        
        std::cout << "所有测试通过！" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "测试失败: " << e.what() << std::endl;
        return 1;
    }
}