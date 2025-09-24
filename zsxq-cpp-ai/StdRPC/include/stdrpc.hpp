#ifndef STDRPC_HPP
#define STDRPC_HPP

/**
 *
 * 这是一个轻量级的RPC（远程过程调用）框架，支持：
 * - 自动序列化/反序列化
 * - TCP网络传输
 * - 同步和异步调用
 * - 连接池支持
 * - 多线程服务器
 *
 * 使用示例：
 *
 * // 服务端
 * RpcServer server(8080);
 * server.registerFunction<int, int, int>("add",
 *     [](int a, int b) { return a + b; });
 * server.run();
 *
 * // 客户端
 * RpcClient client("127.0.0.1", 8080);
 * client.connect();
 * int result = client.call<int>("add", 10, 20);
 */
#include <optional>
#include <sstream>
#include "serializer.hpp"
#include "protocol.hpp"
#include "tcp_transport.hpp"
#include "rpc_server.hpp"
#include "rpc_client.hpp"

namespace stdrpc {

/**
 * RPC框架版本信息
 */
constexpr const char* VERSION = "1.0.0";

/**
 * 获取版本字符串
 * @return 版本字符串
 */
inline const char* getVersion() {
    return VERSION;
}

/**
 * 辅助宏：简化服务注册
 *
 * 使用示例：
 * RPC_REGISTER_FUNCTION(server, "add", int, (int a, int b), {
 *     return a + b;
 * });
 */
#define RPC_REGISTER_FUNCTION(server, name, ret_type, params, body) \
    server.registerFunction<ret_type>(name, [](params) -> ret_type body)

/**
 * 辅助宏：简化无返回值的服务注册
 *
 * 使用示例：
 * RPC_REGISTER_VOID_FUNCTION(server, "print", (const std::string& msg), {
 *     std::cout << msg << std::endl;
 * });
 */
#define RPC_REGISTER_VOID_FUNCTION(server, name, params, body) \
    server.registerFunction(name, [](params) -> void body)

/**
 * 服务描述结构
 * 用于服务发现和文档生成
 */
struct ServiceDescriptor {
    std::string name;           // 服务名称
    std::string description;    // 服务描述
    std::string params_desc;    // 参数描述
    std::string return_desc;    // 返回值描述

    ServiceDescriptor() = default;  // 默认构造函数

    ServiceDescriptor(const std::string& n, const std::string& d,
                     const std::string& p, const std::string& r)
        : name(n), description(d), params_desc(p), return_desc(r) {}
};

/**
 * 服务注册表类
 * 提供服务的元数据管理
 */
class ServiceRegistry {
private:
    std::unordered_map<std::string, ServiceDescriptor> services_;
    mutable std::mutex mutex_;

public:
    /**
     * 注册服务描述
     * @param descriptor 服务描述
     */
    void registerService(const ServiceDescriptor& descriptor) {
        std::lock_guard<std::mutex> lock(mutex_);
        services_[descriptor.name] = descriptor;
    }

    /**
     * 获取服务描述
     * @param name 服务名称
     * @return 服务描述（如果存在）
     */
    std::optional<ServiceDescriptor> getService(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = services_.find(name);
        if (it != services_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /**
     * 获取所有服务
     * @return 服务列表
     */
    std::vector<ServiceDescriptor> getAllServices() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<ServiceDescriptor> result;
        for (const auto& [name, desc] : services_) {
            result.push_back(desc);
        }
        return result;
    }

    /**
     * 生成服务文档
     * @return 文档字符串
     */
    std::string generateDocumentation() const {
        std::stringstream ss;
        ss << "=== RPC 服务文档 ===\n\n";

        auto services = getAllServices();
        for (const auto& service : services) {
            ss << "服务名: " << service.name << "\n";
            ss << "描述: " << service.description << "\n";
            ss << "参数: " << service.params_desc << "\n";
            ss << "返回值: " << service.return_desc << "\n";
            ss << "---\n\n";
        }

        return ss.str();
    }
};

/**
 * RPC服务器扩展类
 * 提供额外的功能，如服务发现和监控
 */
class RpcServerExt : public RpcServer {
private:
    ServiceRegistry registry_;
    std::atomic<uint64_t> total_requests_{0};
    std::atomic<uint64_t> failed_requests_{0};

public:
    /**
     * 构造函数
     */
    RpcServerExt(uint16_t port, size_t num_workers = 4)
        : RpcServer(port, num_workers) {

        // 注册内置服务
        registerBuiltinServices();
    }

    /**
     * 注册带描述的函数
     */
    template<typename RetType, typename... Args>
    void registerFunctionWithDesc(const std::string& name,
                                  std::function<RetType(Args...)> func,
                                  const std::string& description,
                                  const std::string& params_desc,
                                  const std::string& return_desc) {
        // 注册函数
        registerFunction(name, func);

        // 注册描述
        ServiceDescriptor desc(name, description, params_desc, return_desc);
        registry_.registerService(desc);
    }

    /**
     * 获取服务注册表
     */
    ServiceRegistry& getRegistry() {
        return registry_;
    }

