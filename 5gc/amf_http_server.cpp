#include "include/AmfSm.h"
#include "include/SbiMessage.h"
#include "include/HttpServer.h"
#include "include/AmfConfiguration.h"
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <signal.h>

using namespace amf_sm;

// 前向声明
class HttpSbiServer;

// 全局变量声明
volatile sig_atomic_t g_running = 1;
std::unique_ptr<AmfSm> g_amfSm;
std::unique_ptr<HttpSbiServer> g_sbiServer;

class HttpSbiServer {
private:
    int port_;
    int serverSocket_;
    std::unique_ptr<AmfSm> amfSm_;
    bool running_;

public:
    HttpSbiServer(int port = 8080) : port_(port), serverSocket_(-1), running_(false) {
        amfSm_ = std::make_unique<AmfSm>();
        std::cout << "AMF state machine created. Initial state: " 
                  << getStateString(amfSm_->getCurrentState()) << std::endl;
    }

    ~HttpSbiServer() {
        stop();
    }

    bool start() {
        // 创建socket
        serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket_ < 0) {
            std::cerr << "Error creating server socket" << std::endl;
            return false;
        }

        // 设置socket选项
        int opt = 1;
        if (setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            std::cerr << "Error setting socket options" << std::endl;
            close(serverSocket_);
            return false;
        }

        // 绑定地址
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port_);

        if (bind(serverSocket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Error binding socket to port " << port_ << std::endl;
            close(serverSocket_);
            return false;
        }

        // 开始监听
        if (listen(serverSocket_, 5) < 0) {
            std::cerr << "Error listening on socket" << std::endl;
            close(serverSocket_);
            return false;
        }

        running_ = true;
        std::cout << "\n========================================" << std::endl;
        std::cout << "5G AMF HTTP SBI Server Started" << std::endl;
        std::cout << "Listening on port: " << port_ << std::endl;
        std::cout << "Waiting for HTTP SBI messages..." << std::endl;
        std::cout << "========================================" << std::endl;

        return true;
    }

    void run() {
        while (running_ && g_running) {
            // 设置accept超时，以便能够响应停止信号
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(serverSocket_, &readfds);
            
            struct timeval timeout;
            timeout.tv_sec = 1;  // 1秒超时
            timeout.tv_usec = 0;
            
            int result = select(serverSocket_ + 1, &readfds, NULL, NULL, &timeout);
            if (result < 0) {
                if (errno == EINTR) {
                    // 被信号中断，检查是否需要停止
                    continue;
                }
                std::cerr << "Select error: " << strerror(errno) << std::endl;
                break;
            } else if (result == 0) {
                // 超时，继续循环检查停止条件
                continue;
            }
            
            if (!FD_ISSET(serverSocket_, &readfds)) {
                continue;
            }
            
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            
            int clientSocket = accept(serverSocket_, (struct sockaddr*)&client_addr, &client_len);
            if (clientSocket < 0) {
                if (running_ && g_running) {
                    std::cerr << "Error accepting client connection: " << strerror(errno) << std::endl;
                }
                continue;
            }

            // 处理客户端请求
            std::thread clientThread(&HttpSbiServer::handleClient, this, clientSocket);
            clientThread.detach();
        }
        
        std::cout << "SBI Server run loop exited." << std::endl;
    }

    void stop() {
        running_ = false;
        if (serverSocket_ >= 0) {
            close(serverSocket_);
            serverSocket_ = -1;
        }
    }

