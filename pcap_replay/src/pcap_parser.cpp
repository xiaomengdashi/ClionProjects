#include "pcap_parser.h"
#include <iostream>
#include <cstring>
#include <arpa/inet.h>

PcapParser::PcapParser() : pcap_handle_(nullptr), total_packets_(0) {
}

PcapParser::~PcapParser() {
    close();
}

bool PcapParser::openFile(const std::string& filename) {
    char errbuf[PCAP_ERRBUF_SIZE];
    
    pcap_handle_ = pcap_open_offline(filename.c_str(), errbuf);
    if (pcap_handle_ == nullptr) {
        std::cerr << "无法打开PCAP文件: " << filename << " - " << errbuf << std::endl;
        return false;
    }
    
    std::cout << "成功打开PCAP文件: " << filename << std::endl;
    return true;
}

bool PcapParser::parseNextPacket(PacketInfo& packet_info) {
    struct pcap_pkthdr* header;
    const uint8_t* packet_data;
    
    int result = pcap_next_ex(pcap_handle_, &header, &packet_data);
    if (result != 1) {
        return false; // 没有更多数据包或出错
    }
    
    // 复制时间戳
    packet_info.timestamp = header->ts;
    packet_info.packet_size = header->len;
    
    // 解析以太网帧
    return parseEthernetFrame(packet_data, header->caplen, packet_info);
}

bool PcapParser::setFilter(const std::string& filter_expr) {
    if (pcap_handle_ == nullptr) {
        return false;
    }
    
    struct bpf_program filter;
    if (pcap_compile(pcap_handle_, &filter, filter_expr.c_str(), 0, PCAP_NETMASK_UNKNOWN) == -1) {
        std::cerr << "无法编译过滤器: " << filter_expr << std::endl;
        return false;
    }
    
    if (pcap_setfilter(pcap_handle_, &filter) == -1) {
        std::cerr << "无法设置过滤器: " << filter_expr << std::endl;
        pcap_freecode(&filter);
        return false;
    }
    
    pcap_freecode(&filter);
    std::cout << "设置过滤器: " << filter_expr << std::endl;
    return true;
}

void PcapParser::close() {
    if (pcap_handle_) {
        pcap_close(pcap_handle_);
        pcap_handle_ = nullptr;
    }
}

bool PcapParser::parseEthernetFrame(const uint8_t* packet, int packet_len, PacketInfo& info) {
    if (packet_len < 14) { // 以太网头部最小长度
        return false;
    }
    
    // 跳过以太网头部（14字节）
    const uint8_t* ip_packet = packet + 14;
    int ip_len = packet_len - 14;
    
    // 检查是否为IP数据包
    uint16_t ethertype = ntohs(*(uint16_t*)(packet + 12));
    if (ethertype != 0x0800) { // IPv4
        return false;
    }
    
    return parseIPPacket(ip_packet, ip_len, info);
}

bool PcapParser::parseIPPacket(const uint8_t* ip_packet, int ip_len, PacketInfo& info) {
    if (ip_len < 20) { // IP头部最小长度
        return false;
    }
    
#ifdef __APPLE__
    const struct ip* ip_header = reinterpret_cast<const struct ip*>(ip_packet);
    
    // 提取IP地址
    info.src_ip = ip_header->ip_src.s_addr;
    info.dst_ip = ip_header->ip_dst.s_addr;
    info.protocol = ip_header->ip_p;
    
    // 计算IP头部长度
    int ip_header_len = (ip_header->ip_hl) * 4;
#else
    const struct iphdr* ip_header = reinterpret_cast<const struct iphdr*>(ip_packet);
    
    // 提取IP地址
    info.src_ip = ip_header->saddr;
    info.dst_ip = ip_header->daddr;
    info.protocol = ip_header->protocol;
    
    // 计算IP头部长度
    int ip_header_len = (ip_header->ihl) * 4;
#endif
    
    if (ip_len < ip_header_len) {
        return false;
    }
    
    const uint8_t* payload = ip_packet + ip_header_len;
    int payload_len = ip_len - ip_header_len;
    
    // 根据协议类型解析
    switch (info.protocol) {
        case IPPROTO_TCP:
            return parseTCPPacket(payload, payload_len, info);
        case IPPROTO_UDP:
            return parseUDPPacket(payload, payload_len, info);
        default:
            // 其他协议，直接复制载荷
            info.src_port = 0;
            info.dst_port = 0;
            info.payload.assign(payload, payload + payload_len);
            return true;
    }
}

bool PcapParser::parseTCPPacket(const uint8_t* tcp_packet, int tcp_len, PacketInfo& info) {
    if (tcp_len < 20) { // TCP头部最小长度
        return false;
    }
    
    const struct tcphdr* tcp_header = reinterpret_cast<const struct tcphdr*>(tcp_packet);
    
    // 提取端口号
#ifdef __APPLE__
    info.src_port = ntohs(tcp_header->th_sport);
    info.dst_port = ntohs(tcp_header->th_dport);
    
    // 计算TCP头部长度
    int tcp_header_len = tcp_header->th_off * 4;
#else
    info.src_port = ntohs(tcp_header->source);
    info.dst_port = ntohs(tcp_header->dest);
    
    // 计算TCP头部长度
    int tcp_header_len = tcp_header->doff * 4;
#endif
    
    if (tcp_len < tcp_header_len) {
        return false;
    }
    
    // 提取载荷数据
    const uint8_t* payload = tcp_packet + tcp_header_len;
    int payload_len = tcp_len - tcp_header_len;
    
    if (payload_len > 0) {
        info.payload.assign(payload, payload + payload_len);
    }
    
    return true;
}

bool PcapParser::parseUDPPacket(const uint8_t* udp_packet, int udp_len, PacketInfo& info) {
    if (udp_len < 8) { // UDP头部长度
        return false;
    }
    
    const struct udphdr* udp_header = reinterpret_cast<const struct udphdr*>(udp_packet);
    
    // 提取端口号
#ifdef __APPLE__
    info.src_port = ntohs(udp_header->uh_sport);
    info.dst_port = ntohs(udp_header->uh_dport);
#else
    info.src_port = ntohs(udp_header->source);
    info.dst_port = ntohs(udp_header->dest);
#endif
    
    // 提取载荷数据
    const uint8_t* payload = udp_packet + 8;
    int payload_len = udp_len - 8;
    
    if (payload_len > 0) {
        info.payload.assign(payload, payload + payload_len);
    }
    
    return true;
}