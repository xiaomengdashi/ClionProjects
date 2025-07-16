#ifndef PACKET_MODIFIER_H
#define PACKET_MODIFIER_H

#include "pcap_parser.h"
#include <string>
#include <vector>
#include <cstdint>
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
 * 数据包修改器
 * 负责修改数据包的IP地址和端口信息
 */
class PacketModifier {
public:
    PacketModifier();
    ~PacketModifier();
    
    /**
     * 设置IP地址映射
     * @param original_src 原始源IP
     * @param original_dst 原始目标IP
     * @param new_src 新的源IP
     * @param new_dst 新的目标IP
     */
    void setIPMapping(const std::string& original_src, const std::string& original_dst,
                     const std::string& new_src, const std::string& new_dst);
    
    /**
     * 修改数据包信息
     * @param packet_info 要修改的数据包信息
     * @param is_server_to_client 是否为服务端到客户端的包
     */
    void modifyPacket(PacketInfo& packet_info, bool is_server_to_client);
    
    /**
     * 构建完整的网络数据包
     * @param packet_info 数据包信息
     * @return 构建的数据包字节流
     */
    std::vector<uint8_t> buildPacket(const PacketInfo& packet_info);
    
    /**
     * 网络字节序IP转换为字符串
     */
    std::string ipNetworkToString(uint32_t ip);

private:
    uint32_t original_src_ip_;
    uint32_t original_dst_ip_;
    uint32_t new_src_ip_;
    uint32_t new_dst_ip_;
    
    /**
     * 字符串IP转换为网络字节序
     */
    uint32_t ipStringToNetwork(const std::string& ip_str);
    
    /**
     * 计算IP校验和
     */
    uint16_t calculateIPChecksum(const uint8_t* ip_header, int header_len);
    
    /**
     * 计算TCP校验和
     */
    uint16_t calculateTCPChecksum(const uint8_t* ip_header, const uint8_t* tcp_header, int tcp_len);
    
    /**
     * 计算UDP校验和
     */
    uint16_t calculateUDPChecksum(const uint8_t* ip_header, const uint8_t* udp_header, int udp_len);
};

#endif // PACKET_MODIFIER_H