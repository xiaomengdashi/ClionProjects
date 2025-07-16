#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include "crypto_utils.h"

void printHex(const std::vector<unsigned char>& data) {
    for (unsigned char byte : data) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
    }
    std::cout << std::dec << std::endl;
}

int main() {
    std::cout << "=== ProjectB - 使用ProjectA加密库演示 ===" << std::endl;
    
    try {
        // 1. SHA256哈希演示
        std::string message = "Hello, OpenSSL Static Library!";
        std::cout << "\n1. SHA256哈希演示:" << std::endl;
        std::cout << "原文: " << message << std::endl;
        std::string hash = CryptoUtils::sha256(message);
        std::cout << "SHA256: " << hash << std::endl;
        
        // 2. AES-256加密演示
        std::cout << "\n2. AES-256加密演示:" << std::endl;
        
        // 生成随机密钥和IV
        auto key = CryptoUtils::generateRandomBytes(32);  // 256位密钥
        auto iv = CryptoUtils::generateRandomBytes(16);   // 128位IV
        
        std::cout << "密钥 (32字节): ";
        printHex(key);
        std::cout << "IV (16字节): ";
        printHex(iv);
        
        // 加密
        std::string plaintext = "这是一个需要加密的秘密消息！";
        std::cout << "明文: " << plaintext << std::endl;
        
        auto ciphertext = CryptoUtils::aes256Encrypt(plaintext, key, iv);
        std::cout << "密文 (" << ciphertext.size() << "字节): ";
        printHex(ciphertext);
        
        // 解密
        std::string decrypted = CryptoUtils::aes256Decrypt(ciphertext, key, iv);
        std::cout << "解密后: " << decrypted << std::endl;
        
        // 验证
        if (plaintext == decrypted) {
            std::cout << "✓ 加密解密验证成功！" << std::endl;
        } else {
            std::cout << "✗ 加密解密验证失败！" << std::endl;
        }
        
        // 3. 随机数生成演示
        std::cout << "\n3. 随机数生成演示:" << std::endl;
        auto randomBytes = CryptoUtils::generateRandomBytes(16);
        std::cout << "随机16字节: ";
        printHex(randomBytes);
        
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "\n=== 演示完成 ===" << std::endl;
    return 0;
}