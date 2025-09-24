#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include "FileInfo.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>

// 文件管理器类 - 负责文件存储、传输会话管理
class FileManager {
private:
    std::unordered_map<std::string, FileInfo> groupFiles;           // 群文件映射（文件ID -> 文件信息）
    std::unordered_map<std::string, FileTransferSession> sessions;  // 传输会话映射（会话ID -> 会话信息）
    std::string groupFilesDir;                                      // 群文件存储目录
    std::string tempFilesDir;                                       // 临时文件目录
    std::string dataFilePath;                                       // 文件数据持久化路径
    mutable std::mutex filesMutex;                                  // 文件数据互斥锁
    mutable std::mutex sessionsMutex;                               // 会话互斥锁

public:
    // 构造函数
    FileManager(const std::string& groupDir = "group_files", 
               const std::string& tempDir = "temp_files",
               const std::string& dataFile = "files.dat");
    
    // 析构函数
    ~FileManager();
    
    // 文件上传相关
    bool uploadGroupFile(const std::string& username, const std::string& localFilePath, 
                        const std::string& filename);
    
    // 创建文件传输会话（私聊）
    std::string createTransferSession(const std::string& sender, const std::string& receiver,
                                    const std::string& localFilePath, const std::string& filename);
    
    // 处理文件传输响应
    bool acceptFileTransfer(const std::string& sessionId);
    bool rejectFileTransfer(const std::string& sessionId);
    
    // 文件下载相关
    bool downloadGroupFile(const std::string& fileId, const std::string& localPath);
    bool downloadPrivateFile(const std::string& sessionId, const std::string& localPath);
    
    // 查询功能
    std::vector<FileInfo> getGroupFileList() const;
    FileInfo getFileInfo(const std::string& fileId) const;
    FileTransferSession getTransferSession(const std::string& sessionId) const;
    std::vector<FileTransferSession> getUserTransferSessions(const std::string& username) const;
    
    // 会话管理
    bool isValidSession(const std::string& sessionId) const;
    bool isSessionCompleted(const std::string& sessionId) const;
    void updateSessionProgress(const std::string& sessionId, std::size_t transferredBytes);
    void completeSession(const std::string& sessionId);
    void removeSession(const std::string& sessionId);
    
    // 文件操作辅助函数
    bool fileExists(const std::string& filePath) const;
    std::size_t getFileSize(const std::string& filePath) const;
    bool copyFile(const std::string& srcPath, const std::string& destPath) const;
    bool deleteFile(const std::string& filePath) const;
    
    // 数据持久化
    bool saveToFile() const;
    bool loadFromFile();
    
    // 目录管理
    bool createDirectories();
    void cleanupTempFiles();

private:
    // 内部辅助函数
    std::string getGroupFilePath(const std::string& fileId) const;
    std::string getTempFilePath(const std::string& sessionId) const;
    std::string generateUniqueFilename(const std::string& originalName) const;
    bool ensureDirectoryExists(const std::string& dirPath) const;
    std::string trim(const std::string& str) const;
};

#endif // FILE_MANAGER_H