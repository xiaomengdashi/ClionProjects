#include "pcap_server.h"
#include <pcapplusplus/Packet.h>
#include <pcapplusplus/IPv4Layer.h>
#include <pcapplusplus/IPv6Layer.h>
#include <pcapplusplus/TcpLayer.h>
#include <pcapplusplus/UdpLayer.h>
#include <pcapplusplus/PcapLiveDeviceList.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <random>

PcapServer::PcapServer(const std::string& pcap_file_path,
                       const std::string& interface_name,
                       const std::string& client_ip,
                       const ServerReplayConfig& config)
    : pcap_file_path_(pcap_file_path)
    , interface_name_(interface_name)
    , client_ip_(client_ip)
    , config_(config)
    , network_device_(nullptr)
    , capture_running_(false)
    , initialized_(false)
    , running_(false)
    , interrupted_(false)
    , total_packets_(0)
    , sent_packets_(0)
    , failed_packets_(0)
    , received_packets_count_(0) {
}

PcapServer::~PcapServer() {
    cleanup();
}

bool PcapServer::initialize() {
    if (initialized_.load()) {
        std::cout << "服务端已经初始化" << std::endl;
        return true;
    }
    
    try {
        // 验证客户端IP地址
        if (client_ip_.empty()) {
            std::cerr << "错误：客户端IP地址不能为空" << std::endl;
            return false;
        }
        
        // 初始化共享内存
        shm_manager_ = std::make_unique<SharedMemoryManager>(false);
        if (!shm_manager_->initialize()) {
            std::cerr << "错误：无法初始化共享内存" << std::endl;
            return false;
        }
        
        // 初始化包分析器
        packet_analyzer_ = std::make_unique<PacketAnalyzer>(client_ip_);
        
        // 分析PCAP文件
        std::cout << "正在分析PCAP文件: " << pcap_file_path_ << std::endl;
        if (!packet_analyzer_->analyzePcapFile(pcap_file_path_)) {
            std::cerr << "错误：无法分析PCAP文件" << std::endl;
            return false;
        }
        
        // 获取服务端包
        server_packets_ = packet_analyzer_->getServerPackets();
        if (server_packets_.empty()) {
            std::cerr << "警告：PCAP文件中没有找到服务端包" << std::endl;
        }
        
        total_packets_ = static_cast<int>(server_packets_.size());
        
        // 打印统计信息
        packet_analyzer_->printStatistics();
        std::cout << "服务端需要发送的包数量: " << total_packets_ << std::endl;
        
        // 验证网络设备
        if (!validateNetworkDevice()) {
            std::cerr << "错误：网络设备验证失败" << std::endl;
            return false;
        }
        
        // 预处理包数据
        if (!preprocessPackets()) {
            std::cerr << "错误：包数据预处理失败" << std::endl;
            return false;
        }
        
        // 更新共享内存中的统计信息
        auto* shm_data = shm_manager_->getData();
        if (shm_data) {
            shm_data->total_server_packets.store(total_packets_);
            shm_data->server_ready.store(true);
        }
        
        initialized_ = true;
        std::cout << "服务端初始化完成" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "初始化异常: " << e.what() << std::endl;
        return false;
    }
}

