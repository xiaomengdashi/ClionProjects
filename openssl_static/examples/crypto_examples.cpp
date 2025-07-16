#include "crypto_utils.h"
#include "crypto_config.h"
#include "crypto_logger.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <vector>

/**
 * 基础加密操作示例
 */
void basicCryptoExample() {
    std::cout << "\n=== 基础加密操作示例 ===" << std::endl;
    
    // 1. SHA256哈希
    std::string data = "Hello, Crypto World!";
    std::string hash = CryptoUtils::sha256(data);
    std::cout << "原始数据: " << data << std::endl;
    std::cout << "SHA256哈希: " << hash << std::endl;
    
    // 2. AES-256加密
    auto key = CryptoUtils::generateRandomBytes(32);  // 256位密钥
    auto iv = CryptoUtils::generateRandomBytes(16);   // 128位IV
    
    std::string plaintext = "这是一个秘密消息！";
    auto encrypted = CryptoUtils::aes256Encrypt(plaintext, key, iv);
    std::string decrypted = CryptoUtils::aes256Decrypt(encrypted, key, iv);
    
    std::cout << "明文: " << plaintext << std::endl;
    std::cout << "密文长度: " << encrypted.size() << " 字节" << std::endl;
    std::cout << "解密结果: " << decrypted << std::endl;
    std::cout << "加密解密成功: " << (plaintext == decrypted ? "是" : "否") << std::endl;
}

/**
 * 文件加密示例
 */
void fileEncryptionExample() {
    std::cout << "\n=== 文件加密示例 ===" << std::endl;
    
    // 创建测试文件
    std::string filename = "test_file.txt";
    std::string content = "这是一个测试文件的内容。\n包含多行文本。\n用于演示文件加密功能。";
    
    // 写入原始文件
    std::ofstream outFile(filename);
    outFile << content;
    outFile.close();
    
    // 读取文件内容
    std::ifstream inFile(filename);
    std::string fileContent((std::istreambuf_iterator<char>(inFile)),
                           std::istreambuf_iterator<char>());
    inFile.close();
    
    // 生成密钥和IV
    auto key = CryptoUtils::generateRandomBytes(32);
    auto iv = CryptoUtils::generateRandomBytes(16);
    
    // 加密文件内容
    auto encryptedContent = CryptoUtils::aes256Encrypt(fileContent, key, iv);
    
    // 保存加密文件
    std::string encryptedFilename = filename + ".encrypted";
    std::ofstream encFile(encryptedFilename, std::ios::binary);
    encFile.write(reinterpret_cast<const char*>(encryptedContent.data()), encryptedContent.size());
    encFile.close();
    
    // 读取并解密
    std::ifstream decFile(encryptedFilename, std::ios::binary);
    std::vector<unsigned char> encryptedData((std::istreambuf_iterator<char>(decFile)),
                                           std::istreambuf_iterator<char>());
    decFile.close();
    
    std::string decryptedContent = CryptoUtils::aes256Decrypt(encryptedData, key, iv);
    
    std::cout << "原始文件大小: " << fileContent.size() << " 字节" << std::endl;
    std::cout << "加密文件大小: " << encryptedContent.size() << " 字节" << std::endl;
    std::cout << "解密成功: " << (fileContent == decryptedContent ? "是" : "否") << std::endl;
    
    // 清理临时文件
    std::remove(filename.c_str());
    std::remove(encryptedFilename.c_str());
}

/**
 * 批量数据处理示例
 */
