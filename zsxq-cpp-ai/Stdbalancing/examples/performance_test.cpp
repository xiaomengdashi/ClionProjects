/**
 * @file performance_test.cpp
 * @brief 负载均衡算法性能测试程序
 * @details 测试各种算法在高并发场景下的性能表现
 */

#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <map>

#include "server/Server.h"
#include "balancer/LoadBalancer.h"
#include "balancer/RoundRobinBalancer.h"
#include "balancer/ConsistentHashBalancer.h"
#include "balancer/LeastConnectionsBalancer.h"

/**
 * @brief 性能测试结果统计
 */
struct TestResult {
    double average_time_ms;     // 平均响应时间（毫秒）
    double throughput;           // 吞吐量（请求/秒）
    double std_deviation;        // 标准差
    std::map<std::string, int> distribution;  // 请求分布
};

/**
 * @class PerformanceTester
 * @brief 性能测试器
 */
class PerformanceTester {
public:
    /**
     * @brief 运行性能测试
     * @param balancer 负载均衡器
     * @param num_requests 请求总数
     * @param num_threads 并发线程数
     * @param with_keys 是否使用键值（一致性哈希）
     * @return 测试结果
     */
    TestResult runTest(std::shared_ptr<LoadBalancer> balancer,
                       int num_requests,
                       int num_threads,
                       bool with_keys = false) {
        TestResult result;
        std::atomic<int> completed_requests(0);
        std::vector<double> response_times;
        response_times.reserve(num_requests);
        std::mutex times_mutex;

        // 创建服务器
        createServers(balancer, 10);  // 创建10个服务器

        // 开始计时
        auto start = std::chrono::high_resolution_clock::now();

        // 创建并发线程
        std::vector<std::thread> threads;
        int requests_per_thread = num_requests / num_threads;

        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> user_dist(1, 1000);

                for (int j = 0; j < requests_per_thread; ++j) {
                    auto req_start = std::chrono::high_resolution_clock::now();

                    // 选择服务器
                    std::string key;
                    if (with_keys) {
                        key = "user_" + std::to_string(user_dist(gen));
                    }

                    auto server = balancer->selectServer(key);
                    if (server) {
                        // 模拟处理延迟（1-5ms）
                        std::this_thread::sleep_for(
                            std::chrono::microseconds(1000 + j % 4000)
                        );
                        server->removeConnection();
                    }

                    auto req_end = std::chrono::high_resolution_clock::now();
                    double time_ms = std::chrono::duration<double, std::milli>(
                        req_end - req_start).count();

                    // 记录响应时间
                    {
                        std::lock_guard<std::mutex> lock(times_mutex);
                        response_times.push_back(time_ms);
                    }

                    completed_requests++;
                }
            });
        }

        // 等待所有线程完成
        for (auto& t : threads) {
            t.join();
        }

        // 结束计时
        auto end = std::chrono::high_resolution_clock::now();
        double total_time_s = std::chrono::duration<double>(end - start).count();

        // 计算统计数据
        result.throughput = num_requests / total_time_s;

        // 计算平均响应时间
        double sum = 0;
        for (double time : response_times) {
            sum += time;
        }
        result.average_time_ms = sum / response_times.size();

        // 计算标准差
        double sq_sum = 0;
        for (double time : response_times) {
            sq_sum += (time - result.average_time_ms) * (time - result.average_time_ms);
        }
        result.std_deviation = std::sqrt(sq_sum / response_times.size());

        // 统计请求分布
        for (auto& server : balancer->getServers()) {
            result.distribution[server->getId()] = server->getTotalRequests();
        }

        return result;
    }

    /**
     * @brief 打印测试结果
     * @param algorithm_name 算法名称
     * @param result 测试结果
     */
    void printResult(const std::string& algorithm_name, const TestResult& result) {
        std::cout << "\n算法: " << algorithm_name << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "平均响应时间: " << result.average_time_ms << " ms" << std::endl;
        std::cout << "标准差: " << result.std_deviation << " ms" << std::endl;
        std::cout << "吞吐量: " << result.throughput << " req/s" << std::endl;

        // 打印请求分布
        std::cout << "\n请求分布:" << std::endl;
        int total = 0;
        for (const auto& pair : result.distribution) {
            total += pair.second;
        }
        for (const auto& pair : result.distribution) {
            double percentage = (pair.second * 100.0) / total;
            std::cout << "  " << pair.first << ": "
                      << pair.second << " ("
                      << std::fixed << std::setprecision(1)
                      << percentage << "%)" << std::endl;
        }

        // 计算负载均衡度（使用变异系数）
        double mean = static_cast<double>(total) / result.distribution.size();
        double variance = 0;
        for (const auto& pair : result.distribution) {
            variance += std::pow(pair.second - mean, 2);
        }
        variance /= result.distribution.size();
        double cv = std::sqrt(variance) / mean * 100;  // 变异系数

        std::cout << "\n负载均衡度 (变异系数): "
                  << std::fixed << std::setprecision(2)
                  << cv << "% (越小越均衡)" << std::endl;
    }

