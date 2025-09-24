#ifndef RPC_CLIENT_HPP
#define RPC_CLIENT_HPP

#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <future>
#include <unordered_map>
#include <condition_variable>
#include <stdexcept>
#include <iostream>
#include "tcp_transport.hpp"
#include "protocol.hpp"
#include "serializer.hpp"

namespace stdrpc {

/**
 * RPC调用异常类
 */
class RpcException : public std::runtime_error {
private:
    StatusCode status_;

public:
    RpcException(StatusCode status, const std::string& message)
        : std::runtime_error(message), status_(status) {}

    StatusCode getStatus() const { return status_; }
};

/**
 * RPC客户端类
 * 负责连接服务器，发送请求并接收响应
 */
class RpcClient {
private:
    std::unique_ptr<TcpConnection> connection_;    // TCP连接
    std::mutex connection_mutex_;                  // 连接互斥锁
    std::atomic<uint32_t> next_request_id_{1};     // 下一个请求ID

    // 异步调用相关
    struct PendingCall {
        std::promise<ResponseMessage> promise;
        std::chrono::steady_clock::time_point start_time;
    };
    std::unordered_map<uint32_t, PendingCall> pending_calls_;
    std::mutex pending_mutex_;
    std::condition_variable pending_cv_;
    std::thread receiver_thread_;
    std::atomic<bool> running_{false};

    // 配置
    std::string server_addr_;
    uint16_t server_port_;
    int timeout_ms_ = 30000;  // 默认30秒超时

public:
    /**
     * 默认构造函数
     */
    RpcClient() = default;

    /**
     * 构造函数
     * @param addr 服务器地址
     * @param port 服务器端口
     */
    RpcClient(const std::string& addr, uint16_t port)
        : server_addr_(addr), server_port_(port) {}

    /**
     * 析构函数
     */
    ~RpcClient() {
        disconnect();
    }

    /**
     * 连接到服务器
     * @param addr 服务器地址
     * @param port 服务器端口
     * @return 成功返回true，失败返回false
     */
    bool connect(const std::string& addr, uint16_t port) {
        std::lock_guard<std::mutex> lock(connection_mutex_);

        if (connection_ && connection_->isConnected()) {
            return true;  // 已连接
        }

        server_addr_ = addr;
        server_port_ = port;

        connection_ = std::make_unique<TcpConnection>();
        if (!connection_->connect(addr, port)) {
            std::cerr << "[客户端] 连接服务器失败: " << addr << ":" << port << std::endl;
            connection_.reset();
            return false;
        }

        std::cout << "[客户端] 成功连接到服务器: " << addr << ":" << port << std::endl;

        // 启动接收线程
        running_ = true;
        receiver_thread_ = std::thread([this]() { receiveLoop(); });

        return true;
    }

    /**
     * 使用构造函数中的地址连接
     * @return 成功返回true，失败返回false
     */
    bool connect() {
        return connect(server_addr_, server_port_);
    }

    /**
     * 断开连接
     */
    void disconnect() {
        running_ = false;

        // 等待接收线程结束
        if (receiver_thread_.joinable()) {
            receiver_thread_.join();
        }

        // 关闭连接
        std::lock_guard<std::mutex> lock(connection_mutex_);
        if (connection_) {
            connection_->close();
            connection_.reset();
            std::cout << "[客户端] 已断开服务器连接" << std::endl;
        }

        // 清理待处理的调用
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            for (auto& [id, call] : pending_calls_) {
                ResponseMessage response;
                response.status = StatusCode::NETWORK_ERROR;
                response.error_message = "连接已断开";
                call.promise.set_value(std::move(response));
            }
            pending_calls_.clear();
        }
        pending_cv_.notify_all();
    }