private:
    std::string getStateString(UeState state) {
        switch (state) {
            case UeState::DEREGISTERED: return "DEREGISTERED";
            case UeState::REGISTERED_IDLE: return "REGISTERED_IDLE";
            case UeState::REGISTERED_CONNECTED: return "REGISTERED_CONNECTED";
            default: return "UNKNOWN";
        }
    }

    void handleClient(int clientSocket) {
        char buffer[4096];
        ssize_t received = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        
        if (received <= 0) {
            close(clientSocket);
            return;
        }

        buffer[received] = '\0';
        std::string request(buffer);

        std::cout << "\n=== Received HTTP Request ===" << std::endl;
        std::cout << request << std::endl;
        std::cout << "=============================" << std::endl;

        // 解析HTTP请求
        HttpRequest httpReq = parseHttpRequest(request);
        
        // 处理SBI消息
        std::string response = processSbiMessage(httpReq);
        
        // 发送HTTP响应
        send(clientSocket, response.c_str(), response.length(), 0);
        
        close(clientSocket);
    }

    struct HttpRequest {
        std::string method;
        std::string uri;
        std::string version;
        std::map<std::string, std::string> headers;
        std::string body;
    };

    HttpRequest parseHttpRequest(const std::string& request) {
        HttpRequest httpReq;
        std::istringstream stream(request);
        std::string line;

        // 解析请求行
        if (std::getline(stream, line)) {
            std::istringstream lineStream(line);
            lineStream >> httpReq.method >> httpReq.uri >> httpReq.version;
        }

        // 解析头部
        while (std::getline(stream, line) && line != "\r" && !line.empty()) {
            size_t colonPos = line.find(':');
            if (colonPos != std::string::npos) {
                std::string key = line.substr(0, colonPos);
                std::string value = line.substr(colonPos + 1);
                // 去除前后空格
                while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) {
                    value.erase(0, 1);
                }
                while (!value.empty() && (value.back() == '\r' || value.back() == '\n')) {
                    value.pop_back();
                }
                httpReq.headers[key] = value;
            }
        }

        // 解析正文
        std::string bodyLine;
        while (std::getline(stream, bodyLine)) {
            httpReq.body += bodyLine + "\n";
        }
        if (!httpReq.body.empty() && httpReq.body.back() == '\n') {
            httpReq.body.pop_back();
        }

        return httpReq;
    }

    std::string processSbiMessage(const HttpRequest& httpReq) {
        std::cout << "\n=== Processing SBI Message ===" << std::endl;
        std::cout << "Method: " << httpReq.method << std::endl;
        std::cout << "URI: " << httpReq.uri << std::endl;
        std::cout << "Current AMF State: " << getStateString(amfSm_->getCurrentState()) << std::endl;

        // 根据URI确定SBI服务类型和消息类型
        SbiServiceType serviceType = determineServiceType(httpReq.uri);
        SbiMessageType messageType = determineMessageType(httpReq.uri, httpReq.method);
        HttpMethod method = parseHttpMethod(httpReq.method);

        // 创建SBI消息对象
        auto sbiMessage = std::make_shared<SbiMessage>(serviceType, messageType, method);
        sbiMessage->setUri(httpReq.uri);
        sbiMessage->setBody(httpReq.body);
        
        // 设置头部
        for (const auto& header : httpReq.headers) {
            sbiMessage->addHeader(header.first, header.second);
        }

        // 处理消息并更新AMF状态
        amfSm_->handleSbiMessage(sbiMessage);
        
        std::cout << "Message processed successfully" << std::endl;
        std::cout << "New AMF State: " << getStateString(amfSm_->getCurrentState()) << std::endl;
        std::cout << "==============================" << std::endl;

        // 生成HTTP响应
        return generateHttpResponse(true, sbiMessage);
    }

    SbiServiceType determineServiceType(const std::string& uri) {
        if (uri.find("/namf-comm/") != std::string::npos) {
            return SbiServiceType::NAMF_COMMUNICATION;
        } else if (uri.find("/nausf-auth/") != std::string::npos) {
            return SbiServiceType::NAUSF_UE_AUTHENTICATION;
        } else if (uri.find("/nsmf-pdusession/") != std::string::npos) {
            return SbiServiceType::NSMF_PDU_SESSION;
        } else if (uri.find("/npcf-am-policy/") != std::string::npos) {
            return SbiServiceType::NPCF_AM_POLICY_CONTROL;
        } else if (uri.find("/nnrf-nfm/") != std::string::npos) {
            return SbiServiceType::NRF_NFM;
        } else if (uri.find("/nnrf-disc/") != std::string::npos) {
            return SbiServiceType::NRF_NFD;
        }
        return SbiServiceType::NAMF_COMMUNICATION; // 默认
    }

    SbiMessageType determineMessageType(const std::string& uri, const std::string& method) {
        if (uri.find("/ue-contexts") != std::string::npos) {
            if (method == "POST") return SbiMessageType::UE_CONTEXT_CREATE_REQUEST;
            if (method == "PUT") return SbiMessageType::UE_CONTEXT_UPDATE_REQUEST;
            if (method == "DELETE") return SbiMessageType::UE_CONTEXT_RELEASE_REQUEST;
        }
        
        if (uri.find("/authentications") != std::string::npos) {
            return SbiMessageType::UE_AUTHENTICATION_REQUEST;
        }
        
        if (uri.find("/pdu-sessions") != std::string::npos) {
            if (method == "POST") return SbiMessageType::UE_CONTEXT_CREATE_REQUEST; // 使用现有的枚举值
            if (method == "DELETE") return SbiMessageType::UE_CONTEXT_RELEASE_REQUEST; // 使用现有的枚举值
        }
        
        if (uri.find("/registrations") != std::string::npos) {
            return SbiMessageType::UE_CONTEXT_CREATE_REQUEST; // 使用现有的枚举值
        }
        
        if (uri.find("/deregistrations") != std::string::npos) {
            return SbiMessageType::UE_CONTEXT_RELEASE_REQUEST; // 使用现有的枚举值
        }
        
        return SbiMessageType::UE_CONTEXT_CREATE_REQUEST; // 默认值
    }

    HttpMethod parseHttpMethod(const std::string& method) {
        if (method == "GET") return HttpMethod::GET;
        if (method == "POST") return HttpMethod::POST;
        if (method == "PUT") return HttpMethod::PUT;
        if (method == "DELETE") return HttpMethod::DELETE;
        if (method == "PATCH") return HttpMethod::PATCH;
        return HttpMethod::POST; // 默认
    }

    std::string generateHttpResponse(bool success, std::shared_ptr<SbiMessage> message) {
        std::string responseBody = "{\n";
        responseBody += "  \"status\": \"" + std::string(success ? "success" : "error") + "\",\n";
        responseBody += "  \"timestamp\": \"" + getCurrentTimestamp() + "\",\n";
        responseBody += "  \"amfState\": \"" + getStateString(amfSm_->getCurrentState()) + "\",\n";
        responseBody += "  \"processedMessage\": {\n";
        responseBody += "    \"service\": \"" + std::to_string(static_cast<int>(message->getServiceType())) + "\",\n";
        responseBody += "    \"type\": \"" + std::to_string(static_cast<int>(message->getMessageType())) + "\",\n";
        responseBody += "    \"method\": \"" + std::to_string(static_cast<int>(message->getHttpMethod())) + "\",\n";
        responseBody += "    \"uri\": \"" + message->getUri() + "\"\n";
        responseBody += "  }\n";
        responseBody += "}";

        std::string response = "HTTP/1.1 " + std::string(success ? "200 OK" : "400 Bad Request") + "\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + std::to_string(responseBody.length()) + "\r\n";
        response += "Connection: close\r\n";
        response += "\r\n";
        response += responseBody;

        return response;
    }

    std::string getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
};

