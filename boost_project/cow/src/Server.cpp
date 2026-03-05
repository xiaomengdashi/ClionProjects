#include "Server.h"
#include "Daemon.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

// ═════════════════════════════════════════════════════════════════════════════
//  工具函数
// ═════════════════════════════════════════════════════════════════════════════

static std::string formatBytes(uint64_t bytes) {
    char buf[64];
    if (bytes < 1024ULL)
        std::snprintf(buf, sizeof(buf), "%llu B",  (unsigned long long)bytes);
    else if (bytes < 1024ULL * 1024)
        std::snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
    else if (bytes < 1024ULL * 1024 * 1024)
        std::snprintf(buf, sizeof(buf), "%.1f MB", bytes / 1024.0 / 1024.0);
    else
        std::snprintf(buf, sizeof(buf), "%.1f GB", bytes / 1024.0 / 1024.0 / 1024.0);
    return buf;
}

static std::string formatBps(double bps) {
    char buf[64];
    if (bps < 1024.0)
        std::snprintf(buf, sizeof(buf), "%.1f B/s",  bps);
    else if (bps < 1024.0 * 1024)
        std::snprintf(buf, sizeof(buf), "%.1f KB/s", bps / 1024.0);
    else
        std::snprintf(buf, sizeof(buf), "%.1f MB/s", bps / 1024.0 / 1024.0);
    return buf;
}

static std::string formatDuration(std::time_t secs) {
    char buf[32];
    int h = (int)(secs / 3600);
    int m = (int)((secs % 3600) / 60);
    int s = (int)(secs % 60);
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
    return buf;
}

static std::string formatTime(std::time_t t) {
    struct tm tm{};
    ::localtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

static std::string nowStr() {
    return formatTime(std::time(nullptr));
}

// ═════════════════════════════════════════════════════════════════════════════
//  UserSession — 代表一个 TCP 用户连接（echo 服务器）
// ═════════════════════════════════════════════════════════════════════════════

class UserSession : public std::enable_shared_from_this<UserSession> {
public:
    UserSession(boost::asio::ip::tcp::socket socket, Server& server)
        : socket_(std::move(socket)), server_(server) {}

    void start() {
        boost::system::error_code ec;
        auto ep = socket_.remote_endpoint(ec);
        if (ec) { disconnect(); return; }

        id_   = server_.nextUserId();
        info_ = server_.addUser(id_,
                                ep.address().to_string(),
                                ep.port(),
                                [weak = weak_from_this()]() {
                                    if (auto self = weak.lock()) self->forceClose();
                                });

        server_.log("INFO",
            "User #" + std::to_string(id_) + " connected from "
            + ep.address().to_string() + ":" + std::to_string(ep.port()));

        // 发送欢迎消息
        auto welcome = std::make_shared<std::string>(
            "Welcome to COW server! Your ID is #" + std::to_string(id_) + "\r\n"
            "Type anything to echo. Ctrl+C to quit.\r\n");

        boost::asio::async_write(socket_, boost::asio::buffer(*welcome),
            [self = shared_from_this(), welcome](boost::system::error_code ec, std::size_t n) {
                if (!ec) {
                    self->info_->bytes_sent += n;
                    self->server_.addTrafficOut(n);
                    self->doRead();
                } else {
                    self->disconnect();
                }
            });
    }

    /// 由 kick 命令外部调用
    void forceClose() {
        boost::system::error_code ec;
        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        socket_.close(ec);
    }

private:
    void doRead() {
        socket_.async_read_some(boost::asio::buffer(buf_),
            [self = shared_from_this()](boost::system::error_code ec, std::size_t n) {
                if (ec) { self->disconnect(); return; }

                self->info_->bytes_recv += n;
                self->server_.addTrafficIn(n);

                // Echo 回客户端
                auto data = std::make_shared<std::string>(self->buf_.data(), n);
                boost::asio::async_write(self->socket_, boost::asio::buffer(*data),
                    [self, data](boost::system::error_code ec, std::size_t n) {
                        if (!ec) {
                            self->info_->bytes_sent += n;
                            self->server_.addTrafficOut(n);
                            self->doRead();
                        } else {
                            self->disconnect();
                        }
                    });
            });
    }

    void disconnect() {
        server_.removeUser(id_);
        if (id_ > 0)
            server_.log("INFO", "User #" + std::to_string(id_) + " disconnected");
        boost::system::error_code ec;
        socket_.close(ec);
    }

    boost::asio::ip::tcp::socket socket_;
    Server&                      server_;
    int                          id_ = 0;
    std::shared_ptr<UserInfo>    info_;
    std::array<char, 4096>       buf_{};
};

// ═════════════════════════════════════════════════════════════════════════════
//  IpcSession — 处理一条 CLI 命令（读一行 → 响应 → 关闭）
// ═════════════════════════════════════════════════════════════════════════════

class IpcSession : public std::enable_shared_from_this<IpcSession> {
public:
    IpcSession(boost::asio::local::stream_protocol::socket socket, Server& server)
        : socket_(std::move(socket)), server_(server) {}

    void start() { doRead(); }

private:
    void doRead() {
        boost::asio::async_read_until(socket_, read_buf_, '\n',
            [self = shared_from_this()](boost::system::error_code ec, std::size_t) {
                if (ec) return;
                std::istream is(&self->read_buf_);
                std::string  cmd;
                std::getline(is, cmd);
                if (!cmd.empty() && cmd.back() == '\r') cmd.pop_back();

                std::string response = self->server_.handleCommand(cmd);
                self->doWrite(response);
            });
    }

    void doWrite(const std::string& data) {
        auto buf = std::make_shared<std::string>(data);
        boost::asio::async_write(socket_, boost::asio::buffer(*buf),
            [self = shared_from_this(), buf](boost::system::error_code, std::size_t) {
                boost::system::error_code ec;
                self->socket_.close(ec);
            });
    }

    boost::asio::local::stream_protocol::socket socket_;
    Server&                                      server_;
    boost::asio::streambuf                       read_buf_;
};

// ═════════════════════════════════════════════════════════════════════════════
//  Server 实现
// ═════════════════════════════════════════════════════════════════════════════

Server::Server(boost::asio::io_context& ioc, int tcp_port)
    : ioc_(ioc)
    , tcp_acceptor_(ioc,
          boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), tcp_port))
    , ipc_acceptor_(ioc,
          boost::asio::local::stream_protocol::endpoint(Daemon::SOCK_PATH))
    , stats_timer_(ioc)
    , last_stats_time_(std::chrono::steady_clock::now())
    , start_time_(std::chrono::steady_clock::now())
{
    tcp_acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    log("INFO", "COW server started, TCP port " + std::to_string(tcp_port));
    log("INFO", "IPC socket: " + Daemon::SOCK_PATH);
}

