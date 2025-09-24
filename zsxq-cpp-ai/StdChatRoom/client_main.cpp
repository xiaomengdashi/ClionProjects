#include "ChatClient.h"
#include <iostream>
#include <csignal>
#include <memory>

// 全局客户端实例，用于信号处理
std::unique_ptr<ChatClient> globalClient;

// 信号处理函数 - 优雅地关闭客户端
void signalHandler(int signal) {
    std::cout << "\n收到信号 " << signal << "，正在断开连接..." << std::endl;
    
    if (globalClient) {
        globalClient->disconnect();
    }
    
    exit(0);
}

void showUsage(const char* programName) {
    std::cout << "使用方法: " << programName << " [服务器IP] [端口号]" << std::endl;
    std::cout << "默认连接到 127.0.0.1:9999" << std::endl;
    std::cout << "示例: " << programName << " 192.168.1.100 9999" << std::endl;
}

int main(int argc, char* argv[]) {
    std::string serverIP = "127.0.0.1"; // 默认服务器IP
    int serverPort = 9999;               // 默认端口
    
    // 解析命令行参数
    if (argc > 1) {
        if (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help") {
            showUsage(argv[0]);
            return 0;
        }
        
        serverIP = argv[1];
        
        if (argc > 2) {
            try {
                serverPort = std::stoi(argv[2]);
                if (serverPort < 1024 || serverPort > 65535) {
                    std::cerr << "错误: 端口号必须在1024-65535之间" << std::endl;
                    return 1;
                }
            } catch (const std::exception& e) {
                std::cerr << "错误: 无效的端口号" << std::endl;
                return 1;
            }
        }
    }
    
    std::cout << "=== C++多人聊天室客户端 ===" << std::endl;
    std::cout << "版本: 1.0" << std::endl;
    std::cout << "功能: 支持用户注册/登录、群聊、私聊" << std::endl;
    std::cout << "目标服务器: " << serverIP << ":" << serverPort << std::endl;
    std::cout << "============================" << std::endl;
    
    try {
        // 创建客户端实例
        globalClient.reset(new ChatClient(serverIP, serverPort));
        
        // 注册信号处理函数
        signal(SIGINT, signalHandler);   // Ctrl+C
        signal(SIGTERM, signalHandler);  // 终止信号
        
        // 运行客户端
        globalClient->run();
        
    } catch (const std::exception& e) {
        std::cerr << "客户端运行时出现异常: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "客户端已退出" << std::endl;
    return 0;
}

/*
 * 编译命令:
 * g++ -std=c++11 -pthread -o client client_main.cpp ChatClient.cpp Message.cpp
 * 
 * 运行命令:
 * ./client [服务器IP] [端口号]
 * 
 * 示例:
 * ./client
 * ./client 192.168.1.100 9999
 */