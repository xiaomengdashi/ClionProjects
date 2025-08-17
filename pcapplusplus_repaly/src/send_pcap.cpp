/**
 * @file send_pcap.cpp
 * @brief PCAP文件回放工具
 * 
 * 这个程序用于读取PCAP文件并通过指定的网络接口回放数据包。
 * 支持三种回放模式：
 * 
 * 1. 原速回放 (mode=1):
 *    根据PCAP文件中每个包之间的原始时间差来决定发送时间。
 *    例如：如果两个包之间的时间差是100ms，程序会按原速发送，
 *    确保间隔与原始采集时完全一致。程序会自动等待相应的间隔时间。
 * 
 * 2. 固定间隔回放 (mode=2):
 *    按照用户提供的固定间隔时间发送每个数据包，忽略原始时间间隔。
 *    例如：如果设置100ms固定间隔，每个包之间都会保持100毫秒的
 *    固定间隔，回放会以恒定速度连续进行。
 * 
 * 3. 浮动原速回放 (mode=3):
 *    基于包的原始时间间隔，在此基础上添加随机浮动。
 *    例如：设定浮动±10%，原本100ms间隔会随机在90ms~110ms范围内
 *    选择延时，模拟更真实的网络环境。
 * 
 * 依赖库：PcapPlusPlus
 * 编译要求：C++11或更高版本
 * 
 * 使用示例：
 * ./send_pcap sample.pcap eth0 1           # 原速回放
 * ./send_pcap sample.pcap eth0 2 100      # 固定100ms间隔回放
 * ./send_pcap sample.pcap eth0 3 20       # 原速±20%浮动回放
 */

#include <pcapplusplus/PcapLiveDeviceList.h>
#include <pcapplusplus/PcapFileDevice.h>
#include <pcapplusplus/Packet.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <string>
#include <fstream>
#include <stdexcept>
#include <cstdlib>
#include <csignal>
#include <atomic>

// 全局变量用于信号处理
std::atomic<bool> g_interrupted(false);

/**
 * 信号处理函数，用于优雅地处理Ctrl+C中断
 */
void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\n收到中断信号，正在停止回放..." << std::endl;
        g_interrupted = true;
    }
}

/**
 * @enum ReplayMode
 * @brief 回放模式枚举
 * 
 * 定义了三种不同的数据包回放模式
 */
enum class ReplayMode
{
    OriginalSpeed = 1,    ///< 原速回放：严格按照PCAP文件中记录的原始时间间隔
    FixedInterval = 2,    ///< 固定间隔：使用用户指定的固定时间间隔，忽略原始间隔
    FloatingOriginal = 3, ///< 浮动原速：在原始时间间隔基础上添加指定百分比的随机浮动
    ConstantRate = 4      ///< 等速回放：保持每秒恒定的数据流量（字节/秒）
};

/**
 * @struct ReplayConfig
 * @brief 回放配置结构体
 * 
 * 存储回放模式的相关配置参数
 */
struct ReplayConfig
{
    ReplayMode mode;              ///< 回放模式
    int fixedIntervalMs = 0;      ///< 固定间隔模式下的时间间隔（毫秒）
    double floatPercent = 0.0;    ///< 浮动模式下的浮动百分比（0.0-1.0）
    int targetBytesPerSec = 0;    ///< 等速回放模式下的目标流量（字节/秒）
};

/**
 * @brief 主函数
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return 程序退出码，0表示成功，非0表示失败
 * 
 * 程序入口点，负责解析命令行参数、配置回放参数、
 * 打开PCAP文件和网络设备，然后执行数据包回放。
 */
