/**
 * @file ssl_handler.h
 * @brief SSL/TLS处理器头文件
 */

#ifndef STDHTTPS_SSL_HANDLER_H
#define STDHTTPS_SSL_HANDLER_H

#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <mutex>

// 前向声明OpenSSL结构体
typedef struct ssl_st SSL;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct bio_st BIO;
typedef struct x509_st X509;
typedef struct evp_pkey_st EVP_PKEY;

namespace stdhttps {

/**
 * @brief SSL错误类型枚举
 */
enum class SSLError {
    NONE = 0,           // 无错误
    WANT_READ,          // 需要更多输入数据
    WANT_WRITE,         // 需要输出数据
    SYSCALL_ERROR,      // 系统调用错误
    SSL_ERROR,          // SSL协议错误
    CERTIFICATE_ERROR,  // 证书错误
    HANDSHAKE_FAILED,   // 握手失败
    CONNECTION_CLOSED,  // 连接关闭
    INVALID_STATE,      // 无效状态
    MEMORY_ERROR        // 内存错误
};

/**
 * @brief SSL连接状态枚举
 */
enum class SSLState {
    INIT,               // 初始状态
    HANDSHAKING,        // 握手中
    CONNECTED,          // 已连接
    SHUTDOWN,           // 关闭中
    CLOSED,             // 已关闭
    ERROR               // 错误状态
};

/**
 * @brief SSL配置结构体
 */
struct SSLConfig {
    std::string cert_file;          // 证书文件路径
    std::string key_file;           // 私钥文件路径
    std::string ca_file;            // CA证书文件路径
    std::string ca_path;            // CA证书目录路径
    std::string cipher_list;        // 加密套件列表
    std::string protocol_version;   // 协议版本 (TLSv1.2, TLSv1.3等)
    bool verify_peer;               // 是否验证对端证书
    bool verify_hostname;           // 是否验证主机名
    int verify_depth;               // 证书链验证深度
    
    SSLConfig() 
        : verify_peer(true)
        , verify_hostname(true) 
        , verify_depth(9) {}
};

/**
 * @brief SSL上下文管理器
 * @details 管理SSL_CTX的生命周期和配置
 */
class SSLContextManager {
public:
    /**
     * @brief 构造函数
     * @param is_server 是否为服务器模式
     */
    explicit SSLContextManager(bool is_server = false);
    
    /**
     * @brief 析构函数
     */
    ~SSLContextManager();
    
    /**
     * @brief 禁用拷贝构造
     */
    SSLContextManager(const SSLContextManager&) = delete;
    SSLContextManager& operator=(const SSLContextManager&) = delete;
    
    /**
     * @brief 移动构造函数
     */
    SSLContextManager(SSLContextManager&& other) noexcept;
    SSLContextManager& operator=(SSLContextManager&& other) noexcept;

    /**
     * @brief 初始化SSL上下文
     * @param config SSL配置
     * @return 是否成功
     */
    bool initialize(const SSLConfig& config);
    
    /**
     * @brief 获取SSL_CTX指针
     */
    SSL_CTX* get_context() const { return ssl_ctx_; }
    
    /**
     * @brief 是否已初始化
     */
    bool is_initialized() const { return ssl_ctx_ != nullptr; }
    
    /**
     * @brief 获取错误信息
     */
    const std::string& get_error() const { return error_message_; }
    
    /**
     * @brief 设置证书文件
     * @param cert_file 证书文件路径
     * @param key_file 私钥文件路径
     * @return 是否成功
     */
    bool load_certificate(const std::string& cert_file, const std::string& key_file);
    
    /**
     * @brief 设置CA证书
     * @param ca_file CA证书文件路径
     * @param ca_path CA证书目录路径
     * @return 是否成功
     */
    bool load_ca_certificates(const std::string& ca_file, const std::string& ca_path);
    
