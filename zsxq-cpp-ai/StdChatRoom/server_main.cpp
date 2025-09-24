#include "ChatServer.h"
#include <iostream>
#include <csignal>
#include <memory>

// 全局服务器实例，用于信号处理
std::unique_ptr<ChatServer> globalServer;

// 信号处理函数 - 优雅地关闭服务器
void signalHandler(int signal) {
    std::cout << "\n收到信号 " << signal << "，正在关闭服务器..." << std::endl;
    
    if (globalServer) {
        globalServer->stop();
    }
    
    exit(0);
}

int main(int argc, char* argv[]) {
    int port = 9999; // 默认端口
    
    // 解析命令行参数
    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
            if (port < 1024 || port > 65535) {
                std::cerr << "错误: 端口号必须在1024-65535之间" << std::endl;
                return 1;
            }
        } catch (const std::exception& e) {
            std::cerr << "错误: 无效的端口号" << std::endl;
            return 1;
        }
    }
    
    std::cout << "=== C++多人聊天室服务器 ===" << std::endl;
    std::cout << "版本: 1.0" << std::endl;
    std::cout << "功能: 支持用户注册/登录、群聊、私聊、账号持久存储" << std::endl;
    std::cout << "============================" << std::endl;
    
    try {
        // 创建服务器实例
        globalServer.reset(new ChatServer(port));
        
        // 注册信号处理函数
        signal(SIGINT, signalHandler);   // Ctrl+C
        signal(SIGTERM, signalHandler);  // 终止信号
        
        std::cout << "服务器正在启动..." << std::endl;
        
        // 启动服务器
        if (!globalServer->start()) {
            std::cerr << "服务器启动失败!" << std::endl;
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "服务器运行时出现异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

/*
 * 编译命令:
 * g++ -std=c++11 -pthread -o server server_main.cpp ChatServer.cpp UserManager.cpp User.cpp Message.cpp
 * 
 * 运行命令:
 * ./server [端口号]
 * 
 * 示例:
 * ./server 9999
 */