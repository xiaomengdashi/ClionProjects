#include "N1N2Interface.h"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace amf_sm {

// N1Message 实现
std::string N1Message::toString() const {
    std::ostringstream oss;
    oss << "N1Message{type:" << static_cast<int>(messageType) 
        << ",ue_id:" << ueId;
    
    // 添加所有信息元素
    for (const auto& ie : ieList) {
        oss << "," << ie.first << ":" << ie.second;
    }
    
    oss << ",payload_size:" << nasMessageContainer.size() << "}";
    return oss.str();
}

// N2Message 实现
std::string N2Message::toString() const {
    std::ostringstream oss;
    oss << "N2Message{type:" << static_cast<int>(messageType) 
        << ",ran_node_id:" << ranNodeId 
        << ",ue_ngap_id:" << ueNgapId
        << ",amf_ue_ngap_id:" << amfUeNgapId;
    
    // 添加所有信息元素
    for (const auto& ie : ieList) {
        oss << "," << ie.first << ":" << ie.second;
    }
    
    oss << ",payload_size:" << ngapContainer.size() << "}";
    return oss.str();
}

// N1InterfaceHandler 实现
void N1InterfaceHandler::registerN1Callback(N1MessageType msgType, 
                                           std::function<void(const N1Message&)> callback) {
    n1Callbacks_[msgType] = callback;
}

// N2InterfaceHandler 实现
void N2InterfaceHandler::registerN2Callback(N2MessageType msgType, 
                                           std::function<void(const N2Message&)> callback) {
    n2Callbacks_[msgType] = callback;
}

// N1N2InterfaceManager 实现
N1N2InterfaceManager& N1N2InterfaceManager::getInstance() {
    static N1N2InterfaceManager instance;
    return instance;
}

bool N1N2InterfaceManager::startN1N2Service(const std::string& bindAddress, int n2Port) {
    std::lock_guard<std::mutex> lock(interfaceMutex_);
    
    if (serviceRunning_) {
        std::cout << "N1N2 Interface service is already running" << std::endl;
        return true;
    }
    
    bindAddress_ = bindAddress;
    n2Port_ = n2Port;
    serviceRunning_ = true;
    
    std::cout << "N1N2 Interface Manager started successfully on " << bindAddress << ":" << n2Port << std::endl;
    return true;
}

void N1N2InterfaceManager::stopN1N2Service() {
    std::lock_guard<std::mutex> lock(interfaceMutex_);
    
    if (!serviceRunning_) {
        return;
    }
    
    serviceRunning_ = false;
    std::cout << "N1N2 Interface Manager stopped" << std::endl;
}

bool N1N2InterfaceManager::sendN1Message(const N1Message& message) {
    if (!serviceRunning_) {
        std::cerr << "N1N2 Interface service is not running" << std::endl;
        return false;
    }
    
    std::cout << "Sending N1 message: " << message.toString() << std::endl;
    n1MessageCount_++;
    return true;
}

bool N1N2InterfaceManager::sendN2Message(const N2Message& message) {
    if (!serviceRunning_) {
        std::cerr << "N1N2 Interface service is not running" << std::endl;
        return false;
    }
    
    std::cout << "Sending N2 message: " << message.toString() << std::endl;
    n2MessageCount_++;
    return true;
}

void N1N2InterfaceManager::handleN1Message(const N1Message& message) {
    // 查找并调用对应消息类型的回调函数
    auto it = n1Callbacks_.find(message.messageType);
    if (it != n1Callbacks_.end()) {
        it->second(message);
    } else {
        std::cout << "Handling N1 message: " << message.toString() << std::endl;
    }
}

void N1N2InterfaceManager::handleN2Message(const N2Message& message) {
    // 查找并调用对应消息类型的回调函数
    auto it = n2Callbacks_.find(message.messageType);
    if (it != n2Callbacks_.end()) {
        it->second(message);
    } else {
        std::cout << "Handling N2 message: " << message.toString() << std::endl;
    }
}

bool N1N2InterfaceManager::isRanNodeConnected(const std::string& ranNodeId) const {
    std::lock_guard<std::mutex> lock(interfaceMutex_);
    auto it = connectedRanNodes_.find(ranNodeId);
    return (it != connectedRanNodes_.end() && it->second);
}

std::vector<std::string> N1N2InterfaceManager::getConnectedRanNodes() const {
    std::lock_guard<std::mutex> lock(interfaceMutex_);
    std::vector<std::string> result;
    
    for (const auto& entry : connectedRanNodes_) {
        if (entry.second) {
            result.push_back(entry.first);
        }
    }
    
    return result;
}



// 辅助函数实现
std::string n1MessageTypeToString(amf_sm::N1MessageType type) {
    switch (type) {
        case amf_sm::N1MessageType::REGISTRATION_REQUEST: return "REGISTRATION_REQUEST";
        case amf_sm::N1MessageType::REGISTRATION_ACCEPT: return "REGISTRATION_ACCEPT";
        case amf_sm::N1MessageType::AUTHENTICATION_REQUEST: return "AUTHENTICATION_REQUEST";
        case amf_sm::N1MessageType::AUTHENTICATION_RESPONSE: return "AUTHENTICATION_RESPONSE";
        default: return "UNKNOWN";
    }
}

std::string n2MessageTypeToString(amf_sm::N2MessageType type) {
    switch (type) {
        case amf_sm::N2MessageType::INITIAL_CONTEXT_SETUP_REQUEST: return "INITIAL_CONTEXT_SETUP_REQUEST";
        case amf_sm::N2MessageType::UE_CONTEXT_RELEASE_COMMAND: return "UE_CONTEXT_RELEASE_COMMAND";
        case amf_sm::N2MessageType::HANDOVER_REQUEST: return "HANDOVER_REQUEST";
        case amf_sm::N2MessageType::PAGING: return "PAGING";
        default: return "UNKNOWN";
    }
}

amf_sm::N1MessageType stringToN1MessageType(const std::string& str) {
    if (str == "REGISTRATION_REQUEST") return amf_sm::N1MessageType::REGISTRATION_REQUEST;
    if (str == "REGISTRATION_ACCEPT") return amf_sm::N1MessageType::REGISTRATION_ACCEPT;
    if (str == "AUTHENTICATION_REQUEST") return amf_sm::N1MessageType::AUTHENTICATION_REQUEST;
    if (str == "AUTHENTICATION_RESPONSE") return amf_sm::N1MessageType::AUTHENTICATION_RESPONSE;
    return amf_sm::N1MessageType::REGISTRATION_REQUEST; // 默认值
}

amf_sm::N2MessageType stringToN2MessageType(const std::string& str) {
    if (str == "INITIAL_CONTEXT_SETUP_REQUEST") return amf_sm::N2MessageType::INITIAL_CONTEXT_SETUP_REQUEST;
    if (str == "UE_CONTEXT_RELEASE_COMMAND") return amf_sm::N2MessageType::UE_CONTEXT_RELEASE_COMMAND;
    if (str == "HANDOVER_REQUEST") return amf_sm::N2MessageType::HANDOVER_REQUEST;
    if (str == "PAGING") return amf_sm::N2MessageType::PAGING;
    return amf_sm::N2MessageType::INITIAL_CONTEXT_SETUP_REQUEST; // 默认值
}

} // namespace amf_sm