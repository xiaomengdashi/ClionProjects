#include "FileManager.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <sstream>

// 构造函数
FileManager::FileManager(const std::string& groupDir, const std::string& tempDir, 
                        const std::string& dataFile)
    : groupFilesDir(groupDir), tempFilesDir(tempDir), dataFilePath(dataFile) {
    
    // 创建必要的目录
    createDirectories();
    
    // 加载已有的文件数据
    loadFromFile();
    
    std::cout << "[文件管理] 文件管理器初始化完成" << std::endl;
}

// 析构函数
FileManager::~FileManager() {
    saveToFile();
    cleanupTempFiles();
}

// 上传群文件
bool FileManager::uploadGroupFile(const std::string& username, const std::string& localFilePath, 
                                 const std::string& filename) {
    std::lock_guard<std::mutex> lock(filesMutex);
    
    // 检查文件是否存在
    if (!fileExists(localFilePath)) {
        std::cerr << "[文件管理] 文件不存在: " << localFilePath << std::endl;
        return false;
    }
    
    // 创建文件信息
    std::string fileId = FileInfo::generateFileId();
    std::size_t fileSize = getFileSize(localFilePath);
    FileInfo fileInfo(fileId, filename, username, fileSize, true);
    
    // 计算文件存储路径
    std::string destPath = getGroupFilePath(fileId);
    fileInfo.filePath = destPath;
    
    // 复制文件到群文件目录
    if (!copyFile(localFilePath, destPath)) {
        std::cerr << "[文件管理] 文件复制失败" << std::endl;
        return false;
    }
    
    // 计算文件哈希
    fileInfo.fileHash = fileInfo.calculateFileHash(destPath);
    
    // 存储文件信息
    groupFiles[fileId] = fileInfo;
    
    // 立即保存到文件
    saveToFile();
    
    std::cout << "[文件管理] 群文件上传成功: " << filename << " (ID: " << fileId << ")" << std::endl;
    return true;
}

// 创建文件传输会话
std::string FileManager::createTransferSession(const std::string& sender, const std::string& receiver,
                                              const std::string& localFilePath, const std::string& filename) {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    
    // 检查文件是否存在
    if (!fileExists(localFilePath)) {
        std::cerr << "[文件管理] 文件不存在: " << localFilePath << std::endl;
        return "";
    }
    
    // 创建文件信息
    std::string fileId = FileInfo::generateFileId();
    std::size_t fileSize = getFileSize(localFilePath);
    FileInfo fileInfo(fileId, filename, sender, fileSize, false);
    
    // 创建传输会话
    FileTransferSession session(sender, receiver, fileInfo);
    
    // 复制文件到临时目录
    std::string tempPath = getTempFilePath(session.sessionId);
    if (!copyFile(localFilePath, tempPath)) {
        std::cerr << "[文件管理] 临时文件创建失败" << std::endl;
        return "";
    }
    
    session.fileInfo.filePath = tempPath;
    session.fileInfo.fileHash = session.fileInfo.calculateFileHash(tempPath);
    
    // 存储会话信息
    sessions[session.sessionId] = session;
    
    std::cout << "[文件管理] 文件传输会话创建: " << filename << " (会话ID: " << session.sessionId << ")" << std::endl;
    return session.sessionId;
}

// 接受文件传输
bool FileManager::acceptFileTransfer(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    
    auto it = sessions.find(sessionId);
    if (it == sessions.end()) {
        return false;
    }
    
    it->second.isAccepted = true;
    std::cout << "[文件管理] 文件传输被接受: " << sessionId << std::endl;
    return true;
}

// 拒绝文件传输
bool FileManager::rejectFileTransfer(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    
    auto it = sessions.find(sessionId);
    if (it == sessions.end()) {
        return false;
    }
    
    // 删除临时文件
    deleteFile(it->second.fileInfo.filePath);
    
    // 移除会话
    sessions.erase(it);
    
    std::cout << "[文件管理] 文件传输被拒绝: " << sessionId << std::endl;
    return true;
}

// 下载群文件
bool FileManager::downloadGroupFile(const std::string& fileId, const std::string& localPath) {
    std::lock_guard<std::mutex> lock(filesMutex);
    
    auto it = groupFiles.find(fileId);
    if (it == groupFiles.end()) {
        std::cerr << "[文件管理] 文件不存在: " << fileId << std::endl;
        return false;
    }
    
    if (!copyFile(it->second.filePath, localPath)) {
        std::cerr << "[文件管理] 文件下载失败" << std::endl;
        return false;
    }
    
    std::cout << "[文件管理] 文件下载成功: " << it->second.filename << std::endl;
    return true;
}

