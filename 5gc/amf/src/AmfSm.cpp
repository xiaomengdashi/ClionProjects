#include "AmfSm.h"
#include <iostream>
#include <sstream>
#include <algorithm>

namespace amf_sm {

AmfSm::AmfSm() : currentState_(UeState::DEREGISTERED), isInitialized_(false) {
    std::cout << "AMF state machine created. Initial state: DEREGISTERED" << std::endl;
    
    // 初始化统计信息
    statistics_.totalUeRegistrations = 0;
    statistics_.activeUeConnections = 0;
    statistics_.totalPduSessions = 0;
    statistics_.totalHandovers = 0;
    statistics_.totalAuthenticationAttempts = 0;
    statistics_.successfulAuthentications = 0;
    statistics_.totalSbiMessages = 0;
    statistics_.totalN1Messages = 0;
    statistics_.totalN2Messages = 0;
    statistics_.averageResponseTime = 0.0;
    statistics_.systemLoad = 0;
    statistics_.memoryUsage = 0;
    statistics_.cpuUsage = 0;
}

AmfSm::~AmfSm() {
    shutdown();
}

bool AmfSm::initialize(const AmfConfiguration& config) {
    std::lock_guard<std::mutex> lock(amfMutex_);
    
    if (isInitialized_) {
        std::cerr << "AMF SM is already initialized" << std::endl;
        return false;
    }
    
    config_ = config;
    
    // 初始化UE上下文管理器 (使用单例)
    ueContextManager_ = &UeContextManager::getInstance();
    
    // 初始化N1/N2接口管理器 (使用单例)
    n1n2InterfaceManager_ = &N1N2InterfaceManager::getInstance();
    if (!n1n2InterfaceManager_->startN1N2Service(config_.n1n2BindAddress, config_.n2Port)) {
        std::cerr << "Failed to start N1/N2 interface service" << std::endl;
        return false;
    }
    
    // 初始化NF管理器 (使用单例)
    nfManager_ = &NfManager::getInstance();
    if (!nfManager_) {
        std::cerr << "Failed to get NF Manager instance" << std::endl;
        return false;
    }
    
    // 初始化AMF NF实例 (使用单例)
    amfNfInstance_ = &AmfNfInstance::getInstance();
    if (!amfNfInstance_->initialize(config_.amfInstanceId, config_.plmnId)) {
        std::cerr << "Failed to initialize AMF NF Instance" << std::endl;
        return false;
    }
    
    if (!amfNfInstance_->registerWithNrf(config_.nrfUri)) {
        std::cerr << "Failed to register AMF to NRF" << std::endl;
        return false;
    }
    
    // 注册其他NF实例（模拟）
    registerOtherNfInstances();
    
    // 启动监控线程
    startMonitoring();
    
    isInitialized_ = true;
    std::cout << "AMF initialized successfully with instance ID: " << config_.amfInstanceId << std::endl;
    
    return true;
}

void AmfSm::shutdown() {
    std::lock_guard<std::mutex> lock(amfMutex_);
    
    if (!isInitialized_) {
        std::cerr << "AMF SM is not initialized" << std::endl;
        return;
    }
    
    std::cout << "Shutting down AMF..." << std::endl;
    
    // 停止监控
    stopMonitoring();
    
    // 注销NF实例
    if (amfNfInstance_) {
        // 使用NfManager的deregisterNfInstance方法
        if (nfManager_) {
            nfManager_->deregisterNfInstance(amfNfInstance_->getAmfInstance().nfInstanceId);
        }
        amfNfInstance_->stopHeartbeatService();
    }
    
    // 停止NF管理器 - 不需要显式停止，因为它是单例
    
    // 停止N1N2接口管理器
    if (n1n2InterfaceManager_) {
        n1n2InterfaceManager_->stopN1N2Service();
    }
    
    isRunning_ = false;
    isInitialized_ = false;
    std::cout << "AMF SM shutdown successfully" << std::endl;
}

void AmfSm::handleEvent(UeEvent event) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    switch (currentState_) {
        case UeState::DEREGISTERED:
            handleDeregisteredState(event);
            break;
        case UeState::REGISTERED_IDLE:
            handleRegisteredIdleState(event);
            break;
        case UeState::REGISTERED_CONNECTED:
            handleRegisteredConnectedState(event);
            break;
    }
    
    // 更新响应时间统计
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    updateResponseTimeStatistics(duration.count() / 1000.0); // 转换为毫秒
}

// 添加缺失的处理函数
void AmfSm::handleDeregisteredState(UeEvent event) {
    switch (event) {
        case UeEvent::REGISTRATION_REQUEST:
            processRegistrationRequest("unknown_supi", "normal");
            transitionTo(UeState::REGISTERED_CONNECTED);
            statistics_.totalUeRegistrations++;
            break;
        case UeEvent::EMERGENCY_REGISTRATION:
            processRegistrationRequest("unknown_supi", "emergency");
            transitionTo(UeState::REGISTERED_CONNECTED);
            statistics_.totalUeRegistrations++;
            break;
        case UeEvent::REGISTRATION_REJECT:
        case UeEvent::AUTHENTICATION_FAILURE:
        case UeEvent::SECURITY_MODE_REJECT:
        case UeEvent::NETWORK_FAILURE:
            std::cout << "Registration failed, staying in DEREGISTERED state" << std::endl;
            break;
        default:
            std::cout << "Event not handled in DEREGISTERED state: " << static_cast<int>(event) << std::endl;
            break;
    }
}

bool AmfSm::processDeregistrationRequest(const std::string& supi, const std::string& deregCause) {
    std::cout << "Processing deregistration request for SUPI: " << supi 
              << ", cause: " << deregCause << std::endl;
    
    // 获取UE上下文
    auto ueContext = getUeContext(supi);
    if (!ueContext) {
        std::cerr << "UE context not found for SUPI: " << supi << std::endl;
        return false;
    }
    
    // 更新UE上下文信息
    ueContext->setRegistrationState("DEREGISTERED");
    ueContext->setConnectionState("IDLE");
    
    // 更新统计信息
    statistics_.activeUeConnections = std::max(0, statistics_.activeUeConnections - 1);
    
    return true;
}

