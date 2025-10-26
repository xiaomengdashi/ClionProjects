/**
 * @file main.cpp
 * @brief 负载均衡算法学习案例主程序
 * @details 演示各种负载均衡算法的使用和效果
 */

#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <vector>
#include <random>
#include <sstream>
#include <future>
#include <queue>
#include <condition_variable>

#include "server/Server.h"
#include "balancer/LoadBalancer.h"
#include "balancer/RoundRobinBalancer.h"
#include "balancer/ConsistentHashBalancer.h"
#include "balancer/LeastConnectionsBalancer.h"

// 控制台颜色输出
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"

/**
 * @brief 模拟客户端请求
 * @details 模拟处理请求并在一段时间后释放连接
 */
class RequestSimulator {
public:
    RequestSimulator() : gen_(std::random_device{}()), should_stop_(false) {
        // 启动工作线程池
        for (int i = 0; i < 4; ++i) {
            workers_.emplace_back(&RequestSimulator::workerThread, this);
        }
    }

    ~RequestSimulator() {
        // 停止所有工作线程
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            should_stop_ = true;
        }
        queue_cv_.notify_all();

        // 等待所有线程完成
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    /**
     * @brief 模拟单个请求
     * @param balancer 负载均衡器
     * @param request_id 请求ID
     * @param key 用于哈希的键值（可选）
     */
    void simulateRequest(std::shared_ptr<LoadBalancer> balancer,
                         int request_id,
                         const std::string& key = "") {
        // 选择服务器
        auto server = balancer->selectServer(key);

        if (server) {
            std::cout << COLOR_GREEN << "[请求 " << request_id << "] "
                      << "分配到服务器: " << server->getId()
                      << " (地址: " << server->getAddress()
                      << ", 当前连接数: " << server->getCurrentConnections()
                      << ")" << COLOR_RESET << std::endl;

            // 模拟请求处理时间（50-200ms）
            std::uniform_int_distribution<> dist(50, 200);
            int process_time = dist(gen_);

            // 将任务加入队列，由线程池处理
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                task_queue_.push({server, process_time, request_id});
            }
            queue_cv_.notify_one();
        } else {
            std::cout << COLOR_RED << "[请求 " << request_id << "] "
                      << "失败: 没有可用的服务器" << COLOR_RESET << std::endl;
        }
    }

    /**
     * @brief 批量模拟请求
     * @param balancer 负载均衡器
     * @param num_requests 请求数量
     * @param with_keys 是否使用键值（用于一致性哈希）
     * @param fixed_keys 是否使用固定键值（用于演示一致性哈希）
     */
    void simulateBatch(std::shared_ptr<LoadBalancer> balancer,
                       int num_requests,
                       bool with_keys = false,
                       bool fixed_keys = false) {
        for (int i = 1; i <= num_requests; ++i) {
            std::string key;
            if (with_keys) {
                if (fixed_keys) {
                    // 使用固定的用户ID (1-10) 来演示一致性
                    int user_id = ((i - 1) % 10) + 1;
                    key = "user_" + std::to_string(user_id);
                } else {
                    // 生成随机用户ID作为键值
                    std::uniform_int_distribution<> user_dist(1, 100);
                    key = "user_" + std::to_string(user_dist(gen_));
                }
            }
            simulateRequest(balancer, i, key);

            // 请求之间有短暂间隔（10-50ms）
            std::uniform_int_distribution<> interval_dist(10, 50);
            std::this_thread::sleep_for(
                std::chrono::milliseconds(interval_dist(gen_))
            );
        }
    }

    /**
     * @brief 等待所有请求完成
     */
    void waitForCompletion() {
        // 等待队列清空
        std::unique_lock<std::mutex> lock(queue_mutex_);
        empty_cv_.wait(lock, [this] {
            return task_queue_.empty() && active_tasks_ == 0;
        });
    }

private:
    struct Task {
        std::shared_ptr<Server> server;
        int process_time;
        int request_id;
    };

    void workerThread() {
        while (true) {
            Task task;
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                queue_cv_.wait(lock, [this] {
                    return !task_queue_.empty() || should_stop_;
                });

                if (should_stop_ && task_queue_.empty()) {
                    break;
                }

                if (!task_queue_.empty()) {
                    task = task_queue_.front();
                    task_queue_.pop();
                    active_tasks_++;
                }
            }

            // 处理任务
            if (task.server) {
                std::this_thread::sleep_for(std::chrono::milliseconds(task.process_time));
                task.server->removeConnection();

                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    active_tasks_--;
                    if (task_queue_.empty() && active_tasks_ == 0) {
                        empty_cv_.notify_all();
                    }
                }
            }
        }
    }

    std::mt19937 gen_;
    std::vector<std::thread> workers_;
    std::queue<Task> task_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::condition_variable empty_cv_;
    bool should_stop_;
    std::atomic<int> active_tasks_{0};
};

/**
 * @brief 打印分隔线
 * @param title 标题
 */