// 全局变量声明（已在文件开头声明）

// 信号处理函数
void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << " (";
    switch(signal) {
        case SIGINT: std::cout << "SIGINT/Ctrl+C"; break;
        case SIGTERM: std::cout << "SIGTERM"; break;
        default: std::cout << "Unknown"; break;
    }
    std::cout << "), shutting down gracefully..." << std::endl;
    
    g_running = 0;
    
    // 停止SBI服务器
    if (g_sbiServer) {
        g_sbiServer->stop();
    }
    
    // 关闭AMF
    if (g_amfSm) {
        g_amfSm->shutdown();
    }
    
    std::cout << "Shutdown signal processed." << std::endl;
}

// 打印AMF统计信息
void printStatistics() {
    if (!g_amfSm) return;
    
    auto stats = g_amfSm->getStatistics();
    
    std::cout << "\n=== AMF Statistics ===" << std::endl;
    std::cout << "Total UE Registrations: " << stats.totalUeRegistrations << std::endl;
    std::cout << "Active UE Connections: " << stats.activeUeConnections << std::endl;
    std::cout << "Total UE Contexts: " << stats.totalUeContexts << std::endl;
    std::cout << "Total PDU Sessions: " << stats.totalPduSessions << std::endl;
    std::cout << "Active PDU Sessions: " << stats.activePduSessions << std::endl;
    std::cout << "Total Handovers: " << stats.totalHandovers << std::endl;
    std::cout << "Authentication Attempts: " << stats.totalAuthenticationAttempts << std::endl;
    std::cout << "Successful Authentications: " << stats.successfulAuthentications << std::endl;
    std::cout << "Total SBI Messages: " << stats.totalSbiMessages << std::endl;
    std::cout << "Total N1 Messages: " << stats.totalN1Messages << std::endl;
    std::cout << "Total N2 Messages: " << stats.totalN2Messages << std::endl;
    std::cout << "Average Response Time: " << stats.averageResponseTime << " ms" << std::endl;
    std::cout << "System Load: " << stats.systemLoad << "%" << std::endl;
    std::cout << "Memory Usage: " << stats.memoryUsage << "%" << std::endl;
    std::cout << "CPU Usage: " << stats.cpuUsage << "%" << std::endl;
    std::cout << "Registered NF Instances: " << stats.registeredNfInstances << std::endl;
    std::cout << "Healthy NF Instances: " << stats.healthyNfInstances << std::endl;
    std::cout << "========================" << std::endl;
}

