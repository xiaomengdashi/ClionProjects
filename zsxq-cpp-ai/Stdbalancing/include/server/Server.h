/**
 * @file Server.h
 * @brief 服务器节点类定义
 * @details 模拟服务器节点，包含服务器的基本属性和状态
 */

#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <atomic>
#include <chrono>
#include <mutex>

/**
 * @class Server
 * @brief 服务器节点类
 * @details 代表负载均衡系统中的一个服务器节点
 */
class Server {
public:
    /**
     * @brief 构造函数
     * @param id 服务器唯一标识
     * @param address 服务器地址（IP:Port格式）
     * @param weight 服务器权重，默认为1
     */
    Server(const std::string& id, const std::string& address, int weight = 1)
        : server_id_(id)
        , address_(address)
        , weight_(weight)
        , current_connections_(0)
        , total_requests_(0)
        , failed_requests_(0)
        , is_alive_(true)
        , last_check_time_(std::chrono::steady_clock::now()) {
    }

    /**
     * @brief 获取服务器ID
     * @return 服务器唯一标识
     */
    std::string getId() const { return server_id_; }

    /**
     * @brief 获取服务器地址
     * @return 服务器地址字符串
     */
    std::string getAddress() const { return address_; }

    /**
     * @brief 获取服务器权重
     * @return 权重值
     */
    int getWeight() const { return weight_; }

    /**
     * @brief 设置服务器权重
     * @param w 新的权重值
     */
    void setWeight(int w) { weight_ = w; }

    /**
     * @brief 获取当前连接数
     * @return 当前活跃连接数
     */
    int getCurrentConnections() const {
        return current_connections_.load();
    }

    /**
     * @brief 增加连接数
     * @details 当新请求分配到该服务器时调用
     */
    void addConnection() {
        current_connections_++;
        total_requests_++;
    }

    /**
     * @brief 减少连接数
     * @details 当请求处理完成时调用
     */
    void removeConnection() {
        if (current_connections_ > 0) {
            current_connections_--;
        }
    }

    /**
     * @brief 获取总请求数
     * @return 服务器处理的总请求数
     */
    uint64_t getTotalRequests() const {
        return total_requests_.load();
    }

    /**
     * @brief 记录失败请求
     * @details 当请求处理失败时调用
     */
    void recordFailure() {
        failed_requests_++;
    }

    /**
     * @brief 获取失败请求数
     * @return 失败的请求总数
     */
    uint64_t getFailedRequests() const {
        return failed_requests_.load();
    }

    /**
     * @brief 检查服务器是否存活
     * @return true表示服务器可用，false表示不可用
     */
    bool isAlive() const {
        return is_alive_.load();
    }

    /**
     * @brief 设置服务器存活状态
     * @param alive true表示服务器可用，false表示不可用
     */
    void setAlive(bool alive) {
        is_alive_ = alive;
        last_check_time_ = std::chrono::steady_clock::now();
    }

    /**
     * @brief 获取最后健康检查时间
     * @return 最后一次健康检查的时间点
     */
    std::chrono::steady_clock::time_point getLastCheckTime() const {
        return last_check_time_;
    }

    /**
     * @brief 重置服务器统计信息
     * @details 清空所有统计数据
     */
    void reset() {
        current_connections_ = 0;
        total_requests_ = 0;
        failed_requests_ = 0;
    }

private:
    std::string server_id_;                    // 服务器唯一标识
    std::string address_;                       // 服务器地址
    int weight_;                                // 服务器权重
    std::atomic<int> current_connections_;      // 当前连接数
    std::atomic<uint64_t> total_requests_;      // 总请求数
    std::atomic<uint64_t> failed_requests_;     // 失败请求数
    std::atomic<bool> is_alive_;                // 服务器存活状态
    std::chrono::steady_clock::time_point last_check_time_;  // 最后检查时间
};

#endif // SERVER_H