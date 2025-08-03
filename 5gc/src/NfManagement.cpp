#include "NfManagement.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <sstream>
#include <thread>

namespace amf_sm {

// NfService 实现


// NfInstance 实现
NfInstance::NfInstance(const std::string& id, NfType type)
    : nfInstanceId(id), nfType(type), nfStatus(NfStatus::REGISTERED),
      priority(100), capacity(100), load(0) {}

void NfInstance::addService(const NfService& service) {
    nfServices.push_back(service);
}

bool NfInstance::removeService(const std::string& serviceName) {
    auto it = std::find_if(nfServices.begin(), nfServices.end(),
        [&serviceName](const NfService& service) {
            return service.serviceName == serviceName;
        });
    
    if (it != nfServices.end()) {
        nfServices.erase(it);
        return true;
    }
    return false;
}

NfService* NfInstance::getService(const std::string& serviceName) {
    auto it = std::find_if(nfServices.begin(), nfServices.end(),
        [&serviceName](const NfService& service) {
            return service.serviceName == serviceName;
        });
    
    return (it != nfServices.end()) ? &(*it) : nullptr;
}

void NfInstance::updateLoad(int newLoad) {
    load = std::max(0, std::min(100, newLoad));
    lastHeartbeat = std::chrono::system_clock::now();
}

bool NfInstance::isHealthy() const {
    auto now = std::chrono::system_clock::now();
    auto timeSinceHeartbeat = std::chrono::duration_cast<std::chrono::seconds>(
        now - lastHeartbeat).count();
    
    return (nfStatus == NfStatus::REGISTERED) && (timeSinceHeartbeat < 60);
}

// NfManager 实现
NfManager& NfManager::getInstance() {
    static NfManager instance;
    return instance;
}

NfManager::~NfManager() {
    stop();
}

void NfManager::healthCheckLoop() {
    while (running) {
        // 每10秒执行一次健康检查
        std::this_thread::sleep_for(std::chrono::seconds(10));
        
        if (!running) break;
        
        performHealthCheck();
        cleanupExpiredNfInstances();
    }
}

void NfManager::performHealthCheck() {
    std::lock_guard<std::mutex> lock(nfMutex_);
    
    for (auto& pair : nfInstances_) {
        auto& nfInstance = pair.second;
        if (!nfInstance->isHealthy()) {
            nfInstance->nfStatus = NfStatus::SUSPENDED;
            std::cout << "NF Instance marked as unhealthy: " << nfInstance->nfInstanceId << std::endl;
        }
    }
}

void NfManager::cleanupExpiredNfInstances() {
    std::lock_guard<std::mutex> lock(nfMutex_);
    
    auto now = std::chrono::system_clock::now();
    std::vector<std::string> instancesToRemove;
    
    for (auto& pair : nfInstances_) {
        auto& nfInstance = pair.second;
        auto timeSinceHeartbeat = std::chrono::duration_cast<std::chrono::seconds>(
            now - nfInstance->lastHeartbeat).count();
        
        // 如果超过2分钟没有心跳，则移除实例
        if (timeSinceHeartbeat > 120) {
            instancesToRemove.push_back(nfInstance->nfInstanceId);
        }
    }
    
    for (const auto& id : instancesToRemove) {
        auto it = nfInstances_.find(id);
        if (it != nfInstances_.end()) {
            auto nfType = it->second->nfType;
            
            // 从类型索引中移除
            auto& typeList = nfInstancesByType_[nfType];
            typeList.erase(std::remove(typeList.begin(), typeList.end(), id), typeList.end());
            
            // 从实例映射中移除
            nfInstances_.erase(it);
            
            std::cout << "Expired NF Instance removed: " << id << std::endl;
        }
    }
}

bool NfManager::start() {
    if (running) return false;
    
    running = true;
    
    // 启动健康检查线程
    healthCheckThread = std::thread(&NfManager::healthCheckLoop, this);
    
    std::cout << "NF Manager started" << std::endl;
    return true;
}

void NfManager::stop() {
    if (!running) return;
    
    running = false;
    
    if (healthCheckThread.joinable()) {
        healthCheckThread.join();
    }
    
    std::cout << "NF Manager stopped" << std::endl;
}

bool NfManager::registerNfInstance(const NfInstance& nfInstance) {
    std::lock_guard<std::mutex> lock(nfMutex_);
    
    auto it = nfInstances_.find(nfInstance.nfInstanceId);
    if (it != nfInstances_.end()) {
        std::cout << "NF Instance already exists: " << nfInstance.nfInstanceId << std::endl;
        return false;
    }
    
    nfInstances_[nfInstance.nfInstanceId] = std::make_shared<NfInstance>(nfInstance);
    nfInstances_[nfInstance.nfInstanceId]->lastHeartbeat = std::chrono::system_clock::now();
    
    // 按类型索引
    nfInstancesByType_[nfInstance.nfType].push_back(nfInstance.nfInstanceId);
    
    std::cout << "NF Instance registered: " << nfInstance.nfInstanceId
              << " (Type: " << static_cast<int>(nfInstance.nfType) << ")" << std::endl;
    return true;
}

bool NfManager::deregisterNfInstance(const std::string& nfInstanceId) {
    std::lock_guard<std::mutex> lock(nfMutex_);
    
    auto it = nfInstances_.find(nfInstanceId);
    if (it == nfInstances_.end()) {
        return false;
    }
    
    nfInstances_.erase(it);
    std::cout << "NF Instance deregistered: " << nfInstanceId << std::endl;
    
    return true;
}

std::vector<NfInstance> NfManager::discoverNfInstances(const NfDiscoveryQuery& query) {
    std::lock_guard<std::mutex> lock(nfMutex_);
    std::vector<NfInstance> results;
    
    for (const auto& pair : nfInstances_) {
        const auto& nfPtr = pair.second;
        
        // 检查NF类型
        if (query.targetNfType != NfType::UNKNOWN && nfPtr->nfType != query.targetNfType) {
            continue;
        }
        
        // 检查健康状态
        if (!nfPtr->isHealthy()) {
            continue;
        }
        
        // 检查服务
        if (!query.serviceName.empty()) {
            bool hasService = false;
            for (const auto& service : nfPtr->nfServices) {
                if (service.serviceName == query.serviceName) {
                    hasService = true;
                    break;
                }
            }
            if (!hasService) {
                continue;
            }
        }
        
        results.push_back(*nfPtr);
    }
    
    // 根据负载和优先级排序
    std::sort(results.begin(), results.end(),
        [](const NfInstance& a, const NfInstance& b) {
            if (a.priority != b.priority) {
                return a.priority > b.priority; // 优先级高的在前
            }
            return a.load < b.load; // 负载低的在前
        });
    
    std::cout << "NF Discovery: found " << results.size() 
              << " instances for type " << static_cast<int>(query.targetNfType) << std::endl;
    
    return results;
}

bool NfManager::updateNfStatus(const std::string& nfInstanceId, NfStatus status) {
    std::lock_guard<std::mutex> lock(nfMutex_);
    
    auto it = nfInstances_.find(nfInstanceId);
    if (it == nfInstances_.end()) {
        return false;
    }
    
    it->second->nfStatus = status;
    it->second->lastHeartbeat = std::chrono::system_clock::now();
    
    std::cout << "NF Status updated: " << nfInstanceId 
              << " -> " << static_cast<int>(status) << std::endl;
    
    return true;
}

void NfManager::processHeartbeat(const std::string& nfInstanceId) {
    std::lock_guard<std::mutex> lock(nfMutex_);
    
    auto it = nfInstances_.find(nfInstanceId);
    if (it == nfInstances_.end()) {
        return;
    }
    
    // 更新最后心跳时间
    it->second->lastHeartbeat = std::chrono::system_clock::now();
}

std::shared_ptr<NfInstance> NfManager::selectNfInstance(NfType nfType, const std::string& plmnId) {
    NfDiscoveryQuery query;
    query.targetNfType = nfType;
    // 如果提供了PLMN ID，则添加到查询条件中
    if (!plmnId.empty()) {
        query.targetPlmnId = plmnId;
    }
    
    auto candidates = discoverNfInstances(query);
    if (candidates.empty()) {
        return nullptr;
    }
    
    // 简单的负载均衡：选择负载最低的实例
    std::lock_guard<std::mutex> lock(nfMutex_);
    auto it = nfInstances_.find(candidates[0].nfInstanceId);
    return (it != nfInstances_.end()) ? it->second : nullptr;
}

std::map<NfType, size_t> NfManager::getNfStatistics() const {
    std::lock_guard<std::mutex> lock(nfMutex_);
    std::map<NfType, size_t> stats;
    
    // 统计各类型NF实例数量
    for (const auto& pair : nfInstancesByType_) {
        stats[pair.first] = pair.second.size();
    }
    return stats;
}



// AmfNfInstance 实现
AmfNfInstance& AmfNfInstance::getInstance() {
    static AmfNfInstance instance;
    return instance;
}

bool AmfNfInstance::initialize(const std::string& amfInstanceId, const std::string& plmnId) {
    std::lock_guard<std::mutex> lock(instanceMutex_);
    
    // 创建AMF NF实例
    amfInstance_.nfInstanceId = amfInstanceId;
    amfInstance_.nfType = NfType::AMF;
    amfInstance_.plmnId = plmnId;
    amfInstance_.nfStatus = NfStatus::REGISTERED;
    
    // 添加AMF服务
    NfService namfComm;
    namfComm.serviceName = "namf-comm";
    namfComm.scheme = "http";
    namfComm.fqdn = "amf.5gc.mnc001.mcc460.3gppnetwork.org";
    namfComm.port = 8080;
    namfComm.apiPrefix = "/namf-comm/v1";
    amfInstance_.nfServices.push_back(namfComm);
    
    NfService namfEvts;
    namfEvts.serviceName = "namf-evts";
    namfEvts.scheme = "http";
    namfEvts.fqdn = "amf.5gc.mnc001.mcc460.3gppnetwork.org";
    namfEvts.port = 8080;
    namfEvts.apiPrefix = "/namf-evts/v1";
    amfInstance_.nfServices.push_back(namfEvts);
    
    NfService namfMt;
    namfMt.serviceName = "namf-mt";
    namfMt.scheme = "http";
    namfMt.fqdn = "amf.5gc.mnc001.mcc460.3gppnetwork.org";
    namfMt.port = 8080;
    namfMt.apiPrefix = "/namf-mt/v1";
    amfInstance_.nfServices.push_back(namfMt);
    
    NfService namfLoc;
    namfLoc.serviceName = "namf-loc";
    namfLoc.scheme = "http";
    namfLoc.fqdn = "amf.5gc.mnc001.mcc460.3gppnetwork.org";
    namfLoc.port = 8080;
    namfLoc.apiPrefix = "/namf-loc/v1";
    amfInstance_.nfServices.push_back(namfLoc);
    
    return true;
}



bool AmfNfInstance::registerWithNrf(const std::string& nrfUri) {
    std::lock_guard<std::mutex> lock(instanceMutex_);
    
    // 保存NRF URI
    nrfUri_ = nrfUri;
    
    // 模拟向NRF注册
    std::cout << "Registering AMF instance " << amfInstance_.nfInstanceId << " to NRF: " << nrfUri << std::endl;
    
    // 实际实现中这里会发送HTTP请求到NRF
    registered_ = true;
    
    // 启动心跳服务
    startHeartbeatService(30); // 30秒间隔
    
    std::cout << "AMF instance registered successfully" << std::endl;
    return true;
}

bool AmfNfInstance::sendHeartbeat() {
    std::lock_guard<std::mutex> lock(instanceMutex_);
    
    if (!registered_) {
        return false;
    }
    
    std::cout << "Sending heartbeat for AMF instance " << amfInstance_.nfInstanceId << " to NRF" << std::endl;
    
    // 实际实现中这里会发送HTTP请求到NRF
    amfInstance_.lastHeartbeat = std::chrono::system_clock::now();
    
    return true;
}

void AmfNfInstance::updateStatus(NfStatus status) {
    std::lock_guard<std::mutex> lock(instanceMutex_);
    amfInstance_.nfStatus = status;
    
    // 如果已注册到NRF，则通知状态变更
    if (registered_) {
        std::cout << "Updating AMF status to " << static_cast<int>(status) << " in NRF" << std::endl;
        // 实际实现中这里会发送HTTP请求到NRF
    }
}

void AmfNfInstance::updateLoad(int load) {
    std::lock_guard<std::mutex> lock(instanceMutex_);
    amfInstance_.load = load;
    
    // 如果已注册到NRF，则通知负载变更
    if (registered_) {
        std::cout << "Updating AMF load to " << load << " in NRF" << std::endl;
        // 实际实现中这里会发送HTTP请求到NRF
    }
}

void AmfNfInstance::updateCapacity(int capacity) {
    std::lock_guard<std::mutex> lock(instanceMutex_);
    amfInstance_.capacity = capacity;
    
    // 如果已注册到NRF，则通知容量变更
    if (registered_) {
        std::cout << "Updating AMF capacity to " << capacity << " in NRF" << std::endl;
        // 实际实现中这里会发送HTTP请求到NRF
    }
}

void AmfNfInstance::startHeartbeatService(int intervalSeconds) {
    std::lock_guard<std::mutex> lock(instanceMutex_);
    
    // 停止现有的心跳服务（如果有）
    stopHeartbeatService();
    
    // 启动新的心跳服务
    heartbeatInterval_ = intervalSeconds;
    heartbeatRunning_ = true;
    heartbeatThread_ = std::thread([this, intervalSeconds]() {
        while (heartbeatRunning_) {
            // 发送心跳
            this->sendHeartbeat();
            
            // 等待指定的间隔时间
            std::this_thread::sleep_for(std::chrono::seconds(intervalSeconds));
        }
    });
}

void AmfNfInstance::stopHeartbeatService() {
    // 停止心跳线程
    heartbeatRunning_ = false;
    
    if (heartbeatThread_.joinable()) {
        heartbeatThread_.join();
    }
}

// 辅助函数实现
std::string nfTypeToString(NfType type) {
    switch (type) {
        case NfType::AMF: return "AMF";
        case NfType::SMF: return "SMF";
        case NfType::UPF: return "UPF";
        case NfType::AUSF: return "AUSF";
        case NfType::UDM: return "UDM";
        case NfType::UDR: return "UDR";
        case NfType::PCF: return "PCF";
        case NfType::NRF: return "NRF";
        case NfType::NSSF: return "NSSF";
        case NfType::NEF: return "NEF";
        default: return "UNKNOWN";
    }
}

std::string nfStatusToString(NfStatus status) {
    switch (status) {
        case NfStatus::REGISTERED: return "REGISTERED";
        case NfStatus::SUSPENDED: return "SUSPENDED";
        case NfStatus::UNDISCOVERABLE: return "UNDISCOVERABLE";
        case NfStatus::DEREGISTERED: return "DEREGISTERED";
        default: return "UNKNOWN";
    }
}

NfType stringToNfType(const std::string& str) {
    if (str == "AMF") return NfType::AMF;
    if (str == "SMF") return NfType::SMF;
    if (str == "UPF") return NfType::UPF;
    if (str == "AUSF") return NfType::AUSF;
    if (str == "UDM") return NfType::UDM;
    if (str == "UDR") return NfType::UDR;
    if (str == "PCF") return NfType::PCF;
    if (str == "NRF") return NfType::NRF;
    if (str == "NSSF") return NfType::NSSF;
    if (str == "NEF") return NfType::NEF;
    return NfType::UNKNOWN;
}

NfStatus stringToNfStatus(const std::string& str) {
    if (str == "REGISTERED") return NfStatus::REGISTERED;
    if (str == "SUSPENDED") return NfStatus::SUSPENDED;
    if (str == "UNDISCOVERABLE") return NfStatus::UNDISCOVERABLE;
    if (str == "DEREGISTERED") return NfStatus::DEREGISTERED;
    return NfStatus::DEREGISTERED;
}

} // namespace amf_sm