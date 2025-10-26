/*
 * PFCP 协议接收实现示例
 *
 * PFCP = Packet Forwarding Control Protocol
 * 用于 SMF 与 UPF 之间通信，传递会话信息
 *
 * 编译：
 * gcc -o pfcp_receiver pfcp_receiver.c -lpthread
 *
 * 说明：
 * 这是一个独立的示例程序，演示如何接收 PFCP 会话建立消息
 * 在实际 UPF 中，这应该集成到主程序的并行线程中
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

/* ============= PFCP 常量定义 ============= */
#define PFCP_PORT 8805
#define PFCP_VERSION 1
#define PFCP_SESSION_EST_REQ 50    /* 会话建立请求 */
#define PFCP_SESSION_EST_RSP 51    /* 会话建立响应 */

/* ============= 数据结构定义 ============= */

/* PFCP 报头 */
struct pfcp_header {
    uint8_t flags;
    uint8_t msg_type;
    uint16_t msg_length;
    uint64_t seid;
    uint32_t seq_number;
} __attribute__((packed));

/* UE 会话信息 */
typedef struct {
    uint64_t seid;              /* Session Endpoint ID */
    char supi[20];              /* Subscription Permanent Identifier */
    uint32_t ue_ip;             /* UE IPv4 地址 */
    uint32_t gnb_ip;            /* gNodeB IPv4 地址 */
    uint16_t gnb_port;          /* gNodeB UDP 端口 */
    uint32_t teid_downlink;     /* 下行 TEID (UPF -> gNodeB) */
    uint32_t teid_uplink;       /* 上行 TEID (gNodeB -> UPF) */
    uint8_t pdu_session_id;     /* PDU 会话 ID */
    uint8_t qos_priority;       /* QoS 优先级 */
    uint32_t qos_mbr_ul;        /* QoS 上行最大比特率 */
    uint32_t qos_mbr_dl;        /* QoS 下行最大比特率 */
    time_t created_time;        /* 创建时间 */
} ue_session_t;

/* 会话管理 */
#define MAX_SESSIONS 1000
static ue_session_t ue_sessions[MAX_SESSIONS];
static int session_count = 0;
static pthread_mutex_t session_lock = PTHREAD_MUTEX_INITIALIZER;

/* ============= 辅助函数 ============= */

/**
 * 打印十六进制数据（调试用）
 */
void print_hex(const char *title, const uint8_t *data, int len) {
    printf("[%s] ", title);
    for (int i = 0; i < len && i < 32; i++) {
        printf("%02x ", data[i]);
    }
    if (len > 32) printf("...");
    printf("\n");
}

/**
 * 打印 IP 地址
 */
void print_ip(uint32_t ip) {
    printf("%d.%d.%d.%d",
           (ip >>  0) & 0xFF,
           (ip >>  8) & 0xFF,
           (ip >> 16) & 0xFF,
           (ip >> 24) & 0xFF);
}

/**
 * 解析简化的 PFCP 消息 (仅演示，实际需要完整 IE 解析)
 */