private:
    /**
     * @brief 创建测试服务器
     * @param balancer 负载均衡器
     * @param count 服务器数量
     */
    void createServers(std::shared_ptr<LoadBalancer> balancer, int count) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> weight_dist(1, 5);

        for (int i = 1; i <= count; ++i) {
            std::string id = "server-" + std::to_string(i);
            std::string address = "192.168.1." + std::to_string(10 + i) + ":8080";
            int weight = weight_dist(gen);
            auto server = std::make_shared<Server>(id, address, weight);
            balancer->addServer(server);
        }
    }
};

/**
 * @brief 主函数
 */
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "      负载均衡算法性能测试" << std::endl;
    std::cout << "========================================" << std::endl;

    PerformanceTester tester;

    // 测试参数
    const int NUM_REQUESTS = 10000;
    const int NUM_THREADS = 10;

    std::cout << "\n测试参数:" << std::endl;
    std::cout << "  总请求数: " << NUM_REQUESTS << std::endl;
    std::cout << "  并发线程数: " << NUM_THREADS << std::endl;
    std::cout << "  服务器数: 10" << std::endl;

    // 测试轮询算法
    {
        auto balancer = std::make_shared<RoundRobinBalancer>();
        std::cout << "\n正在测试轮询算法..." << std::endl;
        auto result = tester.runTest(balancer, NUM_REQUESTS, NUM_THREADS);
        tester.printResult("轮询算法 (Round Robin)", result);
    }

    // 测试加权轮询算法
    {
        auto balancer = std::make_shared<WeightedRoundRobinBalancer>();
        std::cout << "\n正在测试加权轮询算法..." << std::endl;
        auto result = tester.runTest(balancer, NUM_REQUESTS, NUM_THREADS);
        tester.printResult("加权轮询算法 (Weighted Round Robin)", result);
    }

    // 测试一致性哈希算法
    {
        auto balancer = std::make_shared<ConsistentHashBalancer>(150);
        std::cout << "\n正在测试一致性哈希算法..." << std::endl;
        auto result = tester.runTest(balancer, NUM_REQUESTS, NUM_THREADS, true);
        tester.printResult("一致性哈希算法 (Consistent Hash)", result);
    }

    // 测试最少连接数算法
    {
        auto balancer = std::make_shared<LeastConnectionsBalancer>();
        std::cout << "\n正在测试最少连接数算法..." << std::endl;
        auto result = tester.runTest(balancer, NUM_REQUESTS, NUM_THREADS);
        tester.printResult("最少连接数算法 (Least Connections)", result);
    }

    // 测试加权最少连接数算法
    {
        auto balancer = std::make_shared<WeightedLeastConnectionsBalancer>();
        std::cout << "\n正在测试加权最少连接数算法..." << std::endl;
        auto result = tester.runTest(balancer, NUM_REQUESTS, NUM_THREADS);
        tester.printResult("加权最少连接数算法 (Weighted Least Connections)", result);
    }

    std::cout << "\n========================================" << std::endl;
    std::cout << "          性能测试完成" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}