#include "pcap_client.h"
#include <pcapplusplus/PcapLiveDeviceList.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <algorithm>

PcapClient::PcapClient(const std::string& pcap_file_path,
                       const std::string& interface_name,
                       const std::string& client_ip,
                       const ClientReplayConfig& config)
    : pcap_file_path_(pcap_file_path)
    , interface_name_(interface_name)
    , client_ip_(client_ip)
    , config_(config)
    , network_device_(nullptr)
    , initialized_(false)
    , running_(false)
    , interrupted_(false)
    , total_packets_(0)
    , sent_packets_(0)
    , failed_packets_(0) {
}

PcapClient::~PcapClient() {
    cleanup();
}

bool PcapClient::initialize() {
    if (initialized_.load()) {
        return true;
    }
    
    std::cout << "=== PCAP客户端初始化 ===" << std::endl;
    std::cout << "PCAP文件: " << pcap_file_path_ << std::endl;
    std::cout << "网络接口: " << interface_name_ << std::endl;
    std::cout << "客户端IP: " << client_ip_ << std::endl;
    
    // 验证客户端IP地址格式
    if (!PacketAnalyzer::isValidIPAddress(client_ip_)) {
        std::cerr << "错误: 无效的客户端IP地址: " << client_ip_ << std::endl;
        return false;
    }
    
    // 初始化共享内存管理器（客户端创建共享内存）
    shm_manager_ = std::make_unique<SharedMemoryManager>(true);
    if (!shm_manager_->initialize()) {
        std::cerr << "错误: 无法初始化共享内存" << std::endl;
        return false;
    }
    
    // 初始化包分析器
    packet_analyzer_ = std::make_unique<PacketAnalyzer>(client_ip_);
    if (!packet_analyzer_->analyzePcapFile(pcap_file_path_)) {
        std::cerr << "错误: 无法分析PCAP文件" << std::endl;
        return false;
    }
    
    // 获取客户端包
    client_packets_ = packet_analyzer_->getClientPackets();
    if (client_packets_.empty()) {
        std::cerr << "警告: 没有找到客户端包" << std::endl;
        std::cerr << "请检查客户端IP地址是否正确" << std::endl;
        return false;
    }
    
    std::cout << "找到 " << client_packets_.size() << " 个客户端包" << std::endl;
    
    // 验证网络设备
    if (!validateNetworkDevice()) {
        return false;
    }
    
    // 预处理包数据
    if (!preprocessPackets()) {
        std::cerr << "错误: 包预处理失败" << std::endl;
        return false;
    }
    
    // 更新共享内存中的包统计信息
    SharedMemoryData* shm_data = shm_manager_->getData();
    if (shm_data != nullptr) {
        shm_data->total_client_packets.store(static_cast<int>(client_packets_.size()));
        shm_data->total_server_packets.store(packet_analyzer_->getServerPacketCount());
    }
    
    initialized_.store(true);
    std::cout << "客户端初始化完成" << std::endl;
    
    return true;
}

bool PcapClient::startReplay() {
    if (!initialized_.load()) {
        std::cerr << "错误: 客户端未初始化" << std::endl;
        return false;
    }
    
    if (running_.load()) {
        std::cerr << "警告: 客户端已在运行" << std::endl;
        return true;
    }
    
    std::cout << "\n=== 开始客户端回放 ===" << std::endl;
    
    // 设置客户端就绪状态
    shm_manager_->setReady(true);
    
    // 等待服务端就绪
    std::cout << "等待服务端就绪..." << std::endl;
    if (!waitForServer()) {
        std::cerr << "错误: 等待服务端超时" << std::endl;
        return false;
    }
    
    std::cout << "服务端已就绪，开始回放" << std::endl;
    
    // 设置回放开始状态
    SharedMemoryData* shm_data = shm_manager_->getData();
    if (shm_data != nullptr) {
        shm_data->replay_started.store(true);
    }
    
    running_.store(true);
    start_time_ = std::chrono::high_resolution_clock::now();
    
    // 主回放循环
    for (size_t i = 0; i < packet_delay_pairs_.size() && !interrupted_.load(); ++i) {
        const auto& packet_pair = packet_delay_pairs_[i];
        const PacketInfo& packet_info = packet_pair.first;
        int delay_ms = packet_pair.second;
        
        total_packets_.fetch_add(1);
        
        // 同步发送包
        bool sent_success = synchronizedSendPacket(packet_info, static_cast<int>(i));
        
        if (sent_success) {
            sent_packets_.fetch_add(1);
        } else {
            failed_packets_.fetch_add(1);
        }
        
        // 更新共享内存状态
        updateSharedMemoryState(static_cast<int>(i), sent_success);
        
        // 每发送50个包显示一次进度
        if ((i + 1) % 50 == 0) {
            std::cout << "客户端已发送: " << (i + 1) << "/" << packet_delay_pairs_.size() 
                      << " 包, 成功: " << sent_packets_.load() 
                      << ", 失败: " << failed_packets_.load() << std::endl;
        }
        
        // 执行延迟（跳过第一个包）
        if (i > 0 && delay_ms > 0 && !interrupted_.load()) {
            executeDelay(delay_ms);
        }
    }
    
    end_time_ = std::chrono::high_resolution_clock::now();
    running_.store(false);
    
    // 设置回放完成状态
    if (shm_data != nullptr) {
        shm_data->replay_finished.store(true);
    }
    
    // 打印统计信息
    printStatistics();
    
    return true;
}

