#include "SbiMessageFactory.h"
#include <sstream>
#include <random>
#include <iomanip>

namespace amf_sm {

std::shared_ptr<SbiMessage> SbiMessageFactory::createUeContextCreateRequest(
    const std::string& supi, const std::string& pei) {
    
    auto message = std::make_shared<SbiMessage>(
        SbiServiceType::NAMF_COMMUNICATION,
        SbiMessageType::UE_CONTEXT_CREATE_REQUEST,
        HttpMethod::POST
    );
    
    message->setUri("/namf-comm/v1/ue-contexts");
    
    std::stringstream body;
    body << "{\"supi\":\"" << supi << "\"";
    if (!pei.empty()) {
        body << ",\"pei\":\"" << pei << "\"";
    }
    body << ",\"gpsi\":\"msisdn-" << supi.substr(5) << "\"";
    body << ",\"accessType\":\"3GPP_ACCESS\"";
    body << ",\"ratType\":\"NR\"";
    body << "}";
    
    message->setBody(body.str());
    setCommonHeaders(message);
    
    return message;
}

std::shared_ptr<SbiMessage> SbiMessageFactory::createUeContextReleaseRequest(
    const std::string& ueContextId, const std::string& cause) {
    
    auto message = std::make_shared<SbiMessage>(
        SbiServiceType::NAMF_COMMUNICATION,
        SbiMessageType::UE_CONTEXT_RELEASE_REQUEST,
        HttpMethod::POST
    );
    
    message->setUri("/namf-comm/v1/ue-contexts/" + ueContextId + "/release");
    
    std::stringstream body;
    body << "{\"cause\":\"" << cause << "\"";
    body << ",\"ngApCause\":{\"group\":\"radioNetwork\",\"value\":\"normal-release\"}";
    body << "}";
    
    message->setBody(body.str());
    setCommonHeaders(message);
    
    return message;
}

std::shared_ptr<SbiMessage> SbiMessageFactory::createAuthenticationRequest(
    const std::string& supi, const std::string& servingNetworkName) {
    
    auto message = std::make_shared<SbiMessage>(
        SbiServiceType::NAUSF_UE_AUTHENTICATION,
        SbiMessageType::UE_AUTHENTICATION_REQUEST,
        HttpMethod::POST
    );
    
    message->setUri("/nausf-auth/v1/ue-authentications");
    
    std::stringstream body;
    body << "{\"supi\":\"" << supi << "\"";
    body << ",\"servingNetworkName\":\"" << servingNetworkName << "\"";
    body << ",\"resynchronizationInfo\":{\"rand\":\"" << generateUuid() << "\"}";
    body << ",\"traceData\":{\"traceRef\":\"" << generateUuid() << "\"}";
    body << "}";
    
    message->setBody(body.str());
    setCommonHeaders(message);
    
    return message;
}

std::shared_ptr<SbiMessage> SbiMessageFactory::createPduSessionCreateRequest(
    int pduSessionId, const std::string& dnn, int sst) {
    
    auto message = std::make_shared<SbiMessage>(
        SbiServiceType::NSMF_PDU_SESSION,
        SbiMessageType::PDU_SESSION_CREATE_SM_CONTEXT_REQUEST,
        HttpMethod::POST
    );
    
    message->setUri("/nsmf-pdusession/v1/sm-contexts");
    
    std::stringstream body;
    body << "{\"pduSessionId\":" << pduSessionId;
    body << ",\"dnn\":\"" << dnn << "\"";
    body << ",\"sNssai\":{\"sst\":" << sst << "}";
    body << ",\"pduSessionType\":\"IPV4\"";
    body << ",\"requestType\":\"INITIAL_REQUEST\"";
    body << ",\"priority\":\"PRIORITY_LEVEL_1\"";
    body << "}";
    
    message->setBody(body.str());
    setCommonHeaders(message);
    
    return message;
}

std::shared_ptr<SbiMessage> SbiMessageFactory::createPduSessionReleaseRequest(
    const std::string& smContextId, const std::string& cause) {
    
    auto message = std::make_shared<SbiMessage>(
        SbiServiceType::NSMF_PDU_SESSION,
        SbiMessageType::PDU_SESSION_RELEASE_SM_CONTEXT_REQUEST,
        HttpMethod::POST
    );
    
    message->setUri("/nsmf-pdusession/v1/sm-contexts/" + smContextId + "/release");
    
    std::stringstream body;
    body << "{\"cause\":\"" << cause << "\"";
    body << ",\"ngApCause\":{\"group\":\"nas\",\"value\":\"normal-release\"}";
    body << ",\"5gMmCauseValue\":\"REGULAR_DEACTIVATION\"";
    body << "}";
    
    message->setBody(body.str());
    setCommonHeaders(message);
    
    return message;
}

std::shared_ptr<SbiMessage> SbiMessageFactory::createAmPolicyControlCreateRequest(
    const std::string& supi, const std::string& notificationUri) {
    
    auto message = std::make_shared<SbiMessage>(
        SbiServiceType::NPCF_AM_POLICY_CONTROL,
        SbiMessageType::AM_POLICY_CONTROL_CREATE_REQUEST,
        HttpMethod::POST
    );
    
    message->setUri("/npcf-am-policy-control/v1/policies");
    
    std::stringstream body;
    body << "{\"supi\":\"" << supi << "\"";
    body << ",\"notificationUri\":\"" << notificationUri << "\"";
    body << ",\"accessType\":\"3GPP_ACCESS\"";
    body << ",\"ratType\":\"NR\"";
    body << ",\"servingPlmn\":{\"mcc\":\"001\",\"mnc\":\"001\"}";
    body << ",\"userLocationInfo\":{\"nrLocation\":{\"tai\":{\"plmnId\":{\"mcc\":\"001\",\"mnc\":\"001\"},\"tac\":\"000001\"}}}";
    body << "}";
    
    message->setBody(body.str());
    setCommonHeaders(message);
    
    return message;
}

std::shared_ptr<SbiMessage> SbiMessageFactory::createNfRegisterRequest(
    const std::string& nfInstanceId, const std::string& nfType) {
    
    auto message = std::make_shared<SbiMessage>(
        SbiServiceType::NRF_NFM,
        SbiMessageType::NF_REGISTER_REQUEST,
        HttpMethod::PUT
    );
    
    message->setUri("/nnrf-nfm/v1/nf-instances/" + nfInstanceId);
    
    std::stringstream body;
    body << "{\"nfInstanceId\":\"" << nfInstanceId << "\"";
    body << ",\"nfType\":\"" << nfType << "\"";
    body << ",\"nfStatus\":\"REGISTERED\"";
    body << ",\"plmnList\":[{\"mcc\":\"001\",\"mnc\":\"001\"}]";
    body << ",\"nfServices\":[{\"serviceInstanceId\":\"service-1\",\"serviceName\":\"namf-comm\"}]";
    body << ",\"heartBeatTimer\":60";
    body << "}";
    
    message->setBody(body.str());
    setCommonHeaders(message);
    
    return message;
}

std::shared_ptr<SbiMessage> SbiMessageFactory::createNfDiscoverRequest(
    const std::string& targetNfType, const std::string& requesterNfType) {
    
    auto message = std::make_shared<SbiMessage>(
        SbiServiceType::NRF_NFD,
        SbiMessageType::NF_DISCOVER_REQUEST,
        HttpMethod::GET
    );
    
    std::stringstream uri;
    uri << "/nnrf-disc/v1/nf-instances";
    uri << "?target-nf-type=" << targetNfType;
    uri << "&requester-nf-type=" << requesterNfType;
    
    message->setUri(uri.str());
    setCommonHeaders(message);
    
    return message;
}

void SbiMessageFactory::setCommonHeaders(std::shared_ptr<SbiMessage> message) {
    message->addHeader("Content-Type", "application/json");
    message->addHeader("Accept", "application/json");
    message->addHeader("User-Agent", "5G-AMF/1.0");
    message->addHeader("3gpp-Sbi-Target-apiRoot", "https://example.com");
    message->addHeader("3gpp-Sbi-Request-Id", generateUuid());
}

std::string SbiMessageFactory::generateUuid() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << std::hex;
    
    for (int i = 0; i < 8; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; i++) ss << dis(gen);
    ss << "-4";
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    ss << dis(gen);
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 12; i++) ss << dis(gen);
    
    return ss.str();
}

} // namespace amf_sm