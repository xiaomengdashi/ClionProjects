/**
 * @file ssl_handler.cpp
 * @brief SSL/TLS处理器实现
 * @details 基于OpenSSL的SSL/TLS功能实现
 */

#include "ssl_handler.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/opensslv.h>
#include <sstream>
#include <fstream>
#include <cstring>

namespace stdhttps {

// 静态成员初始化
bool SSLUtils::openssl_initialized_ = false;
std::mutex SSLUtils::init_mutex_;
int SSLInitializer::instance_count_ = 0;
std::mutex SSLInitializer::count_mutex_;

// SSLContextManager实现
SSLContextManager::SSLContextManager(bool is_server) 
    : ssl_ctx_(nullptr), is_server_(is_server) {
    SSLUtils::initialize_openssl();
}

SSLContextManager::~SSLContextManager() {
    cleanup();
}

SSLContextManager::SSLContextManager(SSLContextManager&& other) noexcept
    : ssl_ctx_(other.ssl_ctx_)
    , is_server_(other.is_server_)
    , error_message_(std::move(other.error_message_)) {
    other.ssl_ctx_ = nullptr;
}

SSLContextManager& SSLContextManager::operator=(SSLContextManager&& other) noexcept {
    if (this != &other) {
        cleanup();
        ssl_ctx_ = other.ssl_ctx_;
        is_server_ = other.is_server_;
        error_message_ = std::move(other.error_message_);
        other.ssl_ctx_ = nullptr;
    }
    return *this;
}

bool SSLContextManager::initialize(const SSLConfig& config) {
    cleanup();
    
    // 创建SSL上下文
    const SSL_METHOD* method;
    if (is_server_) {
        method = TLS_server_method();
    } else {
        method = TLS_client_method();
    }
    
    ssl_ctx_ = SSL_CTX_new(method);
    if (!ssl_ctx_) {
        set_error("无法创建SSL上下文: " + SSLUtils::get_openssl_error_string());
        return false;
    }
    
    // 设置协议版本
    if (!config.protocol_version.empty()) {
        if (config.protocol_version == "TLSv1.2") {
            SSL_CTX_set_min_proto_version(ssl_ctx_, TLS1_2_VERSION);
            SSL_CTX_set_max_proto_version(ssl_ctx_, TLS1_2_VERSION);
        } else if (config.protocol_version == "TLSv1.3") {
            SSL_CTX_set_min_proto_version(ssl_ctx_, TLS1_3_VERSION);
            SSL_CTX_set_max_proto_version(ssl_ctx_, TLS1_3_VERSION);
        }
        // 其他版本可以根据需要添加
    }
    
    // 加载证书和私钥
    if (!config.cert_file.empty() && !config.key_file.empty()) {
        if (!load_certificate(config.cert_file, config.key_file)) {
            return false;
        }
    }
    
    // 加载CA证书
    if (!config.ca_file.empty() || !config.ca_path.empty()) {
        if (!load_ca_certificates(config.ca_file, config.ca_path)) {
            return false;
        }
    }
    
    // 设置加密套件
    if (!config.cipher_list.empty()) {
        if (!set_cipher_list(config.cipher_list)) {
            return false;
        }
    }
    
    // 设置验证模式
    int verify_mode = SSL_VERIFY_NONE;
    if (config.verify_peer) {
        verify_mode = SSL_VERIFY_PEER;
        if (is_server_) {
            verify_mode |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
        }
    }
    SSL_CTX_set_verify(ssl_ctx_, verify_mode, nullptr);
    SSL_CTX_set_verify_depth(ssl_ctx_, config.verify_depth);
    
    return true;
}

bool SSLContextManager::load_certificate(const std::string& cert_file, const std::string& key_file) {
    if (!ssl_ctx_) {
        set_error("SSL上下文未初始化");
        return false;
    }
    
    // 加载证书文件
    if (SSL_CTX_use_certificate_file(ssl_ctx_, cert_file.c_str(), SSL_FILETYPE_PEM) <= 0) {
        set_error("无法加载证书文件 " + cert_file + ": " + SSLUtils::get_openssl_error_string());
        return false;
    }
    
    // 加载私钥文件
    if (SSL_CTX_use_PrivateKey_file(ssl_ctx_, key_file.c_str(), SSL_FILETYPE_PEM) <= 0) {
        set_error("无法加载私钥文件 " + key_file + ": " + SSLUtils::get_openssl_error_string());
        return false;
    }
    
    // 验证私钥和证书是否匹配
    if (!SSL_CTX_check_private_key(ssl_ctx_)) {
        set_error("私钥和证书不匹配: " + SSLUtils::get_openssl_error_string());
        return false;
    }
    
    return true;
}

bool SSLContextManager::load_ca_certificates(const std::string& ca_file, const std::string& ca_path) {
    if (!ssl_ctx_) {
        set_error("SSL上下文未初始化");
        return false;
    }
    
    const char* file_ptr = ca_file.empty() ? nullptr : ca_file.c_str();
    const char* path_ptr = ca_path.empty() ? nullptr : ca_path.c_str();
    
    if (!SSL_CTX_load_verify_locations(ssl_ctx_, file_ptr, path_ptr)) {
        set_error("无法加载CA证书: " + SSLUtils::get_openssl_error_string());
        return false;
    }
    
    return true;
}

bool SSLContextManager::set_cipher_list(const std::string& cipher_list) {
    if (!ssl_ctx_) {
        set_error("SSL上下文未初始化");
        return false;
    }
    
    if (!SSL_CTX_set_cipher_list(ssl_ctx_, cipher_list.c_str())) {
        set_error("无法设置加密套件: " + SSLUtils::get_openssl_error_string());
        return false;
    }
    
    return true;
}

void SSLContextManager::cleanup() {
    if (ssl_ctx_) {
        SSL_CTX_free(ssl_ctx_);
        ssl_ctx_ = nullptr;
    }
}

void SSLContextManager::set_error(const std::string& message) {
    error_message_ = message;
}

// SSLHandler实现
SSLHandler::SSLHandler(SSL_CTX* ssl_ctx, bool is_server)
    : ssl_(nullptr), read_bio_(nullptr), write_bio_(nullptr)
    , state_(SSLState::INIT), is_server_(is_server) {
    
    if (!ssl_ctx) {
        set_error("SSL上下文为空");
        state_ = SSLState::ERROR;
        return;
    }
    
    // 创建SSL对象
    ssl_ = SSL_new(ssl_ctx);
    if (!ssl_) {
        set_error("无法创建SSL对象: " + SSLUtils::get_openssl_error_string());
        state_ = SSLState::ERROR;
        return;
    }
    
    // 创建内存BIO
    read_bio_ = BIO_new(BIO_s_mem());
    write_bio_ = BIO_new(BIO_s_mem());
    
    if (!read_bio_ || !write_bio_) {
        set_error("无法创建BIO对象: " + SSLUtils::get_openssl_error_string());
        cleanup();
        state_ = SSLState::ERROR;
        return;
    }
    
    // 设置BIO到SSL对象
    SSL_set_bio(ssl_, read_bio_, write_bio_);
    
    // 设置SSL模式
    if (is_server_) {
        SSL_set_accept_state(ssl_);
    } else {
        SSL_set_connect_state(ssl_);
    }
}

SSLHandler::~SSLHandler() {
    cleanup();
}

SSLHandler::SSLHandler(SSLHandler&& other) noexcept
    : ssl_(other.ssl_)
    , read_bio_(other.read_bio_)
    , write_bio_(other.write_bio_)
    , state_(other.state_)
    , is_server_(other.is_server_)
    , write_callback_(std::move(other.write_callback_))
    , last_error_(std::move(other.last_error_)) {
    
    other.ssl_ = nullptr;
    other.read_bio_ = nullptr;
    other.write_bio_ = nullptr;
    other.state_ = SSLState::CLOSED;
}

SSLHandler& SSLHandler::operator=(SSLHandler&& other) noexcept {
    if (this != &other) {
        cleanup();
        
        ssl_ = other.ssl_;
        read_bio_ = other.read_bio_;
        write_bio_ = other.write_bio_;
        state_ = other.state_;
        is_server_ = other.is_server_;
        write_callback_ = std::move(other.write_callback_);
        last_error_ = std::move(other.last_error_);
        
        other.ssl_ = nullptr;
        other.read_bio_ = nullptr;
        other.write_bio_ = nullptr;
        other.state_ = SSLState::CLOSED;
    }
    return *this;
}

void SSLHandler::set_write_callback(WriteCallback callback) {
    write_callback_ = callback;
}

bool SSLHandler::start_handshake() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_ != SSLState::INIT) {
        set_error("SSL状态不正确，无法开始握手");
        return false;
    }
    
    state_ = SSLState::HANDSHAKING;
    
    int ret;
    if (is_server_) {
        ret = SSL_accept(ssl_);
    } else {
        ret = SSL_connect(ssl_);
    }
    
    if (ret == 1) {
        state_ = SSLState::CONNECTED;
        flush_bio_write();
        return true;
    } else {
        int ssl_error = SSL_get_error(ssl_, ret);
        if (ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE) {
            flush_bio_write();
            return true; // 需要更多数据或者有数据需要发送
        } else {
            handle_ssl_error(ssl_error);
            state_ = SSLState::ERROR;
            return false;
        }
    }
}

