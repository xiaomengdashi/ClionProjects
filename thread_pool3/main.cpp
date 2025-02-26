//
// Created by Kolane on 2025/2/27.
//

// main.cpp
#include <iostream>
#include <thread>
#include <mutex>
#include "thread_pool.hpp"


int main() {
    std::cout << "start" << std::endl;
    //获取线程池
    auto& pool = get_thread_pool(8);
    //向线程池中添加100个任务
    for (int i = 0; i < 100; i++) {
        // 第56个任务调高其优先级
        if (i == 56) {
            pool.add_task_unsafe([=] {
                {	//休眠一秒模拟任务耗时
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    std::cout << i << std::endl;
                }
            }, 3);
            continue;
        }
        pool.add_task_unsafe([=] {
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                std::cout << i << std::endl;
            }
        });
    }
    // 启动线程池
    pool.start();
    //阻塞8秒后优雅停机
    std::this_thread::sleep_for(std::chrono::seconds(8));
    pool.force_stop_gracefully();
    return 0;
}