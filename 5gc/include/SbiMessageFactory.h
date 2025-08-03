#ifndef SBI_MESSAGE_FACTORY_H
#define SBI_MESSAGE_FACTORY_H

#include "SbiMessage.h"
#include <memory>
#include <string>

namespace amf_sm {

class SbiMessageFactory {
public:
    // 创建UE上下文相关消息
    static std::shared_ptr<SbiMessage> createUeContextCreateRequest(
        const std::string& supi, const std::string& pei = "");
    
    static std::shared_ptr<SbiMessage> createUeContextReleaseRequest(
        const std::string& ueContextId, const std::string& cause = "DEREGISTRATION");
    
    // 创建认证相关消息
    static std::shared_ptr<SbiMessage> createAuthenticationRequest(
        const std::string& supi, const std::string& servingNetworkName);
    
    // 创建PDU会话相关消息
    static std::shared_ptr<SbiMessage> createPduSessionCreateRequest(
        int pduSessionId, const std::string& dnn, int sst = 1);
    
    static std::shared_ptr<SbiMessage> createPduSessionReleaseRequest(
        const std::string& smContextId, const std::string& cause = "REGULAR_DEACTIVATION");
    
    // 创建策略控制相关消息
    static std::shared_ptr<SbiMessage> createAmPolicyControlCreateRequest(
        const std::string& supi, const std::string& notificationUri);
    
    // 创建NF管理相关消息
    static std::shared_ptr<SbiMessage> createNfRegisterRequest(
        const std::string& nfInstanceId, const std::string& nfType);
    
    static std::shared_ptr<SbiMessage> createNfDiscoverRequest(
        const std::string& targetNfType, const std::string& requesterNfType);

private:
    // 辅助方法：设置通用头部
    static void setCommonHeaders(std::shared_ptr<SbiMessage> message);
    
    // 辅助方法：生成UUID
    static std::string generateUuid();
};

} // namespace amf_sm

#endif // SBI_MESSAGE_FACTORY_H