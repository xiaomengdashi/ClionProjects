#pragma once

#include "../common/shared_memory.h"
#include "../common/packet_analyzer.h"
#include <pcapplusplus/PcapLiveDevice.h>
#include <string>
#include <vector>
#include <memory>
#include <atomic>

/**
 * 回放模式枚举（与原程序兼容）
 */
enum class ClientReplayMode {
    OriginalSpeed = 1,    // 原速回放
    FixedInterval = 2,    // 固定间隔回放
    FloatingOriginal = 3, // 浮动原速回放
    ConstantRate = 4      // 等速回放
};

/**
 * 客户端回放配置
 */
struct ClientReplayConfig {
    ClientReplayMode mode;          // 回放模式
    int fixedIntervalMs = 0;        // 固定间隔模式下的时间间隔（毫秒）
    double floatPercent = 0.0;      // 浮动模式下的浮动百分比（0.0-1.0）
    int targetBytesPerSec = 0;      // 等速回放模式下的目标流量（字节/秒）
    
    ClientReplayConfig() : mode(ClientReplayMode::OriginalSpeed) {}
};

/**
 * PCAP客户端类
 * 负责发送PCAP文件中的客户端包，与服务端协调同步
 */
class PcapClient {
public:
    /**
     * 构造函数
     * @param pcap_file_path PCAP文件路径
     * @param interface_name 网络接口名称
     * @param client_ip 客户端IP地址
     * @param config 回放配置
     */
    PcapClient(const std::string& pcap_file_path,
               const std::string& interface_name,
               const std::string& client_ip,
               const ClientReplayConfig& config);
    
    /**
     * 析构函数
     */
    ~PcapClient();
    
    /**
     * 初始化客户端
     * @return 成功返回true
     */
    bool initialize();
    
    /**
     * 开始回放
     * @return 成功返回true
     */
    bool startReplay();
    
    /**
     * 停止回放
     */
    void stopReplay();
    
    /**
     * 检查是否正在运行
     * @return 正在运行返回true
     */
    bool isRunning() const;
    
    /**
     * 获取统计信息
     */
    int getTotalPackets() const;
    int getSentPackets() const;
    int getFailedPackets() const;
    double getSuccessRate() const;
    
    /**
     * 打印统计信息
     */
    void printStatistics() const;
    
    /**
     * 设置中断标志
     */
    void setInterrupted();
    
private:
    // 配置参数
    std::string pcap_file_path_;
    std::string interface_name_;
    std::string client_ip_;
    ClientReplayConfig config_;
    
    // 核心组件
    std::unique_ptr<SharedMemoryManager> shm_manager_;
    std::unique_ptr<PacketAnalyzer> packet_analyzer_;
    pcpp::PcapLiveDevice* network_device_;
    
    // 包数据
    std::vector<PacketInfo> client_packets_;
    std::vector<std::pair<PacketInfo, int>> packet_delay_pairs_;
    
    // 状态变量
    std::atomic<bool> initialized_;
    std::atomic<bool> running_;
    std::atomic<bool> interrupted_;
    
    // 统计信息
    std::atomic<int> total_packets_;
    std::atomic<int> sent_packets_;
    std::atomic<int> failed_packets_;
    
    // 时间记录
    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point end_time_;
    
    /**
     * 预处理包数据，计算延迟时间
     * @return 成功返回true
     */
    bool preprocessPackets();
    
    /**
     * 计算包的延迟时间
     * @param current_packet 当前包
     * @param prev_packet 前一个包
     * @param packet_index 包索引
     * @return 延迟时间（毫秒）
     */
    int calculatePacketDelay(const PacketInfo& current_packet,
                           const PacketInfo& prev_packet,
                           int packet_index);
    
    /**
     * 等速回放模式的特殊预处理
     * @return 成功返回true
     */
    bool preprocessConstantRateMode();
    
    /**
     * 发送单个包
     * @param packet_info 包信息
     * @return 成功返回true
     */
    bool sendPacket(const PacketInfo& packet_info);
    
    /**
     * 执行延迟等待
     * @param delay_ms 延迟时间（毫秒）
     */
    void executeDelay(int delay_ms);
    
    /**
     * 等待服务端就绪
     * @param timeout_ms 超时时间（毫秒）
     * @return 成功返回true
     */
    bool waitForServer(int timeout_ms = 30000);
    
    /**
     * 等待轮到客户端发送
     * @param packet_index 包索引
     * @return 成功返回true
     */
    bool waitForClientTurn(int packet_index);
    
    /**
     * 发送客户端包
     * @param packet_info 包信息
     * @param packet_index 包索引
     * @return 发送成功返回true
     */
    bool sendClientPacket(const PacketInfo& packet_info, int packet_index);
    
    /**
     * 切换到接收态，等待服务端发送
     * @param next_packet_index 下一个包的索引
     */
    void switchToReceiveMode(int next_packet_index);
    
    /**
     * 等待服务端处理包
     * @param packet_index 包索引
     */
    void waitForServerPacket(int packet_index);
    
    /**
     * 计算到下一包的延迟时间
     * @param current_packet 当前包
     * @param next_packet 下一包
     * @return 延迟时间（毫秒）
     */
    int calculateDelayToNextPacket(const PacketInfo& current_packet, const PacketInfo& next_packet);
    
    /**
     * 同步发送包（与服务端协调）
     * @param packet_info 包信息
     * @param packet_index 包索引
     * @return 发送成功返回true
     */
    bool synchronizedSendPacket(const PacketInfo& packet_info, int packet_index);
    
    /**
     * 更新共享内存状态
     * @param packet_index 当前包索引
     * @param sent_success 发送是否成功
     */
    void updateSharedMemoryState(int packet_index, bool sent_success);
    
    /**
     * 验证网络设备
     * @return 成功返回true
     */
    bool validateNetworkDevice();
    
    /**
     * 清理资源
     */
    void cleanup();
    
    // 禁用拷贝构造和赋值
    PcapClient(const PcapClient&) = delete;
    PcapClient& operator=(const PcapClient&) = delete;
};