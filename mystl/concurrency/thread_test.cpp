#include <iostream>
#include <chrono>
#include <thread>

void thread_test() {
   auto s =  std::this_thread::get_id();
    std::cout << "thread id: " << s << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::this_thread::sleep_until(std::chrono::system_clock::now() + std::chrono::seconds(1));

    std::this_thread::yield();

    std::thread t([=](int a, int b) {
        std::cout << s << std::endl;
        std::cout << "id: " <<std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));

    }, 1, 2);

    std::cout << t.joinable() << std::endl;
    t.join();
    std::cout << t.joinable();

    std::cout << std::thread::hardware_concurrency() << std::endl;



}

int main() {
    std::cout << "=======Thread Test=======" << std::endl;
    thread_test();
    return 0;
}