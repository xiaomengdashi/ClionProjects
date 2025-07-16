#include "crypto_utils.h"
#include "crypto_security.h"
#include "crypto_config.h"
#include "crypto_logger.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>

void printHex(const std::vector<unsigned char>& data, const std::string& label) {
    std::cout << label << ": ";
    for (unsigned char byte : data) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
    }
    std::cout << std::dec << std::endl;
}

void secureStringExample() {
    std::cout << "\n=== SecureString 示例 ===" << std::endl;
    
    // 创建安全字符串
    std::string password = "MySecretPassword123!";
    CryptoUtils::SecureString securePass(password);
    
    std::cout << "原始密码长度: " << password.length() << std::endl;
    std::cout << "安全字符串长度: " << securePass.size() << std::endl;
    std::cout << "安全字符串是否为空: " << (securePass.empty() ? "是" : "否") << std::endl;
    
    // 移动语义测试
    CryptoUtils::SecureString movedSecure = std::move(securePass);
    std::cout << "移动后原字符串长度: " << securePass.size() << std::endl;
    std::cout << "移动后新字符串长度: " << movedSecure.size() << std::endl;
}

void keyDerivationExample() {
    std::cout << "\n=== 密钥派生示例 ===" << std::endl;
    
    // PBKDF2 示例
    std::string password = "UserPassword123!";
    std::string salt = CryptoUtils::KeyDerivation::generateSalt(16);
    
    std::cout << "PBKDF2 盐值长度: " << salt.length() << " 字节" << std::endl;
    
    try {
        auto derivedKey = CryptoUtils::KeyDerivation::pbkdf2(password, salt, 10000, 32);
        std::cout << "PBKDF2 派生密钥长度: " << derivedKey.size() << " 字节" << std::endl;
        
        // HKDF 示例
        std::string ikm = "input_key_material";
        std::string hkdfSalt = CryptoUtils::KeyDerivation::generateSalt(16);
        std::string info = "application_info";
        
        std::cout << "HKDF 盐值长度: " << hkdfSalt.length() << " 字节" << std::endl;
        
        auto hkdfKey = CryptoUtils::KeyDerivation::hkdf(ikm, hkdfSalt, info, 32);
        std::cout << "HKDF 派生密钥长度: " << hkdfKey.size() << " 字节" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "密钥派生错误: " << e.what() << std::endl;
    }
}

void securityValidationExample() {
    std::cout << "\n=== 安全验证示例 ===" << std::endl;
    
    // 密钥强度验证
    std::string weakKey(16, 0);  // 全零密钥
    auto strongKeyBytes = CryptoUtils::SecureRandom::generateBytes(32);
    std::string strongKey(strongKeyBytes.begin(), strongKeyBytes.end());
    
    std::cout << "弱密钥验证结果: " << (CryptoUtils::SecurityValidator::validateKeyStrength(weakKey) ? "通过" : "失败") << std::endl;
    std::cout << "强密钥验证结果: " << (CryptoUtils::SecurityValidator::validateKeyStrength(strongKey) ? "通过" : "失败") << std::endl;
    
    // 密码强度验证
    std::vector<std::string> passwords = {
        "123",                    // 太短
        "password",              // 只有小写字母
        "Password123",           // 缺少特殊字符
        "Password123!",          // 强密码
        "MyVerySecure@Pass2024"  // 很强的密码
    };
    
    for (const auto& pwd : passwords) {
        int score = CryptoUtils::SecurityValidator::validatePasswordStrength(pwd);
        std::cout << "密码 \"" << pwd << "\" 强度评分: " << score << "/100" << std::endl;
    }
    
    // 数据完整性验证
    std::string testData = "Hello, Security World!";
    
    // 计算正确的哈希
    std::string correctHash = CryptoUtils::sha256(testData);
    
    // 创建错误的哈希
    std::string wrongHash = correctHash;
    wrongHash[0] = (wrongHash[0] == 'a') ? 'b' : 'a';  // 修改第一个字符
    
    std::cout << "正确哈希验证: " << (CryptoUtils::SecurityValidator::verifyIntegrity(testData, correctHash) ? "通过" : "失败") << std::endl;
    std::cout << "错误哈希验证: " << (CryptoUtils::SecurityValidator::verifyIntegrity(testData, wrongHash) ? "通过" : "失败") << std::endl;
}

void secureRandomExample() {
    std::cout << "\n=== 安全随机数示例 ===" << std::endl;
    
    // 生成随机字节
    auto randomBytes = CryptoUtils::SecureRandom::generateBytes(16);
    printHex(randomBytes, "随机字节");
    
    // 生成随机整数
    int randomInt = CryptoUtils::SecureRandom::generateInt(1, 100);
    std::cout << "随机整数 (1-100): " << randomInt << std::endl;
    
    // 生成随机字符串
    std::string charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::string randomStr = CryptoUtils::SecureRandom::generateString(16, charset);
    std::cout << "随机字符串: " << randomStr << std::endl;
    
    // 检查随机数生成器是否可用
    bool available = CryptoUtils::SecureRandom::isAvailable();
    std::cout << "随机数生成器可用性: " << (available ? "可用" : "不可用") << std::endl;
}

void integratedSecurityExample() {
    std::cout << "\n=== 集成安全示例 ===" << std::endl;
    
    try {
        // 1. 生成强密码并验证
        std::string userPassword = "SecureApp@2024!";
        int passwordScore = CryptoUtils::SecurityValidator::validatePasswordStrength(userPassword);
        if (passwordScore < 80) {
            std::cout << "警告: 密码强度不足! 评分: " << passwordScore << "/100" << std::endl;
            return;
        }
        
        // 2. 使用PBKDF2派生加密密钥
        std::string salt = CryptoUtils::KeyDerivation::generateSalt(16);
        auto derivedKey = CryptoUtils::KeyDerivation::pbkdf2(userPassword, salt, 100000, 32);
        
        // 3. 验证派生密钥的强度
        std::string keyStr = derivedKey.toString();
        if (!CryptoUtils::SecurityValidator::validateKeyStrength(keyStr)) {
            std::cout << "错误: 派生密钥强度不足!" << std::endl;
            return;
        }
        
        // 4. 使用派生密钥进行AES加密
        std::string plaintext = "这是需要保护的敏感数据";
        auto ivBytes = CryptoUtils::SecureRandom::generateBytes(16);
        auto keyBytes = derivedKey.toBytes();
        
        auto encrypted = CryptoUtils::aes256Encrypt(plaintext, keyBytes, ivBytes);
        auto decrypted = CryptoUtils::aes256Decrypt(encrypted, keyBytes, ivBytes);
        
        std::cout << "原始数据: " << plaintext << std::endl;
        std::cout << "加密数据长度: " << encrypted.size() << " 字节" << std::endl;
        std::cout << "解密数据: " << decrypted << std::endl;
        
        // 5. 验证数据完整性
        std::string hash = CryptoUtils::sha256(plaintext);
        bool integrityOk = CryptoUtils::SecurityValidator::verifyIntegrity(plaintext, hash);
        std::cout << "数据完整性验证: " << (integrityOk ? "通过" : "失败") << std::endl;
        
        std::cout << "集成安全示例执行成功!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "集成安全示例错误: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "OpenSSL 安全功能扩展示例" << std::endl;
    std::cout << "=========================" << std::endl;
    
    try {
        secureStringExample();
        keyDerivationExample();
        securityValidationExample();
        secureRandomExample();
        integratedSecurityExample();
        
        std::cout << "\n所有安全功能示例执行完成！" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "示例执行错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}