bool AmfSm::processHandoverRequest(const std::string& supi, const std::string& targetRanId) {
    std::cout << "Processing handover request for SUPI: " << supi 
              << ", target RAN: " << targetRanId << std::endl;
    
    // 获取UE上下文
    auto ueContext = getUeContext(supi);
    if (!ueContext) {
        std::cerr << "UE context not found for SUPI: " << supi << std::endl;
        return false;
    }
    
    // 更新UE上下文信息
    // 使用公共访问方法设置RAN节点ID
    ueContext->setRanNodeId(targetRanId);
    
    // 更新统计信息
    statistics_.totalHandovers++;
    
    return true;
}
bool AmfSm::completeHandover(const std::string& supi, const std::string& gnbId) {
    std::cout << "Completing handover for SUPI: " << supi 
              << ", gNB: " << gnbId << std::endl;
    
    // 获取UE上下文
    auto ueContext = getUeContext(supi);
    if (!ueContext) {
        std::cerr << "UE context not found for SUPI: " << supi << std::endl;
        return false;
    }
    
    // 更新UE上下文信息
    ueContext->setRanNodeId(gnbId);
    
    return true;
}

bool AmfSm::createPduSession(const std::string& supi, int sessionId, const std::string& dnn) {
    std::cout << "Creating PDU session for SUPI: " << supi 
              << ", session ID: " << sessionId 
              << ", DNN: " << dnn << std::endl;
    
    // 获取UE上下文
    auto ueContext = getUeContext(supi);
    if (!ueContext) {
        std::cerr << "UE context not found for SUPI: " << supi << std::endl;
        return false;
    }
    
    // 创建PDU会话
    PduSession session;
    session.sessionId = sessionId;
    session.dnn = dnn;
    session.state = "ACTIVE";
    
    // 添加会话到UE上下文
    ueContext->addPduSession(session);
    
    // 更新统计信息
    statistics_.totalPduSessions++;
    
    return true;
}

bool AmfSm::releasePduSession(const std::string& supi, int sessionId) {
    std::cout << "Releasing PDU session for SUPI: " << supi 
              << ", session ID: " << sessionId << std::endl;
    
    // 获取UE上下文
    auto ueContext = getUeContext(supi);
    if (!ueContext) {
        std::cerr << "UE context not found for SUPI: " << supi << std::endl;
        return false;
    }
    
    // 释放PDU会话
    ueContext->removePduSession(sessionId);
    
    return true;
}

bool AmfSm::initiateAuthentication(const std::string& supi) {
    std::cout << "Initiating authentication for SUPI: " << supi << std::endl;
    
    // 获取UE上下文
    auto ueContext = getUeContext(supi);
    if (!ueContext) {
        std::cerr << "UE context not found for SUPI: " << supi << std::endl;
        return false;
    }
    
    // 更新统计信息
    statistics_.totalAuthenticationAttempts++;
    
    return true;
}

bool AmfSm::processAuthenticationResponse(const std::string& supi, const std::string& authResponse) {
    std::cout << "Processing authentication response for SUPI: " << supi << std::endl;
    
    // 获取UE上下文
    auto ueContext = getUeContext(supi);
    if (!ueContext) {
        std::cerr << "UE context not found for SUPI: " << supi << std::endl;
        return false;
    }
    
    // 更新统计信息
    statistics_.successfulAuthentications++;
    
    return true;
}

bool AmfSm::processTrackingAreaUpdate(const std::string& supi, const std::string& newTai) {
    std::cout << "Processing tracking area update for SUPI: " << supi 
              << ", new TAI: " << newTai << std::endl;
    
    // 获取UE上下文
    auto ueContext = getUeContext(supi);
    if (!ueContext) {
        std::cerr << "UE context not found for SUPI: " << supi << std::endl;
        return false;
    }
    
    // 更新UE上下文信息
    ueContext->setTai(newTai);
    
    return true;
}

// 实现必要的辅助函数
bool AmfSm::processRegistrationRequest(const std::string& supi, const std::string& registrationType) {
    std::cout << "Processing registration request for SUPI: " << supi 
              << ", type: " << registrationType << std::endl;
    
    // 创建或获取UE上下文
    auto ueContext = getUeContext(supi);
    if (!ueContext) {
        ueContext = createUeContext(supi);
        if (!ueContext) {
            std::cerr << "Failed to create UE context for SUPI: " << supi << std::endl;
            return false;
        }
    }
    
    // 更新UE上下文信息
    // 这里需要使用公共访问方法，因为私有成员变量不能直接访问
    ueContext->setRegistrationState("REGISTERED");
    ueContext->setConnectionState("CONNECTED");
    
    // 更新统计信息
    statistics_.totalUeRegistrations++;
    statistics_.activeUeConnections++;
    
    return true;
}

