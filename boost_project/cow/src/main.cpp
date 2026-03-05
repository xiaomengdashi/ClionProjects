#include "Daemon.h"
#include "IpcClient.h"
#include "Server.h"

#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>

#include <cstdlib>
#include <iostream>
#include <string>

// ─── 帮助信息 ─────────────────────────────────────────────────────────────────

static void printUsage(const char* prog) {
    std::cout <<
        "COW - 命令行服务器管理工具\n"
        "\n"
        "用法:\n"
        "  " << prog << " start [--port PORT]   在后台启动 cow 守护进程 (默认端口: 8899)\n"
        "  " << prog << " stop                  停止守护进程\n"
        "  " << prog << " status                查看守护进程状态\n"
        "  " << prog << " list                  列出当前连接的所有用户\n"
        "  " << prog << " log [N]               查看最新 N 条日志 (默认: 100)\n"
        "  " << prog << " stats                 查看服务器流量统计\n"
        "  " << prog << " kick <ID>             踢出指定 ID 的用户\n"
        "  " << prog << " help                  显示本帮助信息\n"
        "\n"
        "示例:\n"
        "  cow start --port 9000\n"
        "  cow list\n"
        "  cow log 20\n"
        "  cow stats\n"
        "  cow kick 3\n"
        "  cow stop\n";
}

// ─── start 模式（守护进程内部）────────────────────────────────────────────────

static int runDaemon(int port) {
    // 此时已在守护进程中（daemonize 已完成）
    if (!Daemon::writePid()) {
        // 无法写 PID 文件（stderr 已关闭，只能静默退出）
        return 1;
    }

    boost::asio::io_context ioc;

    // 信号处理：优雅退出
    boost::asio::signal_set signals(ioc, SIGTERM, SIGINT);
    signals.async_wait([&](boost::system::error_code /*ec*/, int /*sig*/) {
        ioc.stop();
    });

    try {
        Server server(ioc, port);
        server.start();
        ioc.run();
    } catch (const std::exception& e) {
        // 无法输出，直接退出
        (void)e;
    }

    Daemon::cleanup();
    return 0;
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 0;
    }

    std::string cmd = argv[1];

    // ── help ──────────────────────────────────────────────────────────────────
    if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        printUsage(argv[0]);
        return 0;
    }

    // ── start ─────────────────────────────────────────────────────────────────
    if (cmd == "start") {
        int port = Daemon::DEFAULT_PORT;
        for (int i = 2; i < argc; ++i) {
            if (std::string(argv[i]) == "--port" && i + 1 < argc) {
                try { port = std::stoi(argv[++i]); }
                catch (...) {
                    std::cerr << "Invalid port: " << argv[i] << "\n";
                    return 1;
                }
            }
        }

        pid_t existing = Daemon::getPid();
        if (existing > 0) {
            std::cerr << "cow is already running (pid=" << existing << ")\n";
            return 1;
        }

        // 在 fork 前打印，fork 后 stdout 关闭
        std::cout << "Starting COW daemon on port " << port << "...\n";
        std::cout.flush();

        if (!Daemon::daemonize()) {
            std::cerr << "Failed to daemonize process\n";
            return 1;
        }

        // ─ 以下运行在守护进程中 ─
        return runDaemon(port);
    }

    // ── stop ──────────────────────────────────────────────────────────────────
    if (cmd == "stop") {
        pid_t pid = Daemon::getPid();
        if (pid == 0) {
            std::cerr << "cow is not running\n";
            return 1;
        }
        try {
            IpcClient client(Daemon::SOCK_PATH);
            std::cout << client.sendCommand("STOP");
        } catch (...) {
            // IPC 失败，回退到 SIGTERM
            if (Daemon::sendStop())
                std::cout << "cow daemon stopped (via signal)\n";
            else
                std::cerr << "Failed to stop cow daemon\n";
        }
        return 0;
    }

    // ── status ────────────────────────────────────────────────────────────────
    if (cmd == "status") {
        pid_t pid = Daemon::getPid();
        if (pid > 0)
            std::cout << "cow is running  (pid=" << pid << ")\n";
        else
            std::cout << "cow is stopped\n";
        return 0;
    }

    // ── 以下命令需要守护进程在线 ──────────────────────────────────────────────
    try {
        IpcClient client(Daemon::SOCK_PATH);

        if (cmd == "list") {
            std::cout << client.sendCommand("LIST");

        } else if (cmd == "log") {
            int n = 100;
            if (argc >= 3) {
                try { n = std::stoi(argv[2]); }
                catch (...) {
                    std::cerr << "Invalid number: " << argv[2] << "\n";
                    return 1;
                }
            }
            std::cout << client.sendCommand("LOG " + std::to_string(n));

        } else if (cmd == "stats") {
            std::cout << client.sendCommand("STATS");

        } else if (cmd == "kick") {
            if (argc < 3) {
                std::cerr << "Usage: cow kick <id>\n";
                return 1;
            }
            std::cout << client.sendCommand("KICK " + std::string(argv[2]));

        } else {
            std::cerr << "Unknown command: " << cmd << "\n\n";
            printUsage(argv[0]);
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }

    return 0;
}
