#ifndef UE_CONTEXT_H
#define UE_CONTEXT_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <mutex>
#include <atomic>

namespace amf_sm {

// 5G标识符定义
struct FiveGIdentifiers {
    std::string supi;           // Subscription Permanent Identifier
    std::string pei;            // Permanent Equipment Identifier  
    std::string gpsi;           // Generic Public Subscription Identifier
    std::string guti;           // 5G Globally Unique Temporary Identifier
    std::string tmsi;           // Temporary Mobile Subscriber Identity
};

// 网络切片信息
struct NetworkSlice {
    int sst;                    // Slice/Service Type
    std::string sd;             // Slice Differentiator
    std::string plmnId;         // Public Land Mobile Network ID
    std::string snssai;         // S-NSSAI (SST+SD)
    
    std::string toString() const {
        return "SST:" + std::to_string(sst) + ",SD:" + sd + ",PLMN:" + plmnId;
    }
};

// 位置信息
struct LocationInfo {
    std::string tai;            // Tracking Area Identity
    std::string cellId;         // Cell Identity
    std::string plmnId;         // PLMN ID
    int tac;                    // Tracking Area Code
    std::string ratType;        // Radio Access Technology Type
    std::chrono::system_clock::time_point lastUpdate;
};

// 安全上下文
struct SecurityContext {
    std::string kAmf;           // AMF Key
    std::string kSeaf;          // SEAF Key
    std::string kAusf;          // AUSF Key
    int ngKsi;                  // Next Generation Key Set Identifier
    std::string authVector;     // Authentication Vector
    bool isAuthenticated;
    std::chrono::system_clock::time_point authTime;
};

// PDU会话信息
struct PduSession {
    int sessionId;
    std::string dnn;            // Data Network Name
    std::string sNssai;         // Single Network Slice Selection Assistance Information
    std::string pduType;        // PDU Session Type (IPv4, IPv6, Ethernet)
    std::string smfId;          // SMF Instance ID
    std::string upfId;          // UPF Instance ID
    std::string qosFlowId;      // QoS Flow Identifier
    std::string state;          // Session state (ACTIVE, INACTIVE, etc.)
    bool isActive;
    std::chrono::system_clock::time_point establishTime;
};

// 接入信息
struct AccessInfo {
    std::string accessType;     // 3GPP_ACCESS, NON_3GPP_ACCESS
    std::string anType;         // Access Network Type
    std::string ranNodeId;      // RAN Node ID (gNB ID)
    std::string anIpAddress;    // Access Network IP Address
    int anPort;                 // Access Network Port
    bool isConnected;
};

// 移动性管理信息
struct MobilityInfo {
    std::vector<std::string> allowedNssai;  // Allowed Network Slice Selection Assistance Information
    std::vector<std::string> configuredNssai; // Configured NSSAI
    std::string serviceAreaRestriction;
    std::vector<std::string> forbiddenAreas;
    bool isRoaming;
    std::string homeNetworkPlmn;
};

// 订阅信息
struct SubscriptionInfo {
    std::string subscriptionId;
    std::vector<NetworkSlice> subscribedSlices;
    std::string accessRestriction;
    std::string coreNetworkType;
    bool isEmergencyRegistered;
    std::map<std::string, std::string> additionalInfo;
};

// UE上下文类
class UeContext {
public:
    UeContext(const std::string& supi);
    
    // 基本信息管理
    const FiveGIdentifiers& getIdentifiers() const { return identifiers_; }
    void setIdentifiers(const FiveGIdentifiers& ids) { identifiers_ = ids; }
    
    // 位置管理
    const LocationInfo& getLocationInfo() const { return locationInfo_; }
    void updateLocation(const LocationInfo& location);
    
    // 安全上下文管理
    const SecurityContext& getSecurityContext() const { return securityContext_; }
    void updateSecurityContext(const SecurityContext& security);
    bool isAuthenticated() const { return securityContext_.isAuthenticated; }
    