// 健康检查线程
void healthCheckLoop() {
    std::cout << "Health check thread started." << std::endl;
    while (g_running) {
        // 使用较短的睡眠间隔，以便能够快速响应停止信号
        for (int i = 0; i < 60 && g_running; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        if (g_running && g_amfSm && !g_amfSm->performHealthCheck()) {
            std::cout << "AMF health check failed!" << std::endl;
        }
    }
    std::cout << "Health check thread exited." << std::endl;
}

// 统计信息打印线程
void statisticsLoop() {
    std::cout << "Statistics thread started." << std::endl;
    while (g_running) {
        // 使用较短的睡眠间隔，以便能够快速响应停止信号
        for (int i = 0; i < 30 && g_running; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        if (g_running) {
            printStatistics();
        }
    }
    std::cout << "Statistics thread exited." << std::endl;
}

int main() {
    std::cout << "Starting 5G AMF HTTP Server..." << std::endl;
    
    // 注册信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGPIPE, SIG_IGN); // 忽略SIGPIPE信号
    
    try {
        // 获取AMF配置
        AmfConfiguration config = getDefaultConfiguration();
        std::cout << "AMF Instance ID: " << config.amfInstanceId << std::endl;
        std::cout << "AMF Name: " << config.amfName << std::endl;
        std::cout << "PLMN ID: " << config.plmnId << std::endl;
        
        // 创建AMF状态机
        g_amfSm = std::make_unique<AmfSm>();
        
        // 创建简化的SBI服务器
        g_sbiServer = std::make_unique<HttpSbiServer>(8080);
        
        // 启动SBI服务器
        if (!g_sbiServer->start()) {
            std::cerr << "Failed to start SBI server" << std::endl;
            return 1;
        }
        
        std::cout << "AMF HTTP Server started successfully!" << std::endl;
        std::cout << "Press Ctrl+C to stop the server." << std::endl;
        
        // 启动健康检查线程
        std::thread healthThread(healthCheckLoop);
        
        // 启动统计信息线程  
        std::thread statsThread(statisticsLoop);
        
        // 在单独线程中运行服务器
        std::thread serverThread([&]() {
            g_sbiServer->run();
        });
        
        // 主循环 - 检查停止条件
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        std::cout << "Stopping server..." << std::endl;
        
        // 等待服务器线程结束
        if (serverThread.joinable()) {
            serverThread.join();
        }
        
        // 等待其他线程结束
        if (healthThread.joinable()) {
            healthThread.join();
        }
        if (statsThread.joinable()) {
            statsThread.join();
        }
        
        std::cout << "AMF HTTP Server stopped gracefully." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        g_running = 0;
        return 1;
    }
    
    return 0;
}