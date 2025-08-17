#include "packet_analyzer.h"
#include <pcapplusplus/PcapFileDevice.h>
#include <pcapplusplus/TcpLayer.h>
#include <pcapplusplus/UdpLayer.h>
#include <iostream>
#include <sstream>
#include <regex>

PacketAnalyzer::PacketAnalyzer(const std::string& client_ip)
    : client_ip_(client_ip)
    , total_packet_count_(0)
    , client_packet_count_(0)
    , server_packet_count_(0)
    , unknown_packet_count_(0) {
}

bool PacketAnalyzer::analyzePcapFile(const std::string& pcap_file_path) {
    // 清空之前的分析结果
    clear();
    
    // 打开PCAP文件
    pcpp::PcapFileReaderDevice reader(pcap_file_path);
    if (!reader.open()) {
        std::cerr << "错误: 无法打开PCAP文件: " << pcap_file_path << std::endl;
        return false;
    }
    
    std::cout << "正在分析PCAP文件: " << pcap_file_path << std::endl;
    std::cout << "客户端IP地址: " << client_ip_ << std::endl;
    
    pcpp::RawPacket raw_packet;
    int packet_index = 0;
    
    // 逐个读取和分析包
    while (reader.getNextPacket(raw_packet)) {
        PacketInfo packet_info = extractPacketInfo(raw_packet, packet_index);
        
        // 添加到所有包列表
        all_packets_.push_back(packet_info);
        
        // 根据方向分类
        switch (packet_info.direction) {
            case PacketDirection::CLIENT_TO_SERVER:
                client_packets_.push_back(packet_info);
                client_packet_count_++;
                break;
            case PacketDirection::SERVER_TO_CLIENT:
                server_packets_.push_back(packet_info);
                server_packet_count_++;
                break;
            case PacketDirection::UNKNOWN:
                unknown_packet_count_++;
                break;
        }
        
        total_packet_count_++;
        packet_index++;
        
        // 每处理1000个包显示一次进度
        if (packet_index % 1000 == 0) {
            std::cout << "已处理 " << packet_index << " 个包..." << std::endl;
        }
    }
    
    reader.close();
    
    // 打印分析结果
    printStatistics();
    
    return true;
}

const std::vector<PacketInfo>& PacketAnalyzer::getClientPackets() const {
    return client_packets_;
}

const std::vector<PacketInfo>& PacketAnalyzer::getServerPackets() const {
    return server_packets_;
}

const std::vector<PacketInfo>& PacketAnalyzer::getAllPackets() const {
    return all_packets_;
}

int PacketAnalyzer::getTotalPacketCount() const {
    return total_packet_count_;
}

int PacketAnalyzer::getClientPacketCount() const {
    return client_packet_count_;
}

int PacketAnalyzer::getServerPacketCount() const {
    return server_packet_count_;
}

int PacketAnalyzer::getUnknownPacketCount() const {
    return unknown_packet_count_;
}

PacketDirection PacketAnalyzer::analyzePacketDirection(const pcpp::Packet& packet) const {
    // 尝试获取IPv4层
    pcpp::IPv4Layer* ipv4_layer = packet.getLayerOfType<pcpp::IPv4Layer>();
    if (ipv4_layer != nullptr) {
        std::string src_ip = ipv4_layer->getSrcIPAddress().toString();
        std::string dst_ip = ipv4_layer->getDstIPAddress().toString();
        
        if (src_ip == client_ip_) {
            return PacketDirection::CLIENT_TO_SERVER;
        } else if (dst_ip == client_ip_) {
            return PacketDirection::SERVER_TO_CLIENT;
        }
    }
    
    // 尝试获取IPv6层
    pcpp::IPv6Layer* ipv6_layer = packet.getLayerOfType<pcpp::IPv6Layer>();
    if (ipv6_layer != nullptr) {
        std::string src_ip = ipv6_layer->getSrcIPAddress().toString();
        std::string dst_ip = ipv6_layer->getDstIPAddress().toString();
        
        if (src_ip == client_ip_) {
            return PacketDirection::CLIENT_TO_SERVER;
        } else if (dst_ip == client_ip_) {
            return PacketDirection::SERVER_TO_CLIENT;
        }
    }
    
    return PacketDirection::UNKNOWN;
}

