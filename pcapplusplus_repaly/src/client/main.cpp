#include "pcap_client.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <fstream>
#include <stdexcept>

// 全局客户端实例指针，用于信号处理
std::atomic<PcapClient*> g_client_instance(nullptr);
std::atomic<bool> g_interrupted(false);

/**
 * 信号处理函数
 * 处理 SIGINT (Ctrl+C) 和 SIGTERM 信号
 */
void signalHandler(int signal) {
    std::cout << "\n收到中断信号 (" << signal << ")，正在停止客户端..." << std::endl;
    g_interrupted.store(true);
    
    PcapClient* client = g_client_instance.load();
    if (client != nullptr) {
        client->setInterrupted();
        client->stopReplay();
    }
}

/**
 * 显示使用帮助
 */
void showUsage(const char* program_name) {
    std::cout << "PCAP客户端 - 发送PCAP文件中的客户端包\n" << std::endl;
    std::cout << "用法:" << std::endl;
    std::cout << "  " << program_name << " <pcap文件> <接口名> <客户端IP> <模式> [参数]\n" << std::endl;
    
    std::cout << "参数说明:" << std::endl;
    std::cout << "  pcap文件    - 要回放的PCAP文件路径" << std::endl;
    std::cout << "  接口名      - 网络接口名称（如 eth0, en0）" << std::endl;
    std::cout << "  客户端IP    - 用于区分包方向的客户端IP地址" << std::endl;
    std::cout << "  模式        - 回放模式（1-4）" << std::endl;
    std::cout << "  参数        - 模式相关的参数\n" << std::endl;
    
    std::cout << "回放模式:" << std::endl;
    std::cout << "  1: 原速回放 - 按照PCAP文件中记录的原始时间间隔" << std::endl;
    std::cout << "  2: 固定间隔回放 [间隔毫秒] - 使用固定的时间间隔" << std::endl;
    std::cout << "  3: 浮动原速回放 [浮动百分比] - 在原始间隔基础上添加随机浮动" << std::endl;
    std::cout << "  4: 等速回放 [目标流量字节/秒] - 保持恒定的数据流量\n" << std::endl;
    
    std::cout << "示例:" << std::endl;
    std::cout << "  " << program_name << " test.pcap eth0 192.168.1.100 1" << std::endl;
    std::cout << "  " << program_name << " test.pcap eth0 192.168.1.100 2 100" << std::endl;
    std::cout << "  " << program_name << " test.pcap eth0 192.168.1.100 3 0.2" << std::endl;
    std::cout << "  " << program_name << " test.pcap eth0 192.168.1.100 4 1048576" << std::endl;
}

/**
 * 解析命令行参数
 */
