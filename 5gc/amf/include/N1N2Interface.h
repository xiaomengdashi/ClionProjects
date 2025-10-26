#ifndef N1N2_INTERFACE_H
#define N1N2_INTERFACE_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <chrono>

namespace amf_sm {

// N1接口消息类型 (AMF <-> UE)
enum class N1MessageType {
    // Registration Management
    REGISTRATION_REQUEST,
    REGISTRATION_ACCEPT,
    REGISTRATION_REJECT,
    REGISTRATION_COMPLETE,
    DEREGISTRATION_REQUEST_UE_ORIG,
    DEREGISTRATION_REQUEST_UE_TERM,
    DEREGISTRATION_ACCEPT_UE_ORIG,
    DEREGISTRATION_ACCEPT_UE_TERM,
    
    // Service Request
    SERVICE_REQUEST,
    SERVICE_ACCEPT,
    SERVICE_REJECT,
    
    // Authentication
    AUTHENTICATION_REQUEST,
    AUTHENTICATION_RESPONSE,
    AUTHENTICATION_RESULT,
    AUTHENTICATION_FAILURE,
    AUTHENTICATION_REJECT,
    
    // Security Mode Control
    SECURITY_MODE_COMMAND,
    SECURITY_MODE_COMPLETE,
    SECURITY_MODE_REJECT,
    
    // Identity Request
    IDENTITY_REQUEST,
    IDENTITY_RESPONSE,
    
    // Configuration Update
    CONFIGURATION_UPDATE_COMMAND,
    CONFIGURATION_UPDATE_COMPLETE,
    
    // DL NAS Transport
    DL_NAS_TRANSPORT,
    UL_NAS_TRANSPORT
};

// N2接口消息类型 (AMF <-> gNB)
enum class N2MessageType {
    // Initial Context Setup
    INITIAL_CONTEXT_SETUP_REQUEST,
    INITIAL_CONTEXT_SETUP_RESPONSE,
    INITIAL_CONTEXT_SETUP_FAILURE,
    
    // UE Context Release
    UE_CONTEXT_RELEASE_COMMAND,
    UE_CONTEXT_RELEASE_COMPLETE,
    UE_CONTEXT_RELEASE_REQUEST,
    
    // UE Context Modification
    UE_CONTEXT_MODIFICATION_REQUEST,
    UE_CONTEXT_MODIFICATION_RESPONSE,
    UE_CONTEXT_MODIFICATION_FAILURE,
    
    // Handover
    HANDOVER_REQUIRED,
    HANDOVER_REQUEST,
    HANDOVER_REQUEST_ACKNOWLEDGE,
    HANDOVER_FAILURE,
    HANDOVER_NOTIFY,
    HANDOVER_CANCEL,
    
    // Paging
    PAGING,
    
    // Path Switch
    PATH_SWITCH_REQUEST,
    PATH_SWITCH_REQUEST_ACKNOWLEDGE,
    PATH_SWITCH_REQUEST_FAILURE,
    
    // Error Indication
    ERROR_INDICATION,
    
    // Reset
    NG_RESET,
    NG_RESET_ACKNOWLEDGE,
    
    // Setup
    NG_SETUP_REQUEST,
    NG_SETUP_RESPONSE,
    NG_SETUP_FAILURE,
    
    // Configuration Update
    AMF_CONFIGURATION_UPDATE,
    AMF_CONFIGURATION_UPDATE_ACKNOWLEDGE,
    AMF_CONFIGURATION_UPDATE_FAILURE,
    RAN_CONFIGURATION_UPDATE,
    RAN_CONFIGURATION_UPDATE_ACKNOWLEDGE,
    RAN_CONFIGURATION_UPDATE_FAILURE
};

// N1消息结构
struct N1Message {
    N1MessageType messageType;
    std::string ueId;                   // UE标识
    std::string nasMessageContainer;    // NAS消息容器
    std::map<std::string, std::string> ieList; // 信息元素列表
    std::chrono::system_clock::time_point timestamp;
    
    std::string toString() const;
};

// N2消息结构
struct N2Message {
    N2MessageType messageType;
    std::string ranNodeId;              // RAN节点ID
    std::string ueNgapId;               // UE NGAP ID
    std::string amfUeNgapId;            // AMF UE NGAP ID
    std::string ngapContainer;          // NGAP消息容器
    std::map<std::string, std::string> ieList; // 信息元素列表
    std::chrono::system_clock::time_point timestamp;
    
    std::string toString() const;
};

// N1接口处理器
class N1InterfaceHandler {
public:
    virtual ~N1InterfaceHandler() = default;
    
    // 发送N1消息到UE
    virtual bool sendN1Message(const N1Message& message) = 0;
    
    // 处理来自UE的N1消息
    virtual void handleN1Message(const N1Message& message) = 0;
    
    // 注册N1消息回调
    void registerN1Callback(N1MessageType msgType, 
                           std::function<void(const N1Message&)> callback);

protected:
    std::map<N1MessageType, std::function<void(const N1Message&)>> n1Callbacks_;
};

// N2接口处理器
class N2InterfaceHandler {
public:
    virtual ~N2InterfaceHandler() = default;
    
    // 发送N2消息到gNB
    virtual bool sendN2Message(const N2Message& message) = 0;
    
    // 处理来自gNB的N2消息
    virtual void handleN2Message(const N2Message& message) = 0;
    
    // 注册N2消息回调
    void registerN2Callback(N2MessageType msgType, 
                           std::function<void(const N2Message&)> callback);

protected:
    std::map<N2MessageType, std::function<void(const N2Message&)>> n2Callbacks_;
};

// N1/N2接口管理器
class N1N2InterfaceManager : public N1InterfaceHandler, public N2InterfaceHandler {
public:
    static N1N2InterfaceManager& getInstance();
    
    // N1接口实现
    bool sendN1Message(const N1Message& message) override;
    void handleN1Message(const N1Message& message) override;
    
    // N2接口实现
    bool sendN2Message(const N2Message& message) override;
    void handleN2Message(const N2Message& message) override;
    
    // 启动N1/N2接口服务
    bool startN1N2Service(const std::string& bindAddress, int n2Port);
    void stopN1N2Service();
    
    // 连接管理
    bool isRanNodeConnected(const std::string& ranNodeId) const;
    std::vector<std::string> getConnectedRanNodes() const;
    
    // 统计信息
    size_t getN1MessageCount() const { return n1MessageCount_; }
    size_t getN2MessageCount() const { return n2MessageCount_; }

private:
    N1N2InterfaceManager() = default;
    
    // 连接的RAN节点
    std::map<std::string, bool> connectedRanNodes_;
    
    // 消息统计
    std::atomic<size_t> n1MessageCount_{0};
    std::atomic<size_t> n2MessageCount_{0};
    
    // 服务状态
    bool serviceRunning_{false};
    std::string bindAddress_;
    int n2Port_;
    
    mutable std::mutex interfaceMutex_;
};

// 辅助函数
std::string n1MessageTypeToString(N1MessageType type);
std::string n2MessageTypeToString(N2MessageType type);
N1MessageType stringToN1MessageType(const std::string& str);
N2MessageType stringToN2MessageType(const std::string& str);

} // namespace amf_sm

#endif // N1N2_INTERFACE_H