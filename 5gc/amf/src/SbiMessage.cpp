#include "SbiMessage.h"
#include <sstream>

namespace amf_sm {

SbiMessage::SbiMessage(SbiServiceType service, SbiMessageType type, HttpMethod method)
    : serviceType_(service), messageType_(type), httpMethod_(method), 
      status_(SbiMessageStatus::PENDING), statusCode_(0) {
    
    // 设置默认的Content-Type
    headers_["Content-Type"] = "application/json";
    headers_["Accept"] = "application/json";
}

std::string SbiMessage::toString() const {
    std::stringstream ss;
    
    // HTTP方法转字符串
    std::string methodStr;
    switch (httpMethod_) {
        case HttpMethod::GET: methodStr = "GET"; break;
        case HttpMethod::POST: methodStr = "POST"; break;
        case HttpMethod::PUT: methodStr = "PUT"; break;
        case HttpMethod::DELETE: methodStr = "DELETE"; break;
        case HttpMethod::PATCH: methodStr = "PATCH"; break;
    }
    
    // 服务类型转字符串
    std::string serviceStr;
    switch (serviceType_) {
        case SbiServiceType::NAMF_COMMUNICATION: serviceStr = "Namf_Communication"; break;
        case SbiServiceType::NAMF_EVENT_EXPOSURE: serviceStr = "Namf_EventExposure"; break;
        case SbiServiceType::NAMF_LOCATION: serviceStr = "Namf_Location"; break;
        case SbiServiceType::NAMF_MT: serviceStr = "Namf_MT"; break;
        case SbiServiceType::NSMF_PDU_SESSION: serviceStr = "Nsmf_PDUSession"; break;
        case SbiServiceType::NUDM_SDM: serviceStr = "Nudm_SDM"; break;
        case SbiServiceType::NUDM_UE_AUTHENTICATION: serviceStr = "Nudm_UEAuthentication"; break;
        case SbiServiceType::NAUSF_UE_AUTHENTICATION: serviceStr = "Nausf_UEAuthentication"; break;
        case SbiServiceType::NPCF_AM_POLICY_CONTROL: serviceStr = "Npcf_AMPolicyControl"; break;
        case SbiServiceType::NRF_NFM: serviceStr = "Nnrf_NFManagement"; break;
        case SbiServiceType::NRF_NFD: serviceStr = "Nnrf_NFDiscovery"; break;
    }
    
    // 消息类型转字符串
    std::string msgTypeStr;
    switch (messageType_) {
        case SbiMessageType::UE_CONTEXT_CREATE_REQUEST: msgTypeStr = "UeContextCreateRequest"; break;
        case SbiMessageType::UE_CONTEXT_CREATE_RESPONSE: msgTypeStr = "UeContextCreateResponse"; break;
        case SbiMessageType::UE_AUTHENTICATION_REQUEST: msgTypeStr = "UeAuthenticationRequest"; break;
        case SbiMessageType::UE_AUTHENTICATION_RESPONSE: msgTypeStr = "UeAuthenticationResponse"; break;
        case SbiMessageType::PDU_SESSION_CREATE_SM_CONTEXT_REQUEST: msgTypeStr = "PduSessionCreateSmContextRequest"; break;
        case SbiMessageType::PDU_SESSION_CREATE_SM_CONTEXT_RESPONSE: msgTypeStr = "PduSessionCreateSmContextResponse"; break;
        case SbiMessageType::NF_REGISTER_REQUEST: msgTypeStr = "NfRegisterRequest"; break;
        case SbiMessageType::NF_REGISTER_RESPONSE: msgTypeStr = "NfRegisterResponse"; break;
        default: msgTypeStr = "UnknownMessage"; break;
    }
    
    ss << "SBI Message: " << serviceStr << " - " << msgTypeStr << "\n";
    ss << "Method: " << methodStr << "\n";
    ss << "URI: " << uri_ << "\n";
    ss << "Status: " << static_cast<int>(status_) << " (Code: " << statusCode_ << ")\n";
    
    if (!headers_.empty()) {
        ss << "Headers:\n";
        for (const auto& header : headers_) {
            ss << "  " << header.first << ": " << header.second << "\n";
        }
    }
    
    if (!body_.empty()) {
        ss << "Body: " << body_.substr(0, 100);
        if (body_.length() > 100) ss << "...";
        ss << "\n";
    }
    
    return ss.str();
}

bool SbiMessage::isRequest() const {
    // 简单判断：消息类型名包含"REQUEST"的为请求
    switch (messageType_) {
        case SbiMessageType::UE_CONTEXT_CREATE_REQUEST:
        case SbiMessageType::UE_CONTEXT_UPDATE_REQUEST:
        case SbiMessageType::UE_CONTEXT_RELEASE_REQUEST:
        case SbiMessageType::UE_AUTHENTICATION_REQUEST:
        case SbiMessageType::UE_AUTHENTICATION_RESULT_REQUEST:
        case SbiMessageType::PDU_SESSION_CREATE_SM_CONTEXT_REQUEST:
        case SbiMessageType::PDU_SESSION_UPDATE_SM_CONTEXT_REQUEST:
        case SbiMessageType::PDU_SESSION_RELEASE_SM_CONTEXT_REQUEST:
        case SbiMessageType::AM_POLICY_CONTROL_CREATE_REQUEST:
        case SbiMessageType::AM_POLICY_CONTROL_UPDATE_REQUEST:
        case SbiMessageType::AM_POLICY_CONTROL_DELETE_REQUEST:
        case SbiMessageType::EVENT_EXPOSURE_SUBSCRIBE_REQUEST:
        case SbiMessageType::EVENT_EXPOSURE_NOTIFY_REQUEST:
        case SbiMessageType::EVENT_EXPOSURE_UNSUBSCRIBE_REQUEST:
        case SbiMessageType::NF_REGISTER_REQUEST:
        case SbiMessageType::NF_UPDATE_REQUEST:
        case SbiMessageType::NF_DEREGISTER_REQUEST:
        case SbiMessageType::NF_STATUS_NOTIFY_REQUEST:
        case SbiMessageType::NF_DISCOVER_REQUEST:
            return true;
        default:
            return false;
    }
}

bool SbiMessage::isResponse() const {
    return !isRequest() && messageType_ != SbiMessageType::PROBLEM_DETAILS;
}

bool SbiMessage::isSuccess() const {
    return status_ == SbiMessageStatus::SUCCESS && statusCode_ >= 200 && statusCode_ < 300;
}

} // namespace amf_sm