#include "packet_modifier.h"
#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

PacketModifier::PacketModifier() 
    : original_src_ip_(0), original_dst_ip_(0), new_src_ip_(0), new_dst_ip_(0) {
}

PacketModifier::~PacketModifier() {
}

void PacketModifier::setIPMapping(const std::string& original_src, const std::string& original_dst,
                                 const std::string& new_src, const std::string& new_dst) {
    original_src_ip_ = ipStringToNetwork(original_src);
    original_dst_ip_ = ipStringToNetwork(original_dst);
    new_src_ip_ = ipStringToNetwork(new_src);
    new_dst_ip_ = ipStringToNetwork(new_dst);
    
    std::cout << "设置IP映射: " << original_src << " -> " << new_src 
              << ", " << original_dst << " -> " << new_dst << std::endl;
}

void PacketModifier::modifyPacket(PacketInfo& packet_info, bool is_server_to_client) {
    if (is_server_to_client) {
        // 服务端到客户端的包：源IP改为新的服务端IP，目标IP改为新的客户端IP
        packet_info.src_ip = new_src_ip_;
        packet_info.dst_ip = new_dst_ip_;
    } else {
        // 客户端到服务端的包：源IP改为新的客户端IP，目标IP改为新的服务端IP
        packet_info.src_ip = new_dst_ip_;
        packet_info.dst_ip = new_src_ip_;
    }
}

std::vector<uint8_t> PacketModifier::buildPacket(const PacketInfo& packet_info) {
    std::vector<uint8_t> packet;
    
    // 构建IP头部
#ifdef __APPLE__
    struct ip ip_header;
#else
    struct iphdr ip_header;
#endif
    memset(&ip_header, 0, sizeof(ip_header));
    
#ifdef __APPLE__
    ip_header.ip_hl = 5; // 20字节头部
    ip_header.ip_v = 4;
    ip_header.ip_tos = 0;
    ip_header.ip_id = htons(12345); // 简单的ID
    ip_header.ip_off = 0;
    ip_header.ip_ttl = 64;
    ip_header.ip_p = packet_info.protocol;
    ip_header.ip_src.s_addr = packet_info.src_ip;
    ip_header.ip_dst.s_addr = packet_info.dst_ip;
#else
    ip_header.ihl = 5; // 20字节头部
    ip_header.version = 4;
    ip_header.tos = 0;
    ip_header.id = htons(12345); // 简单的ID
    ip_header.frag_off = 0;
    ip_header.ttl = 64;
    ip_header.protocol = packet_info.protocol;
    ip_header.saddr = packet_info.src_ip;
    ip_header.daddr = packet_info.dst_ip;
#endif
    
    // 计算总长度
    uint16_t total_len = 20; // IP头部长度
    if (packet_info.protocol == IPPROTO_TCP) {
        total_len += 20; // TCP头部长度
    } else if (packet_info.protocol == IPPROTO_UDP) {
        total_len += 8; // UDP头部长度
    }
    total_len += packet_info.payload.size();
    
#ifdef __APPLE__
    ip_header.ip_len = htons(total_len);
#else
    ip_header.tot_len = htons(total_len);
#endif
    
    // 计算IP校验和
#ifdef __APPLE__
    ip_header.ip_sum = 0;
    ip_header.ip_sum = calculateIPChecksum(reinterpret_cast<const uint8_t*>(&ip_header), 20);
#else
    ip_header.check = 0;
    ip_header.check = calculateIPChecksum(reinterpret_cast<const uint8_t*>(&ip_header), 20);
#endif
    
    // 添加IP头部到数据包
    const uint8_t* ip_bytes = reinterpret_cast<const uint8_t*>(&ip_header);
    packet.insert(packet.end(), ip_bytes, ip_bytes + 20);
    
    // 构建传输层头部
    if (packet_info.protocol == IPPROTO_TCP) {
        struct tcphdr tcp_header;
        memset(&tcp_header, 0, sizeof(tcp_header));
        
#ifdef __APPLE__
        tcp_header.th_sport = htons(packet_info.src_port);
        tcp_header.th_dport = htons(packet_info.dst_port);
        tcp_header.th_seq = htonl(1000);
        tcp_header.th_ack = htonl(1000);
        tcp_header.th_off = 5; // 20字节头部
        tcp_header.th_win = htons(8192);
        tcp_header.th_flags = TH_PUSH | TH_ACK;
        
        // 计算TCP校验和
        tcp_header.th_sum = 0;
        tcp_header.th_sum = calculateTCPChecksum(ip_bytes, 
                                               reinterpret_cast<const uint8_t*>(&tcp_header), 
                                               20 + packet_info.payload.size());
#else
        tcp_header.source = htons(packet_info.src_port);
        tcp_header.dest = htons(packet_info.dst_port);
        tcp_header.seq = htonl(1000);
        tcp_header.ack_seq = htonl(1000);
        tcp_header.doff = 5; // 20字节头部
        tcp_header.window = htons(8192);
        tcp_header.psh = 1;
        tcp_header.ack = 1;
        
        // 计算TCP校验和
        tcp_header.check = 0;
        tcp_header.check = calculateTCPChecksum(ip_bytes, 
                                               reinterpret_cast<const uint8_t*>(&tcp_header), 
                                               20 + packet_info.payload.size());
#endif
        
        // 添加TCP头部
        const uint8_t* tcp_bytes = reinterpret_cast<const uint8_t*>(&tcp_header);
        packet.insert(packet.end(), tcp_bytes, tcp_bytes + 20);
        
    } else if (packet_info.protocol == IPPROTO_UDP) {
        struct udphdr udp_header;
        memset(&udp_header, 0, sizeof(udp_header));
        
#ifdef __APPLE__
        udp_header.uh_sport = htons(packet_info.src_port);
        udp_header.uh_dport = htons(packet_info.dst_port);
        udp_header.uh_ulen = htons(8 + packet_info.payload.size());
        
        // 计算UDP校验和
        udp_header.uh_sum = 0;
        udp_header.uh_sum = calculateUDPChecksum(ip_bytes, 
                                               reinterpret_cast<const uint8_t*>(&udp_header), 
                                               8 + packet_info.payload.size());
#else
        udp_header.source = htons(packet_info.src_port);
        udp_header.dest = htons(packet_info.dst_port);
        udp_header.len = htons(8 + packet_info.payload.size());
        
        // 计算UDP校验和
        udp_header.check = 0;
        udp_header.check = calculateUDPChecksum(ip_bytes, 
                                               reinterpret_cast<const uint8_t*>(&udp_header), 
                                               8 + packet_info.payload.size());
#endif
        
        // 添加UDP头部
        const uint8_t* udp_bytes = reinterpret_cast<const uint8_t*>(&udp_header);
        packet.insert(packet.end(), udp_bytes, udp_bytes + 8);
    }
    
    // 添加载荷数据
    packet.insert(packet.end(), packet_info.payload.begin(), packet_info.payload.end());
    
    return packet;
}

