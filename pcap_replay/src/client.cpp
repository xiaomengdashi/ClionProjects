#include "pcap_parser.h"
#include "packet_modifier.h"
#include "network_utils.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <signal.h>

class PcapClient {
public:
    PcapClient(const std::string& server_ip, uint16_t server_port)
        : server_ip_(server_ip), server_port_(server_port), sockfd_(-1), running_(false) {
    }
    
    ~PcapClient() {
        disconnect();
    }
    
    bool connect() {
        sockfd_ = NetworkUtils::createTCPSocket();
        if (sockfd_ < 0) {
            return false;
        }
        
        if (!NetworkUtils::connectSocket(sockfd_, server_ip_, server_port_)) {
            NetworkUtils::closeSocket(sockfd_);
            sockfd_ = -1;
            return false;
        }
        
        running_ = true;
        std::cout << "连接到服务端: " << server_ip_ << ":" << server_port_ << std::endl;
        
        // 启动接收数据的线程
        receive_thread_ = std::thread(&PcapClient::receiveLoop, this);
        
        return true;
    }
    
    void disconnect() {
        running_ = false;
        
        if (sockfd_ >= 0) {
            NetworkUtils::closeSocket(sockfd_);
            sockfd_ = -1;
        }
        
        if (receive_thread_.joinable()) {
            receive_thread_.join();
        }
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
        size_t client_packets = 0;
        
        std::cout << "开始回放PCAP文件: " << pcap_file << std::endl;
        
        while (parser.parseNextPacket(packet_info) && running_) {
            packet_count++;
            
            // 判断数据包方向
            std::string src_ip = modifier.ipNetworkToString(packet_info.src_ip);
            std::string dst_ip = modifier.ipNetworkToString(packet_info.dst_ip);
            
            bool is_client_to_server = (src_ip == original_client_ip && dst_ip == original_server_ip);
            
            if (is_client_to_server && !packet_info.payload.empty()) {
                // 客户端到服务端的数据包，由客户端发送
                client_packets++;
                
                // 修改数据包IP信息
                modifier.modifyPacket(packet_info, false);
                
                // 发送给服务端
                sendToServer(packet_info.payload);
                
                std::cout << "客户端发送数据包 #" << packet_count 
                         << " (载荷: " << packet_info.payload.size() << " 字节)" << std::endl;
            }
            
            // 添加延时以模拟真实网络环境
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        std::cout << "PCAP回放完成。总数据包: " << packet_count 
                 << ", 客户端发送: " << client_packets << std::endl;
    }
    
    bool isConnected() const {
        return running_ && sockfd_ >= 0;
    }

private:
    std::string server_ip_;
    uint16_t server_port_;
    int sockfd_;
    bool running_;
    std::thread receive_thread_;
    
    void receiveLoop() {
        std::vector<uint8_t> buffer;
        
        while (running_) {
            ssize_t received = NetworkUtils::receiveData(sockfd_, buffer, 4096);
            if (received <= 0) {
                if (running_) {
                    std::cerr << "与服务端连接断开" << std::endl;
                }
                break;
            }
            
            std::cout << "收到服务端数据: " << received << " 字节" << std::endl;
            
            // 这里可以处理服务端发送的数据
            // 可以将数据写入文件或进行其他处理
            processReceivedData(buffer);
        }
        
        running_ = false;
    }
    
    void sendToServer(const std::vector<uint8_t>& data) {
        if (sockfd_ < 0) {
            std::cerr << "未连接到服务端" << std::endl;
            return;
        }
        
        ssize_t sent = NetworkUtils::sendData(sockfd_, data);
        if (sent < 0) {
            std::cerr << "发送数据到服务端失败" << std::endl;
        }
    }
    
    void processReceivedData(const std::vector<uint8_t>& data) {
        // 处理从服务端接收到的数据
        // 这里可以添加数据验证、日志记录等功能
        
        // 简单的十六进制输出（仅显示前32字节）
        std::cout << "数据内容: ";
        size_t display_size = std::min(data.size(), size_t(32));
        for (size_t i = 0; i < display_size; i++) {
            printf("%02x ", data[i]);
        }
        if (data.size() > 32) {
            std::cout << "...";
        }
        std::cout << std::endl;
    }
};

// 全局变量用于信号处理
PcapClient* g_client = nullptr;

void signalHandler(int signal) {
    std::cout << "\n收到信号 " << signal << "，正在关闭客户端..." << std::endl;
    if (g_client) {
        g_client->disconnect();
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    if (argc < 7) {
        std::cout << "用法: " << argv[0] 
                 << " <服务端IP> <服务端端口> <PCAP文件> <原始服务端IP> <原始客户端IP> <新客户端IP> [新服务端IP]" << std::endl;
        std::cout << "示例: " << argv[0] 
                 << " 127.0.0.1 8080 capture.pcap 192.168.1.100 192.168.1.200 127.0.0.1 127.0.0.1" << std::endl;
        return 1;
    }
    
    std::string server_ip = argv[1];
    uint16_t server_port = static_cast<uint16_t>(std::stoi(argv[2]));
    std::string pcap_file = argv[3];
    std::string original_server_ip = argv[4];
    std::string original_client_ip = argv[5];
    std::string new_client_ip = argv[6];
    std::string new_server_ip = (argc > 7) ? argv[7] : "127.0.0.1";
    
    // 设置信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // 创建并连接客户端
    PcapClient client(server_ip, server_port);
    g_client = &client;
    
    if (!client.connect()) {
        std::cerr << "连接服务端失败" << std::endl;
        return 1;
    }
    
    // 等待一段时间确保连接稳定
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 开始回放PCAP文件
    client.replayPcapFile(pcap_file, original_server_ip, original_client_ip, 
                         new_server_ip, new_client_ip);
    
    // 保持客户端运行以接收服务端数据
    std::cout << "等待服务端数据，按 Ctrl+C 停止客户端" << std::endl;
    while (client.isConnected()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}