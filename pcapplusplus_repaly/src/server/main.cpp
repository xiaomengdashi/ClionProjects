#include "pcap_server.h"
#include <iostream>
#include <csignal>
#include <memory>
#include <string>
#include <cstdlib>

// 全局服务端实例指针，用于信号处理
std::unique_ptr<PcapServer> g_server = nullptr;

/**
 * 信号处理函数
 * @param signal 信号类型
 */
void signalHandler(int signal) {
    std::cout << "\n收到信号 " << signal << "，正在停止服务端..." << std::endl;
    if (g_server) {
        g_server->setInterrupted();
    }
}

/**
 * 打印使用帮助
 * @param program_name 程序名称
 */
void printUsage(const char* program_name) {
    std::cout << "PCAP服务端回放工具" << std::endl;
    std::cout << "用法: " << program_name << " <pcap_file> <interface> <client_ip> <mode> [mode_params...]" << std::endl;
    std::cout << std::endl;
    std::cout << "参数说明:" << std::endl;
    std::cout << "  pcap_file    - PCAP文件路径" << std::endl;
    std::cout << "  interface    - 网络接口名称 (如: eth0, en0)" << std::endl;
    std::cout << "  client_ip    - 客户端IP地址" << std::endl;
    std::cout << "  mode         - 回放模式:" << std::endl;
    std::cout << "                 1 = 原速回放" << std::endl;
    std::cout << "                 2 = 固定间隔回放" << std::endl;
    std::cout << "                 3 = 浮动原速回放" << std::endl;
    std::cout << "                 4 = 等速回放" << std::endl;
    std::cout << std::endl;
    std::cout << "模式参数:" << std::endl;
    std::cout << "  模式2: <interval_ms>     - 固定间隔时间（毫秒）" << std::endl;
    std::cout << "  模式3: <float_percent>   - 浮动百分比（0.0-1.0）" << std::endl;
    std::cout << "  模式4: <bytes_per_sec>   - 目标流量（字节/秒）" << std::endl;
    std::cout << std::endl;
    std::cout << "示例:" << std::endl;
    std::cout << "  " << program_name << " test.pcap eth0 192.168.1.100 1" << std::endl;
    std::cout << "  " << program_name << " test.pcap eth0 192.168.1.100 2 100" << std::endl;
    std::cout << "  " << program_name << " test.pcap eth0 192.168.1.100 3 0.1" << std::endl;
    std::cout << "  " << program_name << " test.pcap eth0 192.168.1.100 4 1000000" << std::endl;
}

/**
 * 解析命令行参数
 * @param argc 参数个数
 * @param argv 参数数组
 * @param pcap_file PCAP文件路径
 * @param interface 网络接口名称
 * @param client_ip 客户端IP地址
 * @param config 回放配置
 * @return 解析成功返回true
 */
