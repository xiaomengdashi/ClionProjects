#include "crypto_security.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/kdf.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <random>

namespace CryptoUtils {

// SecureString 实现
SecureString::SecureString() : data_(nullptr), size_(0), capacity_(0) {}

SecureString::SecureString(const std::string& data) 
    : size_(data.size()), capacity_(data.size()) {
    if (size_ > 0) {
        data_ = new char[capacity_];
        std::memcpy(data_, data.c_str(), size_);
    } else {
        data_ = nullptr;
    }
}

SecureString::SecureString(const char* data, size_t length) 
    : size_(length), capacity_(length) {
    if (size_ > 0 && data) {
        data_ = new char[capacity_];
        std::memcpy(data_, data, size_);
    } else {
        data_ = nullptr;
        size_ = 0;
        capacity_ = 0;
    }
}

SecureString::~SecureString() {
    clear();
}

SecureString::SecureString(SecureString&& other) noexcept 
    : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
}

SecureString& SecureString::operator=(SecureString&& other) noexcept {
    if (this != &other) {
        clear();
        data_ = other.data_;
        size_ = other.size_;
        capacity_ = other.capacity_;
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }
    return *this;
}

void SecureString::clear() {
    if (data_) {
        // 安全清零内存
        secureZero(data_, capacity_);
        delete[] data_;
        data_ = nullptr;
    }
    size_ = 0;
    capacity_ = 0;
}

const char* SecureString::data() const {
    return data_;
}

size_t SecureString::size() const {
    return size_;
}

bool SecureString::empty() const {
    return size_ == 0;
}

std::string SecureString::toString() const {
    if (!data_ || size_ == 0) {
        return std::string();
    }
    return std::string(data_, size_);
}

std::vector<unsigned char> SecureString::toBytes() const {
    if (!data_ || size_ == 0) {
        return std::vector<unsigned char>();
    }
    return std::vector<unsigned char>(data_, data_ + size_);
}

void SecureString::resize(size_t new_size) {
    if (new_size > capacity_) {
        reallocate(new_size);
    }
    size_ = new_size;
}

void SecureString::append(const std::string& data) {
    append(data.c_str(), data.size());
}

void SecureString::append(const char* data, size_t length) {
    if (!data || length == 0) return;
    
    size_t new_size = size_ + length;
    if (new_size > capacity_) {
        reallocate(new_size);
    }
    
    std::memcpy(data_ + size_, data, length);
    size_ = new_size;
}

void SecureString::secureZero(void* ptr, size_t size) {
    if (ptr && size > 0) {
        volatile unsigned char* p = static_cast<volatile unsigned char*>(ptr);
        for (size_t i = 0; i < size; ++i) {
            p[i] = 0;
        }
    }
}

void SecureString::reallocate(size_t new_capacity) {
    char* new_data = new char[new_capacity];
    if (data_ && size_ > 0) {
        std::memcpy(new_data, data_, size_);
        secureZero(data_, capacity_);
        delete[] data_;
    }
    data_ = new_data;
    capacity_ = new_capacity;
}

// KeyDerivation 实现
SecureString KeyDerivation::pbkdf2(const std::string& password,
                                   const std::string& salt,
                                   int iterations,
                                   size_t key_length) {
    if (password.empty() || salt.empty() || iterations <= 0 || key_length == 0) {
        throw std::invalid_argument("Invalid PBKDF2 parameters");
    }

    std::vector<unsigned char> derived(key_length);
    
    int result = PKCS5_PBKDF2_HMAC(
        password.c_str(), password.length(),
        reinterpret_cast<const unsigned char*>(salt.c_str()), salt.length(),
        iterations,
        EVP_sha256(),
        key_length,
        derived.data()
    );
    
    if (result != 1) {
        throw std::runtime_error("PBKDF2 derivation failed");
    }
    
    return SecureString(reinterpret_cast<const char*>(derived.data()), key_length);
}

std::string KeyDerivation::generateSalt(size_t length) {
    if (length == 0) length = 16;
    
    std::vector<unsigned char> salt(length);
    if (RAND_bytes(salt.data(), static_cast<int>(length)) != 1) {
        throw std::runtime_error("Failed to generate salt");
    }
    
    return std::string(reinterpret_cast<const char*>(salt.data()), length);
}

SecureString KeyDerivation::hkdf(const std::string& ikm,
                                 const std::string& salt,
                                 const std::string& info,
                                 size_t length) {
    if (ikm.empty() || length == 0) {
        throw std::invalid_argument("Invalid HKDF parameters");
    }

    std::vector<unsigned char> derived(length);
    
    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
    if (!pctx) {
        throw std::runtime_error("Failed to create HKDF context");
    }
    
    try {
        if (EVP_PKEY_derive_init(pctx) <= 0) {
            throw std::runtime_error("HKDF init failed");
        }
        
        if (EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha256()) <= 0) {
            throw std::runtime_error("HKDF set digest failed");
        }
        
        if (EVP_PKEY_CTX_set1_hkdf_key(pctx, 
                                       reinterpret_cast<const unsigned char*>(ikm.c_str()), 
                                       ikm.length()) <= 0) {
            throw std::runtime_error("HKDF set key failed");
        }
        
        if (!salt.empty()) {
            if (EVP_PKEY_CTX_set1_hkdf_salt(pctx, 
                                            reinterpret_cast<const unsigned char*>(salt.c_str()), 
                                            salt.size()) <= 0) {
                throw std::runtime_error("HKDF set salt failed");
            }
        }
        
        if (!info.empty()) {
            if (EVP_PKEY_CTX_add1_hkdf_info(pctx, 
                                            reinterpret_cast<const unsigned char*>(info.c_str()), 
                                            info.length()) <= 0) {
                throw std::runtime_error("HKDF set info failed");
            }
        }
        
        size_t outlen = length;
        if (EVP_PKEY_derive(pctx, derived.data(), &outlen) <= 0) {
            throw std::runtime_error("HKDF derivation failed");
        }
        
        EVP_PKEY_CTX_free(pctx);
        
    } catch (...) {
        EVP_PKEY_CTX_free(pctx);
        throw;
    }
    
    return SecureString(reinterpret_cast<const char*>(derived.data()), length);
}

