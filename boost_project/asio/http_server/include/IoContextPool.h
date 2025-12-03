#ifndef BOOST_DEMO_IOCONTEXTPOOL_H
#define BOOST_DEMO_IOCONTEXTPOOL_H

#include <boost/asio.hpp>
#include <vector>
#include <thread>
#include <memory>

namespace asio = boost::asio;

/**
 * @brief IoContext池管理类 - 实现N+1架构
 * 
 * 使用1个io_context用于接受连接，N个io_context用于处理I/O操作
 * 这样可以防止io_context在没有工作时退出，提高服务器稳定性
 */
class IoContextPool {
public:
    /**
     * @brief 构造函数
     * @param pool_size I/O处理池的大小（0表示使用CPU核心数）
     */
    explicit IoContextPool(std::size_t pool_size = 0);
    
    /**
     * @brief 析构函数
     */
    ~IoContextPool();
    
    /**
     * @brief 启动所有io_context（阻塞调用）
     */
    void Run();
    
    /**
     * @brief 停止所有io_context
     */
    void Stop();
    
    /**
     * @brief 获取用于接受连接的io_context
     */
    asio::io_context& GetAcceptorContext();
    
    /**
     * @brief 获取下一个用于I/O处理的io_context（轮询分配）
     */
    asio::io_context& GetNextIoContext();
    
    /**
     * @brief 获取I/O池的大小
     */
    std::size_t GetPoolSize() const { return pool_size_; }

private:
    // 1个io_context用于接受连接
    asio::io_context acceptor_io_context_;
    std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> acceptor_work_guard_;
    
    // N个io_context用于处理I/O
    std::vector<std::unique_ptr<asio::io_context>> io_contexts_;
    std::vector<std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>>> work_guards_;
    std::vector<std::thread> threads_;
    
    std::size_t next_io_context_index_;
    std::size_t pool_size_;
};

#endif // BOOST_DEMO_IOCONTEXTPOOL_H