bool parseArguments(int argc, char* argv[], std::string& pcap_file, std::string& interface_name,
                   std::string& client_ip, ClientReplayConfig& config) {
    // 检查基本参数数量
    if (argc < 5) {
        std::cerr << "错误: 参数不足" << std::endl;
        return false;
    }
    
    pcap_file = argv[1];
    interface_name = argv[2];
    client_ip = argv[3];
    
    // 验证PCAP文件是否存在
    std::ifstream pcap_test(pcap_file);
    if (!pcap_test.good()) {
        std::cerr << "错误: 无法读取PCAP文件 '" << pcap_file << "'" << std::endl;
        std::cerr << "请检查文件是否存在且有读取权限" << std::endl;
        return false;
    }
    pcap_test.close();
    
    // 验证客户端IP地址
    if (!PacketAnalyzer::isValidIPAddress(client_ip)) {
        std::cerr << "错误: 无效的客户端IP地址: " << client_ip << std::endl;
        return false;
    }
    
    // 解析模式参数
    int mode_val;
    try {
        mode_val = std::stoi(argv[4]);
    } catch (const std::invalid_argument& e) {
        std::cerr << "错误: 模式参数 '" << argv[4] << "' 不是有效的数字" << std::endl;
        return false;
    } catch (const std::out_of_range& e) {
        std::cerr << "错误: 模式参数 '" << argv[4] << "' 超出范围" << std::endl;
        return false;
    }
    
    // 验证模式值范围
    if (mode_val < 1 || mode_val > 4) {
        std::cerr << "错误: 模式值必须在1-4之间，当前值: " << mode_val << std::endl;
        return false;
    }
    
    config.mode = static_cast<ClientReplayMode>(mode_val);
    
    // 根据模式解析额外参数
    switch (config.mode) {
        case ClientReplayMode::OriginalSpeed:
            // 原速回放不需要额外参数
            break;
            
        case ClientReplayMode::FixedInterval:
            if (argc < 6) {
                std::cerr << "错误: 固定间隔模式需要指定间隔毫秒" << std::endl;
                return false;
            }
            try {
                config.fixedIntervalMs = std::stoi(argv[5]);
            } catch (const std::exception& e) {
                std::cerr << "错误: 间隔时间 '" << argv[5] << "' 不是有效的数字" << std::endl;
                return false;
            }
            if (config.fixedIntervalMs < 0) {
                std::cerr << "错误: 间隔时间不能为负数" << std::endl;
                return false;
            }
            break;
            
        case ClientReplayMode::FloatingOriginal:
            if (argc < 6) {
                std::cerr << "错误: 浮动模式需要指定浮动百分比" << std::endl;
                return false;
            }
            try {
                config.floatPercent = std::stod(argv[5]);
            } catch (const std::exception& e) {
                std::cerr << "错误: 浮动百分比 '" << argv[5] << "' 不是有效的数字" << std::endl;
                return false;
            }
            if (config.floatPercent < 0.0 || config.floatPercent > 1.0) {
                std::cerr << "错误: 浮动百分比必须在0.0-1.0之间" << std::endl;
                return false;
            }
            break;
            
        case ClientReplayMode::ConstantRate:
            if (argc < 6) {
                std::cerr << "错误: 等速回放模式需要指定目标流量" << std::endl;
                return false;
            }
            try {
                config.targetBytesPerSec = std::stoi(argv[5]);
            } catch (const std::exception& e) {
                std::cerr << "错误: 目标流量 '" << argv[5] << "' 不是有效的数字" << std::endl;
                return false;
            }
            if (config.targetBytesPerSec <= 0) {
                std::cerr << "错误: 目标流量必须大于0" << std::endl;
                return false;
            }
            if (config.targetBytesPerSec > 1000000000) { // 1GB/s
                std::cerr << "警告: 目标流量过大(>1GB/s): " << config.targetBytesPerSec << " 字节/秒" << std::endl;
            }
            break;
    }
    
    return true;
}

int main(int argc, char* argv[]) {
    // 注册信号处理函数
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // 检查参数
    if (argc < 5) {
        showUsage(argv[0]);
        return 1;
    }
    
    // 解析命令行参数
    std::string pcap_file, interface_name, client_ip;
    ClientReplayConfig config;
    
    if (!parseArguments(argc, argv, pcap_file, interface_name, client_ip, config)) {
        std::cerr << "\n使用 '" << argv[0] << "' 查看帮助信息" << std::endl;
        return 1;
    }
    
    // 创建客户端实例
    PcapClient client(pcap_file, interface_name, client_ip, config);
    g_client_instance.store(&client);
    
    try {
        // 初始化客户端
        if (!client.initialize()) {
            std::cerr << "客户端初始化失败" << std::endl;
            return 1;
        }
        
        // 开始回放
        if (!client.startReplay()) {
            std::cerr << "客户端回放失败" << std::endl;
            return 1;
        }
        
        // 等待回放完成或中断
        while (client.isRunning() && !g_interrupted.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (g_interrupted.load()) {
            std::cout << "客户端被用户中断" << std::endl;
        } else {
            std::cout << "客户端回放完成" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "客户端运行异常: " << e.what() << std::endl;
        return 1;
    }
    
    g_client_instance.store(nullptr);
    return 0;
}