uint32_t PacketModifier::ipStringToNetwork(const std::string& ip_str) {
    struct in_addr addr;
    if (inet_aton(ip_str.c_str(), &addr) == 0) {
        std::cerr << "无效的IP地址: " << ip_str << std::endl;
        return 0;
    }
    return addr.s_addr;
}

std::string PacketModifier::ipNetworkToString(uint32_t ip) {
    struct in_addr addr;
    addr.s_addr = ip;
    return std::string(inet_ntoa(addr));
}

uint16_t PacketModifier::calculateIPChecksum(const uint8_t* ip_header, int header_len) {
    uint32_t sum = 0;
    const uint16_t* ptr = reinterpret_cast<const uint16_t*>(ip_header);
    
    // 计算16位字的和
    for (int i = 0; i < header_len / 2; i++) {
        sum += ntohs(ptr[i]);
    }
    
    // 处理进位
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    // 取反
    return htons(~sum);
}

uint16_t PacketModifier::calculateTCPChecksum(const uint8_t* ip_header, const uint8_t* tcp_header, int tcp_len) {
    // 简化实现，实际应该包含伪头部
    uint32_t sum = 0;
    const uint16_t* ptr = reinterpret_cast<const uint16_t*>(tcp_header);
    
    for (int i = 0; i < tcp_len / 2; i++) {
        sum += ntohs(ptr[i]);
    }
    
    // 处理奇数字节
    if (tcp_len % 2) {
        sum += tcp_header[tcp_len - 1] << 8;
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return htons(~sum);
}

uint16_t PacketModifier::calculateUDPChecksum(const uint8_t* ip_header, const uint8_t* udp_header, int udp_len) {
    // 简化实现，实际应该包含伪头部
    uint32_t sum = 0;
    const uint16_t* ptr = reinterpret_cast<const uint16_t*>(udp_header);
    
    for (int i = 0; i < udp_len / 2; i++) {
        sum += ntohs(ptr[i]);
    }
    
    // 处理奇数字节
    if (udp_len % 2) {
        sum += udp_header[udp_len - 1] << 8;
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return htons(~sum);
}