int main(int argc, char* argv[])
{
    // 注册信号处理函数
    std::signal(SIGINT, signalHandler);
    
    // 检查命令行参数数量
    if (argc < 4) {
        std::cerr << "用法:\n"
                  << argv[0] << " <pcap文件> <接口名> <模式> [参数]\n"
                  << "模式:\n"
                  << " 1: 原速回放\n"
                  << " 2: 固定间隔回放 [间隔毫秒]\n"
                  << " 3: 浮动原速回放 [浮动百分比]\n"
                  << " 4: 等速回放 [目标流量字节/秒]\n"
                  << "示例:\n"
                  << "  " << argv[0] << " test.pcap eth0 1\n"
                  << "  " << argv[0] << " test.pcap eth0 2 100\n"
                  << "  " << argv[0] << " test.pcap eth0 3 20\n"
                  << "  " << argv[0] << " test.pcap eth0 4 1048576\n";
        return 1;
    }

    // 验证PCAP文件是否存在和可读
    std::ifstream pcap_test(argv[1]);
    if (!pcap_test.good()) {
        std::cerr << "错误: 无法读取PCAP文件 '" << argv[1] << "'" << std::endl;
        std::cerr << "请检查文件是否存在且有读取权限" << std::endl;
        return 1;
    }
    pcap_test.close();

    // 解析命令行参数
    std::string pcapFilePath = argv[1];  // PCAP文件路径
    std::string ifaceName = argv[2];     // 网络接口名称
    
    // 安全解析模式参数
    int modeVal;
    try {
        modeVal = std::stoi(argv[3]);
    } catch (const std::invalid_argument& e) {
        std::cerr << "错误: 模式参数 '" << argv[3] << "' 不是有效的数字" << std::endl;
        return 1;
    } catch (const std::out_of_range& e) {
        std::cerr << "错误: 模式参数 '" << argv[3] << "' 超出范围" << std::endl;
        return 1;
    }
    
    // 验证模式值范围
    if (modeVal < 1 || modeVal > 4) {
        std::cerr << "错误: 模式值必须在1-4之间，当前值: " << modeVal << std::endl;
        std::cerr << "1=原速回放, 2=固定间隔回放, 3=浮动原速回放, 4=等速回放" << std::endl;
        return 1;
    }

    // 初始化回放配置
    ReplayConfig config;
    config.mode = static_cast<ReplayMode>(modeVal);

    // 根据回放模式解析额外参数
    if (config.mode == ReplayMode::FixedInterval) {
        if (argc < 5) {
            std::cerr << "错误: 固定间隔模式需要指定间隔毫秒\n";
            return 1;
        }
        
        // 安全解析固定间隔参数
        try {
            config.fixedIntervalMs = std::stoi(argv[4]);
        } catch (const std::invalid_argument& e) {
            std::cerr << "错误: 间隔时间 '" << argv[4] << "' 不是有效的数字" << std::endl;
            return 1;
        } catch (const std::out_of_range& e) {
            std::cerr << "错误: 间隔时间 '" << argv[4] << "' 超出范围" << std::endl;
            return 1;
        }
        
        // 验证间隔时间合理性
        if (config.fixedIntervalMs < 0) {
            std::cerr << "错误: 间隔时间不能为负数: " << config.fixedIntervalMs << std::endl;
            return 1;
        }
        if (config.fixedIntervalMs > 60000) { // 最大60秒
            std::cerr << "警告: 间隔时间过大(>60秒): " << config.fixedIntervalMs << "ms" << std::endl;
        }
    }
    else if (config.mode == ReplayMode::FloatingOriginal) {
        if (argc < 5) {
            std::cerr << "错误: 浮动模式需要指定浮动百分比\n";
            return 1;
        }
        
        // 安全解析浮动百分比参数
        double floatValue;
        try {
            floatValue = std::stod(argv[4]);
        } catch (const std::invalid_argument& e) {
            std::cerr << "错误: 浮动百分比 '" << argv[4] << "' 不是有效的数字" << std::endl;
            return 1;
        } catch (const std::out_of_range& e) {
            std::cerr << "错误: 浮动百分比 '" << argv[4] << "' 超出范围" << std::endl;
            return 1;
        }
        
        // 验证浮动百分比合理性
        if (floatValue < 0.0) {
            std::cerr << "错误: 浮动百分比不能为负数: " << floatValue << std::endl;
            return 1;
        }
        if (floatValue > 100.0) {
            std::cerr << "错误: 浮动百分比不能大于100: " << floatValue << std::endl;
            return 1;
        }
        
        config.floatPercent = floatValue / 100.0;  // 转换为小数
    }
    else if (config.mode == ReplayMode::ConstantRate) {
        if (argc < 5) {
            std::cerr << "错误: 等速回放模式需要指定目标流量(字节/秒)\n";
            return 1;
        }
        
        // 安全解析目标流量参数
        try {
            config.targetBytesPerSec = std::stoi(argv[4]);
        } catch (const std::invalid_argument& e) {
            std::cerr << "错误: 目标流量 '" << argv[4] << "' 不是有效的数字" << std::endl;
            return 1;
        } catch (const std::out_of_range& e) {
            std::cerr << "错误: 目标流量 '" << argv[4] << "' 超出范围" << std::endl;
            return 1;
        }
        
        // 验证目标流量合理性
        if (config.targetBytesPerSec <= 0) {
            std::cerr << "错误: 目标流量必须大于0: " << config.targetBytesPerSec << std::endl;
            return 1;
        }
        if (config.targetBytesPerSec > 1000000000) { // 最大1GB/s
            std::cerr << "警告: 目标流量过大(>1GB/s): " << config.targetBytesPerSec << " 字节/秒" << std::endl;
        }
    }

    // 打开PCAP文件读取器
    pcpp::PcapFileReaderDevice reader(pcapFilePath);
    if (!reader.open()) {
        std::cerr << "错误: 无法打开 pcap 文件: " << pcapFilePath << std::endl;
        std::cerr << "请检查文件格式是否正确" << std::endl;
        return 1;
    }

    // 获取并打开网络设备
    pcpp::PcapLiveDevice* dev = pcpp::PcapLiveDeviceList::getInstance().getDeviceByName(ifaceName);
    if (dev == nullptr) {
        std::cerr << "错误: 找不到接口: " << ifaceName << std::endl;        
        // 显示可用设备列表
        const std::vector<pcpp::PcapLiveDevice*>& devList = pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDevicesList();
        for (const auto& device : devList) {
            std::cerr << "  - " << device->getName() << std::endl;
        }
        
        reader.close();
        return 1;
    }
    if (!dev->open()) {
        std::cerr << "错误: 无法打开设备: " << ifaceName << std::endl;
        std::cerr << "请检查是否有足够的权限访问网络设备" << std::endl;
        reader.close();
        return 1;
    }

    std::cout << "开始回放 PCAP 文件: " << pcapFilePath << std::endl;
    std::cout << "使用设备: " << ifaceName << std::endl;
    std::cout << "回放模式: " << (config.mode == ReplayMode::OriginalSpeed ? "原速回放" :
                                config.mode == ReplayMode::FixedInterval ? "固定间隔回放" :
                                config.mode == ReplayMode::FloatingOriginal ? "浮动原速回放" : "等速回放") << std::endl;
    
    // 统计信息
    int totalPackets = 0;
    int sentPackets = 0;
    int failedPackets = 0;
    auto startTime = std::chrono::high_resolution_clock::now();

    // 预处理：读取所有数据包并计算延迟时间
    std::vector<std::pair<pcpp::RawPacket, int>> packetDelayPairs;
    pcpp::RawPacket tempPacket;
    pcpp::RawPacket prevPacket;
    bool isFirstPacket = true;
    std::default_random_engine rng(std::random_device{}());
    std::uniform_real_distribution<double> floatDist(1.0 - config.floatPercent, 1.0 + config.floatPercent);
    
    std::cout << "正在预处理数据包和计算延迟时间..." << std::endl;
    
    // 等速回放模式需要特殊处理
    if (config.mode == ReplayMode::ConstantRate) {
        // 第一遍扫描：收集所有数据包和计算总字节数
        std::vector<pcpp::RawPacket> allPackets;
        int totalBytes = 0;
        
        while (reader.getNextPacket(tempPacket)) {
            allPackets.push_back(tempPacket);
            totalBytes += tempPacket.getRawDataLen();
        }
        
        std::cout << "等速回放模式：总数据包 " << allPackets.size() << " 个，总字节数 " << totalBytes << " 字节" << std::endl;
        
        // 计算每个数据包的延迟时间以达到目标流量
        int cumulativeBytes = 0;
        for (size_t i = 0; i < allPackets.size(); ++i) {
            int delayMs = 0;
            
            if (i > 0) {
                // 计算到当前包为止应该消耗的时间（毫秒）
                cumulativeBytes += allPackets[i-1].getRawDataLen();
                double expectedTimeMs = (double)cumulativeBytes * 1000.0 / config.targetBytesPerSec;
                delayMs = static_cast<int>(expectedTimeMs) - static_cast<int>((double)(cumulativeBytes - allPackets[i-1].getRawDataLen()) * 1000.0 / config.targetBytesPerSec);
                
                // 确保延迟时间不为负数
                if (delayMs < 0) delayMs = 0;
            }
            
            packetDelayPairs.emplace_back(allPackets[i], delayMs);
        }
    } else {
        // 其他模式的原有逻辑
        while (reader.getNextPacket(tempPacket)) {
            int delayMs = 0;
            
            if (!isFirstPacket) {
                // 计算与前一个数据包的时间差
                timespec prevTs = prevPacket.getPacketTimeStamp();
                timespec currTs = tempPacket.getPacketTimeStamp();
                long secDiff = currTs.tv_sec - prevTs.tv_sec;
                long nsecDiff = currTs.tv_nsec - prevTs.tv_nsec;
                double origDiffMs = secDiff * 1000.0 + nsecDiff / 1.0e6;
                
                // 处理时间戳异常情况
                if (origDiffMs < 0) {
                    origDiffMs = 0;
                }
                if (origDiffMs > 10000) {
                    origDiffMs = 10000;
                }
                
                // 根据模式预计算延迟
                switch (config.mode) {
                    case ReplayMode::OriginalSpeed:
                        delayMs = static_cast<int>(origDiffMs);
                        break;
                    case ReplayMode::FixedInterval:
                        delayMs = config.fixedIntervalMs;
                        break;
                    case ReplayMode::FloatingOriginal: {
                        double factor = floatDist(rng);
                        delayMs = static_cast<int>(origDiffMs * factor);
                        break;
                    }
                    case ReplayMode::ConstantRate:
                        // 这种情况不会到达这里
                        break;
                }
            }
            
            packetDelayPairs.emplace_back(tempPacket, delayMs);
            prevPacket = tempPacket;
            isFirstPacket = false;
        }
    }
    
    std::cout << "预处理完成，共" << packetDelayPairs.size() << "个数据包" << std::endl;
    
    // 重新打开文件准备发送（预处理已经关闭了文件）
    reader.close();

    // 主回放循环：使用预处理的数据包和延迟时间
    for (size_t i = 0; i < packetDelayPairs.size() && !g_interrupted; ++i) {
        const auto& packetPair = packetDelayPairs[i];
        const pcpp::RawPacket& currentPacket = packetPair.first;
        int delayMs = packetPair.second;
        
        totalPackets++;
        
        // 检查数据包是否有效
        if (currentPacket.getRawDataLen() == 0) {
            std::cerr << "警告: 跳过空数据包 (包序号: " << totalPackets << ")" << std::endl;
            continue;
        }
        
        // 检查数据包大小是否合理
        if (currentPacket.getRawDataLen() > 65535) {
            std::cerr << "警告: 数据包过大 (" << currentPacket.getRawDataLen() << " 字节), 包序号: " << totalPackets << std::endl;
        }
        
        // 发送当前数据包到网络接口
        if (!dev->sendPacket(currentPacket)) {
            std::cerr << "发送数据包失败 (包序号: " << totalPackets << ")" << std::endl;
            failedPackets++;
        } else {
            sentPackets++;
        }
        
        // 每发送100个包显示一次进度
        if (totalPackets % 100 == 0) {
            std::cout << "已处理: " << totalPackets << " 包, 成功: " << sentPackets 
                      << ", 失败: " << failedPackets << std::endl;
        }

        // 执行预计算的延迟（跳过第一个数据包）
        if (i > 0 && delayMs > 0 && !g_interrupted) {
            // 分段睡眠，以便能够响应中断信号
            const int sleepChunk = 100;  // 每次睡眠100ms
            int remainingMs = delayMs;
            
            while (remainingMs > 0 && !g_interrupted) {
                int currentSleep = std::min(remainingMs, sleepChunk);
                std::this_thread::sleep_for(std::chrono::milliseconds(currentSleep));
                remainingMs -= currentSleep;
            }
        }
    }

    // 计算总耗时
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    // 显示统计信息
    if (g_interrupted) {
        std::cout << "\n=== 回放被中断统计 ===" << std::endl;
    } else {
        std::cout << "\n=== 回放完成统计 ===" << std::endl;
    }
    std::cout << "总数据包数: " << totalPackets << std::endl;
    std::cout << "成功发送: " << sentPackets << std::endl;
    std::cout << "发送失败: " << failedPackets << std::endl;
    std::cout << "成功率: " << (totalPackets > 0 ? (double)sentPackets / totalPackets * 100.0 : 0.0) << "%" << std::endl;
    std::cout << "总耗时: " << duration.count() << " ms" << std::endl;
    
    if (totalPackets > 0) {
        std::cout << "平均发送速率: " << (double)sentPackets / (duration.count() / 1000.0) << " 包/秒" << std::endl;
    }

    // 清理资源
    reader.close();  // 关闭PCAP文件
    dev->close();    // 关闭网络设备

    return 0;
}
