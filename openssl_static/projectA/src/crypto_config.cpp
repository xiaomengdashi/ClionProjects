#include "crypto_config.h"
#include <fstream>
#include <sstream>

namespace CryptoUtils {

Config& Config::getInstance() {
    static Config instance;
    static bool initialized = false;
    if (!initialized) {
        instance.setDefaults();
        initialized = true;
    }
    return instance;
}

void Config::setLogLevel(const std::string& level) {
    config_map_["log_level"] = level;
}

void Config::setDefaultKeySize(int size) {
    config_map_["default_key_size"] = std::to_string(size);
}

void Config::setDefaultIVSize(int size) {
    config_map_["default_iv_size"] = std::to_string(size);
}

void Config::enableBenchmark(bool enable) {
    config_map_["benchmark_enabled"] = enable ? "true" : "false";
}

std::string Config::getLogLevel() const {
    auto it = config_map_.find("log_level");
    return it != config_map_.end() ? it->second : "INFO";
}

int Config::getDefaultKeySize() const {
    auto it = config_map_.find("default_key_size");
    if (it != config_map_.end()) {
        return std::stoi(it->second);
    }
    return 32; // 默认256位密钥
}

int Config::getDefaultIVSize() const {
    auto it = config_map_.find("default_iv_size");
    if (it != config_map_.end()) {
        return std::stoi(it->second);
    }
    return 16; // 默认128位IV
}

bool Config::isBenchmarkEnabled() const {
    auto it = config_map_.find("benchmark_enabled");
    return it != config_map_.end() && it->second == "true";
}

bool Config::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // 跳过注释和空行
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // 解析键值对
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            // 去除前后空格
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            config_map_[key] = value;
        }
    }
    
    return true;
}

bool Config::saveToFile(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    file << "# CryptoUtils Configuration File\n";
    file << "# Generated automatically\n\n";
    
    for (const auto& pair : config_map_) {
        file << pair.first << " = " << pair.second << "\n";
    }
    
    return true;
}

void Config::setDefaults() {
    config_map_["log_level"] = "INFO";
    config_map_["default_key_size"] = "32";
    config_map_["default_iv_size"] = "16";
    config_map_["benchmark_enabled"] = "false";
}

} // namespace CryptoUtils