#pragma once

#include "../common/shared_memory.h"
#include "../common/packet_analyzer.h"
#include <pcapplusplus/PcapLiveDevice.h>
#include <pcapplusplus/RawPacket.h>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>

/**
 * 回放模式枚举（与客户端兼容）
 */
enum class ServerReplayMode {
    OriginalSpeed = 1,    // 原速回放
    FixedInterval = 2,    // 固定间隔回放
    FloatingOriginal = 3, // 浮动原速回放
    ConstantRate = 4      // 等速回放
};

/**
 * 服务端回放配置
 */
struct ServerReplayConfig {
    ServerReplayMode mode;          // 回放模式
    int fixedIntervalMs = 0;        // 固定间隔模式下的时间间隔（毫秒）
    double floatPercent = 0.0;      // 浮动模式下的浮动百分比（0.0-1.0）
    int targetBytesPerSec = 0;      // 等速回放模式下的目标流量（字节/秒）
    
    ServerReplayConfig() : mode(ServerReplayMode::OriginalSpeed) {}
};

/**
 * 接收到的包信息
 */
struct ReceivedPacketInfo {
    std::string src_ip;
    std::string dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    size_t packet_size;
    uint64_t receive_time_us;  // 接收时间（微秒）
    
    ReceivedPacketInfo() : src_port(0), dst_port(0), packet_size(0), receive_time_us(0) {}
};

/**
 * PCAP服务端类
 * 负责发送PCAP文件中的服务端包，监听网络接口并与客户端协调同步
 */
class PcapServer {
public:
    /**
     * 构造函数
     * @param pcap_file_path PCAP文件路径
     * @param interface_name 网络接口名称
     * @param client_ip 客户端IP地址
     * @param config 回放配置
     */
    PcapServer(const std::string& pcap_file_path,
               const std::string& interface_name,
               const std::string& client_ip,
               const ServerReplayConfig& config);
    
    /**
     * 析构函数
     */
    ~PcapServer();
    
    /**
     * 初始化服务端
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
    int getReceivedPackets() const;
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
    ServerReplayConfig config_;
    
    // 核心组件
    std::unique_ptr<SharedMemoryManager> shm_manager_;
    std::unique_ptr<PacketAnalyzer> packet_analyzer_;
    pcpp::PcapLiveDevice* network_device_;
    
    // 包数据
    std::vector<PacketInfo> server_packets_;
    std::vector<std::pair<PacketInfo, int>> packet_delay_pairs_;
    
    // 网络监听相关
    std::thread packet_capture_thread_;
    std::queue<ReceivedPacketInfo> received_packets_;
    std::mutex received_packets_mutex_;
    std::atomic<bool> capture_running_;
    
    // 状态变量
    std::atomic<bool> initialized_;
    std::atomic<bool> running_;
    std::atomic<bool> interrupted_;
    
    // 统计信息
    std::atomic<int> total_packets_;
    std::atomic<int> sent_packets_;
    std::atomic<int> failed_packets_;
    std::atomic<int> received_packets_count_;
    
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
     * 等待客户端就绪
     * @param timeout_ms 超时时间（毫秒）
     * @return 成功返回true
     */
    bool waitForClient(int timeout_ms = 30000);
    
    /**
     * 同步发送包（与客户端协调）
     * @param packet_info 包信息
     * @param packet_index 包索引
     * @return 成功返回true
     */
    bool synchronizedSendPacket(const PacketInfo& packet_info, int packet_index);
    
    /**
     * 检查是否收到了对应的客户端包
     * @param expected_packet 期望的包信息
     * @param timeout_ms 超时时间（毫秒）
     * @return 收到返回true，超时返回false
     */
    bool checkReceivedPacket(const PacketInfo& expected_packet, int timeout_ms);
    
    /**
     * 等待客户端发送包
     * @param timeout_ms 超时时间（毫秒）
     * @return 客户端已发送返回true
     */
    bool waitForClientPacketSent(int timeout_ms);
    
    /**
     * 启动包捕获线程
     * @return 成功返回true
     */
    bool startPacketCapture();
    
    /**
     * 停止包捕获线程
     */
    void stopPacketCapture();
    
    /**
     * 包捕获线程函数
     */
    void packetCaptureThreadFunc();
    
    /**
     * 包捕获回调函数
     * @param packet 捕获到的包
     * @param dev 网络设备
     * @param cookie 用户数据
     */
    static void onPacketArrives(pcpp::RawPacket* packet, pcpp::PcapLiveDevice* dev, void* cookie);
    
    /**
     * 处理捕获到的包
     * @param packet 捕获到的包
     */
    void handleCapturedPacket(pcpp::RawPacket* packet);
    
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
    
    /**
     * 检查包是否匹配（用于判断是否是期望的客户端包）
     * @param received_packet 接收到的包信息
     * @param expected_packet 期望的包信息
     * @return 匹配返回true
     */
    bool isPacketMatch(const ReceivedPacketInfo& received_packet, const PacketInfo& expected_packet) const;
    
    // 禁用拷贝构造和赋值
    PcapServer(const PcapServer&) = delete;
    PcapServer& operator=(const PcapServer&) = delete;
};