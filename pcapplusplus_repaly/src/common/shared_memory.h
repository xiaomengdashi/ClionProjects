#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <memory>

/**
 * 共享内存数据结构
 * 用于客户端和服务端之间的状态同步和包发送协调
 */
struct SharedMemoryData {
    // 控制信息
    std::atomic<bool> client_ready;         // 客户端就绪
    std::atomic<bool> server_ready;         // 服务端就绪
    std::atomic<bool> replay_started;       // 回放开始
    std::atomic<bool> replay_finished;      // 回放结束
    std::atomic<bool> should_terminate;     // 终止信号
    
    // 包发送状态
    std::atomic<int> current_packet_index;      // 当前包索引
    std::atomic<bool> client_packet_sent;       // 客户端包已发送
    std::atomic<bool> server_packet_received;   // 服务端收到包
    std::atomic<uint64_t> last_send_time_us;    // 最后发送时间(微秒)
    std::atomic<bool> ignore_next_received;     // 忽略下一个收到的包
    
    // 交替发送状态
    std::atomic<int> current_sender;            // 当前发送方 (0=客户端, 1=服务端)
    std::atomic<bool> client_in_receive_mode;   // 客户端处于接收态
    std::atomic<bool> server_in_receive_mode;   // 服务端处于接收态
    std::atomic<int> next_packet_index;         // 下一个要发送的包索引
    std::atomic<bool> waiting_for_peer;         // 等待对方发送
    
    // 统计信息
    std::atomic<int> client_sent_count;     // 客户端发送计数
    std::atomic<int> server_sent_count;     // 服务端发送计数
    std::atomic<int> client_failed_count;   // 客户端失败计数
    std::atomic<int> server_failed_count;   // 服务端失败计数
    std::atomic<int> total_client_packets;  // 客户端包总数
    std::atomic<int> total_server_packets;  // 服务端包总数
    
    // 同步锁和状态
    std::atomic<bool> sync_lock;            // 同步锁
    std::atomic<int> current_timeout_ms;    // 当前超时时间(毫秒)
    
    // 构造函数：初始化所有原子变量
    SharedMemoryData() {
        client_ready.store(false);
        server_ready.store(false);
        replay_started.store(false);
        replay_finished.store(false);
        should_terminate.store(false);
        
        current_packet_index.store(0);
        client_packet_sent.store(false);
        server_packet_received.store(false);
        last_send_time_us.store(0);
        ignore_next_received.store(false);
        
        // 初始化交替发送状态
        current_sender.store(0);  // 从客户端开始
        client_in_receive_mode.store(false);
        server_in_receive_mode.store(false);
        next_packet_index.store(0);
        waiting_for_peer.store(false);
        
        client_sent_count.store(0);
        server_sent_count.store(0);
        client_failed_count.store(0);
        server_failed_count.store(0);
        total_client_packets.store(0);
        total_server_packets.store(0);
        
        sync_lock.store(false);
        current_timeout_ms.store(0);
    }
};

/**
 * 共享内存管理类
 * 负责创建、连接和管理POSIX共享内存
 */
class SharedMemoryManager {
public:
    static const std::string SHARED_MEMORY_NAME;
    static const size_t SHARED_MEMORY_SIZE;
    
    /**
     * 构造函数
     * @param create_new 是否创建新的共享内存（true）或连接现有的（false）
     */
    explicit SharedMemoryManager(bool create_new = false);
    
    /**
     * 析构函数
     * 自动清理资源
     */
    ~SharedMemoryManager();
    
    /**
     * 初始化共享内存
     * @return 成功返回true，失败返回false
     */
    bool initialize();
    
    /**
     * 获取共享内存数据指针
     * @return 共享内存数据指针，失败返回nullptr
     */
    SharedMemoryData* getData();
    
    /**
     * 检查共享内存是否已初始化
     * @return 已初始化返回true
     */
    bool isInitialized() const;
    
    /**
     * 清理共享内存（仅创建者调用）
     * @return 成功返回true
     */
    bool cleanup();
    
    /**
     * 等待对方就绪
     * @param is_client 是否为客户端
     * @param timeout_ms 超时时间（毫秒），0表示无限等待
     * @return 成功返回true，超时返回false
     */
    bool waitForPeer(bool is_client, int timeout_ms = 0);
    
    /**
     * 设置就绪状态
     * @param is_client 是否为客户端
     */
    void setReady(bool is_client);
    
    /**
     * 获取当前时间（微秒）
     * @return 当前时间戳（微秒）
     */
    static uint64_t getCurrentTimeMicros();
    
    /**
     * 获取锁（自旋锁）
     * @param timeout_ms 超时时间（毫秒）
     * @return 成功获取锁返回true
     */
    bool acquireLock(int timeout_ms = 1000);
    
    /**
     * 释放锁
     */
    void releaseLock();
    
private:
    bool create_new_;           // 是否创建新的共享内存
    int shm_fd_;               // 共享内存文件描述符
    SharedMemoryData* data_;   // 共享内存数据指针
    bool initialized_;         // 是否已初始化
    bool is_creator_;          // 是否为创建者
    
    // 禁用拷贝构造和赋值
    SharedMemoryManager(const SharedMemoryManager&) = delete;
    SharedMemoryManager& operator=(const SharedMemoryManager&) = delete;
};