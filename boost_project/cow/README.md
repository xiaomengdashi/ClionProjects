# COW — 命令行服务器管理工具

基于 **Boost.Asio** 实现的守护进程 + CLI 管理系统，演示了进程守护化、Unix Domain Socket IPC、异步 TCP 服务器等核心技术。

## 架构

```
┌──────────────────────────────────────────────────────────┐
│                     cow 二进制                            │
│                                                          │
│   守护进程模式 (cow start)          CLI 模式 (cow list…)  │
│   ┌─────────────────────────┐      ┌──────────────────┐ │
│   │  Boost.Asio io_context  │      │    IpcClient     │ │
│   │  ┌─────────────────┐    │      │  (同步 Unix      │ │
│   │  │  TCP Acceptor   │    │      │   Socket 连接)   │ │
│   │  │  port: 8899     │◄───┼──────┤  发送命令行      │ │
│   │  │  (用户连接)     │    │      │  读取响应        │ │
│   │  └─────────────────┘    │      └──────────────────┘ │
│   │  ┌─────────────────┐    │                           │
│   │  │  IPC Acceptor   │    │  通信协议:                 │
│   │  │  /tmp/cow.sock  │    │  请求: "COMMAND args\n"   │
│   │  │  (CLI 命令通道) │    │  响应: 文本直到 EOF        │
│   │  └─────────────────┘    │                           │
│   │  UserInfo Map           │                           │
│   │  LogEntry Deque         │                           │
│   │  带宽统计定时器 (1s)    │                           │
│   └─────────────────────────┘                           │
└──────────────────────────────────────────────────────────┘
```

## 编译

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
# 产物: bin/cow
```

## 命令速查

| 命令                  | 说明                         |
|-----------------------|------------------------------|
| `cow start`           | 在后台启动守护进程           |
| `cow start --port 9000` | 指定 TCP 监听端口          |
| `cow stop`            | 停止守护进程                 |
| `cow status`          | 查看运行状态和 PID           |
| `cow list`            | 列出所有在线用户（表格）     |
| `cow log`             | 查看最新 100 条日志          |
| `cow log 20`          | 查看最新 20 条日志           |
| `cow stats`           | 查看流量统计（吞吐量等）     |
| `cow kick <id>`       | 强制断开指定 ID 的用户       |

## 示例输出

### `cow list`
```
+---------+------------------+-------+---------------------+------------+------------+----------+
| User ID | Remote Address   | Port  | Connected At        | Received   | Sent       | Duration |
+---------+------------------+-------+---------------------+------------+------------+----------+
|       1 | 192.168.1.10     | 54321 | 2026-03-05 10:00:00 |    1.2 KB  |    2.5 KB  | 00:01:23 |
|       2 | 10.0.0.5         | 12345 | 2026-03-05 10:01:00 |     256 B  |     512 B  | 00:00:23 |
+---------+------------------+-------+---------------------+------------+------------+----------+
Total: 2 user(s) connected
```

### `cow stats`
```
╔══════════════════════════════════════╗
║       COW Server Statistics          ║
╚══════════════════════════════════════╝
  Uptime                : 01:23:45
  Active Users          : 2
  Total Connections     : 15
  ──────────────────────────────────────
  Total Received        : 45.6 MB
  Total Sent            : 123.4 MB
  ──────────────────────────────────────
  Throughput In         : 125.3 KB/s
  Throughput Out        : 98.7 KB/s
```

### `cow log 5`
```
─── Last 5 log entries ───
[2026-03-05 10:00:00] [INFO ] COW server started, TCP port 8899
[2026-03-05 10:00:10] [INFO ] User #1 connected from 192.168.1.10:54321
[2026-03-05 10:01:00] [INFO ] User #2 connected from 10.0.0.5:12345
[2026-03-05 10:02:00] [WARN ] User #2 kicked by admin
[2026-03-05 10:02:00] [INFO ] User #2 disconnected
```

## 技术要点

| 技术 | 实现方式 |
|------|----------|
| 进程守护化 | 两次 `fork()` + `setsid()` + 重定向 stdio |
| PID 管理 | `/tmp/cow.pid`，用 `kill(pid, 0)` 探活 |
| 进程间通信 | Unix Domain Socket (`/tmp/cow.sock`) |
| 异步 I/O | `boost::asio::async_accept` / `async_read_some` / `async_write` |
| 信号处理 | `boost::asio::signal_set` (SIGTERM/SIGINT) |
| 日志缓冲 | `std::deque<LogEntry>`，最多保留 1000 条 |
| 带宽统计 | 1 秒定时采样，计算瞬时吞吐量 |
| 用户踢出 | 存储 `std::function<void()>` closer 回调 |

## 连接测试

启动后，可用 `nc` 或 `telnet` 模拟用户连接：

```bash
nc 127.0.0.1 8899   # 连接后输入任意内容，服务器会 echo 回来
```
