#ifndef FILE_INFO_H
#define FILE_INFO_H

#include <string>
#include <ctime>
#include <vector>

// 文件信息类 - 存储文件的基本信息
class FileInfo {
public:
    std::string fileId;         // 文件唯一标识符
    std::string filename;       // 原始文件名
    std::string uploader;       // 上传者用户名
    std::size_t fileSize;       // 文件大小（字节）
    std::time_t uploadTime;     // 上传时间
    std::string filePath;       // 服务器上的文件路径
    std::string fileHash;       // 文件哈希值（用于验证）
    bool isGroupFile;          // 是否为群文件
    
    // 构造函数
    FileInfo();
    FileInfo(const std::string& id, const std::string& name, const std::string& user, 
             std::size_t size, bool groupFile = false);
    
    // 生成唯一文件ID
    static std::string generateFileId();
    
    // 计算文件哈希值
    std::string calculateFileHash(const std::string& filePath) const;
    
    // 序列化文件信息为字符串
    std::string serialize() const;
    
    // 从字符串反序列化文件信息
    static FileInfo deserialize(const std::string& data);
    
    // 获取格式化的文件信息显示
    std::string getDisplayInfo() const;
    
    // 获取文件大小的友好显示
    std::string getFileSizeString() const;
    
    // 获取上传时间的友好显示
    std::string getUploadTimeString() const;
};

// 文件传输会话信息
class FileTransferSession {
public:
    std::string sessionId;      // 会话ID
    std::string senderId;       // 发送者用户名
    std::string receiverId;     // 接收者用户名
    FileInfo fileInfo;          // 文件信息
    std::time_t requestTime;    // 请求时间
    bool isAccepted;           // 是否已接受
    bool isCompleted;          // 是否已完成
    std::size_t transferredBytes; // 已传输字节数
    
    // 构造函数
    FileTransferSession();
    FileTransferSession(const std::string& sender, const std::string& receiver, 
                       const FileInfo& info);
    
    // 生成会话ID
    static std::string generateSessionId();
    
    // 获取传输进度百分比
    double getProgressPercentage() const;
    
    // 序列化会话信息
    std::string serialize() const;
    
    // 反序列化会话信息
    static FileTransferSession deserialize(const std::string& data);
};

#endif // FILE_INFO_H