SSLError SSLHandler::handle_input(const void* data, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_ == SSLState::ERROR || state_ == SSLState::CLOSED) {
        return SSLError::INVALID_STATE;
    }
    
    // 将数据写入读取BIO
    int written = BIO_write(read_bio_, data, static_cast<int>(size));
    if (written <= 0) {
        set_error("无法写入数据到读取BIO");
        return SSLError::MEMORY_ERROR;
    }
    
    // 如果正在握手，尝试继续握手
    if (state_ == SSLState::HANDSHAKING) {
        int ret;
        if (is_server_) {
            ret = SSL_accept(ssl_);
        } else {
            ret = SSL_connect(ssl_);
        }
        
        if (ret == 1) {
            state_ = SSLState::CONNECTED;
        } else {
            int ssl_error = SSL_get_error(ssl_, ret);
            if (ssl_error != SSL_ERROR_WANT_READ && ssl_error != SSL_ERROR_WANT_WRITE) {
                handle_ssl_error(ssl_error);
                state_ = SSLState::ERROR;
                return handle_ssl_error(ssl_error);
            }
        }
        
        flush_bio_write();
    }
    
    return SSLError::NONE;
}

SSLError SSLHandler::send_data(const void* data, size_t size, size_t& bytes_sent) {
    std::lock_guard<std::mutex> lock(mutex_);
    bytes_sent = 0;
    
    if (state_ != SSLState::CONNECTED) {
        return SSLError::INVALID_STATE;
    }
    
    int written = SSL_write(ssl_, data, static_cast<int>(size));
    if (written > 0) {
        bytes_sent = static_cast<size_t>(written);
        flush_bio_write();
        return SSLError::NONE;
    } else {
        int ssl_error = SSL_get_error(ssl_, written);
        return handle_ssl_error(ssl_error);
    }
}