// 下载私人文件
bool FileManager::downloadPrivateFile(const std::string& sessionId, const std::string& localPath) {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    
    auto it = sessions.find(sessionId);
    if (it == sessions.end() || !it->second.isAccepted) {
        std::cerr << "[文件管理] 会话无效或未接受: " << sessionId << std::endl;
        return false;
    }
    
    if (!copyFile(it->second.fileInfo.filePath, localPath)) {
        std::cerr << "[文件管理] 私人文件下载失败" << std::endl;
        return false;
    }
    
    // 标记会话完成
    it->second.isCompleted = true;
    it->second.transferredBytes = it->second.fileInfo.fileSize;
    
    std::cout << "[文件管理] 私人文件下载成功: " << it->second.fileInfo.filename << std::endl;
    return true;
}

// 获取群文件列表
std::vector<FileInfo> FileManager::getGroupFileList() const {
    std::lock_guard<std::mutex> lock(filesMutex);
    
    std::vector<FileInfo> fileList;
    for (const auto& pair : groupFiles) {
        fileList.push_back(pair.second);
    }
    
    // 按上传时间排序（最新的在前）
    std::sort(fileList.begin(), fileList.end(), 
              [](const FileInfo& a, const FileInfo& b) {
                  return a.uploadTime > b.uploadTime;
              });
    
    return fileList;
}

// 获取文件信息
FileInfo FileManager::getFileInfo(const std::string& fileId) const {
    std::lock_guard<std::mutex> lock(filesMutex);
    
    auto it = groupFiles.find(fileId);
    if (it != groupFiles.end()) {
        return it->second;
    }
    return FileInfo();
}

// 获取传输会话
FileTransferSession FileManager::getTransferSession(const std::string& sessionId) const {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    
    auto it = sessions.find(sessionId);
    if (it != sessions.end()) {
        return it->second;
    }
    return FileTransferSession();
}

// 获取用户相关的传输会话
std::vector<FileTransferSession> FileManager::getUserTransferSessions(const std::string& username) const {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    
    std::vector<FileTransferSession> userSessions;
    for (const auto& pair : sessions) {
        if (pair.second.senderId == username || pair.second.receiverId == username) {
            userSessions.push_back(pair.second);
        }
    }
    
    return userSessions;
}

// 检查会话有效性
bool FileManager::isValidSession(const std::string& sessionId) const {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    return sessions.find(sessionId) != sessions.end();
}

// 检查会话是否完成
bool FileManager::isSessionCompleted(const std::string& sessionId) const {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    
    auto it = sessions.find(sessionId);
    return it != sessions.end() && it->second.isCompleted;
}

// 更新会话进度
void FileManager::updateSessionProgress(const std::string& sessionId, std::size_t transferredBytes) {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    
    auto it = sessions.find(sessionId);
    if (it != sessions.end()) {
        it->second.transferredBytes = transferredBytes;
        if (transferredBytes >= it->second.fileInfo.fileSize) {
            it->second.isCompleted = true;
        }
    }
}

// 完成会话
void FileManager::completeSession(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    
    auto it = sessions.find(sessionId);
    if (it != sessions.end()) {
        it->second.isCompleted = true;
        it->second.transferredBytes = it->second.fileInfo.fileSize;
    }
}

// 移除会话
void FileManager::removeSession(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    
    auto it = sessions.find(sessionId);
    if (it != sessions.end()) {
        // 删除临时文件
        deleteFile(it->second.fileInfo.filePath);
        sessions.erase(it);
    }
}

// 文件操作辅助函数
bool FileManager::fileExists(const std::string& filePath) const {
    struct stat buffer;
    return (stat(filePath.c_str(), &buffer) == 0);
}

std::size_t FileManager::getFileSize(const std::string& filePath) const {
    struct stat buffer;
    if (stat(filePath.c_str(), &buffer) == 0) {
        return buffer.st_size;
    }
    return 0;
}

bool FileManager::copyFile(const std::string& srcPath, const std::string& destPath) const {
    std::ifstream src(srcPath, std::ios::binary);
    std::ofstream dest(destPath, std::ios::binary);
    
    if (!src.is_open()) {
        std::cerr << "[文件管理] 无法打开源文件: " << srcPath << std::endl;
        return false;
    }
    
    if (!dest.is_open()) {
        std::cerr << "[文件管理] 无法创建目标文件: " << destPath << std::endl;
        return false;
    }
    
    dest << src.rdbuf();
    bool success = src.good() && dest.good();
    
    if (!success) {
        std::cerr << "[文件管理] 文件复制过程中出错" << std::endl;
    }
    
    return success;
}