int parse_pfcp_session_establishment(const uint8_t *msg, int msg_len,
                                     ue_session_t *session) {
    if (msg_len < sizeof(struct pfcp_header)) {
        printf("ERROR: Message too short\n");
        return -1;
    }

    struct pfcp_header *hdr = (struct pfcp_header *)msg;

    printf("\n[PFCP] Received message:\n");
    printf("  Version: %d\n", (hdr->flags >> 6) & 0x3);
    printf("  Message Type: %d\n", hdr->msg_type);
    printf("  Message Length: %u\n", ntohs(hdr->msg_length));
    printf("  SEID: 0x%016lx\n", be64toh(hdr->seid));
    printf("  Sequence: %u\n", ntohl(hdr->seq_number));

    /* 验证消息类型 */
    if (hdr->msg_type != PFCP_SESSION_EST_REQ) {
        printf("ERROR: Expected Session Establishment Request (50), got %u\n",
               hdr->msg_type);
        return -1;
    }

    /* ========== 简化的 IE 解析 ========== */
    /* 在这里应该实现完整的 Information Element 解析 */
    /* 这是一个示例实现，真实的需要更复杂的状态机 */

    session->seid = be64toh(hdr->seid);

    /* 示例：模拟从消息中提取的信息 */
    /* 实际应该从消息体中解析 */
    sprintf(session->supi, "234010012340000");
    session->ue_ip = inet_addr("10.0.0.2");
    session->gnb_ip = inet_addr("192.168.1.100");
    session->gnb_port = 2152;
    session->teid_downlink = 0x12345678;
    session->teid_uplink = 0x87654321;
    session->pdu_session_id = 1;
    session->qos_priority = 5;
    session->qos_mbr_ul = 1000000;   /* 1 Mbps */
    session->qos_mbr_dl = 10000000;  /* 10 Mbps */
    session->created_time = time(NULL);

    return 0;
}

/**
 * 创建 PFCP 会话建立响应消息
 */
int create_pfcp_session_response(uint8_t *buffer, int buffer_len,
                                 const ue_session_t *session) {
    struct pfcp_header *hdr = (struct pfcp_header *)buffer;

    /* 构造响应报头 */
    hdr->flags = 0x20;  /* Version 1, Not-S bit set */
    hdr->msg_type = PFCP_SESSION_EST_RSP;  /* 51 */
    hdr->msg_length = htons(sizeof(struct pfcp_header) - 4);  /* 不包括前 4 字节 */
    hdr->seid = htobe64(session->seid);
    hdr->seq_number = htonl(1);

    return sizeof(struct pfcp_header);
}

/**
 * 处理接收到的 PFCP 消息
 */
int handle_pfcp_message(int sock, const uint8_t *msg, int msg_len,
                        struct sockaddr_in *remote_addr) {
    ue_session_t new_session;

    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║       PFCP Session Establishment       ║\n");
    printf("╚════════════════════════════════════════╝\n");

    /* 解析 PFCP 消息 */
    if (parse_pfcp_session_establishment(msg, msg_len, &new_session) < 0) {
        printf("ERROR: Failed to parse PFCP message\n");
        return -1;
    }

    /* 添加到会话表 */
    pthread_mutex_lock(&session_lock);

    if (session_count >= MAX_SESSIONS) {
        printf("ERROR: Session table full (max %d)\n", MAX_SESSIONS);
        pthread_mutex_unlock(&session_lock);
        return -1;
    }

    ue_sessions[session_count++] = new_session;

    printf("\n[SESSION ADDED]\n");
    printf("  Session ID:     %d\n", session_count);
    printf("  SEID:           0x%016lx\n", new_session.seid);
    printf("  SUPI:           %s\n", new_session.supi);
    printf("  UE IP:          ");
    print_ip(new_session.ue_ip);
    printf("\n  gNodeB IP:      ");
    print_ip(new_session.gnb_ip);
    printf("\n  gNodeB Port:    %u\n", new_session.gnb_port);
    printf("  DL TEID:        0x%08x\n", new_session.teid_downlink);
    printf("  UL TEID:        0x%08x\n", new_session.teid_uplink);
    printf("  PDU Session ID: %u\n", new_session.pdu_session_id);
    printf("  QoS Priority:   %u\n", new_session.qos_priority);
    printf("  MBR UL:         %u bps (%.2f Mbps)\n",
           new_session.qos_mbr_ul, new_session.qos_mbr_ul / 1000000.0);
    printf("  MBR DL:         %u bps (%.2f Mbps)\n",
           new_session.qos_mbr_dl, new_session.qos_mbr_dl / 1000000.0);

    pthread_mutex_unlock(&session_lock);

    /* 创建响应消息 */
    uint8_t response[256];
    int response_len = create_pfcp_session_response(response, sizeof(response),
                                                     &new_session);

    /* 发送响应 */
    printf("\n[PFCP] Sending Session Establishment Response\n");
    print_hex("Response Hex", response, response_len);

    if (sendto(sock, response, response_len, 0,
               (struct sockaddr *)remote_addr, sizeof(*remote_addr)) < 0) {
        perror("sendto");
        return -1;
    }

    printf("[SUCCESS] Response sent\n");

    return 0;
}

