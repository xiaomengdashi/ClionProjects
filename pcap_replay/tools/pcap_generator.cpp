#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <arpa/inet.h>
#include <chrono>

// PCAP 文件头结构
struct PcapFileHeader {
    uint32_t magic_number;   // 0xa1b2c3d4
    uint16_t version_major;  // 2
    uint16_t version_minor;  // 4
    int32_t  thiszone;       // 0
    uint32_t sigfigs;        // 0
    uint32_t snaplen;        // 65535
    uint32_t network;        // 1 (Ethernet)
};

// PCAP 数据包头结构
struct PcapPacketHeader {
    uint32_t ts_sec;         // 时间戳秒
    uint32_t ts_usec;        // 时间戳微秒
    uint32_t incl_len;       // 包含长度
    uint32_t orig_len;       // 原始长度
};

// 以太网头结构
struct EthernetHeader {
    uint8_t  dst_mac[6];     // 目标MAC地址
    uint8_t  src_mac[6];     // 源MAC地址
    uint16_t ethertype;      // 0x0800 for IPv4
};

// IP头结构
struct IPHeader {
    uint8_t  version_ihl;    // 版本(4位) + 头长度(4位)
    uint8_t  tos;            // 服务类型
    uint16_t total_length;   // 总长度
    uint16_t identification; // 标识
    uint16_t flags_fragment; // 标志(3位) + 片偏移(13位)
    uint8_t  ttl;            // 生存时间
    uint8_t  protocol;       // 协议 (6=TCP, 17=UDP)
    uint16_t checksum;       // 校验和
    uint32_t src_ip;         // 源IP地址
    uint32_t dst_ip;         // 目标IP地址
};

// TCP头结构
struct TCPHeader {
    uint16_t src_port;       // 源端口
    uint16_t dst_port;       // 目标端口
    uint32_t seq_num;        // 序列号
    uint32_t ack_num;        // 确认号
    uint8_t  data_offset;    // 数据偏移(4位) + 保留(4位)
    uint8_t  flags;          // 标志
    uint16_t window;         // 窗口大小
    uint16_t checksum;       // 校验和
    uint16_t urgent_ptr;     // 紧急指针
};

class PcapGenerator {
public:
    PcapGenerator(const std::string& filename) : filename_(filename) {}
    
    bool createSamplePcap() {
        std::ofstream file(filename_, std::ios::binary);
        if (!file) {
            std::cerr << "无法创建文件: " << filename_ << std::endl;
            return false;
        }
        
        // 写入PCAP文件头
        writePcapHeader(file);
        
        // 创建一些示例数据包
        createSamplePackets(file);
        
        file.close();
        std::cout << "示例PCAP文件已创建: " << filename_ << std::endl;
        return true;
    }

private:
    std::string filename_;
    
    void writePcapHeader(std::ofstream& file) {
        PcapFileHeader header = {};
        header.magic_number = 0xa1b2c3d4;
        header.version_major = 2;
        header.version_minor = 4;
        header.thiszone = 0;
        header.sigfigs = 0;
        header.snaplen = 65535;
        header.network = 1; // Ethernet
        
        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    }
    
    void createSamplePackets(std::ofstream& file) {
        // 创建客户端到服务端的数据包
        createTCPPacket(file, "192.168.1.200", "192.168.1.100", 12345, 80, 
                       "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n", true);
        
        // 创建服务端到客户端的数据包
        createTCPPacket(file, "192.168.1.100", "192.168.1.200", 80, 12345,
                       "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, World!", false);
        
        // 创建更多客户端到服务端的数据包
        createTCPPacket(file, "192.168.1.200", "192.168.1.100", 12346, 80,
                       "POST /api/data HTTP/1.1\r\nHost: example.com\r\nContent-Length: 15\r\n\r\n{\"test\":\"data\"}", true);
        
        // 创建更多服务端到客户端的数据包
        createTCPPacket(file, "192.168.1.100", "192.168.1.200", 80, 12346,
                       "HTTP/1.1 201 Created\r\nContent-Length: 18\r\n\r\n{\"status\":\"ok\"}", false);
        
        std::cout << "已创建4个示例数据包" << std::endl;
    }
    