bool PcapServer::startReplay() {
    if (!initialized_.load()) {
        std::cerr << "错误：服务端未初始化" << std::endl;
        return false;
    }
    
    if (running_.load()) {
        std::cout << "服务端已在运行中" << std::endl;
        return true;
    }
    
    running_ = true;
    interrupted_ = false;
    sent_packets_ = 0;
    failed_packets_ = 0;
    received_packets_count_ = 0;
    
    std::cout << "等待客户端就绪..." << std::endl;
    if (!waitForClient()) {
        std::cerr << "错误：等待客户端超时" << std::endl;
        running_ = false;
        return false;
    }
    
    std::cout << "开始启动包捕获..." << std::endl;
    if (!startPacketCapture()) {
        std::cerr << "错误：无法启动包捕获" << std::endl;
        running_ = false;
        return false;
    }
    
    std::cout << "开始服务端包回放..." << std::endl;
    start_time_ = std::chrono::high_resolution_clock::now();
    
    // 获取所有包的顺序信息
    auto all_packets = packet_analyzer_->getAllPackets();
    
    // 主回放循环 - 基于PCAP包的原始顺序
    for (size_t global_index = 0; global_index < all_packets.size() && !interrupted_.load(); ++global_index) {
        const auto& packet_info = all_packets[global_index];
        
        // 检查是否是服务端包
        if (packet_info.direction == PacketDirection::SERVER_TO_CLIENT) {
            // 等待轮到服务端发送
            if (!waitForServerTurn(static_cast<int>(global_index))) {
                std::cout << "等待服务端发送轮次超时，跳过包 " << global_index << std::endl;
                continue;
            }
            
            total_packets_.fetch_add(1);
            
            // 发送服务端包
            bool sent_success = sendServerPacket(packet_info, static_cast<int>(global_index));
            
            if (sent_success) {
                sent_packets_.fetch_add(1);
            } else {
                failed_packets_.fetch_add(1);
            }
            
            // 检查下一包是否还是服务端包
            bool next_is_server = false;
            if (global_index + 1 < all_packets.size()) {
                next_is_server = (all_packets[global_index + 1].direction == PacketDirection::SERVER_TO_CLIENT);
            }
            
            if (next_is_server) {
                // 下一包还是服务端包，计算延迟后继续发送
                int delay_ms = calculateDelayToNextPacket(packet_info, all_packets[global_index + 1]);
                if (delay_ms > 0) {
                    executeDelay(delay_ms);
                }
            } else {
                // 下一包是客户端包，切换到接收态
                switchToReceiveMode(static_cast<int>(global_index + 1));
            }
            
            // 每发送50个包显示一次进度
            if (sent_packets_.load() % 50 == 0) {
                std::cout << "服务端已发送: " << sent_packets_.load() 
                          << " 包, 成功率: " << (sent_packets_.load() * 100.0 / total_packets_.load()) << "%" << std::endl;
            }
        } else {
            // 客户端包，等待客户端处理
            waitForClientPacket(static_cast<int>(global_index));
        }
        
        // 检查中断
        if (interrupted_.load()) {
            std::cout << "\n收到中断信号，停止回放" << std::endl;
            break;
        }
    }
    
    end_time_ = std::chrono::high_resolution_clock::now();
    
    // 停止包捕获
    stopPacketCapture();
    
    // 更新共享内存完成状态
    auto* shm_data = shm_manager_->getData();
    if (shm_data) {
        shm_data->replay_finished.store(true);
    }
    
    running_ = false;
    
    // 打印最终统计信息
    printStatistics();
    
    return true;
}

void PcapServer::stopReplay() {
    interrupted_ = true;
    running_ = false;
    stopPacketCapture();
}

bool PcapServer::isRunning() const {
    return running_.load();
}

int PcapServer::getTotalPackets() const {
    return total_packets_.load();
}

int PcapServer::getSentPackets() const {
    return sent_packets_.load();
}

int PcapServer::getFailedPackets() const {
    return failed_packets_.load();
}

int PcapServer::getReceivedPackets() const {
    return received_packets_count_.load();
}

double PcapServer::getSuccessRate() const {
    int total = total_packets_.load();
    if (total == 0) return 0.0;
    return static_cast<double>(sent_packets_.load()) / total * 100.0;
}

void PcapServer::printStatistics() const {
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time_ - start_time_);
    
    std::cout << "\n=== 服务端回放统计 ===" << std::endl;
    std::cout << "总包数: " << total_packets_.load() << std::endl;
    std::cout << "成功发送: " << sent_packets_.load() << std::endl;
    std::cout << "发送失败: " << failed_packets_.load() << std::endl;
    std::cout << "接收到的包: " << received_packets_count_.load() << std::endl;
    std::cout << "成功率: " << std::fixed << std::setprecision(2) << getSuccessRate() << "%" << std::endl;
    std::cout << "回放时间: " << duration.count() << " ms" << std::endl;
    
    if (duration.count() > 0) {
        double pps = static_cast<double>(sent_packets_.load()) * 1000.0 / duration.count();
        std::cout << "平均发送速率: " << std::fixed << std::setprecision(2) << pps << " pps" << std::endl;
    }
}

void PcapServer::setInterrupted() {
    interrupted_ = true;
}