void AmfSm::handleRegisteredIdleState(UeEvent event) {
    switch (event) {
        case UeEvent::SERVICE_REQUEST:
        case UeEvent::EMERGENCY_SERVICE_REQUEST:
        case UeEvent::PAGING_RESPONSE:
            // 处理服务请求
            std::cout << "Processing service request..." << std::endl;
            transitionTo(UeState::REGISTERED_CONNECTED);
            break;
        case UeEvent::PDU_SESSION_ESTABLISHMENT_REQUEST:
            // 创建PDU会话
            createPduSession("unknown_supi", 1, "internet");
            transitionTo(UeState::REGISTERED_CONNECTED);
            statistics_.totalPduSessions++;
            break;
        case UeEvent::HANDOVER_REQUEST:
            // 处理切换请求
            processHandoverRequest("unknown_supi", "gnb-001");
            transitionTo(UeState::REGISTERED_CONNECTED);
            statistics_.totalHandovers++;
            break;
        case UeEvent::DEREGISTER_REQUEST:
        case UeEvent::TIMEOUT_T3511:
        case UeEvent::NETWORK_FAILURE:
            processDeregistrationRequest("unknown_supi", "user_request");
            transitionTo(UeState::DEREGISTERED);
            break;
        case UeEvent::TRACKING_AREA_UPDATE:
            processTrackingAreaUpdate("unknown_supi", "tai_001");
            break;
        case UeEvent::PERIODIC_REGISTRATION_UPDATE:
            // 处理周期性注册更新
            processRegistrationRequest("unknown_supi", "periodic");
            break;
        case UeEvent::PAGING_REQUEST:
            // 处理寻呼请求
            std::cout << "Processing paging request for UE..." << std::endl;
            break;
        default:
            std::cout << "Event not handled in REGISTERED_IDLE state: " << static_cast<int>(event) << std::endl;
            break;
    }
}

void AmfSm::handleRegisteredConnectedState(UeEvent event) {
    switch (event) {
        case UeEvent::AN_RELEASE:
        case UeEvent::CONNECTION_RELEASE:
            // 释放连接，转换到IDLE状态
            std::cout << "Releasing UE connection..." << std::endl;
            transitionTo(UeState::REGISTERED_IDLE);
            break;
        case UeEvent::HANDOVER_COMPLETE:
            // 完成切换过程
            completeHandover("unknown_supi", "gnb-001");
            transitionTo(UeState::REGISTERED_IDLE);
            break;
        case UeEvent::PDU_SESSION_RELEASE_REQUEST:
            // 释放PDU会话
            releasePduSession("unknown_supi", 1);
            transitionTo(UeState::REGISTERED_IDLE);
            break;
        case UeEvent::DEREGISTER_REQUEST:
        case UeEvent::DEREGISTER_ACCEPT:
        case UeEvent::NETWORK_FAILURE:
        case UeEvent::AUTHENTICATION_FAILURE:
            processDeregistrationRequest("unknown_supi", "user_request");
            transitionTo(UeState::DEREGISTERED);
            break;
        case UeEvent::AUTHENTICATION_REQUEST:
            // 发起认证过程
            initiateAuthentication("unknown_supi");
            statistics_.totalAuthenticationAttempts++;
            break;
        case UeEvent::AUTHENTICATION_RESPONSE:
            // 处理认证响应
            processAuthenticationResponse("unknown_supi", "auth_response_data");
            statistics_.successfulAuthentications++;
            break;
        case UeEvent::SECURITY_MODE_COMMAND:
            // 安全模式命令处理
            std::cout << "Processing security mode command..." << std::endl;
            break;
        case UeEvent::SECURITY_MODE_COMPLETE:
            // 安全模式完成处理
            std::cout << "Processing security mode complete..." << std::endl;
            break;
        default:
            std::cout << "Event not handled in REGISTERED_CONNECTED state: " << static_cast<int>(event) << std::endl;
            break;
    }
}

UeState AmfSm::getCurrentState() const {
    return currentState_;
}

void AmfSm::transitionTo(UeState newState) {
    if (currentState_ != newState) {
        std::cout << "State transition from " << static_cast<int>(currentState_) 
                  << " to " << static_cast<int>(newState) << std::endl;
        
        // 更新连接统计
        if (newState == UeState::REGISTERED_CONNECTED) {
            statistics_.activeUeConnections++;
        } else if (currentState_ == UeState::REGISTERED_CONNECTED) {
            statistics_.activeUeConnections = std::max(0, statistics_.activeUeConnections - 1);
        }
        
        currentState_ = newState;
    }
}

// UE上下文管理
std::shared_ptr<UeContext> AmfSm::createUeContext(const std::string& supi) {
    if (!ueContextManager_) return nullptr;
    
    auto context = ueContextManager_->createUeContext(supi);
    if (context) {
        std::cout << "UE context created for SUPI: " << supi << std::endl;
    }
    return context;
}

// 注意：UeContextManager中没有updateUeContext方法，需要通过getUeContext获取后更新
// 这个函数在AmfSm.h中没有声明，可以删除或者添加声明
/*
bool AmfSm::updateUeContext(const std::string& supi, const UeContext& context) {
    if (!ueContextManager_) return false;
    
    auto ueContext = ueContextManager_->getUeContext(supi);
    if (ueContext) {
        // 更新UE上下文的各个字段
        // ...
        std::cout << "UE context updated for SUPI: " << supi << std::endl;
        return true;
    }
    return false;
}
*/

bool AmfSm::removeUeContext(const std::string& supi) {
    if (!ueContextManager_) return false;
    
    ueContextManager_->removeUeContext(supi);
    std::cout << "UE context removed for SUPI: " << supi << std::endl;
    return true;
}

std::shared_ptr<UeContext> AmfSm::getUeContext(const std::string& supi) {
    if (!ueContextManager_) return nullptr;
    return ueContextManager_->getUeContext(supi);
}



// 这个函数在AmfSm.h中没有声明，暂时注释掉
/*
void AmfSm::processSecurityModeCommand() {
    std::cout << "Processing security mode command..." << std::endl;
    
    // 发送N1消息到UE
    if (n1n2InterfaceManager_) {
        N1Message n1Msg(N1MessageType::SECURITY_MODE_COMMAND, "imsi-460001234567890");
        n1Msg.payload = "{\"securityAlgorithms\":[\"5G-EA1\",\"5G-IA1\"]}";
        n1n2InterfaceManager_->sendN1Message(n1Msg);
        statistics_.totalN1Messages++;
    }
}
*/

// 这个函数在AmfSm.h中没有声明，暂时注释掉
/*
void AmfSm::processSecurityModeComplete() {
    std::cout << "Processing security mode complete..." << std::endl;
}
*/