    /**
     * @brief 设置加密套件
     * @param cipher_list 加密套件列表
     * @return 是否成功
     */
    bool set_cipher_list(const std::string& cipher_list);

private:
    void cleanup();
    void set_error(const std::string& message);
    
private:
    SSL_CTX* ssl_ctx_;              // SSL上下文
    bool is_server_;                // 是否为服务器模式
    std::string error_message_;     // 错误信息
};

/**
 * @brief SSL连接处理器
 * @details 处理单个SSL连接的握手、数据传输和关闭
 */
class SSLHandler {
public:
    /**
     * @brief 数据回调函数类型
     * @param data 数据指针
     * @param size 数据大小
     * @return 实际发送的字节数，-1表示错误
     */
    using WriteCallback = std::function<int(const void* data, size_t size)>;
    
    /**
     * @brief 构造函数
     * @param ssl_ctx SSL上下文
     * @param is_server 是否为服务器模式
     */
    SSLHandler(SSL_CTX* ssl_ctx, bool is_server = false);
    
    /**
     * @brief 析构函数
     */
    ~SSLHandler();
    
    /**
     * @brief 禁用拷贝构造
     */
    SSLHandler(const SSLHandler&) = delete;
    SSLHandler& operator=(const SSLHandler&) = delete;
    
    /**
     * @brief 移动构造函数
     */
    SSLHandler(SSLHandler&& other) noexcept;
    SSLHandler& operator=(SSLHandler&& other) noexcept;

    /**
     * @brief 设置数据输出回调
     * @param callback 输出回调函数
     */
    void set_write_callback(WriteCallback callback);
    
    /**
     * @brief 开始SSL握手
     * @return 是否成功开始握手
     */
    bool start_handshake();
    
    /**
     * @brief 处理接收到的数据
     * @param data 数据指针
     * @param size 数据大小
     * @return SSL错误码
     */
    SSLError handle_input(const void* data, size_t size);
    
    /**
     * @brief 发送数据
     * @param data 数据指针
     * @param size 数据大小
     * @param bytes_sent 实际发送的字节数（输出参数）
     * @return SSL错误码
     */
    SSLError send_data(const void* data, size_t size, size_t& bytes_sent);
    
    /**
     * @brief 接收数据
     * @param buffer 接收缓冲区
     * @param buffer_size 缓冲区大小
     * @param bytes_received 实际接收的字节数（输出参数）
     * @return SSL错误码
     */
    SSLError receive_data(void* buffer, size_t buffer_size, size_t& bytes_received);
    
    /**
     * @brief 关闭SSL连接
     * @return 是否成功
     */
    bool shutdown();
    
    /**
     * @brief 获取连接状态
     */
    SSLState get_state() const { return state_; }
    
    /**
     * @brief 是否握手完成
     */
    bool is_handshake_completed() const { return state_ == SSLState::CONNECTED; }
    
    /**
     * @brief 获取最后的错误信息
     */
    const std::string& get_last_error() const { return last_error_; }
    
    /**
     * @brief 获取对端证书信息
     * @return 证书信息字符串
     */
    std::string get_peer_certificate_info() const;
    
    /**
     * @brief 获取当前加密套件
     * @return 加密套件名称
     */
    std::string get_cipher_name() const;
    
    /**
     * @brief 获取SSL版本
     * @return SSL版本字符串
     */
    std::string get_ssl_version() const;

private:
    void cleanup();
    void set_error(const std::string& message);
    SSLError handle_ssl_error(int ssl_error);
    void flush_bio_write();
    
private:
    SSL* ssl_;                      // SSL连接对象
    BIO* read_bio_;                 // 读取BIO
    BIO* write_bio_;                // 写入BIO
    SSLState state_;                // 连接状态
    bool is_server_;                // 是否为服务器模式
    WriteCallback write_callback_;  // 数据输出回调
    std::string last_error_;        // 最后的错误信息
    std::mutex mutex_;              // 线程安全锁
};

/**
 * @brief SSL工具类
 * @details 提供SSL相关的实用功能
 */
class SSLUtils {
public:
    /**
     * @brief 初始化OpenSSL库
     * @note 程序启动时应该调用一次
     */
    static void initialize_openssl();
    