SSLError SSLHandler::receive_data(void* buffer, size_t buffer_size, size_t& bytes_received) {
    std::lock_guard<std::mutex> lock(mutex_);
    bytes_received = 0;
    
    if (state_ != SSLState::CONNECTED) {
        return SSLError::INVALID_STATE;
    }
    
    int read_bytes = SSL_read(ssl_, buffer, static_cast<int>(buffer_size));
    if (read_bytes > 0) {
        bytes_received = static_cast<size_t>(read_bytes);
        return SSLError::NONE;
    } else {
        int ssl_error = SSL_get_error(ssl_, read_bytes);
        return handle_ssl_error(ssl_error);
    }
}

bool SSLHandler::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_ == SSLState::CLOSED || state_ == SSLState::ERROR) {
        return true;
    }
    
    state_ = SSLState::SHUTDOWN;
    
    int ret = SSL_shutdown(ssl_);
    flush_bio_write();
    
    if (ret == 1) {
        state_ = SSLState::CLOSED;
        return true;
    } else if (ret == 0) {
        // 第一阶段shutdown完成，等待对端响应
        return true;
    } else {
        int ssl_error = SSL_get_error(ssl_, ret);
        if (ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE) {
            return true; // 需要更多数据
        } else {
            handle_ssl_error(ssl_error);
            state_ = SSLState::ERROR;
            return false;
        }
    }
}