void AmfSm::processPeriodicRegistrationUpdate() {
    std::cout << "Processing periodic registration update..." << std::endl;
}

bool AmfSm::processPeriodicRegistrationUpdate(const std::string& supi) {
    std::cout << "Processing periodic registration update for SUPI: " << supi << std::endl;
    
    // 获取UE上下文
    auto ueContext = getUeContext(supi);
    if (!ueContext) {
        std::cout << "UE context not found for SUPI: " << supi << std::endl;
        return false;
    }
    
    // 更新UE上下文中的注册时间戳
    ueContext->updateLastActivity();
    
    // 发送N1消息确认注册更新
    if (n1n2InterfaceManager_) {
        N1Message n1Msg;
        n1Msg.messageType = N1MessageType::REGISTRATION_ACCEPT;
        n1Msg.ueId = supi;
        n1Msg.ieList["registrationType"] = "periodic-updating";
        sendN1Message(n1Msg);
    }
    
    return true;
}

// 连接管理
void AmfSm::processServiceRequest() {
    std::cout << "Processing service request..." << std::endl;
    
    // 发送N2消息到gNB建立上下文
    if (n1n2InterfaceManager_) {
        N2Message n2Msg;
        n2Msg.messageType = N2MessageType::INITIAL_CONTEXT_SETUP_REQUEST;
        n2Msg.ranNodeId = "gnb-001";
        n2Msg.ieList["ueContext"] = "context-data";
        sendN2Message(n2Msg);
    }
}

void AmfSm::processServiceRequest(const std::string& supi, const std::string& serviceType) {
    std::cout << "Processing service request for SUPI: " << supi 
              << ", service type: " << serviceType << std::endl;
    
    // 发送N2消息到gNB建立上下文
    if (n1n2InterfaceManager_) {
        N2Message n2Msg;
        n2Msg.messageType = N2MessageType::INITIAL_CONTEXT_SETUP_REQUEST;
        n2Msg.ranNodeId = "gnb-001";
        n2Msg.ieList["ueId"] = supi;
        n2Msg.ieList["serviceType"] = serviceType;
        sendN2Message(n2Msg);
    }
}

void AmfSm::processConnectionRelease() {
    std::cout << "Processing connection release..." << std::endl;
    
    // 发送N2消息到gNB释放上下文
    if (n1n2InterfaceManager_) {
        N2Message n2Msg;
        n2Msg.messageType = N2MessageType::UE_CONTEXT_RELEASE_COMMAND;
        n2Msg.ranNodeId = "gnb-001";
        n2Msg.ieList["releaseReason"] = "normal";
        sendN2Message(n2Msg);
    }
}

bool AmfSm::processConnectionRelease(const std::string& supi, const std::string& releaseReason) {
    std::cout << "Processing connection release for SUPI: " << supi 
              << ", reason: " << releaseReason << std::endl;
    
    // 获取UE上下文
    auto ueContext = getUeContext(supi);
    if (!ueContext) {
        std::cout << "UE context not found for SUPI: " << supi << std::endl;
        return false;
    }
    
    // 更新UE上下文状态
    ueContext->setConnectionState("IDLE");
    
    // 发送N2消息到gNB释放上下文
    if (n1n2InterfaceManager_) {
        N2Message n2Msg;
        n2Msg.messageType = N2MessageType::UE_CONTEXT_RELEASE_COMMAND;
        n2Msg.ranNodeId = ueContext->getAccessInfo().ranNodeId;
        n2Msg.ieList["ueId"] = supi;
        n2Msg.ieList["releaseReason"] = releaseReason;
        sendN2Message(n2Msg);
    }
    
    return true;
}

void AmfSm::processPagingRequest() {
    std::cout << "Processing paging request..." << std::endl;
    
    // 发送N2消息到gNB进行寻呼
    if (n1n2InterfaceManager_) {
        N2Message n2Msg;
        n2Msg.messageType = N2MessageType::PAGING;
        n2Msg.ranNodeId = "gnb-001";
        n2Msg.ieList["pagingCause"] = "mt-call";
        sendN2Message(n2Msg);
    }
}

void AmfSm::processPagingRequest(const std::string& supi, const std::string& pagingCause) {
    std::cout << "Processing paging request for SUPI: " << supi 
              << ", cause: " << pagingCause << std::endl;
    
    // 发送N2消息到gNB进行寻呼
    if (n1n2InterfaceManager_) {
        N2Message n2Msg;
        n2Msg.messageType = N2MessageType::PAGING;
        n2Msg.ranNodeId = "gnb-001"; // 实际应该根据UE的位置信息选择合适的gNB
        n2Msg.ieList["ueId"] = supi;
        n2Msg.ieList["pagingCause"] = pagingCause;
        n2Msg.ieList["pagingPriority"] = "normal";
        sendN2Message(n2Msg);
    }
}

// 网络切片管理
bool AmfSm::validateNetworkSlice(const std::string& snssai) {
    // 检查是否支持该网络切片
    auto it = std::find(config_.supportedSlices.begin(), 
                       config_.supportedSlices.end(), snssai);
    return it != config_.supportedSlices.end();
}

std::vector<std::string> AmfSm::getSupportedSlices() const {
    return config_.supportedSlices;
}

std::vector<NetworkSlice> AmfSm::getAllowedSlices(const std::string& supi) {
    std::vector<NetworkSlice> allowedSlices;
    
    // 获取UE上下文
    auto ueContext = getUeContext(supi);
    if (!ueContext) {
        std::cerr << "UE context not found for SUPI: " << supi << std::endl;
        return allowedSlices;
    }
    
    // 获取UE订阅的网络切片
    // 使用公共访问方法获取订阅的切片
    auto subscribedSlices = ueContext->getSubscriptionInfo().subscribedSlices;
    
    // 获取AMF支持的网络切片
    auto supportedSlices = getSupportedSlices();
    
    // 筛选出AMF支持的网络切片作为允许的切片
    for (const auto& slice : subscribedSlices) {
        if (std::find(supportedSlices.begin(), supportedSlices.end(), slice.snssai) != supportedSlices.end()) {
            allowedSlices.push_back(slice);
        }
    }
    
    return allowedSlices;
}