    /**
     * 获取统计信息
     */
    struct Stats {
        uint64_t total_requests;
        uint64_t failed_requests;
        double success_rate;
    };

    Stats getStats() const {
        Stats stats;
        stats.total_requests = total_requests_.load();
        stats.failed_requests = failed_requests_.load();
        stats.success_rate = stats.total_requests > 0 ?
            (double)(stats.total_requests - stats.failed_requests) / stats.total_requests * 100.0 : 0.0;
        return stats;
    }

private:
    /**
     * 注册内置服务
     */
    void registerBuiltinServices() {
        // 服务发现
        registerFunction<std::vector<std::string>>("__list_services",
            std::function<std::vector<std::string>()>([this]() {
                auto services = registry_.getAllServices();
                std::vector<std::string> names;
                for (const auto& service : services) {
                    names.push_back(service.name);
                }
                return names;
            }));

        // 服务文档
        registerFunction<std::string>("__get_documentation",
            std::function<std::string()>([this]() {
                return registry_.generateDocumentation();
            }));

        // 健康检查
        registerFunction<std::string>("__health_check",
            std::function<std::string()>([]() {
                return std::string("OK");
            }));

        // 统计信息
        registerFunction<std::string>("__get_stats",
            std::function<std::string()>([this]() {
                auto stats = getStats();
                std::stringstream ss;
                ss << "总请求数: " << stats.total_requests << "\n";
                ss << "失败请求数: " << stats.failed_requests << "\n";
                ss << "成功率: " << stats.success_rate << "%\n";
                return ss.str();
            }));
    }
};

/**
 * RPC代理类模板
 * 用于创建类型安全的远程服务代理
 */
template<typename T>
class RpcProxy {
private:
    RpcClient* client_;

public:
    explicit RpcProxy(RpcClient* client) : client_(client) {}

    /**
     * 调用远程方法
     */
    template<typename RetType, typename... Args>
    RetType call(const std::string& method, Args... args) {
        return client_->call<RetType>(method, std::forward<Args>(args)...);
    }

    /**
     * 异步调用远程方法
     */
    template<typename RetType, typename... Args>
    std::future<RetType> asyncCall(const std::string& method, Args... args) {
        return client_->asyncCall<RetType>(method, std::forward<Args>(args)...);
    }
};

/**
 * 批量RPC调用类
 * 支持批量发送多个RPC请求
 */
class BatchRpcCall {
private:
    struct Call {
        std::string method_name;
        std::vector<uint8_t> params_data;
        std::function<void(const std::vector<uint8_t>&)> callback;
    };

    std::vector<Call> calls_;
    RpcClient* client_;

public:
    explicit BatchRpcCall(RpcClient* client) : client_(client) {}

    /**
     * 添加调用
     */
    template<typename RetType, typename... Args>
    void addCall(const std::string& method_name,
                 std::function<void(RetType)> callback,
                 Args... args) {
        Serializer serializer;
        serializeArgs(serializer, args...);

        calls_.push_back({
            method_name,
            serializer.getData(),
            [callback](const std::vector<uint8_t>& data) {
                if (!data.empty()) {
                    Serializer deserializer(data.data(), data.size());
                    if constexpr (std::is_same_v<RetType, std::string>) {
                        callback(deserializer.readString());
                    } else {
                        callback(deserializer.read<RetType>());
                    }
                }
            }
        });
    }

    /**
     * 执行批量调用
     */
    void execute() {
        std::vector<std::future<void>> futures;

        for (auto& call : calls_) {
            futures.push_back(std::async(std::launch::async, [this, &call]() {
                try {
                    // 这里应该实现批量调用协议
                    // 简化版本：逐个调用
                    // 注意：这是简化实现，实际应该有更复杂的批量调用机制
                    std::cerr << "批量调用未完全实现: " << call.method_name << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "批量调用失败: " << e.what() << std::endl;
                }
            }));
        }

        // 等待所有调用完成
        for (auto& future : futures) {
            future.wait();
        }
    }

private:
    template<typename... Args>
    void serializeArgs(Serializer& serializer, Args... args) {
        (serializer.write(args), ...);
    }
};

/**
 * 简易的RPC配置类
 */
struct RpcConfig {
    // 服务器配置
    uint16_t server_port = 8080;
    size_t num_workers = 4;
    size_t max_connections = 1000;

    // 客户端配置
    int timeout_ms = 30000;
    size_t pool_size = 4;
    bool enable_compression = false;
    bool enable_encryption = false;

    // 网络配置
    size_t buffer_size = 65536;
    bool tcp_nodelay = true;
    bool tcp_keepalive = true;

    /**
     * 从文件加载配置
     */
    bool loadFromFile(const std::string& filename) {
        // 简化实现，实际应该解析配置文件
        return true;
    }

    /**
     * 保存配置到文件
     */
    bool saveToFile(const std::string& filename) const {
        // 简化实现，实际应该序列化配置
        return true;
    }
};

} // namespace stdrpc

#endif // STDRPC_HPP