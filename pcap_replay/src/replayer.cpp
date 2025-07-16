#include "pcap_parser.h"
#include "packet_modifier.h"
#include "network_utils.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <signal.h>
#include <atomic>

class PcapReplayer {
public:
    PcapReplayer() : running_(false), server_sockfd_(-1), client_sockfd_(-1) {}
    
    ~PcapReplayer() {
        stop();
    }
    
    bool start(const std::string& pcap_file,
               const std::string& original_server_ip,
               const std::string& original_client_ip,
               const std::string& new_server_ip,
               const std::string& new_client_ip,
               uint16_t server_port) {
        
        pcap_file_ = pcap_file;
        original_server_ip_ = original_server_ip;
        original_client_ip_ = original_client_ip;
        new_server_ip_ = new_server_ip;
        new_client_ip_ = new_client_ip;
        server_port_ = server_port;
        
        running_ = true;
        
        // 启动服务端线程
        server_thread_ = std::thread(&PcapReplayer::runServer, this);
        
        // 等待服务端启动
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // 启动客户端线程
        client_thread_ = std::thread(&PcapReplayer::runClient, this);
        
        // 启动回放线程
        replay_thread_ = std::thread(&PcapReplayer::runReplay, this);
        
        return true;
    }
    
    void stop() {
        running_ = false;
        
        if (server_sockfd_ >= 0) {
            NetworkUtils::closeSocket(server_sockfd_);
            server_sockfd_ = -1;
        }
        
        if (client_sockfd_ >= 0) {
            NetworkUtils::closeSocket(client_sockfd_);
            client_sockfd_ = -1;
        }
        
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
        
        if (client_thread_.joinable()) {
            client_thread_.join();
        }
        
        if (replay_thread_.joinable()) {
            replay_thread_.join();
        }
    }
    
    void waitForCompletion() {
        if (replay_thread_.joinable()) {
            replay_thread_.join();
        }
    }

private:
    std::atomic<bool> running_;
    std::string pcap_file_;
    std::string original_server_ip_;
    std::string original_client_ip_;
    std::string new_server_ip_;
    std::string new_client_ip_;
    uint16_t server_port_;
    
    int server_sockfd_;
    int client_sockfd_;
    int server_listen_sockfd_;
    
    std::thread server_thread_;
    std::thread client_thread_;
    std::thread replay_thread_;
    
    void runServer() {
        // 创建监听套接字
        server_listen_sockfd_ = NetworkUtils::createTCPSocket();
        if (server_listen_sockfd_ < 0) {
            std::cerr << "创建服务端套接字失败" << std::endl;
            return;
        }
        
        NetworkUtils::setReuseAddr(server_listen_sockfd_);
        
        if (!NetworkUtils::bindSocket(server_listen_sockfd_, new_server_ip_, server_port_)) {
            std::cerr << "绑定服务端地址失败" << std::endl;
            return;
        }
        
        if (!NetworkUtils::listenSocket(server_listen_sockfd_, 5)) {
            std::cerr << "监听失败" << std::endl;
            return;
        }
        
        std::cout << "服务端监听在 " << new_server_ip_ << ":" << server_port_ << std::endl;
        
        // 接受客户端连接
        std::string client_ip;
        uint16_t client_port;
        server_sockfd_ = NetworkUtils::acceptConnection(server_listen_sockfd_, client_ip, client_port);
        
        if (server_sockfd_ < 0) {
            std::cerr << "接受客户端连接失败" << std::endl;
            return;
        }
        
        std::cout << "客户端已连接: " << client_ip << ":" << client_port << std::endl;
        
        // 接收客户端数据
        std::vector<uint8_t> buffer;
        while (running_) {
            ssize_t received = NetworkUtils::receiveData(server_sockfd_, buffer, 4096);
            if (received <= 0) {
                if (running_) {
                    std::cout << "客户端断开连接" << std::endl;
                }
                break;
            }
            
            std::cout << "服务端收到数据: " << received << " 字节" << std::endl;
            processServerReceivedData(buffer);
        }
    }
    
    void runClient() {
        // 等待服务端启动
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        client_sockfd_ = NetworkUtils::createTCPSocket();
        if (client_sockfd_ < 0) {
            std::cerr << "创建客户端套接字失败" << std::endl;
            return;
        }
        
        if (!NetworkUtils::connectSocket(client_sockfd_, new_server_ip_, server_port_)) {
            std::cerr << "连接服务端失败" << std::endl;
            return;
        }
        
        std::cout << "客户端已连接到服务端" << std::endl;
        
        // 接收服务端数据
        std::vector<uint8_t> buffer;
        while (running_) {
            ssize_t received = NetworkUtils::receiveData(client_sockfd_, buffer, 4096);
            if (received <= 0) {
                if (running_) {
                    std::cout << "与服务端连接断开" << std::endl;
                }
                break;
            }
            
            std::cout << "客户端收到数据: " << received << " 字节" << std::endl;
            processClientReceivedData(buffer);
        }
    }
    
