#ifndef AMF_SM_H
#define AMF_SM_H

#include <iostream>
#include <memory>
#include <vector>
#include <functional>
#include <map>
#include <atomic>
#include <thread>
#include <mutex>
#include <string>
#include <unordered_map>
#include "SbiMessage.h"
#include "UeContext.h"
#include "N1N2Interface.h"
#include "NfManagement.h"
#include "AmfConfiguration.h"

namespace amf_sm {

// AMF States for a UE
enum class UeState {
    DEREGISTERED,
    REGISTERED_IDLE,
    REGISTERED_CONNECTED
};

// Events that can trigger state transitions
enum class UeEvent {
    // Registration Management Events
    REGISTRATION_REQUEST,
    REGISTRATION_ACCEPT,
    REGISTRATION_REJECT,
    DEREGISTER_REQUEST,
    DEREGISTER_ACCEPT,
    
    // Connection Management Events
    SERVICE_REQUEST,
    SERVICE_ACCEPT,
    SERVICE_REJECT,
    AN_RELEASE,              // Access Network Release
    CONNECTION_RELEASE,
    PAGING_REQUEST,
    PAGING_RESPONSE,
    PAGING_FAILURE,
    
    // Mobility Management Events
    HANDOVER_REQUEST,
    HANDOVER_COMMAND,
    HANDOVER_COMPLETE,
    HANDOVER_FAILURE,
    TRACKING_AREA_UPDATE,
    PERIODIC_REGISTRATION_UPDATE,
    
    // Security Events
    AUTHENTICATION_REQUEST,
    AUTHENTICATION_RESPONSE,
    AUTHENTICATION_FAILURE,
    SECURITY_MODE_COMMAND,
    SECURITY_MODE_COMPLETE,
    SECURITY_MODE_REJECT,
    
    // Session Management Events
    PDU_SESSION_ESTABLISHMENT_REQUEST,
    PDU_SESSION_ESTABLISHMENT_ACCEPT,
    PDU_SESSION_ESTABLISHMENT_REJECT,
    PDU_SESSION_MODIFICATION_REQUEST,
    PDU_SESSION_RELEASE_REQUEST,
    
    // Error and Timeout Events
    NETWORK_FAILURE,
    TIMEOUT_T3510,           // Registration timer
    TIMEOUT_T3511,           // Deregistration timer
    TIMEOUT_T3513,           // Paging timer
    TIMEOUT_T3560,           // Authentication timer
    
    // Configuration Management Events
    CONFIGURATION_UPDATE_COMMAND,
    CONFIGURATION_UPDATE_COMPLETE,
    
    // Emergency Events
    EMERGENCY_REGISTRATION,
    EMERGENCY_SERVICE_REQUEST
};

// AMF统计信息
struct AmfStatistics {
    int totalUeRegistrations;
    int activeUeConnections;
    int totalUeContexts;
    int totalPduSessions;
    int activePduSessions;
    int totalHandovers;
    int totalAuthenticationAttempts;
    int successfulAuthentications;
    int totalSbiMessages;
    int totalN1Messages;
    int totalN2Messages;
    double averageResponseTime;
    int systemLoad;
    int memoryUsage;
    int cpuUsage;
    int registeredNfInstances;
    int healthyNfInstances;
};

class AmfSm : public SbiMessageHandler, public N1InterfaceHandler, public N2InterfaceHandler {
public:
    AmfSm();
    ~AmfSm();
    
    // 初始化和配置
    bool initialize(const AmfConfiguration& config);
    void shutdown();
    
    // 传统事件处理
    void handleEvent(UeEvent event);
    UeState getCurrentState() const;
    
    // SBI消息处理接口实现
    void handleSbiMessage(std::shared_ptr<SbiMessage> message) override;
    void sendSbiMessage(std::shared_ptr<SbiMessage> message);
    void registerSbiCallback(SbiMessageType msgType, std::function<void(std::shared_ptr<SbiMessage>)> callback);
    
    // N1接口实现 (AMF <-> UE)
    bool sendN1Message(const N1Message& message) override;
    void handleN1Message(const N1Message& message) override;
    
    // N2接口实现 (AMF <-> gNB)
    bool sendN2Message(const N2Message& message) override;
    void handleN2Message(const N2Message& message) override;
    
    // UE上下文管理
    std::shared_ptr<UeContext> createUeContext(const std::string& supi);
    std::shared_ptr<UeContext> getUeContext(const std::string& supi);
    bool removeUeContext(const std::string& supi);
    
    // 注册管理
    bool processRegistrationRequest(const std::string& supi, const std::string& registrationType);
    bool processDeregistrationRequest(const std::string& supi, const std::string& deregCause);
    
    // 认证管理
    bool initiateAuthentication(const std::string& supi);
    bool processAuthenticationResponse(const std::string& supi, const std::string& authResponse);
    
    // 会话管理
    bool createPduSession(const std::string& supi, int sessionId, const std::string& dnn);
    bool releasePduSession(const std::string& supi, int sessionId);
    
    // 移动性管理
    bool processHandoverRequest(const std::string& supi, const std::string& targetRanId);
    bool initiateHandover(const std::string& supi, const std::string& targetGnbId, const std::string& targetCell);
    bool completeHandover(const std::string& supi, const std::string& gnbId);
    bool processTrackingAreaUpdate(const std::string& supi, const std::string& newTai);
    void processTrackingAreaUpdate();
    void processPeriodicRegistrationUpdate();
    bool processPeriodicRegistrationUpdate(const std::string& supi);
    void processConnectionRelease();
    bool processConnectionRelease(const std::string& supi, const std::string& releaseReason);
    void processPagingRequest();
    void processPagingRequest(const std::string& supi, const std::string& pagingCause);
    void processServiceRequest();
    void processServiceRequest(const std::string& supi, const std::string& serviceType);
    
