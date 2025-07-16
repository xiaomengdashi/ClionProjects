#ifndef CRYPTO_UTILS_H
#define CRYPTO_UTILS_H

#include <string>
#include <vector>

namespace CryptoUtils {
    /**
     * 计算字符串的SHA256哈希值
     * @param input 输入字符串
     * @return SHA256哈希值的十六进制字符串
     */
    std::string sha256(const std::string& input);
    
    /**
     * 使用AES-256-CBC加密数据
     * @param plaintext 明文
     * @param key 密钥(32字节)
     * @param iv 初始化向量(16字节)
     * @return 加密后的数据
     */
    std::vector<unsigned char> aes256Encrypt(const std::string& plaintext, 
                                            const std::vector<unsigned char>& key,
                                            const std::vector<unsigned char>& iv);
    
    /**
     * 使用AES-256-CBC解密数据
     * @param ciphertext 密文
     * @param key 密钥(32字节)
     * @param iv 初始化向量(16字节)
     * @return 解密后的明文
     */
    std::string aes256Decrypt(const std::vector<unsigned char>& ciphertext,
                             const std::vector<unsigned char>& key,
                             const std::vector<unsigned char>& iv);
    
    /**
     * 生成随机字节
     * @param length 字节长度
     * @return 随机字节向量
     */
    std::vector<unsigned char> generateRandomBytes(int length);
}

#endif // CRYPTO_UTILS_H