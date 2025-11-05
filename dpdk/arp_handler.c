#include "arp_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/udp.h>
#include <netinet/icmp6.h>
#include <arpa/inet.h>

/* DPDK头文件 */
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>

/* 定义日志类型 */
static int RTE_LOGTYPE_ARP = 0;

/* ARP头部结构 */
struct arp_header {
    uint16_t ar_hrd;    /* 硬件地址格式 */
    uint16_t ar_pro;    /* 协议地址格式 */
    uint8_t  ar_hln;    /* 硬件地址长度 */
    uint8_t  ar_pln;    /* 协议地址长度 */
    uint16_t ar_op;     /* 操作码 */
    uint8_t  ar_sha[6]; /* 发送方硬件地址 */
    uint32_t ar_sip;    /* 发送方IP地址 */
    uint8_t  ar_tha[6]; /* 目标硬件地址 */
    uint32_t ar_tip;    /* 目标IP地址 */
} __attribute__((packed));

/* 常量定义 */
#define ETHER_TYPE_ARP  0x0806
#define ETHER_TYPE_IPv6 0x86DD
#define ARPOP_REQUEST   1
#define ARPOP_REPLY     2
#ifndef IPPROTO_ICMPV6
#define IPPROTO_ICMPV6  58
#endif
#define ICMP6_ND_NEIGHBOR_SOLICIT 135
#define ICMP6_ND_NEIGHBOR_ADVERT  136

/* 全局变量 */
static uint16_t g_port_id = 0;
static uint8_t local_mac[6] = {0};

/**
 * 初始化ARP处理模块
 */
void init_arp_handler(uint16_t port_id) {
    g_port_id = port_id;
    
    RTE_LOGTYPE_ARP = rte_log_register("arp");
    if (RTE_LOGTYPE_ARP < 0) {
        RTE_LOGTYPE_ARP = 0;  /* 使用默认值 */
    }
    
    /* 获取端口的MAC地址作为本地MAC地址 */
    struct rte_ether_addr mac_addr;
    rte_eth_macaddr_get(port_id, &mac_addr);
    memcpy(local_mac, mac_addr.addr_bytes, 6);
    
    RTE_LOG(INFO, ARP, "ARP handler initialized with MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
            local_mac[0], local_mac[1], local_mac[2], 
            local_mac[3], local_mac[4], local_mac[5]);
}

/**
 * 处理ARP请求并发送ARP响应
 */
static inline void handle_arp_request(struct rte_mbuf *mbuf) {
    struct rte_ether_hdr *eth_hdr = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);
    struct arp_header *arp_hdr = (struct arp_header *)(eth_hdr + 1);
    
    /* 检查是否为ARP请求 */
    if (rte_be_to_cpu_16(arp_hdr->ar_op) != ARPOP_REQUEST) {
        return;
    }
    
    RTE_LOG(DEBUG, ARP, "Received ARP request for IP: %s\n", inet_ntoa(*(struct in_addr*)&arp_hdr->ar_tip));
    
    /* 构造ARP响应 */
    /* 交换以太网头部的源和目标MAC地址 */
    memcpy(eth_hdr->dst_addr.addr_bytes, eth_hdr->src_addr.addr_bytes, 6);
    memcpy(eth_hdr->src_addr.addr_bytes, local_mac, 6);
    
    /* 修改ARP头部 */
    arp_hdr->ar_op = rte_cpu_to_be_16(ARPOP_REPLY); /* 操作码改为ARP响应 */
    
    /* 交换发送方和目标硬件地址 */
    memcpy(arp_hdr->ar_tha, arp_hdr->ar_sha, 6);
    memcpy(arp_hdr->ar_sha, local_mac, 6);
    
    /* 交换发送方和目标IP地址 */
    uint32_t tmp_ip = arp_hdr->ar_sip;
    arp_hdr->ar_sip = arp_hdr->ar_tip;
    arp_hdr->ar_tip = tmp_ip;
    
    /* 从接收端口发送ARP响应 */
    uint16_t sent = rte_eth_tx_burst(g_port_id, 0, &mbuf, 1);
    if (sent > 0) {
        RTE_LOG(DEBUG, ARP, "Sent ARP reply\n");
    } else {
        rte_pktmbuf_free(mbuf);
    }
}

/* 计算ICMPv6校验和 */
static inline uint16_t calculate_icmp6_checksum(struct rte_ipv6_hdr *ip6_hdr, void *icmp6_hdr, uint16_t len) {
    uint32_t cksum = 0;
    uint32_t tmp;
    
    /* 计算伪头部校验和 */
    /* 源IPv6地址 */
    cksum += rte_raw_cksum(ip6_hdr->src_addr, sizeof(ip6_hdr->src_addr));
    
    /* 目标IPv6地址 */
    cksum += rte_raw_cksum(ip6_hdr->dst_addr, sizeof(ip6_hdr->dst_addr));
    
    /* 上层协议长度 */
    tmp = rte_cpu_to_be_32(len);
    cksum += tmp;
    
    /* 上层协议号 */
    tmp = rte_cpu_to_be_32(ip6_hdr->proto);
    cksum += tmp;
    
    /* ICMPv6头部和数据 */
    cksum += rte_raw_cksum(icmp6_hdr, len);
    
    /* 折叠32位校验和为16位 */
    cksum = (cksum >> 16) + (cksum & 0xffff);
    cksum += (cksum >> 16);
    
    /* 返回补码 */
    return (~cksum) & 0xffff;
}