// SecurityValidator 实现
bool SecurityValidator::validateKeyStrength(const std::string& key) {
    if (key.size() < 16) {  // 最小128位
        return false;
    }
    
    // 检查是否全为零
    bool allZero = std::all_of(key.begin(), key.end(), [](char c) { return c == 0; });
    if (allZero) {
        return false;
    }
    
    // 检查是否有足够的熵（简单检查：不能有太多重复字符）
    std::vector<int> charCount(256, 0);
    for (unsigned char c : key) {
        charCount[c]++;
    }
    
    // 如果任何字符出现次数超过密钥长度的一半，认为熵不足
    int maxCount = *std::max_element(charCount.begin(), charCount.end());
    if (maxCount > static_cast<int>(key.size()) / 2) {
        return false;
    }
    
    return true;
}

int SecurityValidator::validatePasswordStrength(const std::string& password) {
    int score = 0;
    
    if (password.length() < 8) {
        return 0;  // 太短，直接返回0分
    }
    
    // 长度评分
    if (password.length() >= 8) score += 20;
    if (password.length() >= 12) score += 10;
    if (password.length() >= 16) score += 10;
    
    bool hasLower = false, hasUpper = false, hasDigit = false, hasSpecial = false;
    
    for (char c : password) {
        if (c >= 'a' && c <= 'z') hasLower = true;
        else if (c >= 'A' && c <= 'Z') hasUpper = true;
        else if (c >= '0' && c <= '9') hasDigit = true;
        else hasSpecial = true;
    }
    
    // 字符类型评分
    if (hasLower) score += 15;
    if (hasUpper) score += 15;
    if (hasDigit) score += 15;
    if (hasSpecial) score += 15;
    
    return std::min(score, 100);
}

bool SecurityValidator::secureCompare(const std::string& a, const std::string& b) {
    if (a.length() != b.length()) {
        return false;
    }
    
    volatile int result = 0;
    for (size_t i = 0; i < a.length(); ++i) {
        result |= a[i] ^ b[i];
    }
    
    return result == 0;
}

bool SecurityValidator::verifyIntegrity(const std::string& data, 
                                       const std::string& expected_hash) {
    if (data.empty() || expected_hash.empty()) {
        return false;
    }
    
    // 使用新的 EVP API 计算SHA-256哈希
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create hash context");
    }
    
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx_guard(ctx, EVP_MD_CTX_free);
    
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        throw std::runtime_error("Failed to initialize SHA256");
    }
    
    if (EVP_DigestUpdate(ctx, data.c_str(), data.size()) != 1) {
        throw std::runtime_error("Failed to update SHA256");
    }
    
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    
    if (EVP_DigestFinal_ex(ctx, hash, &hash_len) != 1) {
        throw std::runtime_error("Failed to finalize SHA256");
    }
    
    // 转换为十六进制字符串
    std::string computed_hash;
    for (unsigned int i = 0; i < hash_len; ++i) {
        char buf[3];
        sprintf(buf, "%02x", hash[i]);
        computed_hash += buf;
    }
    
    return secureCompare(computed_hash, expected_hash);
}

// SecureRandom 实现
std::vector<unsigned char> SecureRandom::generateBytes(size_t length) {
    if (length == 0) {
        return std::vector<unsigned char>();
    }
    
    std::vector<unsigned char> buffer(length);
    
    if (RAND_bytes(buffer.data(), static_cast<int>(length)) != 1) {
        throw std::runtime_error("Failed to generate secure random bytes");
    }
    
    return buffer;
}

int SecureRandom::generateInt(int min, int max) {
    if (min >= max) {
        throw std::invalid_argument("Invalid range for random integer");
    }
    
    uint32_t range = static_cast<uint32_t>(max - min);
    uint32_t random_value;
    
    auto bytes = generateBytes(sizeof(uint32_t));
    std::memcpy(&random_value, bytes.data(), sizeof(uint32_t));
    
    return min + (random_value % range);
}

std::string SecureRandom::generateString(size_t length, const std::string& charset) {
    if (length == 0 || charset.empty()) {
        return std::string();
    }
    
    std::string result;
    result.reserve(length);
    
    for (size_t i = 0; i < length; ++i) {
        int index = generateInt(0, charset.size());
        result += charset[index];
    }
    
    return result;
}

bool SecureRandom::isAvailable() {
    // 尝试生成一个字节来测试随机数生成器
    unsigned char test_byte;
    return RAND_bytes(&test_byte, 1) == 1;
}

} // namespace CryptoUtils