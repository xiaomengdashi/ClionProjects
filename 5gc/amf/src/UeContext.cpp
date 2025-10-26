#include "UeContext.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <mutex>

namespace amf_sm {

// UeContext实现
UeContext::UeContext(const std::string& supi) {
    identifiers_.supi = supi;
    registrationState_ = "DEREGISTERED";
    connectionState_ = "IDLE";
    creationTime_ = std::chrono::system_clock::now();
    lastActivity_ = creationTime_;
    
    // 初始化安全上下文
    securityContext_.isAuthenticated = false;
    securityContext_.ngKsi = 0;
    
    // 初始化接入信息
    accessInfo_.isConnected = false;
    accessInfo_.anPort = 0;
    
    // 初始化移动性信息
    mobilityInfo_.isRoaming = false;
}

void UeContext::updateLocation(const LocationInfo& location) {
    locationInfo_ = location;
    locationInfo_.lastUpdate = std::chrono::system_clock::now();
    updateLastActivity();
}

void UeContext::updateSecurityContext(const SecurityContext& security) {
    securityContext_ = security;
    securityContext_.authTime = std::chrono::system_clock::now();
    updateLastActivity();
}

void UeContext::addPduSession(const PduSession& session) {
    // 检查是否已存在相同ID的会话
    auto it = std::find_if(pduSessions_.begin(), pduSessions_.end(),
        [&session](const PduSession& s) { return s.sessionId == session.sessionId; });
    
    if (it != pduSessions_.end()) {
        *it = session; // 更新现有会话
    } else {
        pduSessions_.push_back(session); // 添加新会话
    }
    updateLastActivity();
}

void UeContext::removePduSession(int sessionId) {
    pduSessions_.erase(
        std::remove_if(pduSessions_.begin(), pduSessions_.end(),
            [sessionId](const PduSession& s) { return s.sessionId == sessionId; }),
        pduSessions_.end());
    updateLastActivity();
}

std::vector<PduSession> UeContext::getActivePduSessions() const {
    std::vector<PduSession> activeSessions;
    std::copy_if(pduSessions_.begin(), pduSessions_.end(),
        std::back_inserter(activeSessions),
        [](const PduSession& s) { return s.isActive; });
    return activeSessions;
}

PduSession* UeContext::getPduSession(int sessionId) {
    auto it = std::find_if(pduSessions_.begin(), pduSessions_.end(),
        [sessionId](const PduSession& s) { return s.sessionId == sessionId; });
    return (it != pduSessions_.end()) ? &(*it) : nullptr;
}

void UeContext::updateAccessInfo(const AccessInfo& access) {
    accessInfo_ = access;
    updateLastActivity();
}

void UeContext::updateMobilityInfo(const MobilityInfo& mobility) {
    mobilityInfo_ = mobility;
    updateLastActivity();
}

void UeContext::updateSubscriptionInfo(const SubscriptionInfo& subscription) {
    subscriptionInfo_ = subscription;
    updateLastActivity();
}

std::string UeContext::toJson() const {
    std::ostringstream json;
    json << "{\n";
    json << "  \"supi\": \"" << identifiers_.supi << "\",\n";
    json << "  \"guti\": \"" << identifiers_.guti << "\",\n";
    json << "  \"pei\": \"" << identifiers_.pei << "\",\n";
    json << "  \"registrationState\": \"" << registrationState_ << "\",\n";
    json << "  \"connectionState\": \"" << connectionState_ << "\",\n";
    json << "  \"isAuthenticated\": " << (securityContext_.isAuthenticated ? "true" : "false") << ",\n";
    json << "  \"isConnected\": " << (accessInfo_.isConnected ? "true" : "false") << ",\n";
    json << "  \"location\": {\n";
    json << "    \"tai\": \"" << locationInfo_.tai << "\",\n";
    json << "    \"cellId\": \"" << locationInfo_.cellId << "\",\n";
    json << "    \"ratType\": \"" << locationInfo_.ratType << "\"\n";
    json << "  },\n";
    json << "  \"activePduSessions\": " << getActivePduSessions().size() << "\n";
    json << "}";
    return json.str();
}

std::string UeContext::toString() const {
    std::ostringstream ss;
    ss << "UE Context [SUPI: " << identifiers_.supi 
       << ", State: " << registrationState_ 
       << "/" << connectionState_
       << ", Auth: " << (securityContext_.isAuthenticated ? "YES" : "NO")
       << ", Connected: " << (accessInfo_.isConnected ? "YES" : "NO")
       << ", Sessions: " << getActivePduSessions().size()
       << "]";
    return ss.str();
}

// UeContextManager实现
UeContextManager& UeContextManager::getInstance() {
    static UeContextManager instance;
    return instance;
}

std::shared_ptr<UeContext> UeContextManager::createUeContext(const std::string& supi) {
    std::lock_guard<std::mutex> lock(contextMutex_);
    
    auto context = std::make_shared<UeContext>(supi);
    ueContexts_[supi] = context;
    
    return context;
}

std::shared_ptr<UeContext> UeContextManager::getUeContext(const std::string& supi) {
    std::lock_guard<std::mutex> lock(contextMutex_);
    
    auto it = ueContexts_.find(supi);
    return (it != ueContexts_.end()) ? it->second : nullptr;
}

std::shared_ptr<UeContext> UeContextManager::getUeContextByGuti(const std::string& guti) {
    std::lock_guard<std::mutex> lock(contextMutex_);
    
    auto it = gutiToSupi_.find(guti);
    if (it != gutiToSupi_.end()) {
        return getUeContext(it->second);
    }
    return nullptr;
}

void UeContextManager::removeUeContext(const std::string& supi) {
    std::lock_guard<std::mutex> lock(contextMutex_);
    
    auto it = ueContexts_.find(supi);
    if (it != ueContexts_.end()) {
        // 清理GUTI映射
        const auto& guti = it->second->getIdentifiers().guti;
        if (!guti.empty()) {
            gutiToSupi_.erase(guti);
        }
        ueContexts_.erase(it);
    }
}

std::vector<std::shared_ptr<UeContext>> UeContextManager::getAllUeContexts() {
    std::lock_guard<std::mutex> lock(contextMutex_);
    
    std::vector<std::shared_ptr<UeContext>> contexts;
    for (const auto& pair : ueContexts_) {
        contexts.push_back(pair.second);
    }
    return contexts;
}

std::vector<std::shared_ptr<UeContext>> UeContextManager::getUeContextsBySlice(const NetworkSlice& slice) {
    std::lock_guard<std::mutex> lock(contextMutex_);
    
    std::vector<std::shared_ptr<UeContext>> contexts;
    for (const auto& pair : ueContexts_) {
        const auto& subscribedSlices = pair.second->getSubscriptionInfo().subscribedSlices;
        for (const auto& subscribedSlice : subscribedSlices) {
            if (subscribedSlice.sst == slice.sst && subscribedSlice.sd == slice.sd) {
                contexts.push_back(pair.second);
                break;
            }
        }
    }
    return contexts;
}

std::vector<std::shared_ptr<UeContext>> UeContextManager::getUeContextsByLocation(const std::string& tai) {
    std::lock_guard<std::mutex> lock(contextMutex_);
    
    std::vector<std::shared_ptr<UeContext>> contexts;
    for (const auto& pair : ueContexts_) {
        if (pair.second->getLocationInfo().tai == tai) {
            contexts.push_back(pair.second);
        }
    }
    return contexts;
}

size_t UeContextManager::getRegisteredUeCount() const {
    std::lock_guard<std::mutex> lock(contextMutex_);
    
    return std::count_if(ueContexts_.begin(), ueContexts_.end(),
        [](const auto& pair) {
            return pair.second->getRegistrationState() != "DEREGISTERED";
        });
}

size_t UeContextManager::getConnectedUeCount() const {
    std::lock_guard<std::mutex> lock(contextMutex_);
    
    return std::count_if(ueContexts_.begin(), ueContexts_.end(),
        [](const auto& pair) {
            return pair.second->isConnected();
        });
}

size_t UeContextManager::getActiveSessionCount() const {
    std::lock_guard<std::mutex> lock(contextMutex_);
    
    size_t count = 0;
    for (const auto& pair : ueContexts_) {
        count += pair.second->getActivePduSessions().size();
    }
    return count;
}

void UeContextManager::cleanupInactiveContexts(std::chrono::minutes inactiveThreshold) {
    std::lock_guard<std::mutex> lock(contextMutex_);
    
    auto now = std::chrono::system_clock::now();
    auto it = ueContexts_.begin();
    
    while (it != ueContexts_.end()) {
        auto timeSinceLastActivity = std::chrono::duration_cast<std::chrono::minutes>(
            now - it->second->getLastActivity());
        
        if (timeSinceLastActivity > inactiveThreshold && 
            it->second->getRegistrationState() == "DEREGISTERED") {
            
            // 清理GUTI映射
            const auto& guti = it->second->getIdentifiers().guti;
            if (!guti.empty()) {
                gutiToSupi_.erase(guti);
            }
            
            it = ueContexts_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace amf_sm