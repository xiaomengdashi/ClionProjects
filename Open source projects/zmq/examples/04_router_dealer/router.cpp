#include <zmq.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <map>
#include <string>

int main() {
    zmq::context_t context(1);
    zmq::socket_t router(context, zmq::socket_type::router);
    router.bind("tcp://*:5558");

    std::cout << "[Router] 已启动，等待客户端连接..." << std::endl;

    std::map<std::string, int> client_requests;
    int request_count = 0;

    while (request_count < 20) {
        // 接收客户端标识
        zmq::message_t client_id_msg;
        auto result = router.recv(client_id_msg, zmq::recv_flags::dontwait);
        if (!result) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        std::string client_id = client_id_msg.to_string();

        // 接收分隔符
        zmq::message_t delimiter;
        router.recv(delimiter, zmq::recv_flags::none);

        // 接收请求内容
        zmq::message_t request_msg;
        router.recv(request_msg, zmq::recv_flags::none);
        std::string request_str = request_msg.to_string();

        client_requests[client_id]++;
        request_count++;

        std::cout << "[Router] 收到来自 Client-" << client_id << " 的请求: "
                  << request_str << " (第" << client_requests[client_id] << "个)" << std::endl;

        // 发送响应
        std::string response = "已处理: " + request_str;

        router.send(zmq::buffer(client_id), zmq::send_flags::sndmore);
        router.send(zmq::buffer(""), zmq::send_flags::sndmore);
        router.send(zmq::buffer(response), zmq::send_flags::none);

        std::cout << "[Router] 发送响应给 Client-" << client_id << ": " << response << std::endl;
    }

    std::cout << "[Router] 已处理20个请求，统计信息:" << std::endl;
    for (const auto& pair : client_requests) {
        std::cout << "  Client-" << pair.first << ": " << pair.second << "个请求" << std::endl;
    }

    return 0;
}
