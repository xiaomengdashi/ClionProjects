/*
 * FTP会话管理头文件
 * 管理FTP连接的会话状态和数据
 */
#ifndef FTP_SESSION_H
#define FTP_SESSION_H

#include <string>
#include <memory>
#include <vector>
#include <chrono>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <sys/socket.h>
#include <netinet/in.h>
#include "ftp_protocol.h"

namespace FTP {

// 会话状态枚举
enum class SessionState {
    CONNECTED,          // 已连接
    AUTHENTICATING,     // 认证中
    AUTHENTICATED,      // 已认证
    TRANSFERRING,       // 传输中
    DISCONNECTED        // 已断开
};

// 数据连接模式
enum class DataConnectionMode {
    NONE,              // 无数据连接
    ACTIVE,            // 主动模式
    PASSIVE            // 被动模式
};

// FTP会话类
class Session {
public:
    // 构造函数
    Session(int controlSocket, const sockaddr_in& clientAddr);
    
    // 析构函数
    ~Session();
    
    // 禁用拷贝构造和赋值
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    
    // 启动会话处理
    void start();
    
    // 停止会话
    void stop();
    
    // 发送响应到客户端
    bool sendResponse(int code, const std::string& message);
    
    // 接收客户端命令
    std::string receiveCommand();
    
    // 处理客户端命令
    void handleCommand(const std::string& command, const std::string& params);
    
    // 获取会话状态
    SessionState getState() const { return state_; }
    
    // 设置会话状态
    void setState(SessionState state) { state_ = state; }
    
    // 获取用户名
    const std::string& getUsername() const { return username_; }
    
    // 获取当前工作目录
    const std::string& getCurrentDirectory() const { return currentDirectory_; }
    
    // 设置当前工作目录
    bool setCurrentDirectory(const std::string& path);
    
private:
    // 会话处理主循环
    void sessionLoop();
    
    // 认证相关命令处理
    void handleUSER(const std::string& username);
    void handlePASS(const std::string& password);
    void handleQUIT();
    
    // 目录操作命令处理
    void handlePWD();
    void handleCWD(const std::string& path);
    void handleCDUP();
    void handleMKD(const std::string& dirname);
    void handleRMD(const std::string& dirname);
    
    // 文件操作命令处理
    void handleDELE(const std::string& filename);
    void handleRNFR(const std::string& filename);
    void handleRNTO(const std::string& filename);
    void handleSIZE(const std::string& filename);
    void handleMDTM(const std::string& filename);
    
    // 传输相关命令处理
    void handleTYPE(const std::string& type);
    void handlePORT(const std::string& params);
    void handlePASV();
    void handleLIST(const std::string& path);
    void handleNLST(const std::string& path);
    void handleRETR(const std::string& filename);
    void handleSTOR(const std::string& filename);
    void handleAPPE(const std::string& filename);
    
    // 系统命令处理
    void handleSYST();
    void handleSTAT();
    void handleHELP(const std::string& command);
    void handleNOOP();
    void handleFEAT();
    
    // 数据连接管理
    bool openDataConnection();
    void closeDataConnection();
    bool sendData(const std::string& data);
    std::string receiveData();
    bool sendFile(const std::string& filepath);
    bool receiveFile(const std::string& filepath, bool append = false);
    
    // 创建被动模式监听套接字
    bool createPassiveListener();
    
    // 连接到主动模式地址
    bool connectToActiveAddress();
    
    // 工具函数
    std::string getAbsolutePath(const std::string& path);
    bool isPathValid(const std::string& path);
    bool isFileExists(const std::string& path);
    bool isDirectory(const std::string& path);
    std::string formatDirListing(const std::string& path, bool detailed);
    
private:
    // 控制连接
    int controlSocket_;                    // 控制连接套接字
    sockaddr_in clientAddr_;               // 客户端地址
    
    // 数据连接
    int dataSocket_;                       // 数据连接套接字
    int passiveListenSocket_;              // 被动模式监听套接字
    DataConnectionMode dataMode_;          // 数据连接模式
    std::string activeHost_;               // 主动模式主机地址
    int activePort_;                       // 主动模式端口
    
    // 会话状态
    std::atomic<SessionState> state_;      // 会话状态
    std::string username_;                 // 用户名
    std::string password_;                 // 密码
    bool authenticated_;                   // 是否已认证
    std::string rootDirectory_;            // 根目录
    std::string currentDirectory_;         // 当前工作目录
    std::string renameFrom_;               // 重命名源文件
    
    // 传输参数
    TransferType transferType_;            // 传输类型
    TransferMode transferMode_;            // 传输模式
    FileStructure fileStructure_;         // 文件结构
    
    // 会话管理
    std::thread sessionThread_;           // 会话处理线程
    std::atomic<bool> running_;           // 运行标志
    std::mutex mutex_;                     // 互斥锁
    std::chrono::steady_clock::time_point lastActivity_; // 最后活动时间
    
    // 缓冲区
    static constexpr size_t BUFFER_SIZE = 8192;
    char buffer_[BUFFER_SIZE];
};

// 会话管理器类
class SessionManager {
public:
    using SessionPtr = std::shared_ptr<Session>;
    
    // 获取单例实例
    static SessionManager& getInstance();
    
    // 创建新会话
    SessionPtr createSession(int socket, const sockaddr_in& clientAddr);
    
    // 移除会话
    void removeSession(SessionPtr session);
    
    // 获取活动会话数
    size_t getSessionCount() const;
    
    // 清理超时会话
    void cleanupTimeoutSessions();
    
private:
    SessionManager() = default;
    ~SessionManager() = default;
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;
    
private:
    std::vector<SessionPtr> sessions_;     // 会话列表
    mutable std::mutex mutex_;             // 互斥锁
};

} // namespace FTP

#endif // FTP_SESSION_H