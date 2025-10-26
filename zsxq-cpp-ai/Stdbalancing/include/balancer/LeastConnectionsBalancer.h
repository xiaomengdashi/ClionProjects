/**
 * @file LeastConnectionsBalancer.h
 * @brief 最少连接数负载均衡算法实现
 * @details 选择当前连接数最少的服务器，支持加权最少连接数
 */

#ifndef LEASTCONNECTIONSBALANCER_H
#define LEASTCONNECTIONSBALANCER_H

#include "balancer/LoadBalancer.h"
#include <algorithm>
#include <limits>
#include <random>

/**
 * @class LeastConnectionsBalancer
 * @brief 最少连接数负载均衡器
 * @details 选择当前活跃连接数最少的服务器处理新请求
 */
class LeastConnectionsBalancer : public LoadBalancer {
public:
    /**
     * @brief 构造函数
     */
    LeastConnectionsBalancer() : gen_(std::random_device{}()) {}

    /**
     * @brief 选择连接数最少的服务器
     * @param key 未使用（最少连接算法不需要键值）
     * @return 选中的服务器
     */
    std::shared_ptr<Server> selectServer(const std::string&) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (servers_.empty()) {
            return nullptr;
        }

        // 收集所有可用的服务器
        std::vector<std::shared_ptr<Server>> available_servers;
        for (const auto& server : servers_) {
            if (server->isAlive()) {
                available_servers.push_back(server);
            }
        }

        if (available_servers.empty()) {
            return nullptr;
        }

        // 找出最少连接数
        int min_connections = std::numeric_limits<int>::max();
        for (const auto& server : available_servers) {
            int connections = server->getCurrentConnections();
            if (connections < min_connections) {
                min_connections = connections;
            }
        }

        // 收集所有具有最少连接数的服务器
        std::vector<std::shared_ptr<Server>> min_connection_servers;
        for (const auto& server : available_servers) {
            if (server->getCurrentConnections() == min_connections) {
                min_connection_servers.push_back(server);
            }
        }

        // 如果有多个服务器具有相同的最少连接数，随机选择一个
        std::shared_ptr<Server> selected = nullptr;
        if (!min_connection_servers.empty()) {
            if (min_connection_servers.size() == 1) {
                selected = min_connection_servers[0];
            } else {
                // 随机选择一个
                std::uniform_int_distribution<size_t> dist(0, min_connection_servers.size() - 1);
                selected = min_connection_servers[dist(gen_)];
            }
        }

        if (selected) {
            selected->addConnection();
        }

        return selected;
    }

    /**
     * @brief 获取算法名称
     * @return 算法名称字符串
     */
    std::string getAlgorithmName() const override {
        return "Least Connections";
    }

private:
    std::mt19937 gen_;  // 随机数生成器
};

/**
 * @class WeightedLeastConnectionsBalancer
 * @brief 加权最少连接数负载均衡器
 * @details 考虑服务器权重的最少连接数算法，选择连接数与权重比值最小的服务器
 */
class WeightedLeastConnectionsBalancer : public LoadBalancer {
public:
    /**
     * @brief 构造函数
     */
    WeightedLeastConnectionsBalancer() : gen_(std::random_device{}()) {}

    /**
     * @brief 选择加权后连接数最少的服务器
     * @param key 未使用（最少连接算法不需要键值）
     * @return 选中的服务器
     */
    std::shared_ptr<Server> selectServer(const std::string&) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (servers_.empty()) {
            return nullptr;
        }

        // 收集所有可用的服务器
        std::vector<std::shared_ptr<Server>> available_servers;
        for (const auto& server : servers_) {
            if (server->isAlive()) {
                available_servers.push_back(server);
            }
        }

        if (available_servers.empty()) {
            return nullptr;
        }

        // 计算每个服务器的加权连接比率（连接数/权重）
        std::vector<std::pair<std::shared_ptr<Server>, double>> weighted_connections;
        for (const auto& server : available_servers) {
            double weight = static_cast<double>(server->getWeight());
            if (weight <= 0) weight = 1.0;  // 防止除零

            double ratio = static_cast<double>(server->getCurrentConnections()) / weight;
            weighted_connections.push_back({server, ratio});
        }

        // 找出最小的加权连接比率
        double min_ratio = std::numeric_limits<double>::max();
        for (const auto& pair : weighted_connections) {
            if (pair.second < min_ratio) {
                min_ratio = pair.second;
            }
        }

        // 收集所有具有最小比率的服务器
        std::vector<std::shared_ptr<Server>> min_ratio_servers;
        const double epsilon = 1e-9;  // 浮点数比较的误差容忍度
        for (const auto& pair : weighted_connections) {
            if (std::abs(pair.second - min_ratio) < epsilon) {
                min_ratio_servers.push_back(pair.first);
            }
        }

        // 如果有多个服务器具有相同的最小比率，选择权重最大的
        std::shared_ptr<Server> selected = nullptr;
        if (!min_ratio_servers.empty()) {
            if (min_ratio_servers.size() == 1) {
                selected = min_ratio_servers[0];
            } else {
                // 选择权重最大的服务器
                selected = *std::max_element(
                    min_ratio_servers.begin(), min_ratio_servers.end(),
                    [](const std::shared_ptr<Server>& a, const std::shared_ptr<Server>& b) {
                        return a->getWeight() < b->getWeight();
                    }
                );
            }
        }

        if (selected) {
            selected->addConnection();
        }