/**
 * 处理ICMPv6 ND NS消息并发送NA响应
 */
static inline void handle_ndp_ns(struct rte_mbuf *mbuf) {
    struct rte_ether_hdr *eth_hdr = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);
    struct rte_ipv6_hdr *ip6_hdr = (struct rte_ipv6_hdr *)(eth_hdr + 1);
    struct nd_neighbor_solicit *ns_hdr = (struct nd_neighbor_solicit *)((uint8_t*)ip6_hdr + sizeof(struct rte_ipv6_hdr));
    struct nd_neighbor_advert *na_hdr;
    struct nd_opt_hdr *opt_hdr;
    
    /* 检查是否为NS消息 */
    if (ns_hdr->nd_ns_type != ICMP6_ND_NEIGHBOR_SOLICIT) {
        return;
    }
    
    RTE_LOG(DEBUG, ARP, "Received NDP NS for target address\n");
    
    /* 构造NA响应 */
    /* 交换以太网头部的源和目标MAC地址 */
    memcpy(eth_hdr->dst_addr.addr_bytes, eth_hdr->src_addr.addr_bytes, 6);
    memcpy(eth_hdr->src_addr.addr_bytes, local_mac, 6);
    
    /* 交换IPv6头部的源和目标地址 */
    uint8_t tmp_addr[16];
    memcpy(tmp_addr, ip6_hdr->src_addr, 16);
    memcpy(ip6_hdr->src_addr, ip6_hdr->dst_addr, 16);
    memcpy(ip6_hdr->dst_addr, tmp_addr, 16);
    
    /* 重新构造ICMPv6 NA消息 */
    na_hdr = (struct nd_neighbor_advert *)ns_hdr;
    na_hdr->nd_na_type = ICMP6_ND_NEIGHBOR_ADVERT;
    na_hdr->nd_na_code = 0;
    na_hdr->nd_na_flags_reserved = rte_cpu_to_be_32(0x60000000); /* 设置S和O标志位 */
    /* 目标地址字段(nd_na_target)保持不变，因为它在NS和NA消息中都在相同位置 */
    
    /* 添加目标链路层地址选项 */
    opt_hdr = (struct nd_opt_hdr *)((uint8_t*)na_hdr + sizeof(struct nd_neighbor_advert));
    opt_hdr->nd_opt_type = ND_OPT_TARGET_LINKADDR;
    opt_hdr->nd_opt_len = 1; /* 长度为1个单位(8字节) */
    memcpy(opt_hdr + 1, local_mac, 6);
    
    /* 更新IPv6头部 */
    uint16_t icmp6_len = sizeof(struct nd_neighbor_advert) + sizeof(struct nd_opt_hdr) + 6; /* NA头部 + 选项头部 + MAC地址 */
    ip6_hdr->payload_len = rte_cpu_to_be_16(icmp6_len);
    ip6_hdr->proto = IPPROTO_ICMPV6;
    ip6_hdr->hop_limits = 255;
    
    /* 计算并设置ICMPv6校验和 */
    na_hdr->nd_na_hdr.icmp6_cksum = 0; /* 先清零 */
    na_hdr->nd_na_hdr.icmp6_cksum = rte_cpu_to_be_16(rte_ipv6_udptcp_cksum(ip6_hdr, na_hdr));
    
    /* 从接收端口发送NA响应 */
    uint16_t sent = rte_eth_tx_burst(g_port_id, 0, &mbuf, 1);
    if (sent > 0) {
        RTE_LOG(DEBUG, ARP, "Sent NDP NA reply\n");
    } else {
        rte_pktmbuf_free(mbuf);
    }
}

/**
 * 处理接收到的数据包，识别并处理ARP/NDP包
 */
void process_arp_packet(struct rte_mbuf *mbuf, uint16_t port_id) {
    struct rte_ether_hdr *eth_hdr = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);
    uint16_t ether_type = rte_be_to_cpu_16(eth_hdr->ether_type);
    
    switch (ether_type) {
        case ETHER_TYPE_ARP:
            RTE_LOG(DEBUG, ARP, "Processing ARP packet\n");
            handle_arp_request(mbuf);
            break;
            
        case ETHER_TYPE_IPv6:
            RTE_LOG(DEBUG, ARP, "Processing IPv6 packet\n");
            /* 检查是否为ICMPv6 ND消息 */
            struct rte_ipv6_hdr *ip6_hdr = (struct rte_ipv6_hdr *)(eth_hdr + 1);
            if (ip6_hdr->proto == IPPROTO_ICMPV6) {
                struct nd_neighbor_solicit *ns_hdr = (struct nd_neighbor_solicit *)((uint8_t*)ip6_hdr + sizeof(struct rte_ipv6_hdr));
                if (ns_hdr->nd_ns_type == ICMP6_ND_NEIGHBOR_SOLICIT) {
                    handle_ndp_ns(mbuf);
                } else {
                    /* 不是NS消息，释放包 */
                    rte_pktmbuf_free(mbuf);
                }
            } else {
                /* 不是ICMPv6，释放包 */
                rte_pktmbuf_free(mbuf);
            }
            break;
            
        default:
            /* 不是ARP或IPv6包，释放包 */
            rte_pktmbuf_free(mbuf);
            break;
    }
}