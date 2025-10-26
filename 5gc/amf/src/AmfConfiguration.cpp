#include "AmfConfiguration.h"
#include <fstream>
#include <sstream>

namespace amf_sm {

AmfConfiguration getDefaultConfiguration() {
    AmfConfiguration config;
    
    // AMF基本信息
    config.amfInstanceId = "amf-001";
    config.amfName = "AMF-Beijing-001";
    config.amfRegionId = "01";
    config.amfSetId = "001";
    config.amfPointer = "01";
    
    // 网络信息
    config.plmnId = "46001";
    config.taiList = {"46001-001", "46001-002", "46001-003"};
    config.plmnList = {"46001", "46000"};
    
    // 服务信息
    config.sbiBindAddress = "0.0.0.0";
    config.sbiPort = 8080;
    config.n1n2BindAddress = "0.0.0.0";
    config.n2Port = 38412;
    
    // 安全配置
    config.amfKey = "0123456789abcdef0123456789abcdef";
    config.supportedAlgorithms = {"5G-EA0", "5G-EA1", "5G-EA2", "5G-IA1", "5G-IA2"};
    config.authenticationTimeout = 30;
    
    // 网络切片配置
    config.supportedSlices = {"SST:1,SD:000001", "SST:2,SD:000002", "SST:3,SD:000003"};
    
    // 负载均衡配置
    config.maxUeConnections = 10000;
    config.loadBalanceThreshold = 80;
    
    // 定时器配置
    config.t3510Timer = 15;
    config.t3511Timer = 10;
    config.t3513Timer = 6;
    config.t3560Timer = 6;
    
    // NF发现配置
    config.nrfUri = "http://nrf.5gc.mnc001.mcc460.3gppnetwork.org:8080";
    config.nfHeartbeatInterval = 30;
    
    // 日志配置
    config.logLevel = "INFO";
    config.logFile = "/var/log/amf/amf.log";
    
    return config;
}

bool loadConfigurationFromFile(const std::string& filename, AmfConfiguration& config) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    // 简单的配置文件解析（实际应用中可以使用JSON或YAML）
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        size_t pos = line.find('=');
        if (pos == std::string::npos) continue;
        
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        
        // 去除空格
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        // 解析配置项
        if (key == "amf_instance_id") config.amfInstanceId = value;
        else if (key == "amf_name") config.amfName = value;
        else if (key == "plmn_id") config.plmnId = value;
        else if (key == "sbi_bind_address") config.sbiBindAddress = value;
        else if (key == "sbi_port") config.sbiPort = std::stoi(value);
        else if (key == "n2_port") config.n2Port = std::stoi(value);
        else if (key == "nrf_uri") config.nrfUri = value;
        else if (key == "log_level") config.logLevel = value;
        else if (key == "log_file") config.logFile = value;
        // 可以添加更多配置项解析
    }
    
    return true;
}

bool saveConfigurationToFile(const std::string& filename, const AmfConfiguration& config) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    file << "# AMF Configuration File\n";
    file << "amf_instance_id=" << config.amfInstanceId << "\n";
    file << "amf_name=" << config.amfName << "\n";
    file << "plmn_id=" << config.plmnId << "\n";
    file << "sbi_bind_address=" << config.sbiBindAddress << "\n";
    file << "sbi_port=" << config.sbiPort << "\n";
    file << "n2_port=" << config.n2Port << "\n";
    file << "nrf_uri=" << config.nrfUri << "\n";
    file << "log_level=" << config.logLevel << "\n";
    file << "log_file=" << config.logFile << "\n";
    
    return true;
}

} // namespace amf_sm