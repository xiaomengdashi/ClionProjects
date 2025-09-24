#ifndef RPC_SERVER_HPP
#define RPC_SERVER_HPP

#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <iostream>
#include <sstream>
#include "tcp_transport.hpp"
#include "protocol.hpp"
#include "serializer.hpp"

namespace stdrpc {

/**
 * RPC方法处理器基类
 */
class MethodHandler {
public:
    virtual ~MethodHandler() = default;

    /**
     * 处理RPC调用
     * @param params_data 参数数据（序列化后）
     * @param result_data 结果数据（序列化后，输出参数）
     * @return 成功返回true，失败返回false
     */
    virtual bool handle(const std::vector<uint8_t>& params_data,
                       std::vector<uint8_t>& result_data) = 0;
};

/**
 * RPC方法处理器模板类
 * 用于包装具体的函数实现
 */
template<typename Func>
class MethodHandlerImpl : public MethodHandler {
private:
    Func func_;  // 函数对象

public:
    explicit MethodHandlerImpl(Func func) : func_(std::move(func)) {}

    bool handle(const std::vector<uint8_t>& params_data,
               std::vector<uint8_t>& result_data) override {
        return func_(params_data, result_data);
    }
};

/**
 * RPC服务器类
 * 负责监听客户端连接，接收请求并调用相应的处理函数
 */
class RpcServer {
private:
    // 方法注册表，存储方法名到处理器的映射
    std::unordered_map<std::string, std::unique_ptr<MethodHandler>> methods_;
    std::mutex methods_mutex_;

    // 服务器状态
    std::atomic<bool> running_{false};
    std::unique_ptr<TcpListener> listener_;
    std::vector<std::thread> worker_threads_;
    std::thread accept_thread_;

    // 工作队列
    struct WorkItem {
        std::unique_ptr<TcpConnection> connection;
        std::unique_ptr<Message> message;
    };
    std::queue<WorkItem> work_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    // 配置参数
    uint16_t port_ = 0;
    size_t num_workers_ = 4;  // 工作线程数

    // 用于跟踪 stop() 是否已经被调用
    std::atomic<bool> stopped_{false};
    std::mutex output_mutex_;  // 用于线程安全的输出

public:
    /**
     * 构造函数
     * @param port 监听端口
     * @param num_workers 工作线程数
     */
    explicit RpcServer(uint16_t port, size_t num_workers = 4)
        : port_(port), num_workers_(num_workers) {}

    /**
     * 析构函数
     */
    ~RpcServer() {
        stop();
    }

    /**
     * 注册RPC方法（简单版本）
     * @param name 方法名
     * @param func 处理函数
     */
    template<typename Func>
    void registerMethod(const std::string& name, Func func) {
        auto handler = std::make_unique<MethodHandlerImpl<Func>>(std::move(func));
        std::lock_guard<std::mutex> lock(methods_mutex_);
        methods_[name] = std::move(handler);
        {
            std::lock_guard<std::mutex> out_lock(output_mutex_);
            std::cout << "[服务器] 注册方法: " << name << std::endl;
        }
    }

    /**
     * 注册带参数和返回值的RPC方法
     * @param name 方法名
     * @param func 处理函数
     */
    template<typename RetType, typename... Args>
    void registerFunction(const std::string& name,
                         std::function<RetType(Args...)> func) {
        auto wrapper = [func](const std::vector<uint8_t>& params_data,
                            std::vector<uint8_t>& result_data) -> bool {
            try {
                // 反序列化参数
                Serializer deserializer(params_data.data(), params_data.size());
                std::tuple<typename std::decay<Args>::type...> args;
                deserializeArgs<0>(deserializer, args);

                // 调用函数
                RetType result = std::apply(func, args);

                // 序列化结果
                Serializer serializer;
                serializer.write(result);
                result_data = serializer.getData();

                return true;
            } catch (const std::exception& e) {
                std::cerr << "[服务器] 方法执行失败: " << e.what() << std::endl;
                return false;
            }
        };

        registerMethod(name, wrapper);
    }

