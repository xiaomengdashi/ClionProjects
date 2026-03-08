//
// Created by Kolane on 2025/1/10.
//

#include <iostream>
#include <mutex>
#include <thread>
#include <chrono>

std::mutex mtx1;
int shared_data = 100;
int operation_count = 0;
const int MAX_OPERATIONS = 10; // 限制操作次数以便测试

void use_lock() {
    while (operation_count < MAX_OPERATIONS) {
        std::lock_guard<std::mutex> lock(mtx1);
        if (operation_count < MAX_OPERATIONS) {
            shared_data++;
            operation_count++;
            std::cout << std::this_thread::get_id() << ": ";
            std::cout << shared_data << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void use_manual_lock() {
    while (operation_count < MAX_OPERATIONS) {
        mtx1.lock();
        if (operation_count < MAX_OPERATIONS) {
            shared_data--;
            operation_count++;
            std::cout << std::this_thread::get_id() << ": ";
            std::cout << shared_data << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        mtx1.unlock();
    }
}

void test_mutex() {
    std::cout << "=======test_mutex========" << std::endl;
    std::cout << "Initial shared_data: " << shared_data << std::endl;
    
    std::thread t1(use_lock);
    std::thread t2(use_manual_lock);

    t1.join();
    t2.join();
    
    std::cout << "Final shared_data: " << shared_data << std::endl;
    std::cout << "Total operations: " << operation_count << std::endl;
}

int main() {
    test_mutex();
    return 0;
}