#ifndef CRYPTO_SECURITY_H
#define CRYPTO_SECURITY_H

#include <string>
#include <vector>
#include <memory>

namespace CryptoUtils {

/**
 * 安全内存管理类
 * 自动清零敏感数据
 */
class SecureString {
public:
    SecureString();
    explicit SecureString(const std::string& data);
    explicit SecureString(const char* data, size_t length);
    ~SecureString();
    
    // 禁用拷贝构造和赋值
    SecureString(const SecureString&) = delete;
    SecureString& operator=(const SecureString&) = delete;
    
    // 移动构造和赋值
    SecureString(SecureString&& other) noexcept;
    SecureString& operator=(SecureString&& other) noexcept;
    
    // 访问方法
    const char* data() const;
    size_t size() const;
    bool empty() const;
    
    // 转换方法
    std::string toString() const;
    std::vector<unsigned char> toBytes() const;
    
    // 操作方法
    void clear();
    void resize(size_t new_size);
    void append(const std::string& data);
    void append(const char* data, size_t length);
    
private:
    char* data_;
    size_t size_;
    size_t capacity_;
    
    void secureZero(void* ptr, size_t size);
    void reallocate(size_t new_capacity);
};

/**
 * 密钥派生函数
 */
class KeyDerivation {
public:
    /**
     * PBKDF2密钥派生
     * @param password 密码
     * @param salt 盐值
     * @param iterations 迭代次数
     * @param key_length 输出密钥长度
     * @return 派生的密钥
     */
    static SecureString pbkdf2(const std::string& password,
                              const std::string& salt,
                              int iterations,
                              size_t key_length);
    
    /**
     * 生成随机盐值
     * @param length 盐值长度
     * @return 随机盐值
     */
    static std::string generateSalt(size_t length = 16);
    
    /**
     * HKDF密钥派生
     * @param ikm 输入密钥材料
     * @param salt 盐值
     * @param info 信息字符串
     * @param length 输出长度
     * @return 派生的密钥
     */
    static SecureString hkdf(const std::string& ikm,
                            const std::string& salt,
                            const std::string& info,
                            size_t length);
};

/**
 * 安全验证工具
 */
class SecurityValidator {
public:
    /**
     * 验证密钥强度
     * @param key 密钥
     * @return 是否足够强
     */
    static bool validateKeyStrength(const std::string& key);
    
    /**
     * 验证密码强度
     * @param password 密码
     * @return 强度评分 (0-100)
     */
    static int validatePasswordStrength(const std::string& password);
    
    /**
     * 安全比较两个字符串（防止时序攻击）
     * @param a 字符串A
     * @param b 字符串B
     * @return 是否相等
     */
    static bool secureCompare(const std::string& a, const std::string& b);
    
    /**
     * 验证数据完整性
     * @param data 数据
     * @param expected_hash 期望的哈希值
     * @return 是否完整
     */
    static bool verifyIntegrity(const std::string& data, 
                               const std::string& expected_hash);
};

/**
 * 安全随机数生成器
 */
class SecureRandom {
public:
    /**
     * 生成加密安全的随机字节
     * @param length 字节数
     * @return 随机字节
     */
    static std::vector<unsigned char> generateBytes(size_t length);
    
    /**
     * 生成随机整数
     * @param min 最小值
     * @param max 最大值
     * @return 随机整数
     */
    static int generateInt(int min, int max);
    
    /**
     * 生成随机字符串
     * @param length 长度
     * @param charset 字符集
     * @return 随机字符串
     */
    static std::string generateString(size_t length, 
                                     const std::string& charset = 
                                     "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
    
    /**
     * 检查随机数生成器是否可用
     * @return 是否可用
     */
    static bool isAvailable();
};

} // namespace CryptoUtils

#endif // CRYPTO_SECURITY_H