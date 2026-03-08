#include <iostream>
#include <future>
#include <iomanip>


void compute(const long num_setp, std::promise<double>&& promise) {
    double step = 1.0 / num_setp;
    double result = 0.0;
    for (long i = 0; i < num_setp; ++i) {
        double x = (i + 0.5) * step;
        result += 4.0 / (1.0 + x * x);
    }

    promise.set_value(result * step);
}

void display(std::future<double>&& receiver) {
    double result = receiver.get();
    std::cout << std::fixed << std::setprecision(12);
    std::cout << "计算结果: " << result << std::endl;
}


int main() {
    const long num_steps = 10000000000;
    std::promise<double> promise;
    std::future<double> future = promise.get_future();

    std::thread producer(compute, num_steps, std::move(promise));
    std::thread consumer(display, std::move(future));

    producer.join();
    consumer.join();

    return 0;
}