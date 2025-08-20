#include <iostream>
#include <chrono>
#include <thread>

using std::chrono::system_clock;

void clock_test() {
    std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
    auto s = start.time_since_epoch();

    std::cout << "s:" << s.count() << std::endl;
    std::chrono::system_clock::time_point end = std::chrono::system_clock::now();
    end.time_since_epoch();
    std::chrono::duration<double> elapsed_seconds = end - start;

    std::cout << "elapsed time: " << elapsed_seconds.count() << "s\n";
    system_clock::time_point epoch;

    std::chrono::hours h(10 *20);

    system_clock::time_point tp = epoch + h;


    std::chrono::time_point<system_clock, system_clock::duration> a = system_clock::now();

    std::cout << "a:" << a.time_since_epoch().count()<< std::endl;

    // 获取当前时间，并格式化成YY-MM-DD HH:MM:SS 格式

    std::time_t now = std::chrono::system_clock::to_time_t(a);

     auto b = std::ctime(&now);
    std::cout << "b:" << b << std::endl;



}

void duration_cast_test() {
    std::chrono::seconds sec(3600);
    std::chrono::hours ms = std::chrono::duration_cast<std::chrono::hours>(sec);
    std::cout << "ms:" << ms.count() << std::endl;
}

template <typename T>
using time_point = std::chrono::time_point<std::chrono::system_clock, T>;

void time_point_cast_test() {


    std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();
    std::chrono::time_point<std::chrono::system_clock, std::chrono::hours> tp2 = std::chrono::time_point_cast<std::chrono::hours>(tp);
    std::cout << "tp2:" << tp2.time_since_epoch().count() << std::endl;
}

int main() {
    std::cout << "=======Time Utils Tests=======" << std::endl;
    
    std::cout << "\n--- Clock Test ---" << std::endl;
    clock_test();
    
    std::cout << "\n--- Duration Cast Test ---" << std::endl;
    duration_cast_test();
    
    std::cout << "\n--- Time Point Cast Test ---" << std::endl;
    time_point_cast_test();
    
    return 0;
}