bool PcapServer::preprocessPackets() {
    if (server_packets_.empty()) {
        std::cout << "没有服务端包需要预处理" << std::endl;
        return true;
    }
    
    packet_delay_pairs_.clear();
    packet_delay_pairs_.reserve(server_packets_.size());
    
    // 等速回放模式需要特殊处理
    if (config_.mode == ServerReplayMode::ConstantRate) {
        return preprocessConstantRateMode();
    }
    
    // 其他模式的处理
    for (size_t i = 0; i < server_packets_.size(); ++i) {
        const auto& current_packet = server_packets_[i];
        int delay_ms = 0;
        
        if (i > 0) {
            const auto& prev_packet = server_packets_[i - 1];
            delay_ms = calculatePacketDelay(current_packet, prev_packet, static_cast<int>(i));
        }
        
        packet_delay_pairs_.emplace_back(current_packet, delay_ms);
    }
    
    std::cout << "包预处理完成，共 " << packet_delay_pairs_.size() << " 个包" << std::endl;
    return true;
}

int PcapServer::calculatePacketDelay(const PacketInfo& current_packet,
                                   const PacketInfo& prev_packet,
                                   int packet_index) {
    int delay_ms = 0;
    
    switch (config_.mode) {
        case ServerReplayMode::OriginalSpeed: {
            // 原速回放：使用原始时间间隔
            uint64_t time_diff_us = current_packet.timestamp_us - prev_packet.timestamp_us;
            delay_ms = static_cast<int>(time_diff_us / 1000);
            break;
        }
        
        case ServerReplayMode::FixedInterval:
            // 固定间隔回放
            delay_ms = config_.fixedIntervalMs;
            break;
            
        case ServerReplayMode::FloatingOriginal: {
            // 浮动原速回放
            uint64_t time_diff_us = current_packet.timestamp_us - prev_packet.timestamp_us;
            double base_delay = static_cast<double>(time_diff_us) / 1000.0;
            double random_factor = 1.0 + (static_cast<double>(rand()) / RAND_MAX - 0.5) * 2.0 * config_.floatPercent;
            delay_ms = static_cast<int>(base_delay * random_factor);
            break;
        }
        
        case ServerReplayMode::ConstantRate:
            // 等速回放模式在preprocessConstantRateMode中处理
            delay_ms = 0;
            break;
    }
    
    return std::max(0, delay_ms);
}

bool PcapServer::preprocessConstantRateMode() {
    if (config_.targetBytesPerSec <= 0) {
        std::cerr << "错误：等速回放模式需要指定目标流量" << std::endl;
        return false;
    }
    
    // 计算总字节数
    size_t total_bytes = 0;
    for (const auto& packet : server_packets_) {
        total_bytes += packet.packet_size;
    }
    
    std::cout << "等速回放模式 - 总字节数: " << total_bytes 
              << ", 目标流量: " << config_.targetBytesPerSec << " bytes/sec" << std::endl;
    
    // 计算每个包的累积延迟
    size_t cumulative_bytes = 0;
    for (size_t i = 0; i < server_packets_.size(); ++i) {
        const auto& packet = server_packets_[i];
        cumulative_bytes += packet.packet_size;
        
        // 计算到当前包应该经过的时间（毫秒）
        int target_time_ms = static_cast<int>((cumulative_bytes * 1000.0) / config_.targetBytesPerSec);
        
        // 计算相对于前一个包的延迟
        int delay_ms = 0;
        if (i > 0) {
            size_t prev_cumulative_bytes = cumulative_bytes - packet.packet_size;
            int prev_target_time_ms = static_cast<int>((prev_cumulative_bytes * 1000.0) / config_.targetBytesPerSec);
            delay_ms = target_time_ms - prev_target_time_ms;
        }
        
        packet_delay_pairs_.emplace_back(packet, std::max(0, delay_ms));
    }
    
    std::cout << "等速回放预处理完成" << std::endl;
    return true;
}

bool PcapServer::sendPacket(const PacketInfo& packet_info) {
    if (!network_device_ || !network_device_->isOpened()) {
        return false;
    }
    
    try {
        // 创建原始包数据
        // 将微秒时间戳转换为timeval结构
        timeval tv;
        tv.tv_sec = packet_info.timestamp_us / 1000000;
        tv.tv_usec = packet_info.timestamp_us % 1000000;
        
        pcpp::RawPacket raw_packet(packet_info.raw_data.data(), 
                                   static_cast<int>(packet_info.raw_data.size()), 
                                   tv, 
                                   false);
        
        // 发送包
        pcpp::Packet packet(&raw_packet);
        bool success = network_device_->sendPacket(&packet);
        
        if (success) {
            sent_packets_++;
        } else {
            failed_packets_++;
        }
        
        return success;
        
    } catch (const std::exception& e) {
        std::cerr << "发送包异常: " << e.what() << std::endl;
        failed_packets_++;
        return false;
    }
}

