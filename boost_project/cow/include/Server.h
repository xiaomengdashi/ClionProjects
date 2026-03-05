#pragma once

#include <boost/asio.hpp>
#include <boost/asio/local/stream_protocol.hpp>

#include <chrono>
#include <ctime>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>

// ─── 数据结构 ──────────────────────────────────────────────────────────────────

struct UserInfo {
    int         id           = 0;
    std::string remote_addr;
    int         remote_port  = 0;
    std::time_t connected_at = 0;
    uint64_t    bytes_recv   = 0;
    uint64_t    bytes_sent   = 0;
};

struct LogEntry {
    std::string timestamp;
    std::string level;   // INFO / WARN / ERROR
    std::string message;
};

// ─── Server ───────────────────────────────────────────────────────────────────

class Server {
public:
    Server(boost::asio::io_context& ioc, int tcp_port);
    ~Server();

    void start();   ///< 启动 TCP acceptor + IPC acceptor + 带宽统计定时器
    void stop();    ///< 取消所有 acceptor / timer（不停止 io_context）

    // ── 供 UserSession / IpcSession 回调 ──────────────────────────────────────

    /// 注册一个新用户，返回共享 UserInfo；closer 用于 kick 命令
    std::shared_ptr<UserInfo> addUser(int id,
                                      const std::string& addr,
                                      int port,
                                      std::function<void()> closer);

    void removeUser(int id);
    void addTrafficIn(uint64_t bytes);
    void addTrafficOut(uint64_t bytes);

    int  nextUserId() { return next_user_id_++; }

    /// 处理 IPC 命令字符串，返回响应文本
    std::string handleCommand(const std::string& cmd);

    /// 记录日志到环形缓冲区
    void log(const std::string& level, const std::string& msg);

private:
    void startTcpAccept();
    void startIpcAccept();
    void startStatsTimer();
    void updateBandwidth();

    std::string cmdList();
    std::string cmdLog(int n);
    std::string cmdStats();
    std::string cmdKick(int id);

    boost::asio::io_context&                              ioc_;
    boost::asio::ip::tcp::acceptor                        tcp_acceptor_;
    boost::asio::local::stream_protocol::acceptor         ipc_acceptor_;
    boost::asio::steady_timer                             stats_timer_;

    int  next_user_id_     = 1;
    int  total_connections_ = 0;

    std::map<int, std::shared_ptr<UserInfo>>    users_;
    std::map<int, std::function<void()>>        user_closers_;

    std::deque<LogEntry> log_buffer_;
    static constexpr std::size_t MAX_LOG = 1000;

    uint64_t total_bytes_in_  = 0;
    uint64_t total_bytes_out_ = 0;
    double   bps_in_          = 0.0;
    double   bps_out_         = 0.0;
    uint64_t last_bytes_in_   = 0;
    uint64_t last_bytes_out_  = 0;

    std::chrono::steady_clock::time_point last_stats_time_;
    std::chrono::steady_clock::time_point start_time_;
};
