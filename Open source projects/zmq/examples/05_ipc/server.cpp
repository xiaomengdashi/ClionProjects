#include <zmq.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <string>

int main() {
    zmq::context_t ctx(1);
    zmq::socket_t socket(ctx, zmq::socket_type::rep);

    // 使用IPC传输（同一台机器上的进程间通信）
    // 比TCP快，但只能用于本地通信
    socket.bind("ipc:///tmp/zmq_ipc.sock");

    std::cout << "[IPC Server] 已绑定到 ipc:///tmp/zmq_ipc.sock" << std::endl;
    std::cout << "[IPC Server] 等待客户端请求..." << std::endl;

    while (true) {
        zmq::message_t request;
        auto result = socket.recv(request, zmq::recv_flags::none);

        if (!result) break;

        std::string msg(static_cast<char*>(request.data()), request.size());
        std::cout << "[IPC Server] 收到请求: " << msg << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        std::string reply = "IPC应答: " + msg;
        socket.send(zmq::buffer(reply), zmq::send_flags::none);
        std::cout << "[IPC Server] 发送应答: " << reply << std::endl;
    }

    return 0;
}
