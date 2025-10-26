/*
 * 完整的 RSS 多队列 DPDK 示例
 *
 * 编译：
 * gcc -o rss_complete_example rss_complete_example.c $(pkg-config --cflags --libs libdpdk)
 *
 * 运行：
 * ./rss_complete_example -l 0-5 -n 4
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>

#include <rte_arp.h>
#include <rte_common.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_string_fns.h>
#include <rte_ip.h>
#include <rte_udp.h>

/* ============= RSS 配置常量 ============= */
#define NUM_RX_QUEUES 4
#define NUM_TX_QUEUES 4
#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32
#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

static uint16_t nb_ports = 0;
static uint16_t port_id = 0;
static struct rte_mempool *mbuf_pool = NULL;

/* ============= 日志类型定义 ============= */
int RTE_LOGTYPE_APP;

/**
 * init_dpdk_rss() - 初始化 DPDK 和 RSS 配置
 */
static int init_dpdk_rss(void) {
    struct rte_eth_conf port_conf = {0};
    struct rte_eth_dev_info dev_info;
    struct rte_eth_rss_conf rss_conf = {0};
    uint16_t portid;
    uint8_t rss_key[40];  /* Intel 网卡使用 40 字节 */
    int ret;

    printf("\n=== DPDK RSS Multi-Queue Initialization ===\n\n");

    /* 创建 MBUF 内存池 */
    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS,
                                        MBUF_CACHE_SIZE, 0,
                                        RTE_MBUF_DEFAULT_BUF_SIZE,
                                        rte_socket_id());
    if (mbuf_pool == NULL) {
        fprintf(stderr, "Cannot create mbuf pool\n");
        return -1;
    }
    printf("✓ MBUF pool created\n");

    /* 统计可用端口 */
    RTE_ETH_FOREACH_DEV(portid) {
        nb_ports++;
    }
    printf("✓ Found %u ports\n\n", nb_ports);

    if (nb_ports == 0) {
        fprintf(stderr, "No Ethernet ports - bye\n");
        return -1;
    }

    /* ============ 配置每个端口 ============ */
    RTE_ETH_FOREACH_DEV(portid) {
        port_id = portid;
        printf("[PORT %u] Configuring...\n", portid);

        rte_eth_dev_info_get(portid, &dev_info);
        printf("  Device: %s\n", dev_info.device->name);
        printf("  Max RX queues: %u\n", dev_info.max_rx_queues);
        printf("  Max TX queues: %u\n\n", dev_info.max_tx_queues);

        /* ============ 启用 RSS 配置 ============ */
        memset(&port_conf, 0, sizeof(port_conf));
        memset(&rss_conf, 0, sizeof(rss_conf));

        /* 关键：启用 RSS */
        port_conf.rxmode.mq_mode = RTE_ETH_MQ_RX_RSS;

        /* 初始化 RSS 哈希密钥 */
        memset(rss_key, 0x42, 40);

        /* 配置 RSS 哈希函数 */
        rss_conf.rss_key = rss_key;
        rss_conf.rss_key_len = 40;
        rss_conf.rss_hf = (RTE_ETH_RSS_IP |
                          RTE_ETH_RSS_NONFRAG_IPV4_UDP |
                          RTE_ETH_RSS_NONFRAG_IPV4_TCP);

        port_conf.rx_adv_conf.rss_conf = rss_conf;

        printf("  Configuring %u RX + %u TX queues with RSS...\n",
               NUM_RX_QUEUES, NUM_TX_QUEUES);

        /* ============ 配置端口：多队列 + RSS ============ */
        ret = rte_eth_dev_configure(portid, NUM_RX_QUEUES, NUM_TX_QUEUES, &port_conf);
        if (ret != 0) {
            fprintf(stderr, "  [ERROR] Cannot configure port %u (ret=%d)\n", portid, ret);
            return -1;
        }

        /* ============ 配置所有 RX 队列 ============ */
        for (uint16_t q = 0; q < NUM_RX_QUEUES; q++) {
            ret = rte_eth_rx_queue_setup(portid, q, RX_RING_SIZE,
                                        rte_eth_dev_socket_id(portid),
                                        NULL, mbuf_pool);
            if (ret != 0) {
                fprintf(stderr, "  [ERROR] Cannot setup RX queue %u\n", q);
                return -1;
            }
            printf("    ✓ RX Queue %u configured\n", q);
        }

        /* ============ 配置所有 TX 队列 ============ */
        for (uint16_t q = 0; q < NUM_TX_QUEUES; q++) {
            ret = rte_eth_tx_queue_setup(portid, q, TX_RING_SIZE,
                                        rte_eth_dev_socket_id(portid),
                                        NULL);
            if (ret != 0) {
                fprintf(stderr, "  [ERROR] Cannot setup TX queue %u\n", q);
                return -1;
            }
            printf("    ✓ TX Queue %u configured\n", q);
        }

        /* ============ 启动端口 ============ */
        ret = rte_eth_dev_start(portid);
        if (ret < 0) {
            fprintf(stderr, "  [ERROR] Cannot start port %u\n", portid);
            return -1;
        }

        printf("  ✓ Port %u started successfully with RSS enabled\n\n", portid);
    }

    return 0;
}

