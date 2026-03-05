#pragma once

#include <stdexcept>
#include <string>

/**
 * IpcClient：通过 Unix Domain Socket 向 cow 守护进程发送一条命令并读取响应。
 * 同步模式，仅用于命令行客户端侧。
 */
class IpcClient {
public:
    explicit IpcClient(const std::string& socket_path);

    /// 发送命令（不含换行），返回完整响应文本。
    /// 若连接失败，抛出 std::runtime_error。
    std::string sendCommand(const std::string& cmd);

private:
    std::string socket_path_;
};