    /**
     * @brief 清理OpenSSL库
     * @note 程序结束时应该调用一次
     */
    static void cleanup_openssl();
    
    /**
     * @brief 获取OpenSSL版本信息
     * @return 版本字符串
     */
    static std::string get_openssl_version();
    
    /**
     * @brief 生成自签名证书
     * @param cert_file 输出证书文件路径
     * @param key_file 输出私钥文件路径
     * @param days 有效天数
     * @param country 国家代码
     * @param org 组织名称
     * @param cn 通用名称（域名）
     * @return 是否成功
     */
    static bool generate_self_signed_cert(const std::string& cert_file,
                                         const std::string& key_file,
                                         int days = 365,
                                         const std::string& country = "CN",
                                         const std::string& org = "Test Organization",
                                         const std::string& cn = "localhost");
    
    /**
     * @brief 验证证书文件
     * @param cert_file 证书文件路径
     * @param key_file 私钥文件路径
     * @return 是否有效
     */
    static bool verify_certificate(const std::string& cert_file, const std::string& key_file);
    
    /**
     * @brief 获取证书信息
     * @param cert_file 证书文件路径
     * @return 证书信息字符串
     */
    static std::string get_certificate_info(const std::string& cert_file);
    
    /**
     * @brief 获取SSL错误描述
     * @param error SSL错误码
     * @return 错误描述字符串
     */
    static std::string get_error_string(SSLError error);
    
    /**
     * @brief 获取OpenSSL错误字符串
     * @return 错误字符串
     */
    static std::string get_openssl_error_string();

private:
    SSLUtils() = delete; // 工具类，不允许实例化
    static bool openssl_initialized_;
    static std::mutex init_mutex_;
};

/**
 * @brief RAII风格的SSL初始化器
 * @details 自动管理OpenSSL的初始化和清理
 */
class SSLInitializer {
public:
    SSLInitializer();
    ~SSLInitializer();
    
    SSLInitializer(const SSLInitializer&) = delete;
    SSLInitializer& operator=(const SSLInitializer&) = delete;
    
private:
    static int instance_count_;
    static std::mutex count_mutex_;
};

/**
 * @brief SSL服务器配置构建器
 * @details 便于构建服务器端SSL配置的辅助类
 */
class SSLServerConfigBuilder {
public:
    SSLServerConfigBuilder();
    
    SSLServerConfigBuilder& certificate(const std::string& cert_file, const std::string& key_file);
    SSLServerConfigBuilder& ca_certificates(const std::string& ca_file, const std::string& ca_path = "");
    SSLServerConfigBuilder& cipher_list(const std::string& ciphers);
    SSLServerConfigBuilder& protocol_version(const std::string& version);
    SSLServerConfigBuilder& verify_peer(bool verify);
    SSLServerConfigBuilder& verify_depth(int depth);
    
    SSLConfig build() const { return config_; }
    
private:
    SSLConfig config_;
};

/**
 * @brief SSL客户端配置构建器
 * @details 便于构建客户端SSL配置的辅助类
 */
class SSLClientConfigBuilder {
public:
    SSLClientConfigBuilder();
    
    SSLClientConfigBuilder& ca_certificates(const std::string& ca_file, const std::string& ca_path = "");
    SSLClientConfigBuilder& client_certificate(const std::string& cert_file, const std::string& key_file);
    SSLClientConfigBuilder& cipher_list(const std::string& ciphers);
    SSLClientConfigBuilder& protocol_version(const std::string& version);
    SSLClientConfigBuilder& verify_peer(bool verify);
    SSLClientConfigBuilder& verify_hostname(bool verify);
    SSLClientConfigBuilder& verify_depth(int depth);
    
    SSLConfig build() const { return config_; }
    
private:
    SSLConfig config_;
};

} // namespace stdhttps

#endif // STDHTTPS_SSL_HANDLER_H