/**
 * PFCP 接收线程
 */
void* pfcp_receiver_thread(void *arg) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr, remote_addr;
    int addr_len = sizeof(remote_addr);
    uint8_t buffer[4096];
    int opt = 1;

    if (sock < 0) {
        perror("socket");
        return NULL;
    }

    /* 允许端口重用 */
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* 绑定端口 */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PFCP_PORT);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sock);
        return NULL;
    }

    printf("[PFCP] Server listening on UDP port %d\n", PFCP_PORT);
    printf("[INFO] Waiting for PFCP messages from SMF...\n\n");

    /* 接收消息 */
    while (1) {
        int recv_len = recvfrom(sock, buffer, sizeof(buffer), 0,
                               (struct sockaddr *)&remote_addr, &addr_len);

        if (recv_len > 0) {
            printf("[PFCP] Received %d bytes from ", recv_len);
            print_ip(ntohl(remote_addr.sin_addr.s_addr));
            printf(":%u\n", ntohs(remote_addr.sin_port));

            handle_pfcp_message(sock, buffer, recv_len, &remote_addr);
        } else if (recv_len < 0) {
            perror("recvfrom");
            break;
        }
    }

    close(sock);
    return NULL;
}

/**
 * 显示所有会话
 */
void show_all_sessions(void) {
    pthread_mutex_lock(&session_lock);

    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║              Current UE Sessions (%d)                       ║\n", session_count);
    printf("╚════════════════════════════════════════════════════════════╝\n\n");

    for (int i = 0; i < session_count; i++) {
        ue_session_t *s = &ue_sessions[i];
        printf("Session %d:\n", i + 1);
        printf("  SUPI: %s\n", s->supi);
        printf("  UE IP: ");
        print_ip(s->ue_ip);
        printf("\n");
        printf("  gNodeB: ");
        print_ip(s->gnb_ip);
        printf(":%u\n", s->gnb_port);
        printf("  TEID UL: 0x%08x, DL: 0x%08x\n", s->teid_uplink, s->teid_downlink);
        printf("\n");
    }

    pthread_mutex_unlock(&session_lock);
}

/**
 * 主函数
 */
int main(int argc, char *argv[]) {
    pthread_t thread_id;

    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║              PFCP Receiver Demonstrator                    ║\n");
    printf("║         (Simulates UPF receiving SMF commands)             ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");

    /* 创建 PFCP 接收线程 */
    if (pthread_create(&thread_id, NULL, pfcp_receiver_thread, NULL) != 0) {
        perror("pthread_create");
        return 1;
    }

    /* 主线程定期显示会话列表 */
    while (1) {
        sleep(5);
        show_all_sessions();
    }

    pthread_join(thread_id, NULL);
    return 0;
}

/* ============= 使用说明 =============

编译：
  gcc -o pfcp_receiver pfcp_receiver.c -lpthread

运行：
  ./pfcp_receiver

在另一个终端发送测试 PFCP 消息：
  # 使用 netcat 发送原始 UDP 消息
  echo -e '\x20\x32\x00\x1a\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x01' | \
    nc -u localhost 8805

或使用 Python：
  python3 -c "
import socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
msg = b'\x20\x32\x00\x1a\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x01'
sock.sendto(msg, ('127.0.0.1', 8805))
sock.close()
"

预期输出：
  [PFCP] Received message from 127.0.0.1:xxxxx
  [SESSION ADDED]
    UE IP: 10.0.0.2
    gNodeB IP: 192.168.1.100
    ...

*/