    /**
     * 注册无返回值的RPC方法
     */
    template<typename... Args>
    void registerFunction(const std::string& name,
                         std::function<void(Args...)> func) {
        auto wrapper = [func](const std::vector<uint8_t>& params_data,
                            std::vector<uint8_t>& result_data) -> bool {
            try {
                // 反序列化参数
                Serializer deserializer(params_data.data(), params_data.size());
                std::tuple<typename std::decay<Args>::type...> args;
                deserializeArgs<0>(deserializer, args);

                // 调用函数
                std::apply(func, args);

                // 无返回值，返回空结果
                result_data.clear();
                return true;
            } catch (const std::exception& e) {
                std::cerr << "[服务器] 方法执行失败: " << e.what() << std::endl;
                return false;
            }
        };

        registerMethod(name, wrapper);
    }

    /**
     * 启动服务器
     * @return 成功返回true，失败返回false
     */
    bool start() {
        if (running_) {
            return false;
        }

        // 创建监听器
        listener_ = std::make_unique<TcpListener>();
        if (!listener_->listen(port_)) {
            std::cerr << "[服务器] 监听端口 " << port_ << " 失败" << std::endl;
            return false;
        }

        running_ = true;
        stopped_ = false;
        {
            std::lock_guard<std::mutex> lock(output_mutex_);
            std::cout << "[服务器] 启动成功，监听端口: " << port_ << std::endl;
        }

        // 当前版本不使用工作线程池，直接在连接线程中处理
        // 未来可以恢复工作线程池以提高并发性能
        // for (size_t i = 0; i < num_workers_; ++i) {
        //     worker_threads_.emplace_back([this]() { workerLoop(); });
        // }

        // 启动接受线程
        accept_thread_ = std::thread([this]() { acceptLoop(); });

        return true;
    }

    /**
     * 停止服务器
     */
    void stop() {
        // 使用 stopped_ 标志避免重复调用
        bool expected = false;
        if (!stopped_.compare_exchange_strong(expected, true)) {
            return;  // 已经停止过了
        }

        if (!running_) {
            return;
        }

        {
            std::lock_guard<std::mutex> lock(output_mutex_);
            std::cout << "[服务器] 正在停止..." << std::endl;
        }
        running_ = false;

        // 停止监听
        if (listener_) {
            listener_->stop();
        }

        // 唤醒所有工作线程
        queue_cv_.notify_all();

        // 等待接受线程结束
        if (accept_thread_.joinable()) {
            accept_thread_.join();
        }

        // 等待工作线程结束（如果有的话）
        for (auto& thread : worker_threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }

        worker_threads_.clear();
        {
            std::lock_guard<std::mutex> lock(output_mutex_);
            std::cout << "[服务器] 已停止" << std::endl;
        }
    }