// 负载均衡
bool AmfSm::checkLoadBalance() {
    int currentLoad = calculateCurrentLoad();
    return currentLoad < config_.loadBalanceThreshold;
}

int AmfSm::calculateCurrentLoad() {
    // 简单的负载计算：基于活跃连接数
    int maxConnections = config_.maxUeConnections;
    if (maxConnections == 0) return 0;
    
    return (statistics_.activeUeConnections * 100) / maxConnections;
}

// NF发现和选择
std::shared_ptr<NfInstance> AmfSm::selectSmfForSession(const std::string& dnn, const NetworkSlice& slice) {
    if (!nfManager_ || !amfNfInstance_) {
        std::cerr << "NF Manager or AMF NF Instance not initialized" << std::endl;
        return nullptr;
    }
    
    // 创建NF发现查询
    NfDiscoveryQuery query;
    query.targetNfType = NfType::SMF;
    query.requesterNfType = nfTypeToString(NfType::AMF);
    query.requesterNfInstanceId = amfNfInstance_->getAmfInstance().nfInstanceId;
    query.dnn = dnn;
    query.snssai = slice.snssai;
    
    // 发现SMF实例
    auto smfInstances = nfManager_->discoverNfInstances(query);
    if (smfInstances.empty()) {
        std::cerr << "No SMF instances found for DNN: " << dnn << " and S-NSSAI: " << slice.snssai << std::endl;
        return nullptr;
    }
    
    // 选择最佳SMF实例
    return nfManager_->selectNfInstance(NfType::SMF, amfNfInstance_->getAmfInstance().plmnId);
}

std::shared_ptr<NfInstance> AmfSm::selectAusfForAuthentication() {
    if (!nfManager_ || !amfNfInstance_) {
        std::cerr << "NF Manager or AMF NF Instance not initialized" << std::endl;
        return nullptr;
    }
    
    // 创建NF发现查询
    NfDiscoveryQuery query;
    query.targetNfType = NfType::AUSF;
    query.requesterNfType = nfTypeToString(NfType::AMF);
    query.requesterNfInstanceId = amfNfInstance_->getAmfInstance().nfInstanceId;
    
    // 发现AUSF实例
    auto ausfInstances = nfManager_->discoverNfInstances(query);
    if (ausfInstances.empty()) {
        std::cerr << "No AUSF instances found" << std::endl;
        return nullptr;
    }
    
    // 选择最佳AUSF实例
    return nfManager_->selectNfInstance(NfType::AUSF, amfNfInstance_->getAmfInstance().plmnId);
}

std::vector<NfInstance> AmfSm::discoverNf(NfType nfType, const std::string& serviceName) {
    if (!nfManager_) return {};
    
    std::cout << "Discovering NF instances of type: " << static_cast<int>(nfType) 
              << ", service: " << serviceName << std::endl;
    
    // 创建查询参数
    NfDiscoveryQuery query;
    query.targetNfType = nfType;
    query.serviceName = serviceName;
    query.requesterNfType = nfTypeToString(NfType::AMF);
    query.requesterNfInstanceId = config_.amfInstanceId;
    
    // 调用NfManager的discoverNfInstances接口
    return nfManager_->discoverNfInstances(query);
}

NfInstance* AmfSm::selectBestNf(NfType nfType, const std::string& serviceName) {
    if (!nfManager_) return nullptr;
    
    std::cout << "Selecting best NF instance of type: " << static_cast<int>(nfType) 
              << ", service: " << serviceName << std::endl;
    
    // 使用NfManager的selectNfInstance接口
    auto nfInstance = nfManager_->selectNfInstance(nfType, config_.plmnId);
    if (nfInstance) {
        // 返回指向共享指针内容的原始指针
        return nfInstance.get();
    }
    
    return nullptr;
}

// 事件订阅和通知
bool AmfSm::subscribeToEvents(const std::string& nfInstanceId, const std::vector<std::string>& eventTypes) {
    std::cout << "Subscribing NF instance " << nfInstanceId << " to events" << std::endl;
    for (const auto& eventType : eventTypes) {
        std::cout << "  - Event type: " << eventType << std::endl;
    }
    return true;
}

void AmfSm::subscribeToEvents(const std::string& eventType, 
                             std::function<void(const std::string&)> callback) {
    eventSubscriptions_[eventType] = callback;
    std::cout << "Subscribed to event type: " << eventType << std::endl;
}

void AmfSm::notifyEvent(const std::string& eventType, const std::string& ueId, const std::string& eventData) {
    std::cout << "Notifying event type: " << eventType << " for UE: " << ueId << std::endl;
    std::cout << "Event data: " << eventData << std::endl;
}

void AmfSm::notifyEvent(const std::string& eventType, const std::string& eventData) {
    auto it = eventSubscriptions_.find(eventType);
    if (it != eventSubscriptions_.end()) {
        it->second(eventData);
    }
}

// 统计和监控
AmfStatistics AmfSm::getStatistics() const {
    AmfStatistics stats = statistics_;
    
    // 更新实时统计信息
    if (ueContextManager_) {
        stats.totalUeContexts = ueContextManager_->getRegisteredUeCount();
        stats.activePduSessions = ueContextManager_->getActiveSessionCount();
    }
    
    if (nfManager_) {
        auto nfStats = nfManager_->getNfStatistics();
        // 由于getNfStatistics返回的是std::map<NfType, size_t>，我们需要直接使用NfType枚举
        stats.registeredNfInstances = nfStats.size(); // 总注册的NF实例数量
        stats.healthyNfInstances = 0; // 这里需要根据实际情况计算健康的NF实例
        
        // 如果需要计算健康实例，可以遍历nfStats并根据某些条件判断
        for (const auto& [type, count] : nfStats) {
            if (count > 0) { // 简单示例：假设count > 0的都是健康实例
                stats.healthyNfInstances += count;
            }
        }
    }
    
    return stats;
}

