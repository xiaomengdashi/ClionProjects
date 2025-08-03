#ifndef SBI_MESSAGE_H
#define SBI_MESSAGE_H

#include <string>
#include <map>
#include <memory>

namespace amf_sm {

// SBI服务类型
enum class SbiServiceType {
    NAMF_COMMUNICATION,      // AMF Communication Service
    NAMF_EVENT_EXPOSURE,     // AMF Event Exposure Service
    NAMF_LOCATION,          // AMF Location Service
    NAMF_MT,                // AMF Mobile Terminated Service
    NSMF_PDU_SESSION,       // SMF PDU Session Service
    NUDM_SDM,               // UDM Subscriber Data Management
    NUDM_UE_AUTHENTICATION, // UDM UE Authentication
    NAUSF_UE_AUTHENTICATION,// AUSF UE Authentication
    NPCF_AM_POLICY_CONTROL, // PCF Access and Mobility Policy Control
    NRF_NFM,                // NRF NF Management
    NRF_NFD                 // NRF NF Discovery
};

// HTTP方法
enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    PATCH
};

// SBI消息类型
enum class SbiMessageType {
    // Registration Management
    UE_CONTEXT_CREATE_REQUEST,
    UE_CONTEXT_CREATE_RESPONSE,
    UE_CONTEXT_UPDATE_REQUEST,
    UE_CONTEXT_UPDATE_RESPONSE,
    UE_CONTEXT_RELEASE_REQUEST,
    UE_CONTEXT_RELEASE_RESPONSE,
    
    // Authentication
    UE_AUTHENTICATION_REQUEST,
    UE_AUTHENTICATION_RESPONSE,
    UE_AUTHENTICATION_RESULT_REQUEST,
    UE_AUTHENTICATION_RESULT_RESPONSE,
    
    // Session Management
    PDU_SESSION_CREATE_SM_CONTEXT_REQUEST,
    PDU_SESSION_CREATE_SM_CONTEXT_RESPONSE,
    PDU_SESSION_UPDATE_SM_CONTEXT_REQUEST,
    PDU_SESSION_UPDATE_SM_CONTEXT_RESPONSE,
    PDU_SESSION_RELEASE_SM_CONTEXT_REQUEST,
    PDU_SESSION_RELEASE_SM_CONTEXT_RESPONSE,
    
    // Policy Control
    AM_POLICY_CONTROL_CREATE_REQUEST,
    AM_POLICY_CONTROL_CREATE_RESPONSE,
    AM_POLICY_CONTROL_UPDATE_REQUEST,
    AM_POLICY_CONTROL_UPDATE_RESPONSE,
    AM_POLICY_CONTROL_DELETE_REQUEST,
    AM_POLICY_CONTROL_DELETE_RESPONSE,
    
    // Event Exposure
    EVENT_EXPOSURE_SUBSCRIBE_REQUEST,
    EVENT_EXPOSURE_SUBSCRIBE_RESPONSE,
    EVENT_EXPOSURE_NOTIFY_REQUEST,
    EVENT_EXPOSURE_NOTIFY_RESPONSE,
    EVENT_EXPOSURE_UNSUBSCRIBE_REQUEST,
    EVENT_EXPOSURE_UNSUBSCRIBE_RESPONSE,
    
    // NF Management
    NF_REGISTER_REQUEST,
    NF_REGISTER_RESPONSE,
    NF_UPDATE_REQUEST,
    NF_UPDATE_RESPONSE,
    NF_DEREGISTER_REQUEST,
    NF_DEREGISTER_RESPONSE,
    NF_STATUS_NOTIFY_REQUEST,
    NF_STATUS_NOTIFY_RESPONSE,
    
    // Discovery
    NF_DISCOVER_REQUEST,
    NF_DISCOVER_RESPONSE,
    
    // Error Messages
    PROBLEM_DETAILS,
    ERROR_RESPONSE
};

// SBI消息状态
enum class SbiMessageStatus {
    PENDING,
    SUCCESS,
    FAILED,
    TIMEOUT
};

// SBI消息类
class SbiMessage {
public:
    SbiMessage(SbiServiceType service, SbiMessageType type, HttpMethod method);
    
    // Getters
    SbiServiceType getServiceType() const { return serviceType_; }
    SbiMessageType getMessageType() const { return messageType_; }
    HttpMethod getHttpMethod() const { return httpMethod_; }
    const std::string& getUri() const { return uri_; }
    const std::string& getBody() const { return body_; }
    const std::map<std::string, std::string>& getHeaders() const { return headers_; }
    SbiMessageStatus getStatus() const { return status_; }
    int getStatusCode() const { return statusCode_; }
    
    // Setters
    void setUri(const std::string& uri) { uri_ = uri; }
    void setBody(const std::string& body) { body_ = body; }
    void addHeader(const std::string& key, const std::string& value) { headers_[key] = value; }
    void setStatus(SbiMessageStatus status) { status_ = status; }
    void setStatusCode(int code) { statusCode_ = code; }
    
    // Utility methods
    std::string toString() const;
    bool isRequest() const;
    bool isResponse() const;
    bool isSuccess() const;

private:
    SbiServiceType serviceType_;
    SbiMessageType messageType_;
    HttpMethod httpMethod_;
    std::string uri_;
    std::string body_;
    std::map<std::string, std::string> headers_;
    SbiMessageStatus status_;
    int statusCode_;
};

// SBI消息处理器接口
class SbiMessageHandler {
public:
    virtual ~SbiMessageHandler() = default;
    virtual void handleSbiMessage(std::shared_ptr<SbiMessage> message) = 0;
};

} // namespace amf_sm

#endif // SBI_MESSAGE_H