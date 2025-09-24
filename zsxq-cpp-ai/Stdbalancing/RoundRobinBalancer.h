/**
 * @file RoundRobinBalancer.h
 * @brief 轮询负载均衡算法实现
 * @details 实现简单轮询和加权轮询两种算法
 */

#ifndef ROUNDROBINBALANCER_H
#define ROUNDROBINBALANCER_H

#include "LoadBalancer.h"
#include <atomic>

/**
 * @class RoundRobinBalancer
 * @brief 简单轮询负载均衡器
 * @details 按照服务器列表顺序依次分配请求
 */
class RoundRobinBalancer : public LoadBalancer {
public:
    /**
     * @brief 构造函数
     */
    RoundRobinBalancer() : current_index_(0) {}

    /**
     * @brief 选择下一个服务器
     * @param key 未使用（轮询算法不需要键值）
     * @return 选中的服务器
     */
    std::shared_ptr<Server> selectServer(const std::string& ) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (servers_.empty()) {
            return nullptr;
        }

        // 获取可用服务器列表
        std::vector<std::shared_ptr<Server>> available_servers;
        for (const auto& server : servers_) {
            if (server->isAlive()) {
                available_servers.push_back(server);
            }
        }

        if (available_servers.empty()) {
            return nullptr;
        }

        // 简单轮询：依次选择下一个服务器
        size_t index = current_index_.fetch_add(1) % available_servers.size();
        auto selected = available_servers[index];

        // 增加连接计数
        selected->addConnection();
        return selected;
    }

    /**
     * @brief 获取算法名称
     * @return 算法名称字符串
     */
    std::string getAlgorithmName() const override {
        return "Round Robin";
    }

protected:
    /**
     * @brief 重置内部状态
     */
    void onReset() override {
        current_index_ = 0;
    }

    /**
     * @brief 服务器移除后调整索引
     * @param server 被移除的服务器
     */
    void onServerRemoved(std::shared_ptr<Server> /*server*/) override {
        // 如果当前索引超出范围，重置为0
        if (servers_.empty()) {
            current_index_ = 0;
        }
    }

private:
    std::atomic<size_t> current_index_;  // 当前轮询索引
};

/**
 * @class WeightedRoundRobinBalancer
 * @brief 加权轮询负载均衡器
 * @details 根据服务器权重分配请求，权重高的服务器获得更多请求
 */
class WeightedRoundRobinBalancer : public LoadBalancer {
public:
    /**
     * @brief 构造函数
     */
    WeightedRoundRobinBalancer() : current_index_(0), gcd_(1), max_weight_(0) {}

    /**
     * @brief 选择下一个服务器
     * @param key 未使用（轮询算法不需要键值）
     * @return 选中的服务器
     */
    std::shared_ptr<Server> selectServer(const std::string&) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (servers_.empty()) {
            return nullptr;
        }

        // 获取可用服务器列表
        std::vector<std::shared_ptr<Server>> available_servers;
        for (const auto& server : servers_) {
            if (server->isAlive()) {
                available_servers.push_back(server);
            }
        }

        if (available_servers.empty()) {
            return nullptr;
        }

        // 使用平滑加权轮询算法（Smooth Weighted Round-Robin）
        return smoothWeightedRoundRobin(available_servers);
    }

    /**
     * @brief 获取算法名称
     * @return 算法名称字符串
     */
    std::string getAlgorithmName() const override {
        return "Weighted Round Robin";
    }

protected:
    /**
     * @brief 服务器添加后更新权重信息
     * @param server 新添加的服务器
     */
    void onServerAdded(std::shared_ptr<Server> server) override {
        updateWeightInfo();
        // 初始化服务器的当前权重
        current_weights_[server->getId()] = 0;
    }

    /**
     * @brief 服务器移除后更新权重信息
     * @param server 被移除的服务器
     */
    void onServerRemoved(std::shared_ptr<Server> server) override {
        current_weights_.erase(server->getId());
        updateWeightInfo();
    }

    /**
     * @brief 重置内部状态
     */
    void onReset() override {
        current_index_ = 0;
        current_weights_.clear();
        for (const auto& server : servers_) {
            current_weights_[server->getId()] = 0;
        }
    }

private:
    /**
     * @brief 平滑加权轮询算法实现
     * @param available_servers 可用服务器列表
     * @return 选中的服务器
     */
    std::shared_ptr<Server> smoothWeightedRoundRobin(
        const std::vector<std::shared_ptr<Server>>& available_servers) {

        int total_weight = 0;
        std::shared_ptr<Server> selected = nullptr;
        int max_current_weight = 0;

        // 步骤1：增加每个服务器的当前权重
        // 步骤2：选择当前权重最大的服务器
        for (const auto& server : available_servers) {
            int weight = server->getWeight();
            total_weight += weight;

            // 增加当前权重
            current_weights_[server->getId()] += weight;
            int current_weight = current_weights_[server->getId()];

            // 选择当前权重最大的服务器
            if (selected == nullptr || current_weight > max_current_weight) {
                selected = server;
                max_current_weight = current_weight;
            }
        }

        // 步骤3：将选中服务器的当前权重减去总权重
        if (selected != nullptr) {
            current_weights_[selected->getId()] -= total_weight;
            selected->addConnection();
        }

        return selected;
    }

    /**
     * @brief 计算最大公约数
     * @param a 第一个数
     * @param b 第二个数
     * @return 最大公约数
     */
    int gcd(int a, int b) {
        while (b != 0) {
            int temp = b;
            b = a % b;
            a = temp;
        }
        return a;
    }

    /**
     * @brief 更新权重相关信息
     */
    void updateWeightInfo() {
        if (servers_.empty()) {
            gcd_ = 1;
            max_weight_ = 0;
            return;
        }

        // 计算所有权重的最大公约数和最大权重
        gcd_ = servers_[0]->getWeight();
        max_weight_ = servers_[0]->getWeight();

        for (size_t i = 1; i < servers_.size(); ++i) {
            int weight = servers_[i]->getWeight();
            gcd_ = gcd(gcd_, weight);
            if (weight > max_weight_) {
                max_weight_ = weight;
            }
        }
    }

private:
    std::atomic<size_t> current_index_;                     // 当前索引
    std::unordered_map<std::string, int> current_weights_;  // 每个服务器的当前权重
    int gcd_;                                               // 权重的最大公约数
    int max_weight_;                                        // 最大权重值
};

#endif // ROUNDROBINBALANCER_H