void AmfSm::updateStatistics() {
    if (ueContextManager_) {
        statistics_.totalUeContexts = ueContextManager_->getRegisteredUeCount();
        statistics_.activePduSessions = ueContextManager_->getActiveSessionCount();
    }
    
    if (nfManager_) {
        auto nfStats = nfManager_->getNfStatistics();
        statistics_.registeredNfInstances = nfStats.size(); // 总注册的NF实例数量
        statistics_.healthyNfInstances = 0; // 这里需要根据实际情况计算健康的NF实例
        
        // 如果需要计算健康实例，可以遍历nfStats并根据某些条件判断
        for (const auto& [type, count] : nfStats) {
            if (count > 0) { // 简单示例：假设count > 0的都是健康实例
                statistics_.healthyNfInstances += count;
            }
        }
    }
    
    statistics_.systemLoad = calculateCurrentLoad();
    
    // 模拟系统资源使用情况
    statistics_.memoryUsage = 45 + (rand() % 20); // 45-65%
    statistics_.cpuUsage = 30 + (rand() % 30);    // 30-60%
}

// 配置管理
bool AmfSm::updateConfiguration(const AmfConfiguration& newConfig) {
    config_ = newConfig;
    std::cout << "AMF configuration updated" << std::endl;
    return true;
}

// 注释掉这个函数，因为它已经在头文件中实现为内联函数
// const AmfConfiguration& AmfSm::getConfiguration() const {
//     return config_;
// }

// 健康检查
bool AmfSm::performHealthCheck() {
    if (!isInitialized_) return false;
    
    bool healthy = true;
    
    // 检查各个组件的健康状态
    if (!ueContextManager_) {
        std::cout << "UE Context Manager not available" << std::endl;
        healthy = false;
    }
    
    if (!n1n2InterfaceManager_) {
        std::cout << "N1N2 Interface Manager not available" << std::endl;
        healthy = false;
    }
    
    if (!nfManager_) {
        std::cout << "NF Manager not available" << std::endl;
        healthy = false;
    }
    
    // 检查系统负载
    if (statistics_.systemLoad > 90) {
        std::cout << "System load too high: " << statistics_.systemLoad << "%" << std::endl;
        healthy = false;
    }
    
    return healthy;
}

// SBI消息处理接口实现
void AmfSm::handleSbiMessage(std::shared_ptr<SbiMessage> message) {
    statistics_.totalSbiMessages++;
    
    std::cout << "\n=== Handling SBI Message ===" << std::endl;
    std::cout << message->toString() << std::endl;
    
    if (message->isRequest()) {
        processSbiRequest(message);
    } else if (message->isResponse()) {
        processSbiResponse(message);
    } else {
        std::cout << "Unknown SBI message type" << std::endl;
    }
}

void AmfSm::sendSbiMessage(std::shared_ptr<SbiMessage> message) {
    std::cout << "\n=== Sending SBI Message ===" << std::endl;
    std::cout << message->toString() << std::endl;
    
    message->setStatus(SbiMessageStatus::PENDING);
    pendingMessages_.push_back(message);
    
    // 实际实现应该通过HTTP客户端发送消息
    std::cout << "SBI message sent successfully" << std::endl;
}

void AmfSm::registerSbiCallback(SbiMessageType msgType, 
                               std::function<void(std::shared_ptr<SbiMessage>)> callback) {
    sbiCallbacks_[msgType] = callback;
    std::cout << "Registered SBI callback for message type: " << static_cast<int>(msgType) << std::endl;
}

void AmfSm::processSbiRequest(std::shared_ptr<SbiMessage> message) {
    switch (message->getServiceType()) {
        case SbiServiceType::NAMF_COMMUNICATION:
            handleRegistrationSbiMessages(message);
            break;
        case SbiServiceType::NSMF_PDU_SESSION:
            handleSessionSbiMessages(message);
            break;
        case SbiServiceType::NAUSF_UE_AUTHENTICATION:
        case SbiServiceType::NUDM_UE_AUTHENTICATION:
            handleAuthenticationSbiMessages(message);
            break;
        case SbiServiceType::NPCF_AM_POLICY_CONTROL:
            handlePolicyControlSbiMessages(message);
            break;
        default:
            std::cout << "Unhandled SBI service type in request" << std::endl;
            break;
    }
}

void AmfSm::processSbiResponse(std::shared_ptr<SbiMessage> message) {
    std::cout << "Processing SBI response with status code: " << message->getStatusCode() << std::endl;
    
    auto it = sbiCallbacks_.find(message->getMessageType());
    if (it != sbiCallbacks_.end()) {
        it->second(message);
    }
    
    if (message->isSuccess()) {
        std::cout << "SBI response processed successfully" << std::endl;
    } else {
        std::cout << "SBI response indicates failure" << std::endl;
    }
}

