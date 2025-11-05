#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/types.h>
#include <netinet/icmp6.h>

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
#include <rte_ring.h>

#include "arp_handler.h"

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32
#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

static struct rte_mempool *mbuf_pool;
static volatile bool force_quit = false;

static void
signal_handler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM) {
        printf("\n\nSignal %d received, preparing to exit...\n", signum);
        force_quit = true;
    }
}

static int
init_dpdk_ports(uint16_t port_id) {
    struct rte_eth_conf port_conf = {0};
    struct rte_eth_dev_info dev_info;
    
    rte_eth_dev_info_get(port_id, &dev_info);
    printf("Port %u: %s\n", port_id, dev_info.driver_name);
    
    /* 配置端口 */
    if (rte_eth_dev_configure(port_id, 1, 1, &port_conf) < 0) {
        printf("Cannot configure port %u\n", port_id);
        return -1;
    }
    
    /* 配置 RX 队列 */
    if (rte_eth_rx_queue_setup(port_id, 0, RX_RING_SIZE,
                                rte_eth_dev_socket_id(port_id),
                                NULL, mbuf_pool) < 0) {
        printf("Cannot setup RX queue for port %u\n", port_id);
        return -1;
    }
    
    /* 配置 TX 队列 */
    if (rte_eth_tx_queue_setup(port_id, 0, TX_RING_SIZE,
                                rte_eth_dev_socket_id(port_id),
                                NULL) < 0) {
        printf("Cannot setup TX queue for port %u\n", port_id);
        return -1;
    }
    
    /* 启动端口 */
    if (rte_eth_dev_start(port_id) < 0) {
        printf("Cannot start port %u\n", port_id);
        return -1;
    }
    
    /* 设置混杂模式 */
    rte_eth_promiscuous_enable(port_id);
    
    printf("Port %u started\n", port_id);
    return 0;
}

static int
lcore_mainloop(uint16_t port_id) {
    struct rte_mbuf *bufs[BURST_SIZE];
    uint16_t nb_rx;
    
    printf("Core %u: Processing packets on port %u\n", rte_lcore_id(), port_id);
    
    while (!force_quit) {
        /* 接收数据包 */
        nb_rx = rte_eth_rx_burst(port_id, 0, bufs, BURST_SIZE);
        if (nb_rx > 0) {
            RTE_LOG(DEBUG, USER1, "Received %u packets\n", nb_rx);
            for (uint16_t i = 0; i < nb_rx; i++) {
                /* 处理ARP/NDP包 */
                process_arp_packet(bufs[i], port_id);
            }
        }
    }
    
    return 0;
}

int
main(int argc, char **argv) {
    int ret;
    uint16_t port_id = 0;
    
    /* DPDK EAL 初始化 */
    ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "Cannot initialize EAL\n");
    }
    
    /* 注册信号处理 */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* 创建内存池 */
    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS,
                                         MBUF_CACHE_SIZE, 0,
                                         RTE_MBUF_DEFAULT_BUF_SIZE,
                                         rte_socket_id());
    if (mbuf_pool == NULL) {
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");
    }
    
    /* 初始化ARP处理模块 */
    init_arp_handler(port_id);
    
    /* 初始化网卡 */
    if (init_dpdk_ports(port_id) < 0) {
        rte_exit(EXIT_FAILURE, "Cannot initialize ports\n");
    }
    
    printf("\n=== DPDK ARP/NDP Handler ===\n");
    printf("Port: %u\n", port_id);
    printf("Press Ctrl+C to stop\n");
    
    /* 启动主循环 */
    lcore_mainloop(port_id);
    
    /* 清理 */
    rte_eth_dev_stop(port_id);
    rte_eth_dev_close(port_id);
    rte_eal_cleanup();
    
    printf("Bye!\n");
    return 0;
}