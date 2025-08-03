#pragma once

#include "SbiMessage.h"
#include <string>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>

namespace http_server {

class HttpServer {
public:
    HttpServer(const std::string& address, int port);
    ~HttpServer();
    
    bool start();
    void stop();
    
    void setMessageHandler(std::function<void(std::shared_ptr<amf_sm::SbiMessage>)> handler);
    
private:
    std::string address_;
    int port_;
    std::atomic<bool> running_;
    std::thread serverThread_;
    std::function<void(std::shared_ptr<amf_sm::SbiMessage>)> messageHandler_;
    
    void serverLoop();
    std::shared_ptr<amf_sm::SbiMessage> createMockSbiMessage();
};

} // namespace http_server