    void runReplay() {
        // 等待连接建立
        std::this_thread::sleep_for(std::chrono::seconds(3));
        
        PcapParser parser;
        if (!parser.openFile(pcap_file_)) {
            std::cerr << "无法打开PCAP文件: " << pcap_file_ << std::endl;
            return;
        }
        
        PacketModifier modifier;
        modifier.setIPMapping(original_server_ip_, original_client_ip_, 
                             new_server_ip_, new_client_ip_);
        
        PacketInfo packet_info;
        size_t packet_count = 0;
        size_t server_packets = 0;
        size_t client_packets = 0;
        
        std::cout << "开始回放PCAP文件: " << pcap_file_ << std::endl;
        
        while (parser.parseNextPacket(packet_info) && running_) {
            packet_count++;
            
            // 判断数据包方向
            std::string src_ip = modifier.ipNetworkToString(packet_info.src_ip);
            std::string dst_ip = modifier.ipNetworkToString(packet_info.dst_ip);
            
            bool is_server_to_client = (src_ip == original_server_ip_ && dst_ip == original_client_ip_);
            bool is_client_to_server = (src_ip == original_client_ip_ && dst_ip == original_server_ip_);
            
            if (!packet_info.payload.empty()) {
                if (is_server_to_client) {
                    // 服务端到客户端的数据包，由服务端发送
                    server_packets++;
                    modifier.modifyPacket(packet_info, true);
                    sendFromServer(packet_info.payload);
                    
                    std::cout << "服务端发送数据包 #" << packet_count 
                             << " (载荷: " << packet_info.payload.size() << " 字节)" << std::endl;
                    
                } else if (is_client_to_server) {
                    // 客户端到服务端的数据包，由客户端发送
                    client_packets++;
                    modifier.modifyPacket(packet_info, false);
                    sendFromClient(packet_info.payload);
                    
                    std::cout << "客户端发送数据包 #" << packet_count 
                             << " (载荷: " << packet_info.payload.size() << " 字节)" << std::endl;
                }
            }
            
            // 添加延时以模拟真实网络环境
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        std::cout << "PCAP回放完成。总数据包: " << packet_count 
                 << ", 服务端发送: " << server_packets 
                 << ", 客户端发送: " << client_packets << std::endl;
    }
    
    void sendFromServer(const std::vector<uint8_t>& data) {
        if (server_sockfd_ < 0) {
            std::cerr << "服务端未连接" << std::endl;
            return;
        }
        
        ssize_t sent = NetworkUtils::sendData(server_sockfd_, data);
        if (sent < 0) {
            std::cerr << "服务端发送数据失败" << std::endl;
        }
    }
    
    void sendFromClient(const std::vector<uint8_t>& data) {
        if (client_sockfd_ < 0) {
            std::cerr << "客户端未连接" << std::endl;
            return;
        }
        
        ssize_t sent = NetworkUtils::sendData(client_sockfd_, data);
        if (sent < 0) {
            std::cerr << "客户端发送数据失败" << std::endl;
        }
    }
    
    void processServerReceivedData(const std::vector<uint8_t>& data) {
        std::cout << "服务端处理数据: ";
        size_t display_size = std::min(data.size(), size_t(32));
        for (size_t i = 0; i < display_size; i++) {
            printf("%02x ", data[i]);
        }
        if (data.size() > 32) {
            std::cout << "...";
        }
        std::cout << std::endl;
    }
    
    void processClientReceivedData(const std::vector<uint8_t>& data) {
        std::cout << "客户端处理数据: ";
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
PcapReplayer* g_replayer = nullptr;

void signalHandler(int signal) {
    std::cout << "\n收到信号 " << signal << "，正在关闭回放程序..." << std::endl;
    if (g_replayer) {
        g_replayer->stop();
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    if (argc < 6) {
        std::cout << "用法: " << argv[0] 
                 << " <PCAP文件> <原始服务端IP> <原始客户端IP> <新服务端IP> <新客户端IP> [端口]" << std::endl;
        std::cout << "示例: " << argv[0] 
                 << " capture.pcap 192.168.1.100 192.168.1.200 127.0.0.1 127.0.0.1 8080" << std::endl;
        return 1;
    }
    
    std::string pcap_file = argv[1];
    std::string original_server_ip = argv[2];
    std::string original_client_ip = argv[3];
    std::string new_server_ip = argv[4];
    std::string new_client_ip = argv[5];
    uint16_t server_port = (argc > 6) ? static_cast<uint16_t>(std::stoi(argv[6])) : 8080;
    
    // 设置信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // 创建并启动回放器
    PcapReplayer replayer;
    g_replayer = &replayer;
    
    std::cout << "启动PCAP回放程序..." << std::endl;
    std::cout << "PCAP文件: " << pcap_file << std::endl;
    std::cout << "原始服务端IP: " << original_server_ip << std::endl;
    std::cout << "原始客户端IP: " << original_client_ip << std::endl;
    std::cout << "新服务端IP: " << new_server_ip << std::endl;
    std::cout << "新客户端IP: " << new_client_ip << std::endl;
    std::cout << "服务端口: " << server_port << std::endl;
    
    if (!replayer.start(pcap_file, original_server_ip, original_client_ip, 
                       new_server_ip, new_client_ip, server_port)) {
        std::cerr << "启动回放程序失败" << std::endl;
        return 1;
    }
    
    // 等待回放完成
    replayer.waitForCompletion();
    
    std::cout << "回放程序运行中，按 Ctrl+C 停止" << std::endl;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}