void PcapClient::stopReplay() {
    interrupted_.store(true);
    running_.store(false);
    
    SharedMemoryData* shm_data = shm_manager_->getData();
    if (shm_data != nullptr) {
        shm_data->should_terminate.store(true);
    }
}

bool PcapClient::isRunning() const {
    return running_.load();
}

int PcapClient::getTotalPackets() const {
    return total_packets_.load();
}

int PcapClient::getSentPackets() const {
    return sent_packets_.load();
}

int PcapClient::getFailedPackets() const {
    return failed_packets_.load();
}

double PcapClient::getSuccessRate() const {
    int total = total_packets_.load();
    if (total == 0) return 0.0;
    return static_cast<double>(sent_packets_.load()) / total * 100.0;
}

void PcapClient::printStatistics() const {
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time_ - start_time_);
    
    std::cout << "\n=== 客户端回放统计 ===" << std::endl;
    if (interrupted_.load()) {
        std::cout << "状态: 被中断" << std::endl;
    } else {
        std::cout << "状态: 完成" << std::endl;
    }
    std::cout << "总包数: " << total_packets_.load() << std::endl;
    std::cout << "成功发送: " << sent_packets_.load() << std::endl;
    std::cout << "发送失败: " << failed_packets_.load() << std::endl;
    std::cout << "成功率: " << getSuccessRate() << "%" << std::endl;
    std::cout << "总耗时: " << duration.count() << " ms" << std::endl;
    
    if (total_packets_.load() > 0 && duration.count() > 0) {
        double rate = static_cast<double>(sent_packets_.load()) / (duration.count() / 1000.0);
        std::cout << "平均发送速率: " << rate << " 包/秒" << std::endl;
    }
    std::cout << "===================" << std::endl;
}

void PcapClient::setInterrupted() {
    interrupted_.store(true);
}

bool PcapClient::preprocessPackets() {
    std::cout << "正在预处理客户端包..." << std::endl;
    
    packet_delay_pairs_.clear();
    
    if (config_.mode == ClientReplayMode::ConstantRate) {
        return preprocessConstantRateMode();
    }
    
    // 其他模式的处理
    std::default_random_engine rng(std::random_device{}());
    std::uniform_real_distribution<double> floatDist(1.0 - config_.floatPercent, 1.0 + config_.floatPercent);
    
    for (size_t i = 0; i < client_packets_.size(); ++i) {
        const PacketInfo& current_packet = client_packets_[i];
        int delay_ms = 0;
        
        if (i > 0) {
            const PacketInfo& prev_packet = client_packets_[i - 1];
            delay_ms = calculatePacketDelay(current_packet, prev_packet, static_cast<int>(i));
        }
        
        packet_delay_pairs_.emplace_back(current_packet, delay_ms);
    }
    
    std::cout << "预处理完成，共 " << packet_delay_pairs_.size() << " 个客户端包" << std::endl;
    return true;
}

int PcapClient::calculatePacketDelay(const PacketInfo& current_packet,
                                   const PacketInfo& prev_packet,
                                   int packet_index) {
    // 计算与前一个数据包的时间差
    timespec prev_ts = prev_packet.timestamp;
    timespec curr_ts = current_packet.timestamp;
    long sec_diff = curr_ts.tv_sec - prev_ts.tv_sec;
    long nsec_diff = curr_ts.tv_nsec - prev_ts.tv_nsec;
    double orig_diff_ms = sec_diff * 1000.0 + nsec_diff / 1.0e6;
    
    // 处理时间戳异常情况
    if (orig_diff_ms < 0) {
        orig_diff_ms = 0;
    }
    if (orig_diff_ms > 10000) {
        orig_diff_ms = 10000;
    }
    
    // 根据模式计算延迟
    switch (config_.mode) {
        case ClientReplayMode::OriginalSpeed:
            return static_cast<int>(orig_diff_ms);
            
        case ClientReplayMode::FixedInterval:
            return config_.fixedIntervalMs;
            
        case ClientReplayMode::FloatingOriginal: {
            std::default_random_engine rng(std::random_device{}());
            std::uniform_real_distribution<double> floatDist(1.0 - config_.floatPercent, 1.0 + config_.floatPercent);
            double factor = floatDist(rng);
            return static_cast<int>(orig_diff_ms * factor);
        }
        
        case ClientReplayMode::ConstantRate:
            // 这种情况不会到达这里
            return 0;
    }
    
    return 0;
}

