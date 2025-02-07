//
// Created by Kolane on 2025/1/10.
//

#include <iostream>
#include <mutex>
#include <thread>


#include "head.h"

std::mutex mtx1;
int shared_data = 100;

 void use_lock() {
     while (1) {
         std::lock_guard<std::mutex> lock(mtx1);
         shared_data++;
         std::cout << std::this_thread::get_id() << ": ";
         std::cout << shared_data << std::endl;
        std::this_thread::sleep_for(std::chrono::microseconds(10));
     }
 }

 void test_lock() {
     std::thread t1(use_lock);

     std::thread t2([]() {
         while (1) {
             mtx1.lock();
             shared_data--;
             std::cout << std::this_thread::get_id() << ": ";
             std::cout << shared_data << std::endl;
             std::this_thread::sleep_for(std::chrono::microseconds(20));
             mtx1.unlock();
         }
     });


     t1.join();
     t2.join();
 }