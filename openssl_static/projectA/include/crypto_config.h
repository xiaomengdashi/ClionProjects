#ifndef CRYPTO_CONFIG_H
#define CRYPTO_CONFIG_H

#include <string>
#include <unordered_map>

namespace CryptoUtils {

/**
 * 加密库配置管理类
 */
class Config {
public:
    static Config& getInstance();
    
    // 配置项设置
    void setLogLevel(const std::string& level);
    void setDefaultKeySize(int size);
    void setDefaultIVSize(int size);
    void enableBenchmark(bool enable);
    
    // 配置项获取
    std::string getLogLevel() const;
    int getDefaultKeySize() const;
    int getDefaultIVSize() const;
    bool isBenchmarkEnabled() const;
    
    // 从文件加载配置
    bool loadFromFile(const std::string& filename);
    
    // 保存配置到文件
    bool saveToFile(const std::string& filename) const;
    
private:
    Config() = default;
    ~Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    
    std::unordered_map<std::string, std::string> config_map_;
    
    // 默认配置
    void setDefaults();
};

} // namespace CryptoUtils

#endif // CRYPTO_CONFIG_H