    // 网络切片管理
    bool validateNetworkSlice(const std::string& snssai);
    std::vector<std::string> getSupportedSlices() const;
    std::vector<NetworkSlice> getAllowedSlices(const std::string& supi);
    
    // 负载均衡
    bool checkLoadBalance();
    int calculateCurrentLoad();
    
    // 负载均衡和NF发现
    std::shared_ptr<NfInstance> selectSmfForSession(const std::string& dnn, const NetworkSlice& slice);
    std::shared_ptr<NfInstance> selectAusfForAuthentication();
    std::vector<NfInstance> discoverNf(NfType nfType, const std::string& serviceName);
    NfInstance* selectBestNf(NfType nfType, const std::string& serviceName);
    
    // 事件订阅和通知
    bool subscribeToEvents(const std::string& nfInstanceId, const std::vector<std::string>& eventTypes);
    void subscribeToEvents(const std::string& eventType, std::function<void(const std::string&)> callback);
    void notifyEvent(const std::string& eventType, const std::string& ueId, const std::string& eventData);
    void notifyEvent(const std::string& eventType, const std::string& eventData);
    
    // 统计和监控
    AmfStatistics getStatistics() const;
    void updateStatistics();
    std::string getStatusReport() const;
    
    // 配置管理
    const AmfConfiguration& getConfiguration() const { return config_; }
    bool updateConfiguration(const AmfConfiguration& newConfig);
    
    // 健康检查
    bool performHealthCheck();
    std::string getHealthStatus() const;

private:
    // 配置信息
    AmfConfiguration config_;
    
    // 当前状态（兼容性保持）
    UeState currentState_;
    
    // 统计信息
    AmfStatistics statistics_;
    
    // 服务状态
    bool isInitialized_{false};
    bool isRunning_{false};
    
    // 线程安全
    mutable std::mutex amfMutex_;
    
    // 待处理的SBI消息
    std::vector<std::shared_ptr<SbiMessage>> pendingMessages_;
    
    // SBI消息回调
    std::map<SbiMessageType, std::function<void(std::shared_ptr<SbiMessage>)>> sbiCallbacks_;
    
    // 事件订阅
    std::unordered_map<std::string, std::function<void(const std::string&)>> eventSubscriptions_;
    
    // 组件管理器
    UeContextManager* ueContextManager_;  // 单例，不需要unique_ptr
    N1N2InterfaceManager* n1n2InterfaceManager_;  // 单例，不需要unique_ptr
    NfManager* nfManager_;  // 单例，不需要unique_ptr
    AmfNfInstance* amfNfInstance_;  // 单例，不需要unique_ptr
    
    // 监控线程
    std::thread monitoringThread_;
    bool monitoringRunning_{false};
    
    // SBI消息处理辅助方法
    void processSbiRequest(std::shared_ptr<SbiMessage> message);
    void processSbiResponse(std::shared_ptr<SbiMessage> message);
    void handleRegistrationSbiMessages(std::shared_ptr<SbiMessage> message);
    void handleSessionSbiMessages(std::shared_ptr<SbiMessage> message);
    void handleAuthenticationSbiMessages(std::shared_ptr<SbiMessage> message);
    void handlePolicyControlSbiMessages(std::shared_ptr<SbiMessage> message);
    void handleEventExposureSbiMessages(std::shared_ptr<SbiMessage> message);
    void handleNfManagementSbiMessages(std::shared_ptr<SbiMessage> message);
    
    // N1消息处理辅助方法
    void processRegistrationRequestN1(const N1Message& message);
    void processServiceRequestN1(const N1Message& message);
    void processAuthenticationResponseN1(const N1Message& message);
    void processSecurityModeCompleteN1(const N1Message& message);
    
    // N2消息处理辅助方法
    void processInitialContextSetupResponseN2(const N2Message& message);
    void processUeContextReleaseRequestN2(const N2Message& message);
    void processHandoverRequiredN2(const N2Message& message);
    void processPathSwitchRequestN2(const N2Message& message);
    
    // 创建响应消息
    std::shared_ptr<SbiMessage> createSbiResponse(std::shared_ptr<SbiMessage> request, 
                                                 SbiMessageType responseType, 
                                                 int statusCode = 200);
    
    N1Message createN1Response(const N1Message& request, N1MessageType responseType);
    N2Message createN2Response(const N2Message& request, N2MessageType responseType);
    
    // 状态转换（兼容性保持）
    void transitionTo(UeState newState);
    
    // 状态处理函数
    void handleDeregisteredState(UeEvent event);
    void handleRegisteredIdleState(UeEvent event);
    void handleRegisteredConnectedState(UeEvent event);
    
    // 辅助方法
    std::string generateGuti(const std::string& supi);
    bool validateUeIdentity(const std::string& supi);
    void logMessage(const std::string& level, const std::string& message);
    void monitoringLoop();
    void updateResponseTimeStatistics(double responseTime);
    void registerOtherNfInstances();
    void startMonitoring();
    void stopMonitoring();
};

} // namespace amf_sm

#endif // AMF_SM_H