#pragma once

#include <pcapplusplus/Packet.h>
#include <pcapplusplus/RawPacket.h>
#include <pcapplusplus/IPv4Layer.h>
#include <pcapplusplus/IPv6Layer.h>
#include <pcapplusplus/EthLayer.h>
#include <string>
#include <vector>
#include <memory>

/**
 * 包方向枚举
 */
enum class PacketDirection {
    CLIENT_TO_SERVER,   // 客户端发送给服务端
    SERVER_TO_CLIENT,   // 服务端发送给客户端
    UNKNOWN             // 无法确定方向
};

/**
 * 包信息结构
 */
struct PacketInfo {
    pcpp::RawPacket raw_packet;     // 原始包数据
    std::vector<uint8_t> raw_data;  // 原始包数据副本
    PacketDirection direction;      // 包方向
    std::string src_ip;            // 源IP地址
    std::string dst_ip;            // 目标IP地址
    uint16_t src_port;             // 源端口
    uint16_t dst_port;             // 目标端口
    size_t packet_size;            // 包大小
    timespec timestamp;            // 时间戳
    uint64_t timestamp_us;         // 时间戳（微秒）
    int original_index;            // 在原PCAP文件中的索引
    
    PacketInfo() : direction(PacketDirection::UNKNOWN), src_port(0), dst_port(0), 
                   packet_size(0), timestamp_us(0), original_index(-1) {
        timestamp.tv_sec = 0;
        timestamp.tv_nsec = 0;
    }
};

/**
 * 包分析器类
 * 负责分析PCAP文件中的包，区分客户端和服务端的包
 */
class PacketAnalyzer {
public:
    /**
     * 构造函数
     * @param client_ip 客户端IP地址，用于区分包方向
     */
    explicit PacketAnalyzer(const std::string& client_ip);
    
    /**
     * 析构函数
     */
    ~PacketAnalyzer() = default;
    
    /**
     * 分析PCAP文件，提取并分类所有包
     * @param pcap_file_path PCAP文件路径
     * @return 成功返回true，失败返回false
     */
    bool analyzePcapFile(const std::string& pcap_file_path);
    
    /**
     * 获取客户端包列表
     * @return 客户端包信息列表
     */
    const std::vector<PacketInfo>& getClientPackets() const;
    
    /**
     * 获取服务端包列表
     * @return 服务端包信息列表
     */
    const std::vector<PacketInfo>& getServerPackets() const;
    
    /**
     * 获取所有包列表（按原始顺序）
     * @return 所有包信息列表
     */
    const std::vector<PacketInfo>& getAllPackets() const;
    
    /**
     * 获取统计信息
     */
    int getTotalPacketCount() const;
    int getClientPacketCount() const;
    int getServerPacketCount() const;
    int getUnknownPacketCount() const;
    
    /**
     * 分析单个包的方向
     * @param packet 要分析的包
     * @return 包方向
     */
    PacketDirection analyzePacketDirection(const pcpp::Packet& packet) const;
    
    /**
     * 提取包的基本信息
     * @param raw_packet 原始包数据
     * @param index 包在文件中的索引
     * @return 包信息结构
     */
    PacketInfo extractPacketInfo(const pcpp::RawPacket& raw_packet, int index) const;
    
    /**
     * 设置客户端IP地址
     * @param client_ip 新的客户端IP地址
     */
    void setClientIP(const std::string& client_ip);
    
    /**
     * 获取客户端IP地址
     * @return 当前设置的客户端IP地址
     */
    const std::string& getClientIP() const;
    
    /**
     * 清空所有分析结果
     */
    void clear();
    
    /**
     * 打印分析统计信息
     */
    void printStatistics() const;
    
    /**
     * 验证IP地址格式
     * @param ip_str IP地址字符串
     * @return 有效返回true
     */
    static bool isValidIPAddress(const std::string& ip_str);
    
private:
    std::string client_ip_;                    // 客户端IP地址
    std::vector<PacketInfo> all_packets_;      // 所有包（按原始顺序）
    std::vector<PacketInfo> client_packets_;   // 客户端包
    std::vector<PacketInfo> server_packets_;   // 服务端包
    
    int total_packet_count_;
    int client_packet_count_;
    int server_packet_count_;
    int unknown_packet_count_;
    
    /**
     * 从IPv4层提取IP地址信息
     * @param ipv4_layer IPv4层指针
     * @param src_ip 输出源IP
     * @param dst_ip 输出目标IP
     * @return 成功返回true
     */
    bool extractIPv4Info(pcpp::IPv4Layer* ipv4_layer, std::string& src_ip, std::string& dst_ip) const;
    
    /**
     * 从IPv6层提取IP地址信息
     * @param ipv6_layer IPv6层指针
     * @param src_ip 输出源IP
     * @param dst_ip 输出目标IP
     * @return 成功返回true
     */
    bool extractIPv6Info(pcpp::IPv6Layer* ipv6_layer, std::string& src_ip, std::string& dst_ip) const;
    
    /**
     * 提取端口信息
     * @param packet 包对象
     * @param src_port 输出源端口
     * @param dst_port 输出目标端口
     * @return 成功返回true
     */
    bool extractPortInfo(const pcpp::Packet& packet, uint16_t& src_port, uint16_t& dst_port) const;
};