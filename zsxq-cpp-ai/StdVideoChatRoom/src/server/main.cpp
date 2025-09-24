#include <iostream>
#include <signal.h>
#include "ChatServer.h"

std::unique_ptr<ChatServer> g_server;

/**
 * 信号处理函数
 */
void signalHandler(int signal) {
    std::cout << "\n收到信号 " << signal << "，正在关闭服务器..." << std::endl;
    if (g_server) {
        g_server->stop();
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    // 设置信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // 默认端口
    int https_port = 8443;
    int ws_port = 9443;
    
    // 解析命令行参数
    if (argc > 1) {
        https_port = std::atoi(argv[1]);
    }
    if (argc > 2) {
        ws_port = std::atoi(argv[2]);
    }
    
    try {
        std::cout << "启动视频聊天服务器..." << std::endl;
        std::cout << "HTTPS端口: " << https_port << std::endl;
        std::cout << "WebSocket端口: " << ws_port << std::endl;
        
        // 创建并启动服务器
        g_server.reset(new ChatServer("certificates/server.crt", "certificates/server.key"));
        g_server->start(https_port, ws_port);
        
    } catch (const std::exception& e) {
        std::cerr << "服务器错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 