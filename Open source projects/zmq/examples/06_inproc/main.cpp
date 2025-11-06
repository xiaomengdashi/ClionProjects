#include <zmq.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <memory>

// INPROC传输：同一进程内线程间通信
// 这是ZeroMQ中最快的传输方式，零拷贝

void server_thread(std::shared_ptr<zmq::context_t> ctx) {
    zmq::socket_t socket(*ctx, zmq::socket_type::rep);

    // 使用INPROC传输（进程内线程通信）
    socket.bind("inproc://test_channel");

    std::cout << "[INPROC Server] 已绑定到 inproc://test_channel" << std::endl;

    for (int i = 0; i < 3; i++) {
        zmq::message_t request;
        auto result = socket.recv(request, zmq::recv_flags::none);

        if (!result) break;

        std::string msg(static_cast<char*>(request.data()), request.size());
        std::cout << "[INPROC Server] 收到: " << msg << std::endl;

        std::string reply = "INPROC应答: " + msg;
        socket.send(zmq::buffer(reply), zmq::send_flags::none);
        std::cout << "[INPROC Server] 已应答" << std::endl;
    }
}

void client_thread(std::shared_ptr<zmq::context_t> ctx) {
    // 延迟确保服务器已启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    zmq::socket_t socket(*ctx, zmq::socket_type::req);
    socket.connect("inproc://test_channel");

    std::cout << "[INPROC Client] 已连接到 inproc://test_channel" << std::endl;

    for (int i = 1; i <= 3; i++) {
        std::string request = "INPROC请求" + std::to_string(i);

        std::cout << "[INPROC Client] 发送: " << request << std::endl;
        socket.send(zmq::buffer(request), zmq::send_flags::none);

        zmq::message_t reply;
        auto result = socket.recv(reply, zmq::recv_flags::none);

        if (result) {
            std::string reply_str(static_cast<char*>(reply.data()), reply.size());
            std::cout << "[INPROC Client] 收到: " << reply_str << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main() {
    // 创建共享的context，这样两个线程才能使用inproc
    auto shared_context = std::make_shared<zmq::context_t>(1);

    std::cout << "=== INPROC传输演示 ===" << std::endl;
    std::cout << "特点: 同一进程内线程通信，最快，零拷贝" << std::endl << std::endl;

    // 创建服务器和客户端线程
    std::jthread server(server_thread, shared_context);
    std::jthread client(client_thread, shared_context);

    std::cout << "\n=== INPROC演示完成 ===" << std::endl;

    return 0;
}
