#pragma once

#include <string>
#include <unistd.h>

/**
 * Daemon 管理类：负责进程守护化、PID 文件、Unix Socket 路径常量。
 */
class Daemon {
public:
    static const std::string PID_FILE;    // /tmp/cow.pid
    static const std::string SOCK_PATH;   // /tmp/cow.sock
    static const int         DEFAULT_PORT; // 8899

    /// 将当前进程守护化（两次 fork + setsid）
    static bool daemonize();

    /// 将当前 PID 写入 PID 文件
    static bool writePid();

    /// 读取 PID 文件，返回正在运行的进程 PID，否则返回 0
    static pid_t getPid();

    /// 向守护进程发送 SIGTERM
    static bool sendStop();

    /// 删除 PID 文件和 socket 文件（退出时调用）
    static void cleanup();
};
