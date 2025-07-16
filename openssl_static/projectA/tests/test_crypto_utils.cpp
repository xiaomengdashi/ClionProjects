#include <gtest/gtest.h>
#include "crypto_utils.h"
#include <vector>
#include <string>

class CryptoUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 测试用的固定数据
        test_data = "Hello, World!";
        test_key = CryptoUtils::generateRandomBytes(32);
        test_iv = CryptoUtils::generateRandomBytes(16);
    }
    
    std::string test_data;
    std::vector<unsigned char> test_key;
    std::vector<unsigned char> test_iv;
};

// SHA256测试
TEST_F(CryptoUtilsTest, SHA256_ConsistentOutput) {
    std::string hash1 = CryptoUtils::sha256(test_data);
    std::string hash2 = CryptoUtils::sha256(test_data);
    
    EXPECT_EQ(hash1, hash2);
    EXPECT_EQ(hash1.length(), 64); // SHA256输出64个十六进制字符
}

TEST_F(CryptoUtilsTest, SHA256_KnownVector) {
    std::string input = "abc";
    std::string expected = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    std::string actual = CryptoUtils::sha256(input);
    
    EXPECT_EQ(actual, expected);
}

// AES加密解密测试
TEST_F(CryptoUtilsTest, AES_EncryptDecrypt_RoundTrip) {
    auto ciphertext = CryptoUtils::aes256Encrypt(test_data, test_key, test_iv);
    std::string decrypted = CryptoUtils::aes256Decrypt(ciphertext, test_key, test_iv);
    
    EXPECT_EQ(test_data, decrypted);
}

TEST_F(CryptoUtilsTest, AES_DifferentInputs_DifferentOutputs) {
    std::string data1 = "Message 1";
    std::string data2 = "Message 2";
    
    auto cipher1 = CryptoUtils::aes256Encrypt(data1, test_key, test_iv);
    auto cipher2 = CryptoUtils::aes256Encrypt(data2, test_key, test_iv);
    
    EXPECT_NE(cipher1, cipher2);
}

// 异常处理测试
TEST_F(CryptoUtilsTest, AES_InvalidKeySize_ThrowsException) {
    std::vector<unsigned char> invalid_key(16); // 错误的密钥长度
    
    EXPECT_THROW(
        CryptoUtils::aes256Encrypt(test_data, invalid_key, test_iv),
        std::invalid_argument
    );
}

TEST_F(CryptoUtilsTest, AES_InvalidIVSize_ThrowsException) {
    std::vector<unsigned char> invalid_iv(8); // 错误的IV长度
    
    EXPECT_THROW(
        CryptoUtils::aes256Encrypt(test_data, test_key, invalid_iv),
        std::invalid_argument
    );
}

// 随机数生成测试
TEST_F(CryptoUtilsTest, RandomBytes_CorrectLength) {
    auto bytes = CryptoUtils::generateRandomBytes(32);
    EXPECT_EQ(bytes.size(), 32);
}

TEST_F(CryptoUtilsTest, RandomBytes_DifferentCalls_DifferentResults) {
    auto bytes1 = CryptoUtils::generateRandomBytes(16);
    auto bytes2 = CryptoUtils::generateRandomBytes(16);
    
    EXPECT_NE(bytes1, bytes2);
}

// 性能测试
TEST_F(CryptoUtilsTest, Performance_LargeData) {
    std::string large_data(1024 * 1024, 'A'); // 1MB数据
    
    auto start = std::chrono::high_resolution_clock::now();
    auto hash = CryptoUtils::sha256(large_data);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_FALSE(hash.empty());
    // 性能要求：1MB数据的SHA256计算应在100ms内完成
    EXPECT_LT(duration.count(), 100);
}