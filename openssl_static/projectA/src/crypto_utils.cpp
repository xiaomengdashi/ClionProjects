#include "crypto_utils.h"
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/aes.h>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <memory>

namespace CryptoUtils {

std::string sha256(const std::string& input) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create hash context");
    }
    
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx_guard(ctx, EVP_MD_CTX_free);
    
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        throw std::runtime_error("Failed to initialize SHA256");
    }
    
    if (EVP_DigestUpdate(ctx, input.c_str(), input.size()) != 1) {
        throw std::runtime_error("Failed to update SHA256");
    }
    
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    
    if (EVP_DigestFinal_ex(ctx, hash, &hash_len) != 1) {
        throw std::runtime_error("Failed to finalize SHA256");
    }
    
    std::stringstream ss;
    for (unsigned int i = 0; i < hash_len; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

std::vector<unsigned char> aes256Encrypt(const std::string& plaintext,
                                        const std::vector<unsigned char>& key,
                                        const std::vector<unsigned char>& iv) {
    if (key.size() != 32) {
        throw std::invalid_argument("Key must be 32 bytes for AES-256");
    }
    if (iv.size() != 16) {
        throw std::invalid_argument("IV must be 16 bytes");
    }
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create cipher context");
    }
    
    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx_guard(ctx, EVP_CIPHER_CTX_free);
    
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(), iv.data()) != 1) {
        throw std::runtime_error("Failed to initialize encryption");
    }
    
    std::vector<unsigned char> ciphertext(plaintext.size() + AES_BLOCK_SIZE);
    int len;
    int ciphertext_len;
    
    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, 
                         reinterpret_cast<const unsigned char*>(plaintext.c_str()), 
                         plaintext.size()) != 1) {
        throw std::runtime_error("Failed to encrypt data");
    }
    ciphertext_len = len;
    
    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
        throw std::runtime_error("Failed to finalize encryption");
    }
    ciphertext_len += len;
    
    ciphertext.resize(ciphertext_len);
    return ciphertext;
}

std::string aes256Decrypt(const std::vector<unsigned char>& ciphertext,
                         const std::vector<unsigned char>& key,
                         const std::vector<unsigned char>& iv) {
    if (key.size() != 32) {
        throw std::invalid_argument("Key must be 32 bytes for AES-256");
    }
    if (iv.size() != 16) {
        throw std::invalid_argument("IV must be 16 bytes");
    }
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create cipher context");
    }
    
    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx_guard(ctx, EVP_CIPHER_CTX_free);
    
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(), iv.data()) != 1) {
        throw std::runtime_error("Failed to initialize decryption");
    }
    
    std::vector<unsigned char> plaintext(ciphertext.size());
    int len;
    int plaintext_len;
    
    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(), ciphertext.size()) != 1) {
        throw std::runtime_error("Failed to decrypt data");
    }
    plaintext_len = len;
    
    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
        throw std::runtime_error("Failed to finalize decryption");
    }
    plaintext_len += len;
    
    return std::string(reinterpret_cast<char*>(plaintext.data()), plaintext_len);
}

std::vector<unsigned char> generateRandomBytes(int length) {
    std::vector<unsigned char> bytes(length);
    if (RAND_bytes(bytes.data(), length) != 1) {
        throw std::runtime_error("Failed to generate random bytes");
    }
    return bytes;
}

}