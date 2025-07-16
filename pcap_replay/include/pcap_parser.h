#ifndef PCAP_PARSER_H
#define PCAP_PARSER_H

#include <string>
#include <vector>
#include <memory>
#include <pcap/pcap.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#ifdef __linux__
#include <netinet/ether.h>
#elif __APPLE__
#include <net/ethernet.h>
#endif

/**
 * 数据包信息结构
 */
struct PacketInfo {
    uint32_t src_ip;        // 源IP地址
    uint32_t dst_ip;        // 目标IP地址
    uint16_t src_port;      // 源端口
    uint16_t dst_port;      // 目标端口
    uint8_t protocol;       // 协议类型 (TCP/UDP)
    std::vector<uint8_t> payload;  // 载荷数据
    struct timeval timestamp;      // 时间戳
    bool is_outgoing;       // 是否为发送包
    size_t packet_size;     // 数据包大小
};

/**
 * PCAP 文件解析器
 */
class PcapParser {
public:
    PcapParser();
    ~PcapParser();
    
    /**
     * 打开 PCAP 文件
     * @param filename PCAP 文件路径
     * @return 是否成功
     */
    bool openFile(const std::string& filename);
    
    /**
     * 解析下一个数据包
     * @param packet_info 输出的数据包信息
     * @return 是否成功解析到数据包
     */
    bool parseNextPacket(PacketInfo& packet_info);
    
    /**
     * 设置过滤器
     * @param filter_expr 过滤表达式
     * @return 是否成功
     */
    bool setFilter(const std::string& filter_expr);
    
    /**
     * 获取总数据包数量
     * @return 数据包数量
     */
    size_t getTotalPackets() const { return total_packets_; }
    
    /**
     * 关闭文件
     */
    void close();

private:
    pcap_t* pcap_handle_;
    size_t total_packets_;
    
    /**
     * 解析以太网帧
     */
    bool parseEthernetFrame(const uint8_t* packet, int packet_len, PacketInfo& info);
    
    /**
     * 解析IP数据包
     */
    bool parseIPPacket(const uint8_t* ip_packet, int ip_len, PacketInfo& info);
    
    /**
     * 解析TCP数据包
     */
    bool parseTCPPacket(const uint8_t* tcp_packet, int tcp_len, PacketInfo& info);
    
    /**
     * 解析UDP数据包
     */
    bool parseUDPPacket(const uint8_t* udp_packet, int udp_len, PacketInfo& info);
};

#endif // PCAP_PARSER_H