void AmfSm::handleRegistrationSbiMessages(std::shared_ptr<SbiMessage> message) {
    switch (message->getMessageType()) {
        case SbiMessageType::UE_CONTEXT_CREATE_REQUEST:
            std::cout << "Processing UE Context Create Request" << std::endl;
            if (currentState_ == UeState::DEREGISTERED) {
                auto response = createSbiResponse(message, 
                    SbiMessageType::UE_CONTEXT_CREATE_RESPONSE, 201);
                response->setBody("{\"ueContextId\":\"ue-12345\",\"status\":\"created\"}");
                sendSbiMessage(response);
                handleEvent(UeEvent::REGISTRATION_REQUEST);
            } else {
                auto response = createSbiResponse(message, 
                    SbiMessageType::UE_CONTEXT_CREATE_RESPONSE, 409);
                response->setBody("{\"error\":\"UE context already exists\"}");
                sendSbiMessage(response);
            }
            break;
            
        case SbiMessageType::UE_CONTEXT_RELEASE_REQUEST:
        {
            std::cout << "Processing UE Context Release Request" << std::endl;
            auto response = createSbiResponse(message, 
                SbiMessageType::UE_CONTEXT_RELEASE_RESPONSE, 200);
            response->setBody("{\"status\":\"released\"}");
            sendSbiMessage(response);
            handleEvent(UeEvent::DEREGISTER_REQUEST);
            break;
        }
            
        default:
            std::cout << "Unhandled registration SBI message type" << std::endl;
            break;
    }
}

void AmfSm::handleSessionSbiMessages(std::shared_ptr<SbiMessage> message) {
    switch (message->getMessageType()) {
        case SbiMessageType::PDU_SESSION_CREATE_SM_CONTEXT_REQUEST:
            std::cout << "Processing PDU Session Create SM Context Request" << std::endl;
            if (currentState_ == UeState::REGISTERED_IDLE || 
                currentState_ == UeState::REGISTERED_CONNECTED) {
                auto response = createSbiResponse(message, 
                    SbiMessageType::PDU_SESSION_CREATE_SM_CONTEXT_RESPONSE, 201);
                response->setBody("{\"smContextId\":\"sm-67890\",\"status\":\"created\"}");
                sendSbiMessage(response);
                handleEvent(UeEvent::PDU_SESSION_ESTABLISHMENT_REQUEST);
            } else {
                auto response = createSbiResponse(message, 
                    SbiMessageType::PDU_SESSION_CREATE_SM_CONTEXT_RESPONSE, 403);
                response->setBody("{\"error\":\"UE not registered\"}");
                sendSbiMessage(response);
            }
            break;
            
        case SbiMessageType::PDU_SESSION_RELEASE_SM_CONTEXT_REQUEST:
        {
            std::cout << "Processing PDU Session Release SM Context Request" << std::endl;
            auto response = createSbiResponse(message, 
                SbiMessageType::PDU_SESSION_RELEASE_SM_CONTEXT_RESPONSE, 200);
            response->setBody("{\"status\":\"released\"}");
            sendSbiMessage(response);
            handleEvent(UeEvent::PDU_SESSION_RELEASE_REQUEST);
            break;
        }
            
        default:
            std::cout << "Unhandled session SBI message type" << std::endl;
            break;
    }
}

void AmfSm::handleAuthenticationSbiMessages(std::shared_ptr<SbiMessage> message) {
    switch (message->getMessageType()) {
        case SbiMessageType::UE_AUTHENTICATION_REQUEST:
        {
            std::cout << "Processing UE Authentication Request" << std::endl;
            auto response = createSbiResponse(message, 
                SbiMessageType::UE_AUTHENTICATION_RESPONSE, 200);
            response->setBody("{\"authenticationVector\":\"av-12345\",\"status\":\"success\"}");
            sendSbiMessage(response);
            handleEvent(UeEvent::AUTHENTICATION_REQUEST);
            break;
        }
            
        default:
            std::cout << "Unhandled authentication SBI message type" << std::endl;
            break;
    }
}

void AmfSm::handlePolicyControlSbiMessages(std::shared_ptr<SbiMessage> message) {
    switch (message->getMessageType()) {
        case SbiMessageType::AM_POLICY_CONTROL_CREATE_REQUEST:
        {
            std::cout << "Processing AM Policy Control Create Request" << std::endl;
            auto response = createSbiResponse(message, 
                SbiMessageType::AM_POLICY_CONTROL_CREATE_RESPONSE, 201);
            response->setBody("{\"policyId\":\"policy-12345\",\"status\":\"created\"}");
            sendSbiMessage(response);
            break;
        }
            
        default:
            std::cout << "Unhandled policy control SBI message type" << std::endl;
            break;
    }
}

std::shared_ptr<SbiMessage> AmfSm::createSbiResponse(std::shared_ptr<SbiMessage> request, 
                                                   SbiMessageType responseType, 
                                                   int statusCode) {
    auto response = std::make_shared<SbiMessage>(
        request->getServiceType(), responseType, HttpMethod::POST);
    
    response->setStatusCode(statusCode);
    response->setStatus(statusCode >= 200 && statusCode < 300 ? 
                       SbiMessageStatus::SUCCESS : SbiMessageStatus::FAILED);
    
    response->addHeader("Content-Type", "application/json");
    response->addHeader("Location", request->getUri() + "/response");
    
    return response;
}

// N1接口实现
bool AmfSm::sendN1Message(const N1Message& message) {
    if (!n1n2InterfaceManager_) {
        std::cerr << "N1N2 Interface Manager is not initialized" << std::endl;
        return false;
    }
    
    std::cout << "Sending N1 message: " << message.toString() << std::endl;
    bool result = n1n2InterfaceManager_->sendN1Message(message);
    if (result) {
        statistics_.totalN1Messages++;
    }
    
    return result;
}

// N2接口实现
bool AmfSm::sendN2Message(const N2Message& message) {
    if (!n1n2InterfaceManager_) {
        std::cerr << "N1N2 Interface Manager is not initialized" << std::endl;
        return false;
    }
    
    std::cout << "Sending N2 message: " << message.toString() << std::endl;
    bool result = n1n2InterfaceManager_->sendN2Message(message);
    if (result) {
        statistics_.totalN2Messages++;
    }
    
    return result;
}