    /**
     * 运行服务器（阻塞直到停止）
     */
    void run() {
        if (!start()) {
            return;
        }

        // 等待停止信号
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

private:
    /**
     * 接受连接循环
     */
    void acceptLoop() {
        while (running_) {
            // 接受客户端连接
            auto connection = listener_->accept(1000);  // 1秒超时
            if (!connection) {
                continue;
            }

            {
                std::lock_guard<std::mutex> lock(output_mutex_);
                std::cout << "[服务器] 接受客户端连接: "
                         << connection->getRemoteAddress() << ":"
                         << connection->getRemotePort() << std::endl;
            }

            // 创建客户端处理线程
            std::thread([this, conn = std::move(connection)]() mutable {
                handleClient(std::move(conn));
            }).detach();
        }
    }

    /**
     * 处理客户端连接
     */
    void handleClient(std::unique_ptr<TcpConnection> connection) {
        while (connection && connection->isConnected() && running_) {
            // 接收消息
            auto message = connection->receiveMessage(5000);  // 5秒超时
            if (!message) {
                break;
            }

            // 直接处理请求，不使用工作队列
            processRequest(connection, message);
        }

        if (connection && connection->isConnected()) {
            std::lock_guard<std::mutex> lock(output_mutex_);
            std::cout << "[服务器] 客户端断开连接: "
                     << connection->getRemoteAddress() << ":"
                     << connection->getRemotePort() << std::endl;
        }
    }

    /**
     * 工作线程循环（当前未使用，保留以备将来扩展）
     */
    void workerLoop() {
        // 当前版本直接在连接线程中处理请求
        // 此函数保留以备将来实现工作线程池
        while (running_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    /**
     * 处理RPC请求
     */
    void processRequest(std::unique_ptr<TcpConnection>& connection,
                       std::unique_ptr<Message>& message) {
        if (!message || !connection) {
            return;
        }

        const auto& header = message->getHeader();
        ResponseMessage response;

        if (header.type == MessageType::REQUEST) {
            // 反序列化请求
            RequestMessage request;
            try {
                Serializer deserializer(message->getBody().data(),
                                       message->getBody().size());
                request.deserialize(deserializer);
            } catch (const std::exception& e) {
                response.status = StatusCode::SERIALIZATION_ERROR;
                response.error_message = e.what();
                sendResponse(connection, header.request_id, response);
                return;
            }

            // 查找方法
            std::unique_lock<std::mutex> lock(methods_mutex_);
            auto it = methods_.find(request.method_name);
            if (it == methods_.end()) {
                lock.unlock();
                response.status = StatusCode::METHOD_NOT_FOUND;
                response.error_message = "方法不存在: " + request.method_name;
                sendResponse(connection, header.request_id, response);
                return;
            }

            auto& handler = it->second;
            lock.unlock();

            // 调用方法
            {
                std::lock_guard<std::mutex> lock(output_mutex_);
                std::cout << "[服务器] 处理请求: " << request.method_name
                         << " (ID: " << header.request_id << ")" << std::endl;
            }

            if (handler->handle(request.params_data, response.result_data)) {
                response.status = StatusCode::OK;
            } else {
                response.status = StatusCode::INTERNAL_ERROR;
                response.error_message = "方法执行失败";
            }

            sendResponse(connection, header.request_id, response);
        }
    }

    /**
     * 发送响应
     */
    void sendResponse(std::unique_ptr<TcpConnection>& connection,
                     uint32_t request_id,
                     const ResponseMessage& response) {
        if (!connection || !connection->isConnected()) {
            return;
        }

        Message msg(request_id, response);
        if (!connection->sendMessage(msg)) {
            std::cerr << "[服务器] 发送响应失败" << std::endl;
        }
    }

    /**
     * 辅助函数：反序列化参数（静态函数）
     */
    template<size_t I, typename Tuple>
    static typename std::enable_if<I == std::tuple_size<Tuple>::value, void>::type
    deserializeArgs(Serializer&, Tuple&) {}

    template<size_t I, typename Tuple>
    static typename std::enable_if<I < std::tuple_size<Tuple>::value, void>::type
    deserializeArgs(Serializer& deserializer, Tuple& t) {
        using T = typename std::tuple_element<I, Tuple>::type;
        if constexpr (std::is_same_v<T, std::string>) {
            std::get<I>(t) = deserializer.readString();
        } else if constexpr (std::is_same_v<T, std::vector<int>>) {
            std::get<I>(t) = deserializer.readVector<int>();
        } else if constexpr (std::is_same_v<T, std::vector<double>>) {
            std::get<I>(t) = deserializer.readVector<double>();
        } else {
            std::get<I>(t) = deserializer.read<T>();
        }
        deserializeArgs<I + 1>(deserializer, t);
    }
};

} // namespace stdrpc

#endif // RPC_SERVER_HPP