    /**
     * 检查是否已连接
     * @return 如果已连接返回true，否则返回false
     */
    bool isConnected() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(connection_mutex_));
        return connection_ && connection_->isConnected();
    }

    /**
     * 设置调用超时时间
     * @param timeout_ms 超时时间（毫秒）
     */
    void setTimeout(int timeout_ms) {
        timeout_ms_ = timeout_ms;
    }

    /**
     * 调用远程方法（带参数和返回值）
     * @param method_name 方法名
     * @param args 方法参数
     * @return 方法返回值
     */
    template<typename RetType, typename... Args>
    RetType call(const std::string& method_name, Args... args) {
        // 序列化参数
        Serializer serializer;
        serializeArgs(serializer, args...);

        // 发送请求并获取响应
        auto response = doCall(method_name, serializer.getData());

        // 检查响应状态
        if (response.status != StatusCode::OK) {
            throw RpcException(response.status,
                "RPC调用失败: " + response.error_message);
        }

        // 反序列化结果
        if constexpr (!std::is_void_v<RetType>) {
            if (response.result_data.empty()) {
                throw RpcException(StatusCode::SERIALIZATION_ERROR,
                    "响应数据为空");
            }
            Serializer deserializer(response.result_data.data(),
                                   response.result_data.size());
            if constexpr (std::is_same_v<RetType, std::string>) {
                return deserializer.readString();
            } else if constexpr (std::is_same_v<RetType, std::vector<std::string>>) {
                return deserializer.readVector<std::string>();
            } else if constexpr (std::is_same_v<RetType, std::vector<uint8_t>>) {
                return deserializer.readVector<uint8_t>();
            } else if constexpr (std::is_same_v<RetType, std::vector<int>>) {
                return deserializer.readVector<int>();
            } else if constexpr (std::is_same_v<RetType, std::vector<double>>) {
                return deserializer.readVector<double>();
            } else {
                return deserializer.read<RetType>();
            }
        }
    }

    /**
     * 调用远程方法（无返回值）
     * @param method_name 方法名
     * @param args 方法参数
     */
    template<typename... Args>
    void callVoid(const std::string& method_name, Args... args) {
        // 序列化参数
        Serializer serializer;
        serializeArgs(serializer, args...);

        // 发送请求并获取响应
        auto response = doCall(method_name, serializer.getData());

        // 检查响应状态
        if (response.status != StatusCode::OK) {
            throw RpcException(response.status,
                "RPC调用失败: " + response.error_message);
        }
    }

    /**
     * 异步调用远程方法
     * @param method_name 方法名
     * @param args 方法参数
     * @return future对象，用于获取结果
     */
    template<typename RetType, typename... Args>
    std::future<RetType> asyncCall(const std::string& method_name, Args... args) {
        return std::async(std::launch::async, [this, method_name, args...]() {
            return this->call<RetType>(method_name, args...);
        });
    }