        return selected;
    }

    /**
     * @brief 获取算法名称
     * @return 算法名称字符串
     */
    std::string getAlgorithmName() const override {
        return "Weighted Least Connections";
    }

    /**
     * @brief 获取服务器的负载因子
     * @param server 服务器指针
     * @return 负载因子（连接数/权重）
     */
    double getLoadFactor(const std::shared_ptr<Server>& server) const {
        double weight = static_cast<double>(server->getWeight());
        if (weight <= 0) weight = 1.0;
        return static_cast<double>(server->getCurrentConnections()) / weight;
    }

private:
    std::mt19937 gen_;  // 随机数生成器
};

/**
 * @class DynamicLeastConnectionsBalancer
 * @brief 动态最少连接数负载均衡器
 * @details 根据服务器的响应时间和当前连接数动态调整选择策略
 */
class DynamicLeastConnectionsBalancer : public LoadBalancer {
public:
    /**
     * @brief 构造函数
     * @param response_weight 响应时间在计算中的权重系数，默认0.3
     */
    explicit DynamicLeastConnectionsBalancer(double response_weight = 0.3)
        : response_weight_(response_weight)
        , gen_(std::random_device{}()) {
        if (response_weight_ < 0.0) response_weight_ = 0.0;
        if (response_weight_ > 1.0) response_weight_ = 1.0;
    }

    /**
     * @brief 选择最优服务器
     * @param key 未使用
     * @return 选中的服务器
     */
    std::shared_ptr<Server> selectServer(const std::string&) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (servers_.empty()) {
            return nullptr;
        }

        // 收集所有可用的服务器
        std::vector<std::shared_ptr<Server>> available_servers;
        for (const auto& server : servers_) {
            if (server->isAlive()) {
                available_servers.push_back(server);
            }
        }

        if (available_servers.empty()) {
            return nullptr;
        }

        // 计算每个服务器的动态分数
        std::vector<std::pair<std::shared_ptr<Server>, double>> server_scores;
        for (const auto& server : available_servers) {
            double score = calculateDynamicScore(server);
            server_scores.push_back({server, score});
        }

        // 选择分数最低的服务器（分数越低越好）
        auto selected_pair = std::min_element(
            server_scores.begin(), server_scores.end(),
            [](const std::pair<std::shared_ptr<Server>, double>& a,
               const std::pair<std::shared_ptr<Server>, double>& b) {
                return a.second < b.second;
            }
        );

        std::shared_ptr<Server> selected = nullptr;
        if (selected_pair != server_scores.end()) {
            selected = selected_pair->first;
            selected->addConnection();

            // 记录选择时间（用于计算响应时间）
            server_selection_times_[selected->getId()] =
                std::chrono::steady_clock::now();
        }

        return selected;
    }

    /**
     * @brief 记录服务器响应
     * @param server_id 服务器ID
     * @param success 请求是否成功
     */
    void recordResponse(const std::string& server_id, bool success) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = server_map_.find(server_id);
        if (it != server_map_.end()) {
            auto server = it->second;
            server->removeConnection();

            if (!success) {
                server->recordFailure();
            }

            // 更新平均响应时间
            auto time_it = server_selection_times_.find(server_id);
            if (time_it != server_selection_times_.end()) {
                auto response_time = std::chrono::steady_clock::now() - time_it->second;
                updateAverageResponseTime(server_id, response_time);
                server_selection_times_.erase(time_it);
            }
        }
    }

    /**
     * @brief 获取算法名称
     * @return 算法名称字符串
     */
    std::string getAlgorithmName() const override {
        return "Dynamic Least Connections";
    }

protected:
    /**
     * @brief 重置内部状态
     */
    void onReset() override {
        average_response_times_.clear();
        server_selection_times_.clear();
    }

private:
    /**
     * @brief 计算服务器的动态分数
     * @param server 服务器指针
     * @return 动态分数（越低越好）
     */
    double calculateDynamicScore(const std::shared_ptr<Server>& server) {
        // 基础分数：当前连接数与权重的比率
        double weight = static_cast<double>(server->getWeight());
        if (weight <= 0) weight = 1.0;
        double connection_score = static_cast<double>(server->getCurrentConnections()) / weight;

        // 响应时间分数
        double response_score = 0.0;
        auto it = average_response_times_.find(server->getId());
        if (it != average_response_times_.end()) {
            response_score = it->second.count() / 1000.0;  // 转换为秒
        }

        // 失败率分数
        double failure_rate = 0.0;
        if (server->getTotalRequests() > 0) {
            failure_rate = static_cast<double>(server->getFailedRequests()) /
                          static_cast<double>(server->getTotalRequests());
        }

        // 综合分数计算
        double score = (1.0 - response_weight_) * connection_score +
                      response_weight_ * response_score +
                      failure_rate * 10.0;  // 失败率权重较高

        return score;
    }

    /**
     * @brief 更新服务器的平均响应时间
     * @param server_id 服务器ID
     * @param response_time 本次响应时间
     */
    void updateAverageResponseTime(const std::string& server_id,
                                   std::chrono::steady_clock::duration response_time) {
        const double alpha = 0.2;  // 指数移动平均系数
        auto& avg_time = average_response_times_[server_id];

        if (avg_time.count() == 0) {
            // 第一次记录
            avg_time = response_time;
        } else {
            // 指数移动平均
            avg_time = std::chrono::steady_clock::duration(
                static_cast<long long>(
                    alpha * response_time.count() +
                    (1.0 - alpha) * avg_time.count()
                )
            );
        }
    }

private:
    double response_weight_;  // 响应时间在评分中的权重
    std::mt19937 gen_;       // 随机数生成器
    std::unordered_map<std::string, std::chrono::steady_clock::duration> average_response_times_;  // 平均响应时间
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> server_selection_times_;  // 选择时间记录
};

#endif // LEASTCONNECTIONSBALANCER_H