void printSeparator(const std::string& title = "") {
    std::cout << "\n" << COLOR_YELLOW;
    std::cout << "========================================";
    if (!title.empty()) {
        std::cout << " " << title << " ";
    }
    std::cout << "========================================";
    std::cout << COLOR_RESET << "\n" << std::endl;
}

/**
 * @brief 打印服务器统计信息
 * @param balancer 负载均衡器
 */
void printStatistics(std::shared_ptr<LoadBalancer> balancer) {
    auto servers = balancer->getServers();

    std::cout << COLOR_MAGENTA << "\n--- 服务器统计信息 ---" << COLOR_RESET << std::endl;
    std::cout << std::left << std::setw(15) << "服务器ID"
              << std::setw(20) << "地址"
              << std::setw(10) << "权重"
              << std::setw(15) << "当前连接数"
              << std::setw(15) << "总请求数"
              << std::setw(10) << "状态"
              << std::endl;

    std::cout << std::string(85, '-') << std::endl;

    for (const auto& server : servers) {
        std::cout << std::left
                  << std::setw(15) << server->getId()
                  << std::setw(20) << server->getAddress()
                  << std::setw(10) << server->getWeight()
                  << std::setw(15) << server->getCurrentConnections()
                  << std::setw(15) << server->getTotalRequests()
                  << std::setw(10) << (server->isAlive() ? "在线" : "离线")
                  << std::endl;
    }
}

/**
 * @brief 创建测试服务器
 * @return 服务器列表
 */
std::vector<std::shared_ptr<Server>> createTestServers() {
    std::vector<std::shared_ptr<Server>> servers;

    // 创建不同权重的服务器
    servers.push_back(std::make_shared<Server>("server-1", "192.168.1.10:8080", 1));
    servers.push_back(std::make_shared<Server>("server-2", "192.168.1.11:8080", 2));
    servers.push_back(std::make_shared<Server>("server-3", "192.168.1.12:8080", 3));
    servers.push_back(std::make_shared<Server>("server-4", "192.168.1.13:8080", 1));
    servers.push_back(std::make_shared<Server>("server-5", "192.168.1.14:8080", 2));

    return servers;
}

/**
 * @brief 测试轮询算法
 */
void testRoundRobin() {
    printSeparator("测试简单轮询算法 (Round Robin)");

    auto balancer = std::make_shared<RoundRobinBalancer>();
    auto servers = createTestServers();

    // 添加服务器
    for (const auto& server : servers) {
        balancer->addServer(server);
    }

    std::cout << COLOR_BLUE << "算法说明: " << COLOR_RESET
              << "按顺序依次将请求分配给每个服务器，不考虑服务器权重" << std::endl;
    std::cout << "服务器总数: " << balancer->getServerCount() << std::endl;

    // 模拟请求
    RequestSimulator simulator;
    simulator.simulateBatch(balancer, 10);

    // 等待所有请求处理完成
    simulator.waitForCompletion();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 打印统计信息
    printStatistics(balancer);
}

/**
 * @brief 测试加权轮询算法
 */
void testWeightedRoundRobin() {
    printSeparator("测试加权轮询算法 (Weighted Round Robin)");

    auto balancer = std::make_shared<WeightedRoundRobinBalancer>();
    auto servers = createTestServers();

    // 添加服务器
    for (const auto& server : servers) {
        balancer->addServer(server);
    }

    std::cout << COLOR_BLUE << "算法说明: " << COLOR_RESET
              << "根据服务器权重分配请求，权重高的服务器获得更多请求" << std::endl;
    std::cout << "服务器权重: ";
    for (const auto& server : servers) {
        std::cout << server->getId() << "(" << server->getWeight() << ") ";
    }
    std::cout << std::endl;

    // 模拟请求
    RequestSimulator simulator;
    simulator.simulateBatch(balancer, 20);

    // 等待所有请求处理完成
    simulator.waitForCompletion();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 打印统计信息
    printStatistics(balancer);
}

/**
 * @brief 测试一致性哈希算法
 */
void testConsistentHash() {
    printSeparator("测试一致性哈希算法 (Consistent Hash)");

    auto balancer = std::make_shared<ConsistentHashBalancer>(100);
    auto servers = createTestServers();

    // 添加前3个服务器
    for (int i = 0; i < 3; ++i) {
        balancer->addServer(servers[i]);
    }

    std::cout << COLOR_BLUE << "算法说明: " << COLOR_RESET
              << "使用哈希环将请求映射到服务器，相同的键总是路由到同一服务器" << std::endl;
    std::cout << "初始服务器数: " << balancer->getServerCount()
              << ", 虚拟节点总数: " << balancer->getHashRingSize() << std::endl;

    // 模拟请求
    RequestSimulator simulator;
    std::cout << "\n--- 第一批请求（使用固定用户ID作为键，演示一致性） ---" << std::endl;
    simulator.simulateBatch(balancer, 10, true, true);

    // 动态添加服务器
    std::cout << "\n" << COLOR_YELLOW << ">>> 动态添加新服务器: server-4"
              << COLOR_RESET << std::endl;
    balancer->addServer(servers[3]);

    std::cout << "\n--- 第二批请求（节点变化后，相同的键应该大部分保持不变） ---" << std::endl;
    simulator.simulateBatch(balancer, 10, true, true);

    // 等待所有请求处理完成
    simulator.waitForCompletion();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 打印统计信息
    printStatistics(balancer);
}

