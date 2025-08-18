#include "shared_memory.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <chrono>
#include <thread>
#include <errno.h>

// 静态成员定义
const std::string SharedMemoryManager::SHARED_MEMORY_NAME = "/pcap_replay_shm";
const size_t SharedMemoryManager::SHARED_MEMORY_SIZE = sizeof(SharedMemoryData);

SharedMemoryManager::SharedMemoryManager(bool create_new)
    : create_new_(create_new)
    , shm_fd_(-1)
    , data_(nullptr)
    , initialized_(false)
    , is_creator_(create_new) {
}

SharedMemoryManager::~SharedMemoryManager() {
    if (data_ != nullptr) {
        munmap(data_, SHARED_MEMORY_SIZE);
        data_ = nullptr;
    }
    
    if (shm_fd_ != -1) {
        close(shm_fd_);
        shm_fd_ = -1;
    }
    
    // 如果是创建者，清理共享内存
    if (is_creator_) {
        shm_unlink(SHARED_MEMORY_NAME.c_str());
    }
}

bool SharedMemoryManager::initialize() {
    if (initialized_) {
        return true;
    }
    
    // 创建或打开共享内存
    if (create_new_) {
        // 先尝试删除可能存在的旧共享内存
        shm_unlink(SHARED_MEMORY_NAME.c_str());
        
        // 创建新的共享内存
        shm_fd_ = shm_open(SHARED_MEMORY_NAME.c_str(), O_CREAT | O_RDWR, 0666);
        if (shm_fd_ == -1) {
            std::cerr << "错误: 无法创建共享内存: " << strerror(errno) << std::endl;
            return false;
        }
        
        // 设置共享内存大小
        if (ftruncate(shm_fd_, SHARED_MEMORY_SIZE) == -1) {
            std::cerr << "错误: 无法设置共享内存大小: " << strerror(errno) << std::endl;
            close(shm_fd_);
            shm_fd_ = -1;
            shm_unlink(SHARED_MEMORY_NAME.c_str());
            return false;
        }
    } else {
        // 连接现有的共享内存
        shm_fd_ = shm_open(SHARED_MEMORY_NAME.c_str(), O_RDWR, 0666);
        if (shm_fd_ == -1) {
            std::cerr << "错误: 无法连接共享内存: " << strerror(errno) << std::endl;
            std::cerr << "请确保客户端程序已经启动" << std::endl;
            return false;
        }
    }
    
    // 映射共享内存到进程地址空间
    data_ = static_cast<SharedMemoryData*>(
        mmap(nullptr, SHARED_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0)
    );
    
    if (data_ == MAP_FAILED) {
        std::cerr << "错误: 无法映射共享内存: " << strerror(errno) << std::endl;
        close(shm_fd_);
        shm_fd_ = -1;
        if (create_new_) {
            shm_unlink(SHARED_MEMORY_NAME.c_str());
        }
        data_ = nullptr;
        return false;
    }
    
    // 如果是创建者，初始化数据结构
    if (create_new_) {
        // 使用placement new来正确初始化原子变量
        new (data_) SharedMemoryData();
    }
    
    initialized_ = true;
    return true;
}

SharedMemoryData* SharedMemoryManager::getData() {
    return initialized_ ? data_ : nullptr;
}

bool SharedMemoryManager::isInitialized() const {
    return initialized_;
}

bool SharedMemoryManager::cleanup() {
    if (!is_creator_) {
        return false;
    }
    
    if (data_ != nullptr) {
        munmap(data_, SHARED_MEMORY_SIZE);
        data_ = nullptr;
    }
    
    if (shm_fd_ != -1) {
        close(shm_fd_);
        shm_fd_ = -1;
    }
    
    int result = shm_unlink(SHARED_MEMORY_NAME.c_str());
    initialized_ = false;
    
    return result == 0;
}

bool SharedMemoryManager::waitForPeer(bool is_client, int timeout_ms) {
    if (!initialized_ || data_ == nullptr) {
        return false;
    }
    
    auto start_time = std::chrono::steady_clock::now();
    
    while (true) {
        if (is_client) {
            // 客户端等待服务端就绪
            if (data_->server_ready.load()) {
                return true;
            }
        } else {
            // 服务端等待客户端就绪
            if (data_->client_ready.load()) {
                return true;
            }
        }
        
        // 检查超时
        if (timeout_ms > 0) {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                current_time - start_time
            ).count();
            
            if (elapsed >= timeout_ms) {
                return false;
            }
        }
        
        // 检查终止信号
        if (data_->should_terminate.load()) {
            return false;
        }
        
        // 短暂休眠避免忙等待
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void SharedMemoryManager::setReady(bool is_client) {
    if (!initialized_ || data_ == nullptr) {
        return;
    }
    
    if (is_client) {
        data_->client_ready.store(true);
    } else {
        data_->server_ready.store(true);
    }
}

uint64_t SharedMemoryManager::getCurrentTimeMicros() {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

bool SharedMemoryManager::acquireLock(int timeout_ms) {
    if (!initialized_ || data_ == nullptr) {
        return false;
    }
    
    auto start_time = std::chrono::steady_clock::now();
    
    while (true) {
        // 尝试获取锁
        bool expected = false;
        if (data_->sync_lock.compare_exchange_weak(expected, true)) {
            return true;
        }
        
        // 检查超时
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time - start_time
        ).count();
        
        if (elapsed >= timeout_ms) {
            return false;
        }
        
        // 短暂休眠避免忙等待
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

void SharedMemoryManager::releaseLock() {
    if (initialized_ && data_ != nullptr) {
        data_->sync_lock.store(false);
    }
}