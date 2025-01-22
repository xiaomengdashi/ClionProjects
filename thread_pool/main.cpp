#include <iostream>
#include "threadpool.h"

using FunctionPtr = void (*)(void);

int calc(int a, int b) {
    int c = a + b;

    std::this_thread::sleep_until(std::chrono::system_clock::now() + std::chrono::seconds(1));
    return c;
}



int main() {

    Threadpool pool;

//    for (int i = 0; i < 10; i++) {
//        pool.addTask(std::bind(calc, i, i+1));
//    }
    std::vector<std::future<int>> results;
    for (int i = 0; i < 10; i++) {
        results.emplace_back(pool.addTask(calc, i, i+1));
    }

    for (auto &result : results) {
        std::cout << "result: " << result.get() << std::endl;
    }

    getchar();

    return 0;
}
