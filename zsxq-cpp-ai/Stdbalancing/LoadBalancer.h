/**
 * @file LoadBalancer.h
 * @brief 负载均衡器基类定义
 * @details 定义负载均衡器的接口和基本功能
 */

#ifndef LOADBALANCER_H
#define LOADBALANCER_H

#include "Server.h"
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <algorithm>

/**
 * @class LoadBalancer
 * @brief 负载均衡器基类
 * @details 所有负载均衡算法的抽象基类
 */
class LoadBalancer {
public:
    /**
     * @brief 默认构造函数
     */
    LoadBalancer() = default;

    /**
     * @brief 虚析构函数
     */
    virtual ~LoadBalancer() = default;

    /**
     * @brief 添加服务器节点
     * @param server 服务器智能指针
     */
    virtual void addServer(std::shared_ptr<Server> server) {
        std::lock_guard<std::mutex> lock(mutex_);
        servers_.push_back(server);
        server_map_[server->getId()] = server;
        onServerAdded(server);
    }

    /**
     * @brief 移除服务器节点
     * @param server_id 要移除的服务器ID
     * @return true表示移除成功，false表示服务器不存在
     */
    virtual bool removeServer(const std::string& server_id) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = server_map_.find(server_id);
        if (it == server_map_.end()) {
            return false;
        }

        // 从服务器列表中移除
        servers_.erase(
            std::remove_if(servers_.begin(), servers_.end(),
                [&server_id](const std::shared_ptr<Server>& s) {
                    return s->getId() == server_id;
                }),
            servers_.end()
        );

        onServerRemoved(it->second);
        server_map_.erase(it);
        return true;
    }

    /**
     * @brief 获取下一个服务器（核心算法接口）
     * @param key 用于路由的键值（可选，用于哈希等算法）
     * @return 选中的服务器指针，如果没有可用服务器则返回nullptr
     */
    virtual std::shared_ptr<Server> selectServer(const std::string& key = "") = 0;

    /**
     * @brief 获取所有服务器列表
     * @return 服务器列表的副本
     */
    std::vector<std::shared_ptr<Server>> getServers() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return servers_;
    }

    /**
     * @brief 获取可用服务器列表
     * @return 所有存活服务器的列表
     */
    std::vector<std::shared_ptr<Server>> getAvailableServers() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::shared_ptr<Server>> available;
        for (const auto& server : servers_) {
            if (server->isAlive()) {
                available.push_back(server);
            }
        }
        return available;
    }

    /**
     * @brief 标记服务器为下线
     * @param server_id 服务器ID
     */
    void markServerDown(const std::string& server_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = server_map_.find(server_id);
        if (it != server_map_.end()) {
            it->second->setAlive(false);
            onServerMarkedDown(it->second);
        }
    }

    /**
     * @brief 标记服务器为上线
     * @param server_id 服务器ID
     */
    void markServerUp(const std::string& server_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = server_map_.find(server_id);
        if (it != server_map_.end()) {
            it->second->setAlive(true);
            onServerMarkedUp(it->second);
        }
    }

    /**
     * @brief 获取服务器总数
     * @return 服务器数量
     */
    size_t getServerCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return servers_.size();
    }

    /**
     * @brief 获取可用服务器数量
     * @return 存活的服务器数量
     */
    size_t getAvailableServerCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t count = 0;
        for (const auto& server : servers_) {
            if (server->isAlive()) {
                count++;
            }
        }
        return count;
    }

    /**
     * @brief 获取算法名称
     * @return 负载均衡算法的名称
     */
    virtual std::string getAlgorithmName() const = 0;

    /**
     * @brief 重置负载均衡器状态
     * @details 清空内部状态和统计信息
     */
    virtual void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& server : servers_) {
            server->reset();
        }
        onReset();
    }

protected:
    /**
     * @brief 服务器添加后的钩子函数
     * @param server 新添加的服务器
     */
    virtual void onServerAdded(std::shared_ptr<Server> /*server*/) {}

    /**
     * @brief 服务器移除后的钩子函数
     * @param server 被移除的服务器
     */
    virtual void onServerRemoved(std::shared_ptr<Server> /*server*/) {}

    /**
     * @brief 服务器标记为下线后的钩子函数
     * @param server 被标记下线的服务器
     */
    virtual void onServerMarkedDown(std::shared_ptr<Server> /*server*/) {}

    /**
     * @brief 服务器标记为上线后的钩子函数
     * @param server 被标记上线的服务器
     */
    virtual void onServerMarkedUp(std::shared_ptr<Server> /*server*/) {}

    /**
     * @brief 重置时的钩子函数
     */
    virtual void onReset() {}

protected:
    mutable std::mutex mutex_;                           // 保护内部数据的互斥锁
    std::vector<std::shared_ptr<Server>> servers_;       // 服务器列表
    std::unordered_map<std::string, std::shared_ptr<Server>> server_map_;  // 服务器ID映射

    /**
     * @brief 获取第一个可用服务器
     * @return 第一个存活的服务器，如果都不可用则返回nullptr
     */
    std::shared_ptr<Server> getFirstAvailable() const {
        for (const auto& server : servers_) {
            if (server->isAlive()) {
                return server;
            }
        }
        return nullptr;
    }
};

#endif // LOADBALANCER_H