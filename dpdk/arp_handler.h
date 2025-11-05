#ifndef _ARP_HANDLER_H_
#define _ARP_HANDLER_H_

#include <stdint.h>

/* 如果DPDK可用，则包含DPDK头文件 */
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_tcp.h>

/**
 * 初始化ARP处理模块
 */
void init_arp_handler(uint16_t port_id);

/**
 * 处理接收到的数据包，识别并处理ARP/NDP包
 * @param mbuf 数据包缓冲区
 * @param port_id 接收端口ID
 */
void process_arp_packet(struct rte_mbuf *mbuf, uint16_t port_id);

#endif /* _ARP_HANDLER_H_ */