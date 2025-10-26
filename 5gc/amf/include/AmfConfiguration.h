#ifndef AMF_CONFIGURATION_H
#define AMF_CONFIGURATION_H

#include <string>
#include <vector>
#include <map>

namespace amf_sm {

// AMF配置信息
struct AmfConfiguration {
    // AMF基本信息
    std::string amfInstanceId;          // AMF实例ID
    std::string amfName;                // AMF名称
    std::string amfRegionId;            // AMF区域ID
    std::string amfSetId;               // AMF集合ID
    std::string amfPointer;             // AMF指针
    
    // 网络信息
    std::string plmnId;                 // PLMN ID
    std::vector<std::string> taiList;   // 支持的TAI列表
    std::vector<std::string> plmnList;  // 支持的PLMN列表
    
    // 服务信息
    std::string sbiBindAddress;         // SBI绑定地址
    int sbiPort;                        // SBI端口
    std::string n1n2BindAddress;        // N1/N2接口绑定地址
    int n2Port;                         // N2端口
    
    // 安全配置
    std::string amfKey;                 // AMF密钥
    std::vector<std::string> supportedAlgorithms; // 支持的加密算法
    int authenticationTimeout;          // 认证超时时间(秒)
    
    // 网络切片配置
    std::vector<std::string> supportedSlices; // 支持的网络切片
    
    // 负载均衡配置
    int maxUeConnections;               // 最大UE连接数
    int loadBalanceThreshold;           // 负载均衡阈值
    
    // 定时器配置
    int t3510Timer;                     // 注册定时器(秒)
    int t3511Timer;                     // 注销定时器(秒)
    int t3513Timer;                     // 寻呼定时器(秒)
    int t3560Timer;                     // 认证定时器(秒)
    
    // NF发现配置
    std::string nrfUri;                 // NRF地址
    int nfHeartbeatInterval;            // NF心跳间隔(秒)
    
    // 日志配置
    std::string logLevel;               // 日志级别
    std::string logFile;                // 日志文件路径
};

// 获取默认配置
AmfConfiguration getDefaultConfiguration();

// 从文件加载配置
bool loadConfigurationFromFile(const std::string& filename, AmfConfiguration& config);

// 保存配置到文件
bool saveConfigurationToFile(const std::string& filename, const AmfConfiguration& config);

} // namespace amf_sm

#endif // AMF_CONFIGURATION_H