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
    
    // 获取所有包的顺序信息
    auto all_packets = packet_analyzer_->getAllPackets();
    
    // 主回放循环 - 基于PCAP包的原始顺序
    for (size_t global_index = 0; global_index < all_packets.size() && !interrupted_.load(); ++global_index) {
        const auto& packet_info = all_packets[global_index];
        
        // 检查是否是客户端包
        if (packet_info.direction == PacketDirection::CLIENT_TO_SERVER) {
            // 等待轮到客户端发送
            if (!waitForClientTurn(static_cast<int>(global_index))) {
                std::cout << "等待客户端发送轮次超时，跳过包 " << global_index << std::endl;
                continue;
            }
            
            total_packets_.fetch_add(1);
            
            // 发送客户端包
            bool sent_success = sendClientPacket(packet_info, static_cast<int>(global_index));
            
            if (sent_success) {
                sent_packets_.fetch_add(1);
            } else {
                failed_packets_.fetch_add(1);
            }
            
            // 检查下一包是否还是客户端包
            bool next_is_client = false;
            if (global_index + 1 < all_packets.size()) {
                next_is_client = (all_packets[global_index + 1].direction == PacketDirection::CLIENT_TO_SERVER);
            }
            
            if (next_is_client) {
                // 下一包还是客户端包，计算延迟后继续发送
                int delay_ms = calculateDelayToNextPacket(packet_info, all_packets[global_index + 1]);
                if (delay_ms > 0) {
                    executeDelay(delay_ms);
                }
            } else {
                // 下一包是服务端包，切换到接收态
                switchToReceiveMode(static_cast<int>(global_index + 1));
            }
            
            // 每发送50个包显示一次进度
            if (sent_packets_.load() % 50 == 0) {
                std::cout << "客户端已发送: " << sent_packets_.load() 
                          << " 包, 成功率: " << (sent_packets_.load() * 100.0 / total_packets_.load()) << "%" << std::endl;
            }
        } else {
            // 服务端包，等待服务端处理
            waitForServerPacket(static_cast<int>(global_index));
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

bool PcapClient::waitForClientTurn(int packet_index) {
    SharedMemoryData* shm_data = shm_manager_->getData();
    if (!shm_data) return false;
    
    auto start_time = std::chrono::steady_clock::now();
    const int timeout_ms = 1000; // 1秒超时
    
    while (!interrupted_.load()) {
        // 检查是否轮到发送这个包
        if (shm_data->next_packet_index.load() == packet_index) {
            return true;
        }
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);
        if (elapsed.count() >= timeout_ms) {
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    return false;
}

bool PcapClient::sendClientPacket(const PacketInfo& packet_info, int packet_index) {
    // 发送包
    bool sent_success = sendPacket(packet_info);
    
    // 更新共享内存状态
    SharedMemoryData* shm_data = shm_manager_->getData();
    if (shm_data != nullptr) {
        shm_data->current_packet_index.store(packet_index);
        shm_data->client_packet_sent.store(sent_success);
        shm_data->last_send_time_us.store(SharedMemoryManager::getCurrentTimeMicros());
        
        // 客户端发送包后，更新共享内存状态
        shm_data->next_packet_index.store(packet_index + 1); // 指向下一个包（可能是客户端或服务端）
        
        if (sent_success) {
            shm_data->client_sent_count.fetch_add(1);
            std::cout << "客户端发送包 " << packet_index+1 << " 成功" << std::endl;
        } else {
            shm_data->client_failed_count.fetch_add(1);
            std::cout << "客户端发送包 " << packet_index+1 << " 失败" << std::endl;
        }
    }
    
    return sent_success;
}

void PcapClient::switchToReceiveMode(int next_packet_index) {
    SharedMemoryData* shm_data = shm_manager_->getData();
    if (!shm_data) return;
    
    std::cout << "客户端切换到接收态，等待服务端发送包 " << next_packet_index << std::endl;
    
    // 设置客户端为接收态
    shm_data->client_in_receive_mode.store(true);
    shm_data->waiting_for_peer.store(true);
}

void PcapClient::waitForServerPacket(int packet_index) {
    SharedMemoryData* shm_data = shm_manager_->getData();
    if (!shm_data) return;
    
    // 简化逻辑：只等待next_packet_index更新到下一个包
    auto start_time = std::chrono::steady_clock::now();
    const int timeout_ms = 2000; // 2秒超时
    
    while (!interrupted_.load()) {
        // 检查服务端是否已经处理完当前包
        if (shm_data->next_packet_index.load() > packet_index) {
            break;
        }
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);
        if (elapsed.count() >= timeout_ms) {
            std::cout << "等待服务端处理包 " << packet_index << " 超时" << std::endl;
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

int PcapClient::calculateDelayToNextPacket(const PacketInfo& current_packet, const PacketInfo& next_packet) {
    // 计算时间差（微秒）
    uint64_t time_diff_us = next_packet.timestamp_us - current_packet.timestamp_us;
    
    // 根据回放模式调整延迟
    switch (config_.mode) {
        case ClientReplayMode::OriginalSpeed:
            return static_cast<int>(time_diff_us / 1000); // 转换为毫秒
            
        case ClientReplayMode::FixedInterval:
            return config_.fixedIntervalMs;
            
        case ClientReplayMode::FloatingOriginal: {
            double base_delay = time_diff_us / 1000.0;
            double variation = base_delay * config_.floatPercent;
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<> dis(-variation, variation);
            return static_cast<int>(base_delay + dis(gen));
        }
        
        case ClientReplayMode::ConstantRate:
            // 基于目标流量计算延迟
            if (config_.targetBytesPerSec > 0) {
                double delay_sec = static_cast<double>(current_packet.packet_size) / config_.targetBytesPerSec;
                return static_cast<int>(delay_sec * 1000);
            }
            return 0;
            
        default:
            return static_cast<int>(time_diff_us / 1000);
    }
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