Server::~Server() = default;

void Server::start() {
    startTcpAccept();
    startIpcAccept();
    startStatsTimer();
}

void Server::stop() {
    boost::system::error_code ec;
    tcp_acceptor_.cancel(ec);
    ipc_acceptor_.cancel(ec);
    stats_timer_.cancel(ec);
}

// ─── acceptor 循环 ────────────────────────────────────────────────────────────

void Server::startTcpAccept() {
    tcp_acceptor_.async_accept(
        [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (!ec) {
                std::make_shared<UserSession>(std::move(socket), *this)->start();
            }
            if (tcp_acceptor_.is_open())
                startTcpAccept();
        });
}

void Server::startIpcAccept() {
    auto sock = std::make_shared<boost::asio::local::stream_protocol::socket>(ioc_);
    ipc_acceptor_.async_accept(*sock,
        [this, sock](boost::system::error_code ec) {
            if (!ec) {
                std::make_shared<IpcSession>(std::move(*sock), *this)->start();
            }
            if (ipc_acceptor_.is_open())
                startIpcAccept();
        });
}

// ─── 带宽统计定时器（每秒采样）────────────────────────────────────────────────

void Server::startStatsTimer() {
    stats_timer_.expires_after(std::chrono::seconds(1));
    stats_timer_.async_wait([this](boost::system::error_code ec) {
        if (!ec) {
            updateBandwidth();
            startStatsTimer();
        }
    });
}

void Server::updateBandwidth() {
    auto   now = std::chrono::steady_clock::now();
    double dt  = std::chrono::duration<double>(now - last_stats_time_).count();
    if (dt > 0.0) {
        bps_in_  = (total_bytes_in_  - last_bytes_in_)  / dt;
        bps_out_ = (total_bytes_out_ - last_bytes_out_) / dt;
    }
    last_bytes_in_   = total_bytes_in_;
    last_bytes_out_  = total_bytes_out_;
    last_stats_time_ = now;
}

// ─── 用户管理 ─────────────────────────────────────────────────────────────────

std::shared_ptr<UserInfo> Server::addUser(int id,
                                          const std::string& addr,
                                          int port,
                                          std::function<void()> closer) {
    auto info          = std::make_shared<UserInfo>();
    info->id           = id;
    info->remote_addr  = addr;
    info->remote_port  = port;
    info->connected_at = std::time(nullptr);
    users_[id]         = info;
    user_closers_[id]  = std::move(closer);
    ++total_connections_;
    return info;
}

void Server::removeUser(int id) {
    users_.erase(id);
    user_closers_.erase(id);
}

void Server::addTrafficIn(uint64_t bytes)  { total_bytes_in_  += bytes; }
void Server::addTrafficOut(uint64_t bytes) { total_bytes_out_ += bytes; }

// ─── 日志 ─────────────────────────────────────────────────────────────────────