/**
 * @brief 测试最少连接数算法
 */
void testLeastConnections() {
    printSeparator("测试最少连接数算法 (Least Connections)");

    auto balancer = std::make_shared<LeastConnectionsBalancer>();
    auto servers = createTestServers();

    // 添加服务器
    for (const auto& server : servers) {
        balancer->addServer(server);
    }

    std::cout << COLOR_BLUE << "算法说明: " << COLOR_RESET
              << "选择当前活跃连接数最少的服务器处理新请求" << std::endl;

    // 给某些服务器预先添加一些连接
    servers[0]->addConnection();
    servers[0]->addConnection();
    servers[1]->addConnection();
    std::cout << "预设连接: server-1(2个), server-2(1个)" << std::endl;

    // 模拟请求
    RequestSimulator simulator;
    simulator.simulateBatch(balancer, 15);

    // 等待所有请求处理完成
    simulator.waitForCompletion();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 打印统计信息
    printStatistics(balancer);
}

/**
 * @brief 测试加权最少连接数算法
 */
void testWeightedLeastConnections() {
    printSeparator("测试加权最少连接数算法 (Weighted Least Connections)");

    auto balancer = std::make_shared<WeightedLeastConnectionsBalancer>();
    auto servers = createTestServers();

    // 添加服务器
    for (const auto& server : servers) {
        balancer->addServer(server);
    }

    std::cout << COLOR_BLUE << "算法说明: " << COLOR_RESET
              << "考虑服务器权重，选择(连接数/权重)比值最小的服务器" << std::endl;

    // 模拟请求
    RequestSimulator simulator;
    simulator.simulateBatch(balancer, 20);

    // 等待所有请求处理完成
    simulator.waitForCompletion();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 打印统计信息
    printStatistics(balancer);
}

/**
 * @brief 测试故障转移场景
 */
void testFailover() {
    printSeparator("测试故障转移场景");

    auto balancer = std::make_shared<WeightedRoundRobinBalancer>();
    auto servers = createTestServers();

    // 添加服务器
    for (const auto& server : servers) {
        balancer->addServer(server);
    }

    std::cout << "初始可用服务器数: " << balancer->getAvailableServerCount() << std::endl;

    // 创建请求模拟器（注意：需要在整个测试过程中使用同一个实例）
    RequestSimulator simulator;
    simulator.simulateBatch(balancer, 5);

    // 模拟服务器故障
    std::cout << "\n" << COLOR_RED << ">>> 模拟服务器故障: server-2 和 server-3 下线"
              << COLOR_RESET << std::endl;
    balancer->markServerDown("server-2");
    balancer->markServerDown("server-3");

    std::cout << "当前可用服务器数: " << balancer->getAvailableServerCount() << std::endl;

    // 继续请求
    std::cout << "\n--- 故障后的请求分配 ---" << std::endl;
    simulator.simulateBatch(balancer, 5);

    // 服务器恢复
    std::cout << "\n" << COLOR_GREEN << ">>> 服务器恢复: server-2 重新上线"
              << COLOR_RESET << std::endl;
    balancer->markServerUp("server-2");

    std::cout << "当前可用服务器数: " << balancer->getAvailableServerCount() << std::endl;

    // 恢复后的请求
    std::cout << "\n--- 恢复后的请求分配 ---" << std::endl;
    simulator.simulateBatch(balancer, 5);

    // 等待所有请求处理完成
    simulator.waitForCompletion();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 打印统计信息
    printStatistics(balancer);
}

/**
 * @brief 主函数
 */
int main() {
    std::cout << COLOR_CYAN;
    std::cout << "\n╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║           负载均衡算法学习案例 - C++11实现                    ║\n";
    std::cout << "║                                                              ║\n";
    std::cout << "║  演示内容:                                                   ║\n";
    std::cout << "║  1. 轮询算法 (Round Robin)                                  ║\n";
    std::cout << "║  2. 加权轮询算法 (Weighted Round Robin)                     ║\n";
    std::cout << "║  3. 一致性哈希算法 (Consistent Hash)                        ║\n";
    std::cout << "║  4. 最少连接数算法 (Least Connections)                      ║\n";
    std::cout << "║  5. 加权最少连接数算法 (Weighted Least Connections)         ║\n";
    std::cout << "║  6. 故障转移场景 (Failover)                                 ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
    std::cout << COLOR_RESET << std::endl;

    // 运行测试
    testRoundRobin();
    testWeightedRoundRobin();
    testConsistentHash();
    testLeastConnections();
    testWeightedLeastConnections();
    testFailover();

    printSeparator("测试完成");
    std::cout << COLOR_GREEN << "所有测试已完成！" << COLOR_RESET << std::endl;

    return 0;
}