void batchProcessingExample() {
    std::cout << "\n=== 批量数据处理示例 ===" << std::endl;
    
    // 生成测试数据
    std::vector<std::string> testData = {
        "数据块1: 用户信息",
        "数据块2: 交易记录",
        "数据块3: 系统日志",
        "数据块4: 配置文件",
        "数据块5: 临时数据"
    };
    
    // 生成统一的密钥和IV
    auto key = CryptoUtils::generateRandomBytes(32);
    auto iv = CryptoUtils::generateRandomBytes(16);
    
    std::vector<std::vector<unsigned char>> encryptedData;
    std::vector<std::string> hashes;
    
    // 批量加密和哈希
    for (const auto& data : testData) {
        // 加密
        auto encrypted = CryptoUtils::aes256Encrypt(data, key, iv);
        encryptedData.push_back(encrypted);
        
        // 计算哈希
        std::string hash = CryptoUtils::sha256(data);
        hashes.push_back(hash);
    }
    
    // 批量解密和验证
    bool allValid = true;
    for (size_t i = 0; i < testData.size(); ++i) {
        std::string decrypted = CryptoUtils::aes256Decrypt(encryptedData[i], key, iv);
        std::string verifyHash = CryptoUtils::sha256(decrypted);
        
        if (decrypted != testData[i] || verifyHash != hashes[i]) {
            allValid = false;
            break;
        }
    }
    
    std::cout << "处理数据块数量: " << testData.size() << std::endl;
    std::cout << "批量处理结果: " << (allValid ? "全部成功" : "存在错误") << std::endl;
}

/**
 * 性能测试示例
 */
void performanceExample() {
    std::cout << "\n=== 性能测试示例 ===" << std::endl;
    
    const size_t dataSize = 1024 * 1024;  // 1MB数据
    const int iterations = 10;
    
    // 生成测试数据
    std::string testData(dataSize, 'A');
    auto key = CryptoUtils::generateRandomBytes(32);
    auto iv = CryptoUtils::generateRandomBytes(16);
    
    // SHA256性能测试
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        CryptoUtils::sha256(testData);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double throughput = (dataSize * iterations) / (duration.count() / 1000.0) / (1024 * 1024);
    std::cout << "SHA256性能: " << std::fixed << std::setprecision(2) 
              << throughput << " MB/s" << std::endl;
    
    // AES加密性能测试
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto result = CryptoUtils::aes256Encrypt(testData, key, iv);
        (void)result; // 防止编译器优化
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    throughput = (dataSize * iterations) / (duration.count() / 1000.0) / (1024 * 1024);
    std::cout << "AES加密性能: " << std::fixed << std::setprecision(2) 
              << throughput << " MB/s" << std::endl;
}

/**
 * 配置和日志示例
 */
void configAndLoggingExample() {
    std::cout << "\n=== 配置和日志示例 ===" << std::endl;
    
    // 配置日志系统
    auto& logger = CryptoUtils::Logger::getInstance();
    logger.setLevel(CryptoUtils::LogLevel::DEBUG);
    logger.enableConsoleOutput(true);
    
    // 配置系统参数
    auto& config = CryptoUtils::Config::getInstance();
    config.setLogLevel("INFO");
    config.setDefaultKeySize(32);
    config.setDefaultIVSize(16);
    
    // 使用日志记录操作
    CRYPTO_LOG_INFO("开始加密操作演示");
    
    std::string data = "测试数据";
    auto key = CryptoUtils::generateRandomBytes(config.getDefaultKeySize());
    auto iv = CryptoUtils::generateRandomBytes(config.getDefaultIVSize());
    
    CRYPTO_LOG_DEBUG("生成密钥和IV完成");
    
    try {
        auto encrypted = CryptoUtils::aes256Encrypt(data, key, iv);
        CRYPTO_LOG_INFO("加密操作成功完成");
        
        std::string decrypted = CryptoUtils::aes256Decrypt(encrypted, key, iv);
        CRYPTO_LOG_INFO("解密操作成功完成");
        
        if (data == decrypted) {
            CRYPTO_LOG_INFO("数据完整性验证通过");
        } else {
            CRYPTO_LOG_ERROR("数据完整性验证失败");
        }
    } catch (const std::exception& e) {
        CRYPTO_LOG_ERROR("操作失败: " + std::string(e.what()));
    }
}

int main() {
    std::cout << "OpenSSL加密库使用示例" << std::endl;
    std::cout << "=====================" << std::endl;
    
    try {
        basicCryptoExample();
        fileEncryptionExample();
        batchProcessingExample();
        performanceExample();
        configAndLoggingExample();
        
        std::cout << "\n所有示例执行完成！" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}