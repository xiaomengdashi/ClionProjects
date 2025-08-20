#include <iostream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <cstdio>

using namespace std;

std::mutex mtx;
std::condition_variable cond_var;
bool flag = false;
int print_count = 0;
const int MAX_PRINTS = 5; // 限制打印次数以便测试

void PrintA () {
    while (print_count < MAX_PRINTS) {
        unique_lock<std::mutex> lck(mtx);
        cond_var.wait(lck, []
        {
            return flag;
        });

        if (print_count < MAX_PRINTS) {
            printf("A\n");
            print_count++;
            flag = false;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            cond_var.notify_one();
        }
    }
}

void PrintB () {
    while (print_count < MAX_PRINTS)
    {
        unique_lock<std::mutex> lck(mtx);
        cond_var.wait(lck, []
        {
            return !flag;
        });

        if (print_count < MAX_PRINTS) {
            printf("B\n");
            print_count++;
            flag = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            cond_var.notify_one();
        }
    }
}

void test_lock() {
    std::cout << "=======test_lock========" << std::endl;
    
    thread t1([]() {
        PrintA();
    });

    thread t2([]() {
        PrintB();
    });

    // 启动第一个线程
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    {
        unique_lock<std::mutex> lck(mtx);
        flag = true;
        cond_var.notify_one();
    }

    t1.join();
    t2.join();
    
    std::cout << "Lock test completed" << std::endl;
}

int main() {
    test_lock();
    return 0;
}