bool FileManager::deleteFile(const std::string& filePath) const {
    return unlink(filePath.c_str()) == 0;
}

// 保存数据到文件
bool FileManager::saveToFile() const {
    std::ofstream file(dataFilePath, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "[文件管理] 无法打开数据文件进行写入: " << dataFilePath << std::endl;
        return false;
    }
    
    // 保存群文件信息
    file << "[GROUP_FILES]" << std::endl;
    for (const auto& pair : groupFiles) {
        file << pair.second.serialize() << std::endl;
    }
    
    file.close();
    return true;
}

// 从文件加载数据
bool FileManager::loadFromFile() {
    std::ifstream file(dataFilePath);
    if (!file.is_open()) {
        std::cout << "[文件管理] 数据文件不存在，将创建新文件: " << dataFilePath << std::endl;
        return true;
    }
    
    std::string line;
    bool inGroupFiles = false;
    int loadedCount = 0;
    
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty()) continue;
        
        if (line == "[GROUP_FILES]") {
            inGroupFiles = true;
            continue;
        }
        
        if (inGroupFiles && !line.empty()) {
            try {
                FileInfo fileInfo = FileInfo::deserialize(line);
                if (!fileInfo.fileId.empty()) {
                    groupFiles[fileInfo.fileId] = fileInfo;
                    loadedCount++;
                }
            } catch (const std::exception& e) {
                std::cerr << "[文件管理] 解析文件数据失败: " << line << " 错误: " << e.what() << std::endl;
            }
        }
    }
    
    file.close();
    std::cout << "[文件管理] 从文件加载了 " << loadedCount << " 个群文件数据" << std::endl;
    return true;
}

// 创建目录
bool FileManager::createDirectories() {
    bool success = true;
    
    success &= ensureDirectoryExists(groupFilesDir);
    success &= ensureDirectoryExists(tempFilesDir);
    
    if (success) {
        std::cout << "[文件管理] 目录创建成功" << std::endl;
    } else {
        std::cerr << "[文件管理] 目录创建失败" << std::endl;
    }
    
    return success;
}

// 清理临时文件
void FileManager::cleanupTempFiles() {
    DIR* dir = opendir(tempFilesDir.c_str());
    if (dir == nullptr) return;
    
    struct dirent* entry;
    int cleanedCount = 0;
    
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_REG) { // 普通文件
            std::string filePath = tempFilesDir + "/" + entry->d_name;
            if (deleteFile(filePath)) {
                cleanedCount++;
            }
        }
    }
    
    closedir(dir);
    
    if (cleanedCount > 0) {
        std::cout << "[文件管理] 清理了 " << cleanedCount << " 个临时文件" << std::endl;
    }
}

// 内部辅助函数
std::string FileManager::getGroupFilePath(const std::string& fileId) const {
    return groupFilesDir + "/" + fileId;
}

std::string FileManager::getTempFilePath(const std::string& sessionId) const {
    return tempFilesDir + "/" + sessionId;
}

std::string FileManager::generateUniqueFilename(const std::string& originalName) const {
    std::string baseName = originalName;
    std::string extension = "";
    
    // 分离文件名和扩展名
    size_t dotPos = originalName.find_last_of('.');
    if (dotPos != std::string::npos) {
        baseName = originalName.substr(0, dotPos);
        extension = originalName.substr(dotPos);
    }
    
    std::string uniqueName = originalName;
    int counter = 1;
    
    while (fileExists(groupFilesDir + "/" + uniqueName)) {
        std::ostringstream oss;
        oss << baseName << "(" << counter << ")" << extension;
        uniqueName = oss.str();
        counter++;
    }
    
    return uniqueName;
}

bool FileManager::ensureDirectoryExists(const std::string& dirPath) const {
    struct stat info;
    
    if (stat(dirPath.c_str(), &info) != 0) {
        // 目录不存在，创建它
        if (mkdir(dirPath.c_str(), 0755) != 0) {
            std::cerr << "[文件管理] 无法创建目录: " << dirPath << std::endl;
            return false;
        }
    } else if (!(info.st_mode & S_IFDIR)) {
        std::cerr << "[文件管理] 路径存在但不是目录: " << dirPath << std::endl;
        return false;
    }
    
    return true;
}

std::string FileManager::trim(const std::string& str) const {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}