std::string SSLHandler::get_peer_certificate_info() const {
    if (!ssl_ || state_ != SSLState::CONNECTED) {
        return "";
    }
    
    X509* cert = SSL_get_peer_certificate(ssl_);
    if (!cert) {
        return "";
    }
    
    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio) {
        X509_free(cert);
        return "";
    }
    
    X509_print(bio, cert);
    
    char* data;
    long len = BIO_get_mem_data(bio, &data);
    std::string result(data, len);
    
    BIO_free(bio);
    X509_free(cert);
    
    return result;
}

std::string SSLHandler::get_cipher_name() const {
    if (!ssl_ || state_ != SSLState::CONNECTED) {
        return "";
    }
    
    const char* cipher = SSL_get_cipher(ssl_);
    return cipher ? cipher : "";
}

std::string SSLHandler::get_ssl_version() const {
    if (!ssl_ || state_ != SSLState::CONNECTED) {
        return "";
    }
    
    const char* version = SSL_get_version(ssl_);
    return version ? version : "";
}

void SSLHandler::cleanup() {
    if (ssl_) {
        SSL_free(ssl_); // 这也会释放相关联的BIO
        ssl_ = nullptr;
        read_bio_ = nullptr;
        write_bio_ = nullptr;
    }
}

void SSLHandler::set_error(const std::string& message) {
    last_error_ = message;
}

SSLError SSLHandler::handle_ssl_error(int ssl_error) {
    switch (ssl_error) {
        case SSL_ERROR_WANT_READ:
            return SSLError::WANT_READ;
        case SSL_ERROR_WANT_WRITE:
            return SSLError::WANT_WRITE;
        case SSL_ERROR_SYSCALL:
            set_error("SSL系统调用错误: " + SSLUtils::get_openssl_error_string());
            return SSLError::SYSCALL_ERROR;
        case SSL_ERROR_SSL:
            set_error("SSL协议错误: " + SSLUtils::get_openssl_error_string());
            return SSLError::SSL_ERROR;
        case SSL_ERROR_ZERO_RETURN:
            set_error("连接被对端关闭");
            state_ = SSLState::CLOSED;
            return SSLError::CONNECTION_CLOSED;
        default:
            set_error("未知SSL错误: " + std::to_string(ssl_error));
            return SSLError::SSL_ERROR;
    }
}

void SSLHandler::flush_bio_write() {
    if (!write_bio_ || !write_callback_) {
        return;
    }
    
    char buffer[8192];
    int bytes_read;
    
    while ((bytes_read = BIO_read(write_bio_, buffer, sizeof(buffer))) > 0) {
        write_callback_(buffer, bytes_read);
    }
}

// SSLUtils实现
void SSLUtils::initialize_openssl() {
    std::lock_guard<std::mutex> lock(init_mutex_);
    if (!openssl_initialized_) {
        SSL_load_error_strings();
        SSL_library_init();
        RAND_poll(); // 初始化随机数生成器
        openssl_initialized_ = true;
    }
}

void SSLUtils::cleanup_openssl() {
    std::lock_guard<std::mutex> lock(init_mutex_);
    if (openssl_initialized_) {
        EVP_cleanup();
        ERR_free_strings();
        openssl_initialized_ = false;
    }
}

std::string SSLUtils::get_openssl_version() {
    return OPENSSL_VERSION_TEXT;
}

