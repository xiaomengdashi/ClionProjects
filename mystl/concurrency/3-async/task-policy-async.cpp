#include <future>
#include <iostream>
#include <future>
#include <vector>
#include <numeric>
#include <chrono>
#include <thread>
#include <map>
#include <random>


float Compute(std::vector<float> &v) {
    std::this_thread::sleep_for(std::chrono::seconds(2)); // simulate work
    std::cout << "执行任务的线程Id:" << std::this_thread::get_id() << std::endl;
    auto r = std::accumulate(v.begin(), v.end(), 0.0f);
    return r;
}

int main() {
    const int RN_MAX = 10000;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, RN_MAX);
    std::vector<float> numbers(100, 0.0f);
    std::generate(numbers.begin(), numbers.end(), [&]() { return float(dist(gen)) / RN_MAX; });
    
    std::map<std::string, std::launch> policies = {
        {"默认策略", (std::launch)0},
        {"异步执行", std::launch::async},
        {"延迟执行", std::launch::deferred}
    };

    // iterate through each policy and measure
    for (auto &p : policies) {
        std::cout << "=== 策略: " << p.first << " ===" << std::endl;
        auto start = std::chrono::steady_clock::now();
        std::cout << "主线程Id:" << std::this_thread::get_id() << std::endl;

        std::future<float> result;
        if(int(p.second) == 0) {
            // default policy: let the implementation decide
            result = std::async(Compute, std::ref(numbers));
        } else {
            result = std::async(p.second, Compute, std::ref(numbers));
        }
        // simulate other work
        std::this_thread::sleep_for(std::chrono::seconds(2));
        float r = result.get();
        std::cout << "结果: " << r << std::endl;

        auto end = std::chrono::steady_clock::now();
        std::cout << "总耗时: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;
        std::cout << std::endl;
    }

    return 0;
}