// N1接口处理
void AmfSm::handleN1Message(const N1Message& message) {
    std::cout << "Handling N1 message: " << message.toString() << std::endl;
    statistics_.totalN1Messages++;
    
    switch (message.messageType) {
        case N1MessageType::REGISTRATION_REQUEST:
            handleEvent(UeEvent::REGISTRATION_REQUEST);
            break;
        case N1MessageType::DEREGISTRATION_REQUEST_UE_ORIG:
        case N1MessageType::DEREGISTRATION_REQUEST_UE_TERM:
            handleEvent(UeEvent::DEREGISTER_REQUEST);
            break;
        case N1MessageType::SERVICE_REQUEST:
            handleEvent(UeEvent::SERVICE_REQUEST);
            break;
        case N1MessageType::AUTHENTICATION_RESPONSE:
            handleEvent(UeEvent::AUTHENTICATION_RESPONSE);
            break;
        case N1MessageType::SECURITY_MODE_COMPLETE:
            handleEvent(UeEvent::SECURITY_MODE_COMPLETE);
            break;
        default:
            std::cout << "Unhandled N1 message type: " << static_cast<int>(message.messageType) << std::endl;
            break;
    }
}

// N2接口处理
void AmfSm::handleN2Message(const N2Message& message) {
    std::cout << "Handling N2 message: " << message.toString() << std::endl;
    statistics_.totalN2Messages++;
    
    switch (message.messageType) {
        case N2MessageType::INITIAL_CONTEXT_SETUP_RESPONSE:
            std::cout << "Initial context setup completed" << std::endl;
            break;
        case N2MessageType::UE_CONTEXT_RELEASE_COMPLETE:
            handleEvent(UeEvent::CONNECTION_RELEASE);
            break;
        case N2MessageType::HANDOVER_REQUEST_ACKNOWLEDGE:
            std::cout << "Handover request acknowledged" << std::endl;
            break;
        case N2MessageType::HANDOVER_NOTIFY:
            handleEvent(UeEvent::HANDOVER_COMPLETE);
            break;
        default:
            std::cout << "Unhandled N2 message type: " << static_cast<int>(message.messageType) << std::endl;
            break;
    }
}

// 私有辅助方法
void AmfSm::registerOtherNfInstances() {
    if (!nfManager_) return;
    
    // 注册SMF实例（模拟）
    NfInstance smfInstance;
    smfInstance.nfInstanceId = "smf-001";
    smfInstance.nfType = NfType::SMF;
    smfInstance.plmnId = "plmn-001";
    smfInstance.nfStatus = NfStatus::REGISTERED;
    
    NfService smfService;
    smfService.serviceInstanceId = "smf-service-001";
    smfService.serviceName = "smf-service";
    smfInstance.nfServices.push_back(smfService);
    
    nfManager_->registerNfInstance(smfInstance);
    
    // 注册UPF实例（模拟）
    NfInstance upfInstance;
    upfInstance.nfInstanceId = "upf-001";
    upfInstance.nfType = NfType::UPF;
    upfInstance.plmnId = "plmn-001";
    upfInstance.nfStatus = NfStatus::REGISTERED;
    
    NfService upfService;
    upfService.serviceInstanceId = "upf-service-001";
    upfService.serviceName = "upf-service";
    upfInstance.nfServices.push_back(upfService);
    
    nfManager_->registerNfInstance(upfInstance);
    
    // 注册AUSF实例（模拟）
    NfInstance ausfInstance;
    ausfInstance.nfInstanceId = "ausf-001";
    ausfInstance.nfType = NfType::AUSF;
    ausfInstance.plmnId = "plmn-001";
    ausfInstance.nfStatus = NfStatus::REGISTERED;
    
    NfService ausfService;
    ausfService.serviceInstanceId = "ausf-service-001";
    ausfService.serviceName = "ausf-service";
    ausfInstance.nfServices.push_back(ausfService);
    
    nfManager_->registerNfInstance(ausfInstance);
    
    // 注册UDM实例（模拟）
    NfInstance udmInstance;
    udmInstance.nfInstanceId = "udm-001";
    udmInstance.nfType = NfType::UDM;
    udmInstance.plmnId = "plmn-001";
    udmInstance.nfStatus = NfStatus::REGISTERED;
    
    NfService udmService;
    udmService.serviceInstanceId = "udm-service-001";
    udmService.serviceName = "udm-service";
    udmInstance.nfServices.push_back(udmService);
    
    nfManager_->registerNfInstance(udmInstance);
    
    // 注册PCF实例（模拟）
    NfInstance pcfInstance;
    pcfInstance.nfInstanceId = "pcf-001";
    pcfInstance.nfType = NfType::PCF;
    pcfInstance.plmnId = "plmn-001";
    pcfInstance.nfStatus = NfStatus::REGISTERED;
    
    NfService pcfService;
    pcfService.serviceInstanceId = "pcf-service-001";
    pcfService.serviceName = "pcf-service";
    pcfInstance.nfServices.push_back(pcfService);
    
    nfManager_->registerNfInstance(pcfInstance);
}

void AmfSm::startMonitoring() {
    monitoringRunning_ = true;
    monitoringThread_ = std::thread(&AmfSm::monitoringLoop, this);
    std::cout << "Started AMF monitoring" << std::endl;
}

void AmfSm::stopMonitoring() {
    monitoringRunning_ = false;
    if (monitoringThread_.joinable()) {
        monitoringThread_.join();
    }
    std::cout << "Stopped AMF monitoring" << std::endl;
}

void AmfSm::monitoringLoop() {
    while (monitoringRunning_) {
        updateStatistics();
        
        // 更新AMF NF实例的负载
        if (amfNfInstance_) {
            amfNfInstance_->updateLoad(statistics_.systemLoad);
        }
        
        // 每30秒更新一次统计信息
        std::this_thread::sleep_for(std::chrono::seconds(30));
    }
}

void AmfSm::updateResponseTimeStatistics(double responseTime) {
    // 简单的移动平均
    static double totalTime = 0.0;
    static int count = 0;
    
    totalTime += responseTime;
    count++;
    
    statistics_.averageResponseTime = totalTime / count;
}

} // namespace amf_sm