    void createTCPPacket(std::ofstream& file, const std::string& src_ip, const std::string& dst_ip,
                        uint16_t src_port, uint16_t dst_port, const std::string& payload, bool is_client) {
        
        // 获取当前时间戳
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto duration = now.time_since_epoch();
        auto micros = std::chrono::duration_cast<std::chrono::microseconds>(duration).count() % 1000000;
        
        // 构建数据包
        std::vector<uint8_t> packet;
        
        // 以太网头
        EthernetHeader eth_header = {};
        // 设置示例MAC地址
        uint8_t src_mac[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
        uint8_t dst_mac[] = {0x00, 0xaa, 0xbb, 0xcc, 0xdd, 0xee};
        
        if (is_client) {
            memcpy(eth_header.src_mac, src_mac, 6);
            memcpy(eth_header.dst_mac, dst_mac, 6);
        } else {
            memcpy(eth_header.src_mac, dst_mac, 6);
            memcpy(eth_header.dst_mac, src_mac, 6);
        }
        eth_header.ethertype = htons(0x0800); // IPv4
        
        // IP头
        IPHeader ip_header = {};
        ip_header.version_ihl = 0x45; // IPv4, 20字节头长度
        ip_header.tos = 0;
        ip_header.total_length = htons(sizeof(IPHeader) + sizeof(TCPHeader) + payload.length());
        ip_header.identification = htons(0x1234);
        ip_header.flags_fragment = htons(0x4000); // Don't fragment
        ip_header.ttl = 64;
        ip_header.protocol = 6; // TCP
        ip_header.checksum = 0; // 稍后计算
        ip_header.src_ip = inet_addr(src_ip.c_str());
        ip_header.dst_ip = inet_addr(dst_ip.c_str());
        
        // TCP头
        TCPHeader tcp_header = {};
        tcp_header.src_port = htons(src_port);
        tcp_header.dst_port = htons(dst_port);
        tcp_header.seq_num = htonl(0x12345678);
        tcp_header.ack_num = htonl(0x87654321);
        tcp_header.data_offset = 0x50; // 20字节头长度
        tcp_header.flags = 0x18; // PSH + ACK
        tcp_header.window = htons(65535);
        tcp_header.checksum = 0; // 稍后计算
        tcp_header.urgent_ptr = 0;
        
        // 组装数据包
        packet.resize(sizeof(EthernetHeader) + sizeof(IPHeader) + sizeof(TCPHeader) + payload.length());
        
        size_t offset = 0;
        memcpy(packet.data() + offset, &eth_header, sizeof(EthernetHeader));
        offset += sizeof(EthernetHeader);
        
        memcpy(packet.data() + offset, &ip_header, sizeof(IPHeader));
        offset += sizeof(IPHeader);
        
        memcpy(packet.data() + offset, &tcp_header, sizeof(TCPHeader));
        offset += sizeof(TCPHeader);
        
        memcpy(packet.data() + offset, payload.c_str(), payload.length());
        
        // 写入PCAP数据包头
        PcapPacketHeader pkt_header = {};
        pkt_header.ts_sec = static_cast<uint32_t>(time_t);
        pkt_header.ts_usec = static_cast<uint32_t>(micros);
        pkt_header.incl_len = packet.size();
        pkt_header.orig_len = packet.size();
        
        file.write(reinterpret_cast<const char*>(&pkt_header), sizeof(pkt_header));
        file.write(reinterpret_cast<const char*>(packet.data()), packet.size());
        
        std::cout << "创建数据包: " << src_ip << ":" << src_port 
                 << " -> " << dst_ip << ":" << dst_port 
                 << " (载荷: " << payload.length() << " 字节)" << std::endl;
    }
};

int main(int argc, char* argv[]) {
    std::string filename = (argc > 1) ? argv[1] : "sample.pcap";
    
    std::cout << "创建示例PCAP文件: " << filename << std::endl;
    
    PcapGenerator generator(filename);
    if (!generator.createSamplePcap()) {
        return 1;
    }
    
    std::cout << "示例PCAP文件创建完成！" << std::endl;
    std::cout << "可以使用以下命令测试回放程序：" << std::endl;
    std::cout << "./build/pcap_replayer " << filename 
             << " 192.168.1.100 192.168.1.200 127.0.0.1 127.0.0.1 8080" << std::endl;
    
    return 0;
}