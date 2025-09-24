#ifndef ROOM_MANAGER_H
#define ROOM_MANAGER_H

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <memory>
#include <vector>
#include <chrono>
#include <deque>

/**
 * 用户信息结构体
 */
struct UserInfo {
    std::string userId;      // 用户ID
    std::string userName;    // 用户名
    std::string connectionId; // WebSocket连接ID
};

/**
 * 聊天消息结构体
 */
struct ChatMessage {
    std::string messageId;   // 消息ID
    std::string userId;      // 发送者用户ID
    std::string userName;    // 发送者用户名
    std::string content;     // 消息内容
    std::chrono::system_clock::time_point timestamp; // 时间戳
    
    ChatMessage(const std::string& id, const std::string& uid, const std::string& name, 
               const std::string& msg) 
        : messageId(id), userId(uid), userName(name), content(msg), 
          timestamp(std::chrono::system_clock::now()) {}
};

/**
 * 文件信息结构体
 */
struct FileInfo {
    std::string fileId;       // 文件唯一ID
    std::string originalName; // 原始文件名
    std::string uploaderUserId; // 上传者用户ID
    std::string uploaderUserName; // 上传者用户名
    size_t fileSize;         // 文件大小（字节）
    std::string filePath;    // 服务器存储路径
    std::string mimeType;    // MIME类型
    std::chrono::system_clock::time_point uploadTime; // 上传时间
    
    // 默认构造函数
    FileInfo() : fileSize(0), uploadTime(std::chrono::system_clock::now()) {}
    
    // 带参数的构造函数
    FileInfo(const std::string& id, const std::string& name, const std::string& uid, 
             const std::string& uname, size_t size, const std::string& path, 
             const std::string& mime) 
        : fileId(id), originalName(name), uploaderUserId(uid), uploaderUserName(uname),
          fileSize(size), filePath(path), mimeType(mime), 
          uploadTime(std::chrono::system_clock::now()) {}
};

/**
 * 房间信息结构体
 */
struct RoomInfo {
    std::string roomId;                           // 房间ID
    std::unordered_map<std::string, UserInfo> users; // 房间内的用户
    std::deque<ChatMessage> messages;             // 聊天消息历史（最多保存100条）
    std::unordered_set<std::string> typingUsers;  // 正在输入的用户
    std::unordered_map<std::string, FileInfo> files; // 房间文件列表
    std::mutex mutex;                             // 房间锁
};

/**
 * 房间管理器类
 * 负责管理所有房间的创建、销毁、用户加入和离开
 */
class RoomManager {
public:
    static RoomManager& getInstance() {
        static RoomManager instance;
        return instance;
    }

    /**
     * 用户加入房间
     * @param roomId 房间ID
     * @param userInfo 用户信息
     * @return 是否成功加入
     */
    bool joinRoom(const std::string& roomId, const UserInfo& userInfo);

    /**
     * 用户离开房间
     * @param roomId 房间ID
     * @param userId 用户ID
     * @return 是否成功离开
     */
    bool leaveRoom(const std::string& roomId, const std::string& userId);

    /**
     * 获取房间内的所有用户
     * @param roomId 房间ID
     * @return 用户列表
     */
    std::vector<UserInfo> getRoomUsers(const std::string& roomId);

    /**
     * 根据连接ID查找用户所在的房间
     * @param connectionId WebSocket连接ID
     * @return 房间ID，如果未找到返回空字符串
     */
    std::string findRoomByConnection(const std::string& connectionId);

    /**
     * 根据连接ID移除用户
     * @param connectionId WebSocket连接ID
     */
    void removeUserByConnection(const std::string& connectionId);

    /**
     * 添加聊天消息到房间
     * @param roomId 房间ID
     * @param message 聊天消息
     * @return 是否成功添加
     */
    bool addMessage(const std::string& roomId, const ChatMessage& message);

    /**
     * 获取房间的聊天消息历史
     * @param roomId 房间ID
     * @param limit 获取的消息数量限制（默认50条）
     * @return 消息列表
     */
    std::vector<ChatMessage> getMessages(const std::string& roomId, size_t limit = 50);

    /**
     * 设置用户输入状态
     * @param roomId 房间ID
     * @param userId 用户ID
     * @param isTyping 是否正在输入
     * @return 是否成功设置
     */
    bool setUserTyping(const std::string& roomId, const std::string& userId, bool isTyping);

    /**
     * 获取正在输入的用户列表
     * @param roomId 房间ID
     * @return 正在输入的用户ID列表
     */
    std::vector<std::string> getTypingUsers(const std::string& roomId);

    /**
     * 添加文件到房间
     * @param roomId 房间ID
     * @param fileInfo 文件信息
     * @return 是否成功添加
     */
    bool addFile(const std::string& roomId, const FileInfo& fileInfo);

    /**
     * 获取房间的文件列表
     * @param roomId 房间ID
     * @return 文件列表
     */
    std::vector<FileInfo> getFiles(const std::string& roomId);

    /**
     * 根据文件ID获取文件信息
     * @param fileId 文件ID
     * @return 文件信息，如果未找到返回nullptr
     */
    std::shared_ptr<FileInfo> getFileById(const std::string& fileId);

    /**
     * 验证用户是否有权限访问文件
     * @param fileId 文件ID
     * @param connectionId 用户连接ID
     * @return 是否有权限
     */
    bool hasFilePermission(const std::string& fileId, const std::string& connectionId);

    /**
     * 删除房间的所有文件
     * @param roomId 房间ID
     * @return 是否成功删除
     */
    bool deleteRoomFiles(const std::string& roomId);

    /**
     * 生成文件ID
     * @return 唯一的文件ID
     */
    std::string generateFileId();

    /**
     * 获取文件的MIME类型
     * @param filename 文件名
     * @return MIME类型
     */
    std::string getMimeType(const std::string& filename);

    /**
     * 验证文件类型是否允许
     * @param filename 文件名
     * @return 是否允许上传
     */
    bool isFileTypeAllowed(const std::string& filename);

private:
    RoomManager() = default;
    ~RoomManager() = default;
    RoomManager(const RoomManager&) = delete;
    RoomManager& operator=(const RoomManager&) = delete;

    std::unordered_map<std::string, std::shared_ptr<RoomInfo>> rooms_; // 所有房间
    std::unordered_map<std::string, std::string> connectionToRoom_;    // 连接ID到房间ID的映射
    std::unordered_map<std::string, std::string> fileToRoom_;          // 文件ID到房间ID的映射
    std::mutex globalMutex_;                                           // 全局锁
};

#endif // ROOM_MANAGER_H 