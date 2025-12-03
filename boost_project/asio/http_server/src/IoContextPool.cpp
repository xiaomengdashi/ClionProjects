#include "IoContextPool.h"
#include <iostream>
#include <thread>

IoContextPool::IoContextPool(std::size_t pool_size)
    : next_io_context_index_(0) {
    
    // 如果没有指定数量，使用CPU核心数
    if (pool_size == 0) {
        pool_size = std::thread::hardware_concurrency();
        if (pool_size == 0) {
            pool_size = 1; // 如果无法检测，至少使用1个
        }
    }
    
    pool_size_ = pool_size;
    
    // 创建N个io_context用于处理I/O
    for (std::size_t i = 0; i < pool_size_; ++i) {
        io_contexts_.emplace_back(std::make_unique<asio::io_context>());
        work_guards_.emplace_back(
            std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
                asio::make_work_guard(*io_contexts_[i])));
    }
    
    // 为acceptor创建work_guard
    acceptor_work_guard_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
        asio::make_work_guard(acceptor_io_context_));
    
    std::cout << "IoContextPool initialized with " << pool_size_ << " I/O contexts" << std::endl;
}

IoContextPool::~IoContextPool() {
    Stop();
}

asio::io_context& IoContextPool::GetAcceptorContext() {
    return acceptor_io_context_;
}

asio::io_context& IoContextPool::GetNextIoContext() {
    // 轮询分配io_context
    auto& io_context = *io_contexts_[next_io_context_index_];
    next_io_context_index_ = (next_io_context_index_ + 1) % pool_size_;
    return io_context;
}

void IoContextPool::Run() {
    // 启动acceptor线程
    std::thread acceptor_thread([this]() {
        acceptor_io_context_.run();
    });
    
    // 启动I/O处理线程池
    for (std::size_t i = 0; i < pool_size_; ++i) {
        threads_.emplace_back([this, i]() {
            io_contexts_[i]->run();
        });
    }
    
    // 等待acceptor线程
    acceptor_thread.join();
    
    // 等待所有I/O线程
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void IoContextPool::Stop() {
    // 停止acceptor
    acceptor_work_guard_.reset();
    acceptor_io_context_.stop();
    
    // 停止所有I/O contexts
    for (auto& work_guard : work_guards_) {
        work_guard.reset();
    }
    
    for (auto& io_context : io_contexts_) {
        io_context->stop();
    }
}

