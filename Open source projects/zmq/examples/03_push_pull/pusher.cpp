#include <zmq.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <string>

int main() {
    zmq::context_t context(1);
    zmq::socket_t pusher(context, zmq::socket_type::push);
    pusher.bind("tcp://*:5557");

    std::cout << "[Pusher] 已启动，开始发送任务" << std::endl;

    // 等待拉取者连接
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 发送100个任务
    for (int i = 1; i <= 100; i++) {
        std::string task = "Task-" + std::to_string(i);

        pusher.send(zmq::buffer(task), zmq::send_flags::none);
        std::cout << "[Pusher] 发送任务: " << task << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "[Pusher] 所有任务已发送" << std::endl;

    return 0;
}