bool SSLUtils::generate_self_signed_cert(const std::string& cert_file,
                                        const std::string& key_file,
                                        int days,
                                        const std::string& country,
                                        const std::string& org,
                                        const std::string& cn) {
    initialize_openssl();
    
    // 生成RSA密钥对
    EVP_PKEY* pkey = EVP_PKEY_new();
    if (!pkey) {
        return false;
    }
    
    RSA* rsa = RSA_new();
    BIGNUM* bn = BN_new();
    if (!rsa || !bn) {
        EVP_PKEY_free(pkey);
        if (rsa) RSA_free(rsa);
        if (bn) BN_free(bn);
        return false;
    }
    
    BN_set_word(bn, RSA_F4);
    if (RSA_generate_key_ex(rsa, 2048, bn, nullptr) != 1) {
        EVP_PKEY_free(pkey);
        RSA_free(rsa);
        BN_free(bn);
        return false;
    }
    
    EVP_PKEY_assign_RSA(pkey, rsa);
    BN_free(bn);
    
    // 创建X509证书
    X509* x509 = X509_new();
    if (!x509) {
        EVP_PKEY_free(pkey);
        return false;
    }
    
    // 设置证书版本
    X509_set_version(x509, 2);
    
    // 设置序列号
    ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);
    
    // 设置有效期
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), days * 24 * 3600);
    
    // 设置公钥
    X509_set_pubkey(x509, pkey);
    
    // 设置主题和签发者信息
    X509_NAME* name = X509_get_subject_name(x509);
    X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, 
                               reinterpret_cast<const unsigned char*>(country.c_str()), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC,
                               reinterpret_cast<const unsigned char*>(org.c_str()), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                               reinterpret_cast<const unsigned char*>(cn.c_str()), -1, -1, 0);
    
    X509_set_issuer_name(x509, name);
    
    // 自签名
    if (X509_sign(x509, pkey, EVP_sha256()) == 0) {
        X509_free(x509);
        EVP_PKEY_free(pkey);
        return false;
    }
    
    // 保存证书
    FILE* cert_fp = fopen(cert_file.c_str(), "w");
    if (!cert_fp || PEM_write_X509(cert_fp, x509) != 1) {
        if (cert_fp) fclose(cert_fp);
        X509_free(x509);
        EVP_PKEY_free(pkey);
        return false;
    }
    fclose(cert_fp);
    
    // 保存私钥
    FILE* key_fp = fopen(key_file.c_str(), "w");
    if (!key_fp || PEM_write_PrivateKey(key_fp, pkey, nullptr, nullptr, 0, nullptr, nullptr) != 1) {
        if (key_fp) fclose(key_fp);
        X509_free(x509);
        EVP_PKEY_free(pkey);
        return false;
    }
    fclose(key_fp);
    
    X509_free(x509);
    EVP_PKEY_free(pkey);
    return true;
}

bool SSLUtils::verify_certificate(const std::string& cert_file, const std::string& key_file) {
    initialize_openssl();
    
    // 加载证书
    FILE* cert_fp = fopen(cert_file.c_str(), "r");
    if (!cert_fp) {
        return false;
    }
    
    X509* cert = PEM_read_X509(cert_fp, nullptr, nullptr, nullptr);
    fclose(cert_fp);
    if (!cert) {
        return false;
    }
    
    // 加载私钥
    FILE* key_fp = fopen(key_file.c_str(), "r");
    if (!key_fp) {
        X509_free(cert);
        return false;
    }
    
    EVP_PKEY* pkey = PEM_read_PrivateKey(key_fp, nullptr, nullptr, nullptr);
    fclose(key_fp);
    if (!pkey) {
        X509_free(cert);
        return false;
    }
    
    // 验证密钥和证书是否匹配
    EVP_PKEY* cert_pkey = X509_get_pubkey(cert);
    bool matches = false;
    if (cert_pkey) {
        matches = (EVP_PKEY_cmp(pkey, cert_pkey) == 1);
        EVP_PKEY_free(cert_pkey);
    }
    
    X509_free(cert);
    EVP_PKEY_free(pkey);
    
    return matches;
}

std::string SSLUtils::get_certificate_info(const std::string& cert_file) {
    initialize_openssl();
    
    FILE* fp = fopen(cert_file.c_str(), "r");
    if (!fp) {
        return "";
    }
    
    X509* cert = PEM_read_X509(fp, nullptr, nullptr, nullptr);
    fclose(fp);
    if (!cert) {
        return "";
    }
    
    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio) {
        X509_free(cert);
        return "";
    }
    
    X509_print(bio, cert);
    
    char* data;
    long len = BIO_get_mem_data(bio, &data);
    std::string result(data, len);
    
    BIO_free(bio);
    X509_free(cert);
    
    return result;
}

