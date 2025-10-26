#ifndef NF_MANAGEMENT_H
#define NF_MANAGEMENT_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <mutex>
#include <thread>

namespace amf_sm {

// 网络功能类型
enum class NfType {
    AMF,        // Access and Mobility Management Function
    SMF,        // Session Management Function
    UPF,        // User Plane Function
    AUSF,       // Authentication Server Function
    UDM,        // Unified Data Management
    UDR,        // Unified Data Repository
    PCF,        // Policy Control Function
    NRF,        // Network Repository Function
    NSSF,       // Network Slice Selection Function
    NEF,        // Network Exposure Function
    UNKNOWN     // 未知类型
};

// NF状态
enum class NfStatus {
    REGISTERED,
    SUSPENDED,
    UNDISCOVERABLE,
    DEREGISTERED
};

// NF服务信息
struct NfService {
    std::string serviceInstanceId;
    std::string serviceName;
    std::vector<std::string> versions;
    std::string scheme;                 // http, https
    std::string fqdn;                   // Fully Qualified Domain Name
    std::string ipv4Address;
    std::string ipv6Address;
    int port;
    std::string apiPrefix;
    std::map<std::string, std::string> supportedFeatures;
    NfStatus status;
};

// NF实例信息
struct NfInstance {
    std::string nfInstanceId;
    NfType nfType;
    NfStatus nfStatus;
    std::string heartBeatTimer;
    std::string plmnId;
    std::vector<std::string> sNssais;   // Supported Network Slice Selection Assistance Information
    std::vector<std::string> taiList;   // Tracking Area Identity List
    std::string fqdn;
    std::string ipv4Address;
    std::string ipv6Address;
    int priority;
    int capacity;
    int load;
    std::vector<NfService> nfServices;
    std::chrono::system_clock::time_point registrationTime;
    std::chrono::system_clock::time_point lastHeartbeat;
    std::map<std::string, std::string> customInfo;
    
    NfInstance() = default;
    NfInstance(const std::string& id, NfType type);
    
    void addService(const NfService& service);
    bool removeService(const std::string& serviceName);
    NfService* getService(const std::string& serviceName);
    void updateLoad(int newLoad);
    bool isHealthy() const;
    
    std::string toString() const;
    std::string toJson() const;
};

// NF发现查询参数
struct NfDiscoveryQuery {
    NfType targetNfType;
    std::string requesterNfType;
    std::string requesterNfInstanceId;
    std::string targetPlmnId;
    std::string targetNfInstanceId;
    std::vector<std::string> requesterSNssais;
    std::vector<std::string> targetSNssais;
    std::string dnn;                    // Data Network Name
    std::string snssai;                 // Single Network Slice Selection Assistance Information
    std::string serviceName;
    int maxNfInstances;
    
    std::string toString() const;
};

// NF管理器
class NfManager {
public:
    static NfManager& getInstance();
    
    // NF注册管理
    bool registerNfInstance(const NfInstance& nfInstance);
    bool updateNfInstance(const std::string& nfInstanceId, const NfInstance& nfInstance);
    bool deregisterNfInstance(const std::string& nfInstanceId);
    
    // NF发现
    std::vector<NfInstance> discoverNfInstances(const NfDiscoveryQuery& query);
    std::shared_ptr<NfInstance> getNfInstance(const std::string& nfInstanceId);
    
    // NF状态管理
    bool updateNfStatus(const std::string& nfInstanceId, NfStatus status);
    void processHeartbeat(const std::string& nfInstanceId);
    
    // 负载均衡
    std::shared_ptr<NfInstance> selectNfInstance(NfType nfType, const std::string& plmnId = "");
    std::vector<NfInstance> getNfInstancesByType(NfType nfType);
    
    // 健康检查
    void performHealthCheck();
    void markNfInstanceUnavailable(const std::string& nfInstanceId);
    
    // 统计信息
    size_t getRegisteredNfCount() const;
    size_t getRegisteredNfCountByType(NfType nfType) const;
    std::map<NfType, size_t> getNfStatistics() const;
    
    // 清理操作
    void cleanupExpiredNfInstances();

    // 管理器控制
    bool start();
    void stop();

private:
    NfManager() = default;
    ~NfManager();
    
    std::map<std::string, std::shared_ptr<NfInstance>> nfInstances_;
    std::map<NfType, std::vector<std::string>> nfInstancesByType_;
    mutable std::mutex nfMutex_;
    
    bool running = false;
    std::thread healthCheckThread;
    
    // 健康检查线程
    void healthCheckLoop();
    
    // 辅助方法
    bool matchesQuery(const NfInstance& nfInstance, const NfDiscoveryQuery& query) const;
    int calculateNfScore(const NfInstance& nfInstance) const;
};

// AMF自身的NF实例管理
class AmfNfInstance {
public:
    static AmfNfInstance& getInstance();
    
    // 初始化AMF NF实例
    bool initialize(const std::string& amfInstanceId, const std::string& plmnId);
    
    // 向NRF注册
    bool registerWithNrf(const std::string& nrfUri);
    
    // 发送心跳
    bool sendHeartbeat();
    
    // 更新状态
    void updateStatus(NfStatus status);
    void updateLoad(int load);
    void updateCapacity(int capacity);
    
    // 获取AMF实例信息
    const NfInstance& getAmfInstance() const { return amfInstance_; }
    
    // 启动/停止心跳服务
    void startHeartbeatService(int intervalSeconds);
    void stopHeartbeatService();

private:
    AmfNfInstance() = default;
    
    NfInstance amfInstance_;
    std::string nrfUri_;
    bool registered_{false};
    bool heartbeatRunning_{false};
    std::thread heartbeatThread_;
    int heartbeatInterval_{30}; // 默认30秒
    mutable std::mutex instanceMutex_;
};

// 辅助函数
std::string nfTypeToString(NfType type);
std::string nfStatusToString(NfStatus status);
NfType stringToNfType(const std::string& str);
NfStatus stringToNfStatus(const std::string& str);

} // namespace amf_sm

#endif // NF_MANAGEMENT_H