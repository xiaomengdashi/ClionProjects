#include "pcap_parser.h"
#include "packet_modifier.h"
#include "network_utils.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <signal.h>

class PcapServer {
public:
    PcapServer(const std::string& listen_ip, uint16_t listen_port)
        : listen_ip_(listen_ip), listen_port_(listen_port), running_(false) {
    }
    
    ~PcapServer() {
        stop();
    }
    
    bool start() {
        // 创建监听套接字
        listen_sockfd_ = NetworkUtils::createTCPSocket();
        if (listen_sockfd_ < 0) {
            return false;
        }
        
        // 设置地址重用
        NetworkUtils::setReuseAddr(listen_sockfd_);
        
        // 绑定地址
        if (!NetworkUtils::bindSocket(listen_sockfd_, listen_ip_, listen_port_)) {
            NetworkUtils::closeSocket(listen_sockfd_);
            return false;
        }
        
        // 开始监听
        if (!NetworkUtils::listenSocket(listen_sockfd_)) {
            NetworkUtils::closeSocket(listen_sockfd_);
            return false;
        }
        
        running_ = true;
        std::cout << "PCAP服务端启动，监听地址: " << listen_ip_ << ":" << listen_port_ << std::endl;
        
        // 启动接受连接的线程
        accept_thread_ = std::thread(&PcapServer::acceptLoop, this);
        
        return true;
    }
    
    void stop() {
        running_ = false;
        
        if (listen_sockfd_ >= 0) {
            NetworkUtils::closeSocket(listen_sockfd_);
            listen_sockfd_ = -1;
        }
        
        if (accept_thread_.joinable()) {
            accept_thread_.join();
        }
        
        // 关闭所有客户端连接
        for (auto& client : clients_) {
            NetworkUtils::closeSocket(client.sockfd);
        }
        clients_.clear();
    }
    
    void replayPcapFile(const std::string& pcap_file, 
                       const std::string& original_server_ip,
                       const std::string& original_client_ip,
                       const std::string& new_server_ip,
                       const std::string& new_client_ip) {
        
        PcapParser parser;
        if (!parser.openFile(pcap_file)) {
            std::cerr << "无法打开PCAP文件: " << pcap_file << std::endl;
            return;
        }
        
        PacketModifier modifier;
        modifier.setIPMapping(original_server_ip, original_client_ip, 
                             new_server_ip, new_client_ip);
        
        PacketInfo packet_info;
        size_t packet_count = 0;
        size_t server_packets = 0;
        
        std::cout << "开始回放PCAP文件: " << pcap_file << std::endl;
        
        while (parser.parseNextPacket(packet_info) && running_) {
            packet_count++;
            
            // 判断数据包方向
            std::string src_ip = modifier.ipNetworkToString(packet_info.src_ip);
            std::string dst_ip = modifier.ipNetworkToString(packet_info.dst_ip);
            
            bool is_server_to_client = (src_ip == original_server_ip && dst_ip == original_client_ip);
            
            if (is_server_to_client && !packet_info.payload.empty()) {
                // 服务端到客户端的数据包，由服务端发送
                server_packets++;
                
                // 修改数据包IP信息
                modifier.modifyPacket(packet_info, true);
                
                // 发送给所有连接的客户端
                sendToAllClients(packet_info.payload);
                
                std::cout << "服务端发送数据包 #" << packet_count 
                         << " (载荷: " << packet_info.payload.size() << " 字节)" << std::endl;
            }
            
            // 添加延时以模拟真实网络环境
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        std::cout << "PCAP回放完成。总数据包: " << packet_count 
                 << ", 服务端发送: " << server_packets << std::endl;
    }

private:
    struct ClientInfo {
        int sockfd;
        std::string ip;
        uint16_t port;
        std::thread handler_thread;
    };
    
    std::string listen_ip_;
    uint16_t listen_port_;
    int listen_sockfd_;
    bool running_;
    std::thread accept_thread_;
    std::vector<ClientInfo> clients_;
    
    void acceptLoop() {
        while (running_) {
            std::string client_ip;
            uint16_t client_port;
            
            int client_sockfd = NetworkUtils::acceptConnection(listen_sockfd_, client_ip, client_port);
            if (client_sockfd < 0) {
                if (running_) {
                    std::cerr << "接受客户端连接失败" << std::endl;
                }
                continue;
            }
            
            std::cout << "客户端连接: " << client_ip << ":" << client_port << std::endl;
            
            // 创建客户端信息
            ClientInfo client_info;
            client_info.sockfd = client_sockfd;
            client_info.ip = client_ip;
            client_info.port = client_port;
            client_info.handler_thread = std::thread(&PcapServer::handleClient, this, client_sockfd);
            
            clients_.push_back(std::move(client_info));
        }
    }
    
    void handleClient(int client_sockfd) {
        std::vector<uint8_t> buffer;
        
        while (running_) {
            ssize_t received = NetworkUtils::receiveData(client_sockfd, buffer, 4096);
            if (received <= 0) {
                break; // 客户端断开连接
            }
            
            std::cout << "收到客户端数据: " << received << " 字节" << std::endl;
            
            // 这里可以处理客户端发送的数据
            // 在实际的PCAP回放中，客户端的数据包会在回放器中处理
        }
        
        std::cout << "客户端断开连接" << std::endl;
        NetworkUtils::closeSocket(client_sockfd);
    }
    
    void sendToAllClients(const std::vector<uint8_t>& data) {
        for (auto& client : clients_) {
            ssize_t sent = NetworkUtils::sendData(client.sockfd, data);
            if (sent < 0) {
                std::cerr << "发送数据到客户端失败" << std::endl;
            }
        }
    }
};

// 全局变量用于信号处理
PcapServer* g_server = nullptr;

void signalHandler(int signal) {
    std::cout << "\n收到信号 " << signal << "，正在关闭服务端..." << std::endl;
    if (g_server) {
        g_server->stop();
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    if (argc < 7) {
        std::cout << "用法: " << argv[0] 
                 << " <监听IP> <监听端口> <PCAP文件> <原始服务端IP> <原始客户端IP> <新服务端IP> [新客户端IP]" << std::endl;
        std::cout << "示例: " << argv[0] 
                 << " 0.0.0.0 8080 capture.pcap 192.168.1.100 192.168.1.200 127.0.0.1 127.0.0.1" << std::endl;
        return 1;
    }
    
    std::string listen_ip = argv[1];
    uint16_t listen_port = static_cast<uint16_t>(std::stoi(argv[2]));
    std::string pcap_file = argv[3];
    std::string original_server_ip = argv[4];
    std::string original_client_ip = argv[5];
    std::string new_server_ip = argv[6];
    std::string new_client_ip = (argc > 7) ? argv[7] : "127.0.0.1";
    
    // 设置信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // 创建并启动服务端
    PcapServer server(listen_ip, listen_port);
    g_server = &server;
    
    if (!server.start()) {
        std::cerr << "启动服务端失败" << std::endl;
        return 1;
    }
    
    // 等待一段时间让客户端连接
    std::cout << "等待客户端连接..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // 开始回放PCAP文件
    server.replayPcapFile(pcap_file, original_server_ip, original_client_ip, 
                         new_server_ip, new_client_ip);
    
    // 保持服务端运行
    std::cout << "按 Ctrl+C 停止服务端" << std::endl;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}