std::string SSLUtils::get_error_string(SSLError error) {
    switch (error) {
        case SSLError::NONE: return "无错误";
        case SSLError::WANT_READ: return "需要更多输入数据";
        case SSLError::WANT_WRITE: return "需要输出数据";
        case SSLError::SYSCALL_ERROR: return "系统调用错误";
        case SSLError::SSL_ERROR: return "SSL协议错误";
        case SSLError::CERTIFICATE_ERROR: return "证书错误";
        case SSLError::HANDSHAKE_FAILED: return "握手失败";
        case SSLError::CONNECTION_CLOSED: return "连接关闭";
        case SSLError::INVALID_STATE: return "无效状态";
        case SSLError::MEMORY_ERROR: return "内存错误";
        default: return "未知错误";
    }
}

std::string SSLUtils::get_openssl_error_string() {
    unsigned long error = ERR_get_error();
    if (error == 0) {
        return "";
    }
    
    char buffer[256];
    ERR_error_string_n(error, buffer, sizeof(buffer));
    return std::string(buffer);
}

// SSLInitializer实现
SSLInitializer::SSLInitializer() {
    std::lock_guard<std::mutex> lock(count_mutex_);
    if (instance_count_ == 0) {
        SSLUtils::initialize_openssl();
    }
    ++instance_count_;
}

SSLInitializer::~SSLInitializer() {
    std::lock_guard<std::mutex> lock(count_mutex_);
    --instance_count_;
    if (instance_count_ == 0) {
        SSLUtils::cleanup_openssl();
    }
}

// 配置构建器实现
SSLServerConfigBuilder::SSLServerConfigBuilder() {
    // 服务器默认配置
    config_.verify_peer = false; // 服务器默认不强制验证客户端证书
}

SSLServerConfigBuilder& SSLServerConfigBuilder::certificate(const std::string& cert_file, const std::string& key_file) {
    config_.cert_file = cert_file;
    config_.key_file = key_file;
    return *this;
}

SSLServerConfigBuilder& SSLServerConfigBuilder::ca_certificates(const std::string& ca_file, const std::string& ca_path) {
    config_.ca_file = ca_file;
    config_.ca_path = ca_path;
    return *this;
}

SSLServerConfigBuilder& SSLServerConfigBuilder::cipher_list(const std::string& ciphers) {
    config_.cipher_list = ciphers;
    return *this;
}

SSLServerConfigBuilder& SSLServerConfigBuilder::protocol_version(const std::string& version) {
    config_.protocol_version = version;
    return *this;
}

SSLServerConfigBuilder& SSLServerConfigBuilder::verify_peer(bool verify) {
    config_.verify_peer = verify;
    return *this;
}

SSLServerConfigBuilder& SSLServerConfigBuilder::verify_depth(int depth) {
    config_.verify_depth = depth;
    return *this;
}

SSLClientConfigBuilder::SSLClientConfigBuilder() {
    // 客户端默认配置
    config_.verify_peer = true;      // 客户端默认验证服务器证书
    config_.verify_hostname = true;  // 客户端默认验证主机名
}

SSLClientConfigBuilder& SSLClientConfigBuilder::ca_certificates(const std::string& ca_file, const std::string& ca_path) {
    config_.ca_file = ca_file;
    config_.ca_path = ca_path;
    return *this;
}

SSLClientConfigBuilder& SSLClientConfigBuilder::client_certificate(const std::string& cert_file, const std::string& key_file) {
    config_.cert_file = cert_file;
    config_.key_file = key_file;
    return *this;
}

SSLClientConfigBuilder& SSLClientConfigBuilder::cipher_list(const std::string& ciphers) {
    config_.cipher_list = ciphers;
    return *this;
}

SSLClientConfigBuilder& SSLClientConfigBuilder::protocol_version(const std::string& version) {
    config_.protocol_version = version;
    return *this;
}

SSLClientConfigBuilder& SSLClientConfigBuilder::verify_peer(bool verify) {
    config_.verify_peer = verify;
    return *this;
}

SSLClientConfigBuilder& SSLClientConfigBuilder::verify_hostname(bool verify) {
    config_.verify_hostname = verify;
    return *this;
}

SSLClientConfigBuilder& SSLClientConfigBuilder::verify_depth(int depth) {
    config_.verify_depth = depth;
    return *this;
}

} // namespace stdhttps