#include <iostream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>

using namespace std;

std::mutex mtx;
std::condition_variable cond_var;
bool flag = false;


void PrintA () {
    while (1) {
        unique_lock<std::mutex> lck(mtx);
        cond_var.wait(lck, []
        {
            return flag;
        });

        printf("A\n");
        flag = false;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        cond_var.notify_one();
    }
}

void PrintB () {
    while (1)
    {
        unique_lock<std::mutex> lck(mtx);
        cond_var.wait(lck, []
        {
            return !flag;
        });

        printf("B\n");
        flag = true;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        cond_var.notify_one();
    }
}

void test_lock() {
    thread t1([]() {
        PrintA();
    });

    thread t2([]() {
        PrintB();
    });

    t1.join();
    t2.join();
}