bool PcapClient::preprocessConstantRateMode() {
    std::cout << "等速回放模式预处理..." << std::endl;
    
    // 计算总字节数
    int total_bytes = 0;
    for (const auto& packet : client_packets_) {
        total_bytes += static_cast<int>(packet.packet_size);
    }
    
    std::cout << "客户端包总字节数: " << total_bytes << " 字节" << std::endl;
    std::cout << "目标流量: " << config_.targetBytesPerSec << " 字节/秒" << std::endl;
    
    // 计算每个包的延迟时间
    int cumulative_bytes = 0;
    for (size_t i = 0; i < client_packets_.size(); ++i) {
        int delay_ms = 0;
        
        if (i > 0) {
            // 计算到当前包为止应该消耗的时间（毫秒）
            cumulative_bytes += static_cast<int>(client_packets_[i-1].packet_size);
            double expected_time_ms = static_cast<double>(cumulative_bytes) * 1000.0 / config_.targetBytesPerSec;
            double prev_expected_time_ms = static_cast<double>(cumulative_bytes - client_packets_[i-1].packet_size) * 1000.0 / config_.targetBytesPerSec;
            delay_ms = static_cast<int>(expected_time_ms - prev_expected_time_ms);
            
            // 确保延迟时间不为负数
            if (delay_ms < 0) delay_ms = 0;
        }
        
        packet_delay_pairs_.emplace_back(client_packets_[i], delay_ms);
    }
    
    return true;
}

bool PcapClient::sendPacket(const PacketInfo& packet_info) {
    if (network_device_ == nullptr) {
        return false;
    }
    
    // 检查数据包是否有效
    if (packet_info.packet_size == 0) {
        return false;
    }
    
    // 发送数据包
    return network_device_->sendPacket(packet_info.raw_packet);
}

void PcapClient::executeDelay(int delay_ms) {
    if (delay_ms <= 0) return;
    
    // 分段睡眠，以便能够响应中断信号
    const int sleep_chunk = 50;  // 每次睡眠50ms
    int remaining_ms = delay_ms;
    
    while (remaining_ms > 0 && !interrupted_.load()) {
        int current_sleep = std::min(remaining_ms, sleep_chunk);
        std::this_thread::sleep_for(std::chrono::milliseconds(current_sleep));
        remaining_ms -= current_sleep;
    }
}

bool PcapClient::waitForServer(int timeout_ms) {
    return shm_manager_->waitForPeer(true, timeout_ms);
}

bool PcapClient::synchronizedSendPacket(const PacketInfo& packet_info, int packet_index) {
    // 发送包
    bool sent_success = sendPacket(packet_info);
    
    // 更新共享内存状态
    SharedMemoryData* shm_data = shm_manager_->getData();
    if (shm_data != nullptr) {
        shm_data->current_packet_index.store(packet_index);
        shm_data->client_packet_sent.store(true);
        shm_data->last_send_time_us.store(SharedMemoryManager::getCurrentTimeMicros());
        
        if (sent_success) {
            shm_data->client_sent_count.fetch_add(1);
        } else {
            shm_data->client_failed_count.fetch_add(1);
        }
    }
    
    return sent_success;
}

void PcapClient::updateSharedMemoryState(int packet_index, bool sent_success) {
    SharedMemoryData* shm_data = shm_manager_->getData();
    if (shm_data == nullptr) return;
    
    // 获取锁
    if (shm_manager_->acquireLock(100)) {
        shm_data->current_packet_index.store(packet_index);
        shm_data->client_packet_sent.store(sent_success);
        
        // 释放锁
        shm_manager_->releaseLock();
    }
}

bool PcapClient::validateNetworkDevice() {
    // 获取网络设备
    network_device_ = pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDeviceByName(interface_name_);
    if (network_device_ == nullptr) {
        std::cerr << "错误: 找不到网络接口: " << interface_name_ << std::endl;
        std::cerr << "可用的网络接口:" << std::endl;
        
        const std::vector<pcpp::PcapLiveDevice*>& dev_list = 
            pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDevicesList();
        for (const auto& device : dev_list) {
            std::cerr << "  - " << device->getName() << std::endl;
        }
        return false;
    }
    
    // 打开网络设备
    if (!network_device_->open()) {
        std::cerr << "错误: 无法打开网络设备: " << interface_name_ << std::endl;
        std::cerr << "请检查是否有足够的权限访问网络设备" << std::endl;
        return false;
    }
    
    std::cout << "网络设备验证成功: " << interface_name_ << std::endl;
    return true;
}

void PcapClient::cleanup() {
    running_.store(false);
    
    if (network_device_ != nullptr) {
        network_device_->close();
        network_device_ = nullptr;
    }
    
    // 清理共享内存（客户端负责清理）
    if (shm_manager_) {
        SharedMemoryData* shm_data = shm_manager_->getData();
        if (shm_data != nullptr) {
            shm_data->should_terminate.store(true);
        }
        shm_manager_->cleanup();
    }
}