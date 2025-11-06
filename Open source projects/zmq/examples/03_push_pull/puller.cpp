#include <zmq.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <cstdlib>
#include <ctime>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "用法: " << argv[0] << " <worker_id>" << std::endl;
        return 1;
    }

    std::string worker_id = argv[1];
    srand(time(NULL));

    zmq::context_t context(1);
    zmq::socket_t puller(context, zmq::socket_type::pull);
    puller.connect("tcp://localhost:5557");

    std::cout << "[Worker-" << worker_id << "] 已连接，等待任务..." << std::endl;

    int task_count = 0;
    while (task_count < 20) {
        zmq::message_t message;
        auto result = puller.recv(message, zmq::recv_flags::none);
        if (!result) break;

        std::string task = message.to_string();
        std::cout << "[Worker-" << worker_id << "] 收到: " << task;

        // 模拟处理任务（随机延迟100-500ms）
        int process_time = 100 + (rand() % 400);
        std::this_thread::sleep_for(std::chrono::milliseconds(process_time));

        std::cout << " (耗时" << process_time << "ms)" << std::endl;

        task_count++;
    }

    std::cout << "[Worker-" << worker_id << "] 已处理" << task_count << "个任务，退出" << std::endl;

    return 0;
}