void PcapServer::executeDelay(int delay_ms) {
    if (delay_ms <= 0) return;
    
    const int check_interval_ms = 10;
    int remaining_ms = delay_ms;
    
    while (remaining_ms > 0 && !interrupted_.load()) {
        int sleep_time = std::min(remaining_ms, check_interval_ms);
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
        remaining_ms -= sleep_time;
    }
}

bool PcapServer::waitForClient(int timeout_ms) {
    auto* shm_data = shm_manager_->getData();
    if (!shm_data) {
        return false;
    }
    
    auto start_time = std::chrono::steady_clock::now();
    
    while (!interrupted_.load()) {
        if (shm_data->client_ready.load()) {
            std::cout << "客户端已就绪" << std::endl;
            return true;
        }
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);
        
        if (elapsed.count() >= timeout_ms) {
            std::cerr << "等待客户端超时" << std::endl;
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return false;
}

bool PcapServer::waitForServerTurn(int packet_index) {
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

bool PcapServer::sendServerPacket(const PacketInfo& packet_info, int packet_index) {
    // 发送包
    bool sent_success = sendPacket(packet_info);
    
    // 更新共享内存状态
    SharedMemoryData* shm_data = shm_manager_->getData();
    if (shm_data != nullptr) {
        shm_data->current_packet_index.store(packet_index);
        shm_data->server_packet_received.store(sent_success);
        shm_data->last_send_time_us.store(SharedMemoryManager::getCurrentTimeMicros());
        
        // 服务端发送包后，更新共享内存状态
        shm_data->next_packet_index.store(packet_index + 1); // 指向下一个包（可能是客户端或服务端）
        
        if (sent_success) {
            shm_data->server_sent_count.fetch_add(1);
            std::cout << "服务端发送包 " << packet_index+1 << " 成功" << std::endl;
        } else {
            shm_data->server_failed_count.fetch_add(1);
            std::cout << "服务端发送包 " << packet_index+1 << " 失败" << std::endl;
        }
    }
    
    return sent_success;
}

void PcapServer::switchToReceiveMode(int next_packet_index) {
    SharedMemoryData* shm_data = shm_manager_->getData();
    if (!shm_data) return;
    
    std::cout << "服务端切换到接收态，等待客户端发送包 " << next_packet_index << std::endl;
    
    // 设置服务端为接收态
    shm_data->server_in_receive_mode.store(true);
    shm_data->current_sender.store(0); // 0表示客户端
    shm_data->next_packet_index.store(next_packet_index);
    shm_data->waiting_for_peer.store(true);
}

void PcapServer::waitForClientPacket(int packet_index) {
    SharedMemoryData* shm_data = shm_manager_->getData();
    if (!shm_data) return;
    
    // 简化逻辑：只等待next_packet_index更新到下一个包
    auto start_time = std::chrono::steady_clock::now();
    const int timeout_ms = 2000; // 2秒超时
    
    while (!interrupted_.load()) {
        // 检查客户端是否已经处理完当前包
        if (shm_data->next_packet_index.load() > packet_index) {
            break;
        }
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);
        if (elapsed.count() >= timeout_ms) {
            std::cout << "等待客户端处理包 " << packet_index << " 超时" << std::endl;
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

int PcapServer::calculateDelayToNextPacket(const PacketInfo& current_packet, const PacketInfo& next_packet) {
    // 计算时间差（微秒）
    uint64_t time_diff_us = next_packet.timestamp_us - current_packet.timestamp_us;
    
    // 根据回放模式调整延迟
    switch (config_.mode) {
        case ServerReplayMode::OriginalSpeed:
            return static_cast<int>(time_diff_us / 1000); // 转换为毫秒
            
        case ServerReplayMode::FixedInterval:
            return config_.fixedIntervalMs;
            
        case ServerReplayMode::FloatingOriginal: {
            double base_delay = time_diff_us / 1000.0;
            double variation = base_delay * config_.floatPercent;
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<> dis(-variation, variation);
            return static_cast<int>(base_delay + dis(gen));
        }
        
        case ServerReplayMode::ConstantRate:
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

bool PcapServer::synchronizedSendPacket(const PacketInfo& packet_info, int packet_index) {
    auto* shm_data = shm_manager_->getData();
    if (!shm_data) {
        return sendPacket(packet_info);
    }
    
    // 获取对应的客户端包（如果存在）
    auto client_packets = packet_analyzer_->getClientPackets();
    PacketInfo expected_client_packet;
    bool has_expected_client = false;
    
    // 查找对应的客户端包（基于原始索引或时间戳）
    for (const auto& client_packet : client_packets) {
        if (client_packet.original_index < packet_info.original_index) {
            expected_client_packet = client_packet;
            has_expected_client = true;
        } else {
            break;
        }
    }
    
    if (has_expected_client) {
        // 计算超时时间（基于PCAP时间间隔）
        uint64_t time_diff_us = packet_info.timestamp_us - expected_client_packet.timestamp_us;
        int timeout_ms = static_cast<int>(time_diff_us / 1000) + 1000; // 额外1秒缓冲
        timeout_ms = std::max(timeout_ms, 1000); // 最少1秒超时
        
        // 检查是否收到了对应的客户端包
        if (checkReceivedPacket(expected_client_packet, timeout_ms)) {
            // 收到了客户端包，按照PCAP时间间隔发送
            std::cout << "收到客户端包，按时间间隔发送服务端包 " << packet_index << std::endl;
            return sendPacket(packet_info);
        } else {
            // 没收到客户端包，检查客户端是否已发送
            if (waitForClientPacketSent(timeout_ms)) {
                // 客户端已发送，在超时后发送服务端包
                std::cout << "客户端已发送但未收到，超时后发送服务端包 " << packet_index << std::endl;
                return sendPacket(packet_info);
            } else {
                // 客户端未发送，跳过此包
                std::cout << "客户端未发送，跳过服务端包 " << packet_index << std::endl;
                return false;
            }
        }
    } else {
        // 没有对应的客户端包，直接发送
        std::cout << "无对应客户端包，直接发送服务端包 " << packet_index << std::endl;
        return sendPacket(packet_info);
    }
}

bool PcapServer::checkReceivedPacket(const PacketInfo& expected_packet, int timeout_ms) {
    auto start_time = std::chrono::steady_clock::now();
    
    while (!interrupted_.load()) {
        // 检查接收队列
        {
            std::lock_guard<std::mutex> lock(received_packets_mutex_);
            while (!received_packets_.empty()) {
                auto received = received_packets_.front();
                received_packets_.pop();
                
                if (isPacketMatch(received, expected_packet)) {
                    received_packets_count_++;
                    return true;
                }
            }
        }
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);
        
        if (elapsed.count() >= timeout_ms) {
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return false;
}

bool PcapServer::waitForClientPacketSent(int timeout_ms) {
    auto* shm_data = shm_manager_->getData();
    if (!shm_data) {
        return false;
    }
    
    auto start_time = std::chrono::steady_clock::now();
    int last_sent_count = shm_data->client_sent_count.load();
    
    while (!interrupted_.load()) {
        int current_sent_count = shm_data->client_sent_count.load();
        if (current_sent_count > last_sent_count) {
            return true;
        }
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);
        
        if (elapsed.count() >= timeout_ms) {
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    return false;
}

bool PcapServer::startPacketCapture() {
    if (!network_device_ || !network_device_->isOpened()) {
        std::cerr << "错误：网络设备未打开" << std::endl;
        return false;
    }
    
    capture_running_ = true;
    
    // 启动捕获
    if (!network_device_->startCapture(onPacketArrives, this)) {
        std::cerr << "错误：无法启动包捕获" << std::endl;
        capture_running_ = false;
        return false;
    }
    
    std::cout << "包捕获已启动" << std::endl;
    return true;
}

void PcapServer::stopPacketCapture() {
    if (capture_running_.load() && network_device_ && network_device_->isOpened()) {
        network_device_->stopCapture();
        capture_running_ = false;
        std::cout << "包捕获已停止" << std::endl;
    }
}

void PcapServer::onPacketArrives(pcpp::RawPacket* packet, pcpp::PcapLiveDevice* dev, void* cookie) {
    auto* server = static_cast<PcapServer*>(cookie);
    if (server && server->capture_running_.load()) {
        server->handleCapturedPacket(packet);
    }
}

void PcapServer::handleCapturedPacket(pcpp::RawPacket* packet) {
    if (!packet) return;
    
    try {
        pcpp::Packet parsed_packet(packet);
        ReceivedPacketInfo received_info;
        
        // 提取IP信息
        if (parsed_packet.isPacketOfType(pcpp::IPv4)) {
            auto* ipv4_layer = parsed_packet.getLayerOfType<pcpp::IPv4Layer>();
            if (ipv4_layer) {
                received_info.src_ip = ipv4_layer->getSrcIPAddress().toString();
                received_info.dst_ip = ipv4_layer->getDstIPAddress().toString();
            }
        } else if (parsed_packet.isPacketOfType(pcpp::IPv6)) {
            auto* ipv6_layer = parsed_packet.getLayerOfType<pcpp::IPv6Layer>();
            if (ipv6_layer) {
                received_info.src_ip = ipv6_layer->getSrcIPAddress().toString();
                received_info.dst_ip = ipv6_layer->getDstIPAddress().toString();
            }
        }
        
        // 提取端口信息
        if (parsed_packet.isPacketOfType(pcpp::TCP)) {
            auto* tcp_layer = parsed_packet.getLayerOfType<pcpp::TcpLayer>();
            if (tcp_layer) {
                received_info.src_port = tcp_layer->getSrcPort();
                received_info.dst_port = tcp_layer->getDstPort();
            }
        } else if (parsed_packet.isPacketOfType(pcpp::UDP)) {
            auto* udp_layer = parsed_packet.getLayerOfType<pcpp::UdpLayer>();
            if (udp_layer) {
                received_info.src_port = udp_layer->getSrcPort();
                received_info.dst_port = udp_layer->getDstPort();
            }
        }
        
        received_info.packet_size = packet->getRawDataLen();
        received_info.receive_time_us = SharedMemoryManager::getCurrentTimeMicros();
        
        // 只处理来自客户端的包
        if (received_info.src_ip == client_ip_) {
            std::lock_guard<std::mutex> lock(received_packets_mutex_);
            received_packets_.push(received_info);
        }
        
    } catch (const std::exception& e) {
        // 忽略解析错误的包
    }
}

void PcapServer::updateSharedMemoryState(int packet_index, bool sent_success) {
    auto* shm_data = shm_manager_->getData();
    if (!shm_data) return;
    
    if (sent_success) {
        shm_data->server_sent_count.store(sent_packets_.load());
    } else {
        shm_data->server_failed_count.store(failed_packets_.load());
    }
    
    shm_data->current_packet_index.store(packet_index);
    shm_data->last_send_time_us.store(SharedMemoryManager::getCurrentTimeMicros());
}

bool PcapServer::validateNetworkDevice() {
    // 获取网络设备
    network_device_ = pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDeviceByName(interface_name_);
    if (!network_device_) {
        std::cerr << "错误：找不到网络接口 '" << interface_name_ << "'" << std::endl;
        return false;
    }
    
    // 打开设备
    if (!network_device_->open()) {
        std::cerr << "错误：无法打开网络接口 '" << interface_name_ << "'" << std::endl;
        return false;
    }
    
    std::cout << "网络接口 '" << interface_name_ << "' 已打开" << std::endl;
    return true;
}

void PcapServer::cleanup() {
    stopReplay();
    
    if (network_device_ && network_device_->isOpened()) {
        network_device_->close();
        network_device_ = nullptr;
    }
    
    packet_analyzer_.reset();
    shm_manager_.reset();
    
    server_packets_.clear();
    packet_delay_pairs_.clear();
    
    // 清空接收队列
    {
        std::lock_guard<std::mutex> lock(received_packets_mutex_);
        while (!received_packets_.empty()) {
            received_packets_.pop();
        }
    }
}

bool PcapServer::isPacketMatch(const ReceivedPacketInfo& received_packet, const PacketInfo& expected_packet) const {
    // 简单的匹配逻辑：比较源IP、目的IP和包大小
    return (received_packet.src_ip == expected_packet.src_ip &&
            received_packet.dst_ip == expected_packet.dst_ip &&
            abs(static_cast<int>(received_packet.packet_size) - static_cast<int>(expected_packet.packet_size)) <= 10);
}