PacketInfo PacketAnalyzer::extractPacketInfo(const pcpp::RawPacket& raw_packet, int index) const {
    PacketInfo info;
    info.raw_packet = raw_packet;
    info.packet_size = raw_packet.getRawDataLen();
    info.timestamp = raw_packet.getPacketTimeStamp();
    info.timestamp_us = static_cast<uint64_t>(info.timestamp.tv_sec) * 1000000 + info.timestamp.tv_nsec / 1000;
    info.original_index = index;
    
    // 复制原始数据
    const uint8_t* raw_data_ptr = raw_packet.getRawData();
    info.raw_data.assign(raw_data_ptr, raw_data_ptr + info.packet_size);
    
    // 解析包内容
    pcpp::RawPacket* raw_packet_ptr = const_cast<pcpp::RawPacket*>(&raw_packet);
    pcpp::Packet packet(raw_packet_ptr);
    
    // 分析包方向
    info.direction = analyzePacketDirection(packet);
    
    // 提取IP地址信息
    pcpp::IPv4Layer* ipv4_layer = packet.getLayerOfType<pcpp::IPv4Layer>();
    if (ipv4_layer != nullptr) {
        extractIPv4Info(ipv4_layer, info.src_ip, info.dst_ip);
    } else {
        pcpp::IPv6Layer* ipv6_layer = packet.getLayerOfType<pcpp::IPv6Layer>();
        if (ipv6_layer != nullptr) {
            extractIPv6Info(ipv6_layer, info.src_ip, info.dst_ip);
        }
    }
    
    // 提取端口信息
    extractPortInfo(packet, info.src_port, info.dst_port);
    
    return info;
}

void PacketAnalyzer::setClientIP(const std::string& client_ip) {
    client_ip_ = client_ip;
}

const std::string& PacketAnalyzer::getClientIP() const {
    return client_ip_;
}

void PacketAnalyzer::clear() {
    all_packets_.clear();
    client_packets_.clear();
    server_packets_.clear();
    
    total_packet_count_ = 0;
    client_packet_count_ = 0;
    server_packet_count_ = 0;
    unknown_packet_count_ = 0;
}

void PacketAnalyzer::printStatistics() const {
    std::cout << "\n=== PCAP文件分析结果 ===" << std::endl;
    std::cout << "总包数: " << total_packet_count_ << std::endl;
    std::cout << "客户端包数: " << client_packet_count_ << " (" 
              << (total_packet_count_ > 0 ? (double)client_packet_count_ / total_packet_count_ * 100.0 : 0.0) 
              << "%)" << std::endl;
    std::cout << "服务端包数: " << server_packet_count_ << " ("
              << (total_packet_count_ > 0 ? (double)server_packet_count_ / total_packet_count_ * 100.0 : 0.0)
              << "%)" << std::endl;
    std::cout << "未知方向包数: " << unknown_packet_count_ << " ("
              << (total_packet_count_ > 0 ? (double)unknown_packet_count_ / total_packet_count_ * 100.0 : 0.0)
              << "%)" << std::endl;
    std::cout << "客户端IP: " << client_ip_ << std::endl;
    std::cout << "========================" << std::endl;
}

bool PacketAnalyzer::isValidIPAddress(const std::string& ip_str) {
    // IPv4地址正则表达式
    std::regex ipv4_regex(
        R"(^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)"
    );
    
    // IPv6地址正则表达式（简化版）
    std::regex ipv6_regex(
        R"(^(?:[0-9a-fA-F]{1,4}:){7}[0-9a-fA-F]{1,4}$|^::1$|^::$)"
    );
    
    return std::regex_match(ip_str, ipv4_regex) || std::regex_match(ip_str, ipv6_regex);
}

bool PacketAnalyzer::extractIPv4Info(pcpp::IPv4Layer* ipv4_layer, std::string& src_ip, std::string& dst_ip) const {
    if (ipv4_layer == nullptr) {
        return false;
    }
    
    src_ip = ipv4_layer->getSrcIPAddress().toString();
    dst_ip = ipv4_layer->getDstIPAddress().toString();
    
    return true;
}

bool PacketAnalyzer::extractIPv6Info(pcpp::IPv6Layer* ipv6_layer, std::string& src_ip, std::string& dst_ip) const {
    if (ipv6_layer == nullptr) {
        return false;
    }
    
    src_ip = ipv6_layer->getSrcIPAddress().toString();
    dst_ip = ipv6_layer->getDstIPAddress().toString();
    
    return true;
}

bool PacketAnalyzer::extractPortInfo(const pcpp::Packet& packet, uint16_t& src_port, uint16_t& dst_port) const {
    // 尝试TCP层
    pcpp::TcpLayer* tcp_layer = packet.getLayerOfType<pcpp::TcpLayer>();
    if (tcp_layer != nullptr) {
        src_port = tcp_layer->getSrcPort();
        dst_port = tcp_layer->getDstPort();
        return true;
    }
    
    // 尝试UDP层
    pcpp::UdpLayer* udp_layer = packet.getLayerOfType<pcpp::UdpLayer>();
    if (udp_layer != nullptr) {
        src_port = udp_layer->getSrcPort();
        dst_port = udp_layer->getDstPort();
        return true;
    }
    
    // 没有找到传输层，设置为0
    src_port = 0;
    dst_port = 0;
    return false;
}