void Server::log(const std::string& level, const std::string& msg) {
    LogEntry e;
    e.timestamp = nowStr();
    e.level     = level;
    e.message   = msg;
    log_buffer_.push_back(std::move(e));
    if (log_buffer_.size() > MAX_LOG)
        log_buffer_.pop_front();
}

// ─── IPC 命令分发 ─────────────────────────────────────────────────────────────

std::string Server::handleCommand(const std::string& cmd) {
    if (cmd == "LIST")                          return cmdList();
    if (cmd == "STATS")                         return cmdStats();
    if (cmd == "PING")                          return "PONG\n";

    if (cmd.size() >= 3 && cmd.substr(0, 3) == "LOG") {
        int n = 100;
        if (cmd.size() > 4) {
            try { n = std::stoi(cmd.substr(4)); } catch (...) {}
        }
        return cmdLog(n);
    }

    if (cmd.size() >= 4 && cmd.substr(0, 4) == "KICK") {
        int id = 0;
        if (cmd.size() > 5) {
            try { id = std::stoi(cmd.substr(5)); } catch (...) {}
        }
        return cmdKick(id);
    }

    if (cmd == "STOP") {
        log("INFO", "Shutdown requested via IPC");
        // 延迟 200ms，让 IPC 响应先发出去
        auto timer = std::make_shared<boost::asio::steady_timer>(ioc_);
        timer->expires_after(std::chrono::milliseconds(200));
        timer->async_wait([this, timer](boost::system::error_code) {
            stop();
            ioc_.stop();
        });
        return "Stopping COW daemon...\n";
    }

    return "Unknown command: " + cmd + "\nRun 'cow help' for usage.\n";
}

// ─── 命令实现 ─────────────────────────────────────────────────────────────────

std::string Server::cmdList() {
    if (users_.empty())
        return "No users connected.\n";

    std::ostringstream ss;
    const char* LINE =
        "+---------+------------------+-------+---------------------+"
        "------------+------------+----------+\n";

    ss << LINE
       << "| User ID | Remote Address   | Port  | Connected At        |"
          " Received   | Sent       | Duration |\n"
       << LINE;

    std::time_t now = std::time(nullptr);
    for (auto& [id, info] : users_) {
        std::time_t dur = now - info->connected_at;
        ss << "| " << std::setw(7) << std::right << info->id              << " "
           << "| " << std::setw(16)<< std::left  << info->remote_addr     << " "
           << "| " << std::setw(5) << std::right << info->remote_port     << " "
           << "| " << std::setw(19)<< std::left  << formatTime(info->connected_at) << " "
           << "| " << std::setw(10)<< std::right << formatBytes(info->bytes_recv)  << " "
           << "| " << std::setw(10)<< std::right << formatBytes(info->bytes_sent)  << " "
           << "| " << std::setw(8) << std::right << formatDuration(dur)   << " |\n";
    }
    ss << LINE
       << "Total: " << users_.size() << " user(s) connected\n";
    return ss.str();
}

std::string Server::cmdLog(int n) {
    if (log_buffer_.empty())
        return "No log entries.\n";

    n = std::min(n, (int)log_buffer_.size());
    std::ostringstream ss;
    ss << "─── Last " << n << " log entries ───\n";

    auto start = log_buffer_.end() - n;
    for (auto it = start; it != log_buffer_.end(); ++it) {
        ss << "[" << it->timestamp << "] "
           << "[" << std::setw(5) << std::left << it->level << "] "
           << it->message << "\n";
    }
    return ss.str();
}

std::string Server::cmdStats() {
    auto now    = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                      now - start_time_).count();

    std::ostringstream ss;
    ss << "╔══════════════════════════════════════╗\n"
       << "║       COW Server Statistics          ║\n"
       << "╚══════════════════════════════════════╝\n";

    auto row = [&](const std::string& label, const std::string& val) {
        ss << "  " << std::left << std::setw(22) << label << ": " << val << "\n";
    };

    row("Uptime",             formatDuration(uptime));
    row("Active Users",       std::to_string(users_.size()));
    row("Total Connections",  std::to_string(total_connections_));
    ss << "  ──────────────────────────────────────\n";
    row("Total Received",     formatBytes(total_bytes_in_));
    row("Total Sent",         formatBytes(total_bytes_out_));
    ss << "  ──────────────────────────────────────\n";
    row("Throughput In",      formatBps(bps_in_));
    row("Throughput Out",     formatBps(bps_out_));

    return ss.str();
}

std::string Server::cmdKick(int id) {
    auto it = user_closers_.find(id);
    if (it == user_closers_.end())
        return "User #" + std::to_string(id) + " not found.\n";

    it->second();   // 调用 forceClose
    log("WARN", "User #" + std::to_string(id) + " kicked by admin");
    return "User #" + std::to_string(id) + " has been kicked.\n";
}
