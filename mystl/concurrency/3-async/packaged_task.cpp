#include <future>
#include <iostream>
#include <future>
#include <vector>
#include <numeric>
#include <chrono>
#include <thread>
#include <random>


float Compute(std::vector<float> &v) {
    std::this_thread::sleep_for(std::chrono::seconds(2));
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
    
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    std::cout << "主线程Id:" << std::this_thread::get_id() << std::endl;
    
    std::packaged_task<float(std::vector<float>&)> task(Compute);

    std::cout << "**********直接调用**********" << std::endl;
    std::future<float> result1 = task.get_future();
    task(numbers);
    std::cout << "结果: " << result1.get() << std::endl;

    task.reset(); // reset the task to reuse it

    std::cout << "**********异步调用**********" << std::endl;
    result1 = task.get_future();
    std::thread t(std::move(task), std::ref(numbers));
    std::this_thread::sleep_for(std::chrono::seconds(2)); // simulate other work
    std::cout << "结果: " << result1.get() << std::endl;
    t.join();

    return 0;
}