private:
    /**
     * 执行RPC调用
     * @param method_name 方法名
     * @param params_data 参数数据
     * @return 响应消息
     */
    ResponseMessage doCall(const std::string& method_name,
                          const std::vector<uint8_t>& params_data) {
        // 检查连接
        if (!isConnected()) {
            if (!connect()) {
                ResponseMessage response;
                response.status = StatusCode::NETWORK_ERROR;
                response.error_message = "无法连接到服务器";
                return response;
            }
        }

        // 创建请求
        RequestMessage request;
        request.method_name = method_name;
        request.params_data = params_data;

        // 生成请求ID
        uint32_t request_id = next_request_id_.fetch_add(1);

        // 创建待处理调用
        std::future<ResponseMessage> future;
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            auto& pending = pending_calls_[request_id];
            future = pending.promise.get_future();
            pending.start_time = std::chrono::steady_clock::now();
        }

        // 发送请求
        {
            std::lock_guard<std::mutex> lock(connection_mutex_);
            Message msg(request_id, request);
            if (!connection_->sendMessage(msg)) {
                // 发送失败，清理并返回错误
                {
                    std::lock_guard<std::mutex> lock2(pending_mutex_);
                    pending_calls_.erase(request_id);
                }
                ResponseMessage response;
                response.status = StatusCode::NETWORK_ERROR;
                response.error_message = "发送请求失败";
                return response;
            }
        }

        std::cout << "[客户端] 发送请求: " << method_name
                 << " (ID: " << request_id << ")" << std::endl;

        // 等待响应
        auto status = future.wait_for(std::chrono::milliseconds(timeout_ms_));
        if (status == std::future_status::timeout) {
            // 超时，清理并返回错误
            {
                std::lock_guard<std::mutex> lock(pending_mutex_);
                pending_calls_.erase(request_id);
            }
            ResponseMessage response;
            response.status = StatusCode::TIMEOUT;
            response.error_message = "请求超时";
            return response;
        }

        return future.get();
    }

    /**
     * 接收消息循环
     */
    void receiveLoop() {
        while (running_) {
            if (!connection_ || !connection_->isConnected()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            // 接收消息
            auto message = connection_->receiveMessage(1000);  // 1秒超时
            if (!message) {
                continue;
            }

            // 处理响应
            if (message->getHeader().type == MessageType::RESPONSE) {
                processResponse(std::move(message));
            }
        }
    }

    /**
     * 处理响应消息
     */
    void processResponse(std::unique_ptr<Message> message) {
        const auto& header = message->getHeader();

        // 反序列化响应
        ResponseMessage response;
        try {
            Serializer deserializer(message->getBody().data(),
                                   message->getBody().size());
            response.deserialize(deserializer);
        } catch (const std::exception& e) {
            response.status = StatusCode::SERIALIZATION_ERROR;
            response.error_message = e.what();
        }

        // 查找并完成待处理调用
        std::lock_guard<std::mutex> lock(pending_mutex_);
        auto it = pending_calls_.find(header.request_id);
        if (it != pending_calls_.end()) {
            it->second.promise.set_value(std::move(response));
            pending_calls_.erase(it);

            std::cout << "[客户端] 收到响应 (ID: " << header.request_id << ")" << std::endl;
        }
    }

    /**
     * 序列化参数（递归终止）
     */
    void serializeArgs(Serializer&) {}

    /**
     * 序列化参数（递归）
     */
    template<typename T, typename... Rest>
    void serializeArgs(Serializer& serializer, T&& arg, Rest&&... rest) {
        serializer.write(std::forward<T>(arg));
        serializeArgs(serializer, std::forward<Rest>(rest)...);
    }
};

/**
 * RPC连接池类
 * 管理多个客户端连接，提供负载均衡
 */
class RpcClientPool {
private:
    std::vector<std::unique_ptr<RpcClient>> clients_;
    std::atomic<size_t> next_client_{0};
    std::string server_addr_;
    uint16_t server_port_;
    size_t pool_size_;

public:
    /**
     * 构造函数
     * @param addr 服务器地址
     * @param port 服务器端口
     * @param pool_size 连接池大小
     */
    RpcClientPool(const std::string& addr, uint16_t port, size_t pool_size = 4)
        : server_addr_(addr), server_port_(port), pool_size_(pool_size) {

        // 创建客户端连接
        for (size_t i = 0; i < pool_size; ++i) {
            auto client = std::make_unique<RpcClient>();
            client->connect(addr, port);
            clients_.push_back(std::move(client));
        }
    }

    /**
     * 获取下一个客户端（轮询）
     * @return 客户端指针
     */
    RpcClient* getClient() {
        size_t index = next_client_.fetch_add(1) % pool_size_;
        return clients_[index].get();
    }

    /**
     * 调用远程方法
     */
    template<typename RetType, typename... Args>
    RetType call(const std::string& method_name, Args... args) {
        auto client = getClient();
        return client->call<RetType>(method_name, std::forward<Args>(args)...);
    }

    /**
     * 异步调用远程方法
     */
    template<typename RetType, typename... Args>
    std::future<RetType> asyncCall(const std::string& method_name, Args... args) {
        auto client = getClient();
        return client->asyncCall<RetType>(method_name, std::forward<Args>(args)...);
    }
};

} // namespace stdrpc

#endif // RPC_CLIENT_HPP