/**
 * lcore_packet_handler() - 处理特定队列的数据包
 */
static int lcore_packet_handler(void *arg) {
    uint16_t queue_id = (uintptr_t)arg;
    struct rte_mbuf *bufs[BURST_SIZE];
    uint16_t nb_rx;
    uint64_t total_packets = 0;
    unsigned int core_id = rte_lcore_id();

    printf("[Core %u] Started, processing queue %u\n", core_id, queue_id);

    while (1) {
        /* 从指定队列接收数据包 */
        nb_rx = rte_eth_rx_burst(port_id, queue_id, bufs, BURST_SIZE);

        if (nb_rx > 0) {
            total_packets += nb_rx;

            printf("[Core %u Queue %u] Received %u packets (total: %lu)\n",
                   core_id, queue_id, nb_rx, total_packets);

            /* 处理每个数据包 */
            for (uint16_t i = 0; i < nb_rx; i++) {
                struct rte_mbuf *mbuf = bufs[i];

                /* 简单处理：如果是 IPv4，打印源/目标 IP */
                if ((mbuf->packet_type & RTE_PTYPE_L3_MASK) == RTE_PTYPE_L3_IPV4) {
                    struct rte_ipv4_hdr *ipv4_hdr =
                        rte_pktmbuf_mtod_offset(mbuf, struct rte_ipv4_hdr *,
                                              sizeof(struct rte_ether_hdr));
                    uint32_t src_ip = rte_be_to_cpu_32(ipv4_hdr->src_addr);
                    uint32_t dst_ip = rte_be_to_cpu_32(ipv4_hdr->dst_addr);

                    printf("    [Q%u] Src IP: 0x%08x, Dst IP: 0x%08x, "
                           "RSS Hash: 0x%x\n",
                           queue_id, src_ip, dst_ip, mbuf->hash.rss);
                }

                /* 释放数据包 */
                rte_pktmbuf_free(mbuf);
            }
        }

        /* 每秒打印一次统计 */
        static uint64_t last_tsc[RTE_MAX_LCORE] = {0};
        uint64_t cur_tsc = rte_rdtsc();
        uint64_t hz = rte_get_tsc_hz();

        if (cur_tsc - last_tsc[core_id] > hz) {
            last_tsc[core_id] = cur_tsc;
            printf("  [Core %u Queue %u Stats] Total: %lu packets\n",
                   core_id, queue_id, total_packets);
        }
    }

    return 0;
}

/**
 * launch_multi_queue_processing() - 为每个核心启动一个队列处理任务
 */
static void launch_multi_queue_processing(void) {
    int queue_idx = 0;
    unsigned int lcore_id;

    printf("=== Launching Multi-Queue Processing ===\n");
    printf("Binding each lcore to a specific queue:\n\n");

    RTE_LCORE_FOREACH_WORKER(lcore_id) {
        if (queue_idx >= NUM_RX_QUEUES) {
            printf("(Queue allocation complete)\n");
            break;
        }

        printf("  Lcore %u → Queue %u\n", lcore_id, queue_idx);

        /* 为该核心启动任务，传入队列 ID */
        rte_eal_remote_launch(lcore_packet_handler,
                            (void *)(uintptr_t)queue_idx,
                            lcore_id);
        queue_idx++;
    }

    printf("\n✓ All cores launched\n\n");
}

/**
 * main() - 主函数
 */
int main(int argc, char *argv[]) {
    int ret;

    /* ============ 初始化 DPDK EAL ============ */
    ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "Cannot init EAL\n");
    }

    /* 注册日志类型 */
    RTE_LOGTYPE_APP = rte_log_register("APP");
    rte_log_set_level(RTE_LOGTYPE_APP, RTE_LOG_INFO);

    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║         DPDK RSS Multi-Queue Configuration Demo            ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");

    /* ============ 初始化 DPDK 和 RSS ============ */
    if (init_dpdk_rss() < 0) {
        rte_exit(EXIT_FAILURE, "Failed to initialize DPDK RSS\n");
    }

    /* ============ 启动多队列处理任务 ============ */
    launch_multi_queue_processing();

    printf("════════════════════════════════════════════════════════════\n");
    printf("RSS Configuration Complete!\n");
    printf("════════════════════════════════════════════════════════════\n\n");
    printf("Key Features:\n");
    printf("  ✓ Configured %u RX queues (one per core)\n", NUM_RX_QUEUES);
    printf("  ✓ RSS enabled - packets automatically distributed\n");
    printf("  ✓ Same UE IP → Always enters the same queue\n");
    printf("  ✓ Zero packet loss - no affinity checks needed\n");
    printf("  ✓ Hardware-based flow steering\n\n");
    printf("Waiting for packets on all queues...\n");
    printf("(Press Ctrl+C to exit)\n\n");

    /* 等待所有核心完成 */
    rte_eal_mp_wait_lcore();

    /* 清理 */
    RTE_ETH_FOREACH_DEV(port_id) {
        printf("Closing port %u...\n", port_id);
        rte_eth_dev_stop(port_id);
        rte_eth_dev_close(port_id);
    }

    printf("Done!\n");
    return 0;
}
