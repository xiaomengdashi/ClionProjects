/*
 * FTP服务器主程序
 * 实现FTP服务器的启动、监听和客户端连接管理
 */
#include <iostream>
#include <thread>
#include <vector>
#include <algorithm>
#include <csignal>
#include <cstring>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include "../include/ftp_protocol.h"
#include "../include/ftp_session.h"

using namespace FTP;

// 全局运行标志
std::atomic<bool> g_running(true);

// 信号处理函数
void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\n正在关闭服务器..." << std::endl;
        g_running = false;
    }
}

// FTP服务器类
class FTPServer {
public:
    // 构造函数
    FTPServer(int port = Config::DEFAULT_CONTROL_PORT, const std::string& rootDir = "/tmp/ftp")
        : port_(port), rootDirectory_(rootDir), listenSocket_(-1) {
        // 创建根目录（使用系统调用）
        mkdir(rootDirectory_.c_str(), 0755);
    }
    
    // 析构函数
    ~FTPServer() {
        stop();
    }
    
    // 启动服务器
    bool start() {
        std::cout << "FTP服务器正在启动..." << std::endl;
        std::cout << "端口: " << port_ << std::endl;
        std::cout << "根目录: " << rootDirectory_ << std::endl;
        
        // 创建监听套接字
        listenSocket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listenSocket_ < 0) {
            std::cerr << "创建套接字失败: " << strerror(errno) << std::endl;
            return false;
        }
        
        // 设置套接字选项
        int optval = 1;
        if (setsockopt(listenSocket_, SOL_SOCKET, SO_REUSEADDR, 
                      &optval, sizeof(optval)) < 0) {
            std::cerr << "设置套接字选项失败: " << strerror(errno) << std::endl;
            close(listenSocket_);
            return false;
        }
        
        // 绑定地址
        struct sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port_);
        
        if (bind(listenSocket_, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            std::cerr << "绑定端口失败: " << strerror(errno) << std::endl;
            std::cerr << "请检查端口 " << port_ << " 是否已被占用" << std::endl;
            close(listenSocket_);
            return false;
        }
        
        // 开始监听
        if (listen(listenSocket_, Config::LISTEN_BACKLOG) < 0) {
            std::cerr << "监听失败: " << strerror(errno) << std::endl;
            close(listenSocket_);
            return false;
        }
        
        std::cout << "FTP服务器启动成功，正在监听端口 " << port_ << "..." << std::endl;
        std::cout << "按 Ctrl+C 停止服务器" << std::endl;
        
        // 设置监听套接字为非阻塞模式
        int flags = fcntl(listenSocket_, F_GETFL, 0);
        fcntl(listenSocket_, F_SETFL, flags | O_NONBLOCK);
        
        return true;
    }
    
    // 运行服务器主循环
    void run() {
        while (g_running) {
            // 接受新连接
            struct sockaddr_in clientAddr;
            socklen_t clientLen = sizeof(clientAddr);
            
            int clientSocket = accept(listenSocket_, 
                                    (struct sockaddr*)&clientAddr, &clientLen);
            
            if (clientSocket >= 0) {
                // 获取客户端信息
                char clientIP[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
                int clientPort = ntohs(clientAddr.sin_port);
                
                std::cout << "新客户端连接: " << clientIP << ":" << clientPort << std::endl;
                
                // 创建会话并处理
                handleClient(clientSocket, clientAddr);
            } else {
                // 检查是否是因为非阻塞导致的错误
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    std::cerr << "接受连接失败: " << strerror(errno) << std::endl;
                }
                
                // 短暂休眠，避免忙等待
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            // 定期清理超时会话
            static auto lastCleanup = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - lastCleanup).count() > 60) {
                SessionManager::getInstance().cleanupTimeoutSessions();
                lastCleanup = now;
            }
        }
    }
    
    // 停止服务器
    void stop() {
        g_running = false;
        
        if (listenSocket_ >= 0) {
            close(listenSocket_);
            listenSocket_ = -1;
        }
        
        // 等待所有客户端处理线程结束
        for (auto& thread : clientThreads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        clientThreads_.clear();
        
        std::cout << "FTP服务器已停止" << std::endl;
    }
    
private:
    // 处理客户端连接
    void handleClient(int clientSocket, const sockaddr_in& clientAddr) {
        // 创建新线程处理客户端
        clientThreads_.emplace_back([clientSocket, clientAddr, this]() {
            // 创建会话
            auto session = SessionManager::getInstance().createSession(clientSocket, clientAddr);
            
            // 启动会话处理
            session->start();
            
            // 等待会话结束
            while (session->getState() != SessionState::DISCONNECTED && g_running) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            
            // 移除会话
            SessionManager::getInstance().removeSession(session);
            
            // 获取客户端信息用于日志
            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
            std::cout << "客户端断开连接: " << clientIP << std::endl;
            std::cout << "当前活动会话数: " 
                     << SessionManager::getInstance().getSessionCount() << std::endl;
        });
        
        // 清理已结束的线程
        clientThreads_.erase(
            std::remove_if(clientThreads_.begin(), clientThreads_.end(),
                          [](std::thread& t) {
                              if (t.joinable()) {
                                  t.join();
                                  return true;
                              }
                              return false;
                          }),
            clientThreads_.end()
        );
    }
    
private:
    int port_;                              // 监听端口
    std::string rootDirectory_;             // FTP根目录
    int listenSocket_;                      // 监听套接字
    std::vector<std::thread> clientThreads_; // 客户端处理线程列表
};

// 主函数
int main(int argc, char* argv[]) {
    // 设置信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // 解析命令行参数
    int port = Config::DEFAULT_CONTROL_PORT;
    std::string rootDir = "/tmp/ftp";
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            try {
                port = std::stoi(argv[++i]);
                if (port < 1 || port > 65535) {
                    std::cerr << "端口号必须在1-65535之间" << std::endl;
                    return 1;
                }
            } catch (...) {
                std::cerr << "无效的端口号" << std::endl;
                return 1;
            }
        } else if ((arg == "-d" || arg == "--dir") && i + 1 < argc) {
            rootDir = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "用法: " << argv[0] << " [选项]" << std::endl;
            std::cout << "选项:" << std::endl;
            std::cout << "  -p, --port <端口>    指定监听端口 (默认: 21)" << std::endl;
            std::cout << "  -d, --dir <目录>     指定FTP根目录 (默认: /tmp/ftp)" << std::endl;
            std::cout << "  -h, --help          显示帮助信息" << std::endl;
            return 0;
        } else {
            std::cerr << "未知选项: " << arg << std::endl;
            std::cerr << "使用 -h 或 --help 查看帮助信息" << std::endl;
            return 1;
        }
    }
    
    // 创建并启动FTP服务器
    FTPServer server(port, rootDir);
    
    if (!server.start()) {
        std::cerr << "FTP服务器启动失败" << std::endl;
        return 1;
    }
    
    // 运行服务器
    server.run();
    
    return 0;
}