    // PDU会话管理
    void addPduSession(const PduSession& session);
    void removePduSession(int sessionId);
    std::vector<PduSession> getActivePduSessions() const;
    PduSession* getPduSession(int sessionId);
    
    // 接入管理
    const AccessInfo& getAccessInfo() const { return accessInfo_; }
    void updateAccessInfo(const AccessInfo& access);
    bool isConnected() const { return accessInfo_.isConnected; }
    
    // 移动性管理
    const MobilityInfo& getMobilityInfo() const { return mobilityInfo_; }
    void updateMobilityInfo(const MobilityInfo& mobility);
    
    // 订阅信息管理
    const SubscriptionInfo& getSubscriptionInfo() const { return subscriptionInfo_; }
    void updateSubscriptionInfo(const SubscriptionInfo& subscription);
    
    // 状态管理
    void setRegistrationState(const std::string& state) { registrationState_ = state; }
    const std::string& getRegistrationState() const { return registrationState_; }
    
    void setConnectionState(const std::string& state) { connectionState_ = state; }
    const std::string& getConnectionState() const { return connectionState_; }
    
    // 时间戳管理
    void updateLastActivity() { lastActivity_ = std::chrono::system_clock::now(); }
    std::chrono::system_clock::time_point getLastActivity() const { return lastActivity_; }
    
    // 额外的访问方法
    void setRanNodeId(const std::string& ranNodeId) { accessInfo_.ranNodeId = ranNodeId; }
    void setTai(const std::string& tai) { locationInfo_.tai = tai; }
    
    // 序列化
    std::string toJson() const;
    void fromJson(const std::string& json);
    
    // 调试信息
    std::string toString() const;

private:
    FiveGIdentifiers identifiers_;
    LocationInfo locationInfo_;
    SecurityContext securityContext_;
    std::vector<PduSession> pduSessions_;
    AccessInfo accessInfo_;
    MobilityInfo mobilityInfo_;
    SubscriptionInfo subscriptionInfo_;
    
    std::string registrationState_;
    std::string connectionState_;
    std::chrono::system_clock::time_point lastActivity_;
    std::chrono::system_clock::time_point creationTime_;
    
    // 额外的成员变量，用于AmfSm.cpp中的函数
    std::string tai;                                // 跟踪区标识
    std::string ranNodeId;                         // RAN节点标识
    std::time_t lastRegistrationTime;              // 上次注册时间
    std::vector<NetworkSlice> subscribedSlices;    // 订阅的网络切片
};

// UE上下文管理器
class UeContextManager {
public:
    static UeContextManager& getInstance();
    
    // UE上下文管理
    std::shared_ptr<UeContext> createUeContext(const std::string& supi);
    std::shared_ptr<UeContext> getUeContext(const std::string& supi);
    std::shared_ptr<UeContext> getUeContextByGuti(const std::string& guti);
    void removeUeContext(const std::string& supi);
    
    // 批量操作
    std::vector<std::shared_ptr<UeContext>> getAllUeContexts();
    std::vector<std::shared_ptr<UeContext>> getUeContextsBySlice(const NetworkSlice& slice);
    std::vector<std::shared_ptr<UeContext>> getUeContextsByLocation(const std::string& tai);
    
    // 统计信息
    size_t getRegisteredUeCount() const;
    size_t getConnectedUeCount() const;
    size_t getActiveSessionCount() const;
    
    // 清理操作
    void cleanupInactiveContexts(std::chrono::minutes inactiveThreshold);

private:
    UeContextManager() = default;
    std::map<std::string, std::shared_ptr<UeContext>> ueContexts_;  // SUPI -> UeContext
    std::map<std::string, std::string> gutiToSupi_;                 // GUTI -> SUPI
    mutable std::mutex contextMutex_;
};

} // namespace amf_sm

#endif // UE_CONTEXT_H