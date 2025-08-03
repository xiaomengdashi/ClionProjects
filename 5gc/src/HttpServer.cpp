#include "HttpServer.h"
#include <iostream>
#include <sstream>
#include <thread>

namespace http_server {

HttpServer::HttpServer(const std::string& address, int port) 
    : address_(address), port_(port), running_(false) {
}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::start() {
    if (running_) {
        std::cout << "HTTP Server already running" << std::endl;
        return true;
    }
    
    std::cout << "Starting HTTP Server on " << address_ << ":" << port_ << std::endl;
    
    // 模拟HTTP服务器启动
    running_ = true;
    
    // 启动服务器线程
    serverThread_ = std::thread(&HttpServer::serverLoop, this);
    
    std::cout << "HTTP Server started successfully" << std::endl;
    return true;
}

void HttpServer::stop() {
    if (!running_) return;
    
    std::cout << "Stopping HTTP Server..." << std::endl;
    running_ = false;
    
    if (serverThread_.joinable()) {
        serverThread_.join();
    }
    
    std::cout << "HTTP Server stopped" << std::endl;
}

void HttpServer::setMessageHandler(std::function<void(std::shared_ptr<amf_sm::SbiMessage>)> handler) {
    messageHandler_ = handler;
}

void HttpServer::serverLoop() {
    std::cout << "HTTP Server listening on " << address_ << ":" << port_ << std::endl;
    
    while (running_) {
        // 模拟处理HTTP请求
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // 模拟接收到SBI消息
        if (messageHandler_ && (rand() % 1000) < 5) { // 0.5%概率模拟接收消息
            auto message = createMockSbiMessage();
            if (message) {
                messageHandler_(message);
            }
        }
    }
}

std::shared_ptr<amf_sm::SbiMessage> HttpServer::createMockSbiMessage() {
    using namespace amf_sm;
    
    // 随机创建不同类型的SBI消息
    int msgType = rand() % 4;
    
    switch (msgType) {
        case 0: {
            auto message = std::make_shared<SbiMessage>(
                SbiServiceType::NAMF_COMMUNICATION,
                SbiMessageType::UE_CONTEXT_CREATE_REQUEST,
                HttpMethod::POST
            );
            message->setUri("/namf-comm/v1/ue-contexts");
            message->setBody("{\"supi\":\"imsi-460001234567890\",\"pei\":\"imeisv-1234567890123456\"}");
            message->addHeader("Content-Type", "application/json");
            return message;
        }
        
        case 1: {
            auto message = std::make_shared<SbiMessage>(
                SbiServiceType::NSMF_PDU_SESSION,
                SbiMessageType::PDU_SESSION_CREATE_SM_CONTEXT_REQUEST,
                HttpMethod::POST
            );
            message->setUri("/nsmf-pdusession/v1/sm-contexts");
            message->setBody("{\"pduSessionId\":5,\"dnn\":\"internet\",\"sNssai\":{\"sst\":1}}");
            message->addHeader("Content-Type", "application/json");
            return message;
        }
        
        case 2: {
            auto message = std::make_shared<SbiMessage>(
                SbiServiceType::NAUSF_UE_AUTHENTICATION,
                SbiMessageType::UE_AUTHENTICATION_REQUEST,
                HttpMethod::POST
            );
            message->setUri("/nausf-auth/v1/ue-authentications");
            message->setBody("{\"supiOrSuci\":\"imsi-460001234567890\",\"servingNetworkName\":\"5G:mnc001.mcc460.3gppnetwork.org\"}");
            message->addHeader("Content-Type", "application/json");
            return message;
        }
        
        case 3: {
            auto message = std::make_shared<SbiMessage>(
                SbiServiceType::NPCF_AM_POLICY_CONTROL,
                SbiMessageType::AM_POLICY_CONTROL_CREATE_REQUEST,
                HttpMethod::POST
            );
            message->setUri("/npcf-am-policy-control/v1/policies");
            message->setBody("{\"supi\":\"imsi-460001234567890\",\"notificationUri\":\"http://amf.5gc.mnc001.mcc460.3gppnetwork.org:8080/namf-callback/v1/am-policy\"}");
            message->addHeader("Content-Type", "application/json");
            return message;
        }
        
        default:
            return nullptr;
    }
}

} // namespace http_server