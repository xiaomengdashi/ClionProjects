#include <future>
#include <iostream>
#include <future>
#include <vector>
#include <numeric>
#include <chrono>
#include <thread>
#include <syncstream>
#include <random>


float Compute(std::vector<float> &v) {
    std::this_thread::sleep_for(std::chrono::seconds(2)); // simulate work
    std::osyncstream(std::cout) << "执行任务的线程Id:" << std::this_thread::get_id() << std::endl;
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
    
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    std::cout << "主线程Id:" << std::this_thread::get_id() << std::endl;
    std::future<float> result = std::async(std::launch::async, Compute, std::ref(numbers));

    std::this_thread::sleep_for(std::chrono::seconds(2));
    auto r = result.get();
    std::cout <<"结果: " << r << std::endl;
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    std::cout << "总耗时: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;

    return 0;
}

