#include "Daemon.h"

#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <csignal>
#include <sys/stat.h>
#include <unistd.h>

const std::string Daemon::PID_FILE    = "/tmp/cow.pid";
const std::string Daemon::SOCK_PATH   = "/tmp/cow.sock";
const int         Daemon::DEFAULT_PORT = 8899;

bool Daemon::daemonize() {
    // ── 第一次 fork ────────────────────────────────────────────────────────────
    pid_t pid = ::fork();
    if (pid < 0)  return false;
    if (pid > 0)  ::_exit(0);   // 父进程退出，让 shell 提示符返回

    // 成为新会话领导者，脱离控制终端
    if (::setsid() < 0) return false;

    // ── 第二次 fork（防止重新获取终端）────────────────────────────────────────
    pid = ::fork();
    if (pid < 0)  return false;
    if (pid > 0)  ::_exit(0);

    // 设置文件权限掩码、切换工作目录
    ::umask(0);
    if (::chdir("/") < 0) return false;

    // 重定向标准 I/O 到 /dev/null
    int devnull = ::open("/dev/null", O_RDWR);
    if (devnull >= 0) {
        ::dup2(devnull, STDIN_FILENO);
        ::dup2(devnull, STDOUT_FILENO);
        ::dup2(devnull, STDERR_FILENO);
        if (devnull > 2) ::close(devnull);
    }

    return true;
}

bool Daemon::writePid() {
    std::ofstream f(PID_FILE);
    if (!f) return false;
    f << ::getpid() << '\n';
    return true;
}

pid_t Daemon::getPid() {
    std::ifstream f(PID_FILE);
    if (!f) return 0;
    pid_t pid = 0;
    f >> pid;
    if (pid <= 0) return 0;
    // 用 kill(pid, 0) 探测进程是否存在
    if (::kill(pid, 0) == 0) return pid;
    return 0;
}

bool Daemon::sendStop() {
    pid_t pid = getPid();
    if (pid == 0) return false;
    return ::kill(pid, SIGTERM) == 0;
}

void Daemon::cleanup() {
    ::unlink(PID_FILE.c_str());
    ::unlink(SOCK_PATH.c_str());
}