bool parseArguments(int argc, char* argv[], 
                   std::string& pcap_file,
                   std::string& interface,
                   std::string& client_ip,
                   ServerReplayConfig& config) {
    if (argc < 5) {
        std::cerr << "错误：参数不足" << std::endl;
        return false;
    }
    
    pcap_file = argv[1];
    interface = argv[2];
    client_ip = argv[3];
    
    // 解析回放模式
    int mode = std::atoi(argv[4]);
    if (mode < 1 || mode > 4) {
        std::cerr << "错误：无效的回放模式 " << mode << "，必须是1-4" << std::endl;
        return false;
    }
    
    config.mode = static_cast<ServerReplayMode>(mode);
    
    // 根据模式解析额外参数
    switch (config.mode) {
        case ServerReplayMode::OriginalSpeed:
            // 原速回放不需要额外参数
            if (argc != 5) {
                std::cerr << "警告：原速回放模式不需要额外参数，忽略多余参数" << std::endl;
            }
            break;
            
        case ServerReplayMode::FixedInterval:
            // 固定间隔回放需要间隔时间参数
            if (argc != 6) {
                std::cerr << "错误：固定间隔模式需要指定间隔时间（毫秒）" << std::endl;
                return false;
            }
            config.fixedIntervalMs = std::atoi(argv[5]);
            if (config.fixedIntervalMs <= 0) {
                std::cerr << "错误：间隔时间必须大于0" << std::endl;
                return false;
            }
            break;
            
        case ServerReplayMode::FloatingOriginal:
            // 浮动原速回放需要浮动百分比参数
            if (argc != 6) {
                std::cerr << "错误：浮动原速模式需要指定浮动百分比（0.0-1.0）" << std::endl;
                return false;
            }
            config.floatPercent = std::atof(argv[5]);
            if (config.floatPercent < 0.0 || config.floatPercent > 1.0) {
                std::cerr << "错误：浮动百分比必须在0.0-1.0之间" << std::endl;
                return false;
            }
            break;
            
        case ServerReplayMode::ConstantRate:
            // 等速回放需要目标流量参数
            if (argc != 6) {
                std::cerr << "错误：等速回放模式需要指定目标流量（字节/秒）" << std::endl;
                return false;
            }
            config.targetBytesPerSec = std::atoi(argv[5]);
            if (config.targetBytesPerSec <= 0) {
                std::cerr << "错误：目标流量必须大于0" << std::endl;
                return false;
            }
            break;
    }
    
    return true;
}

/**
 * 打印配置信息
 * @param pcap_file PCAP文件路径
 * @param interface 网络接口名称
 * @param client_ip 客户端IP地址
 * @param config 回放配置
 */
void printConfiguration(const std::string& pcap_file,
                       const std::string& interface,
                       const std::string& client_ip,
                       const ServerReplayConfig& config) {
    std::cout << "=== 服务端配置信息 ===" << std::endl;
    std::cout << "PCAP文件: " << pcap_file << std::endl;
    std::cout << "网络接口: " << interface << std::endl;
    std::cout << "客户端IP: " << client_ip << std::endl;
    
    std::cout << "回放模式: ";
    switch (config.mode) {
        case ServerReplayMode::OriginalSpeed:
            std::cout << "原速回放" << std::endl;
            break;
        case ServerReplayMode::FixedInterval:
            std::cout << "固定间隔回放 (" << config.fixedIntervalMs << " ms)" << std::endl;
            break;
        case ServerReplayMode::FloatingOriginal:
            std::cout << "浮动原速回放 (浮动: " << (config.floatPercent * 100) << "%)" << std::endl;
            break;
        case ServerReplayMode::ConstantRate:
            std::cout << "等速回放 (" << config.targetBytesPerSec << " bytes/sec)" << std::endl;
            break;
    }
    std::cout << "==================" << std::endl;
}

int main(int argc, char* argv[]) {
    // 检查参数
    if (argc < 5) {
        printUsage(argv[0]);
        return 1;
    }
    
    // 解析命令行参数
    std::string pcap_file, interface, client_ip;
    ServerReplayConfig config;
    
    if (!parseArguments(argc, argv, pcap_file, interface, client_ip, config)) {
        printUsage(argv[0]);
        return 1;
    }
    
    // 打印配置信息
    printConfiguration(pcap_file, interface, client_ip, config);
    
    try {
        // 创建服务端实例
        g_server = std::make_unique<PcapServer>(pcap_file, interface, client_ip, config);
        
        // 注册信号处理函数
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        
        // 初始化服务端
        std::cout << "正在初始化服务端..." << std::endl;
        if (!g_server->initialize()) {
            std::cerr << "服务端初始化失败" << std::endl;
            return 1;
        }
        
        // 开始回放
        std::cout << "开始服务端回放..." << std::endl;
        if (!g_server->startReplay()) {
            std::cerr << "服务端回放失败" << std::endl;
            return 1;
        }
        
        std::cout << "服务端回放完成" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "程序异常: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "未知异常" << std::endl;
        return 1;
    }
}