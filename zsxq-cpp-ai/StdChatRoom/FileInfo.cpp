#include "FileInfo.h"
#include <sstream>
#include <iomanip>
#include <fstream>
#include <random>
#include <functional>

// FileInfo类实现

// 默认构造函数
FileInfo::FileInfo() : fileSize(0), uploadTime(std::time(nullptr)), isGroupFile(false) {
}

// 构造函数
FileInfo::FileInfo(const std::string& id, const std::string& name, const std::string& user, 
                   std::size_t size, bool groupFile)
    : fileId(id), filename(name), uploader(user), fileSize(size), 
      uploadTime(std::time(nullptr)), isGroupFile(groupFile) {
}

// 生成唯一文件ID
std::string FileInfo::generateFileId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);
    
    std::time_t now = std::time(nullptr);
    std::ostringstream oss;
    oss << "FILE_" << now << "_" << dis(gen);
    return oss.str();
}

// 计算文件哈希值（简单实现）
std::string FileInfo::calculateFileHash(const std::string& filePath) const {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }
    
    std::hash<std::string> hasher;
    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    
    size_t hashValue = hasher(content);
    std::ostringstream oss;
    oss << std::hex << hashValue;
    return oss.str();
}

// 序列化文件信息为字符串
std::string FileInfo::serialize() const {
    std::ostringstream oss;
    oss << fileId << "|" 
        << filename << "|" 
        << uploader << "|" 
        << fileSize << "|" 
        << uploadTime << "|" 
        << filePath << "|" 
        << fileHash << "|" 
        << (isGroupFile ? "1" : "0");
    return oss.str();
}

// 从字符串反序列化文件信息
FileInfo FileInfo::deserialize(const std::string& data) {
    FileInfo info;
    std::istringstream iss(data);
    std::string token;
    
    if (std::getline(iss, token, '|')) info.fileId = token;
    if (std::getline(iss, token, '|')) info.filename = token;
    if (std::getline(iss, token, '|')) info.uploader = token;
    if (std::getline(iss, token, '|')) info.fileSize = std::stoull(token);
    if (std::getline(iss, token, '|')) info.uploadTime = std::stoll(token);
    if (std::getline(iss, token, '|')) info.filePath = token;
    if (std::getline(iss, token, '|')) info.fileHash = token;
    if (std::getline(iss, token, '|')) info.isGroupFile = (token == "1");
    
    return info;
}

// 获取格式化的文件信息显示
std::string FileInfo::getDisplayInfo() const {
    std::ostringstream oss;
    oss << "文件名: " << filename << "\n";
    oss << "大小: " << getFileSizeString() << "\n";
    oss << "上传者: " << uploader << "\n";
    oss << "上传时间: " << getUploadTimeString() << "\n";
    oss << "类型: " << (isGroupFile ? "群文件" : "私人文件");
    return oss.str();
}

// 获取文件大小的友好显示
std::string FileInfo::getFileSizeString() const {
    double size = static_cast<double>(fileSize);
    const char* units[] = {"B", "KB", "MB", "GB"};
    int unitIndex = 0;
    
    while (size >= 1024.0 && unitIndex < 3) {
        size /= 1024.0;
        unitIndex++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];
    return oss.str();
}

// 获取上传时间的友好显示
std::string FileInfo::getUploadTimeString() const {
    std::tm* tm_time = std::localtime(&uploadTime);
    std::ostringstream oss;
    oss << std::put_time(tm_time, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// FileTransferSession类实现

// 默认构造函数
FileTransferSession::FileTransferSession() 
    : requestTime(std::time(nullptr)), isAccepted(false), 
      isCompleted(false), transferredBytes(0) {
}

// 构造函数
FileTransferSession::FileTransferSession(const std::string& sender, const std::string& receiver, 
                                       const FileInfo& info)
    : senderId(sender), receiverId(receiver), fileInfo(info),
      requestTime(std::time(nullptr)), isAccepted(false), 
      isCompleted(false), transferredBytes(0) {
    sessionId = generateSessionId();
}

// 生成会话ID
std::string FileTransferSession::generateSessionId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);
    
    std::time_t now = std::time(nullptr);
    std::ostringstream oss;
    oss << "SESSION_" << now << "_" << dis(gen);
    return oss.str();
}

// 获取传输进度百分比
double FileTransferSession::getProgressPercentage() const {
    if (fileInfo.fileSize == 0) return 0.0;
    return (static_cast<double>(transferredBytes) / fileInfo.fileSize) * 100.0;
}

// 序列化会话信息
std::string FileTransferSession::serialize() const {
    std::ostringstream oss;
    oss << sessionId << "|" 
        << senderId << "|" 
        << receiverId << "|" 
        << fileInfo.serialize() << "|" 
        << requestTime << "|" 
        << (isAccepted ? "1" : "0") << "|" 
        << (isCompleted ? "1" : "0") << "|" 
        << transferredBytes;
    return oss.str();
}

// 反序列化会话信息
FileTransferSession FileTransferSession::deserialize(const std::string& data) {
    FileTransferSession session;
    std::istringstream iss(data);
    std::string token;
    
    if (std::getline(iss, token, '|')) session.sessionId = token;
    if (std::getline(iss, token, '|')) session.senderId = token;
    if (std::getline(iss, token, '|')) session.receiverId = token;
    
    // 解析文件信息（需要特殊处理，因为FileInfo也用|分隔）
    std::string fileInfoStr;
    for (int i = 0; i < 8; i++) { // FileInfo有8个字段
        if (std::getline(iss, token, '|')) {
            if (i > 0) fileInfoStr += "|";
            fileInfoStr += token;
        }
    }
    session.fileInfo = FileInfo::deserialize(fileInfoStr);
    
    if (std::getline(iss, token, '|')) session.requestTime = std::stoll(token);
    if (std::getline(iss, token, '|')) session.isAccepted = (token == "1");
    if (std::getline(iss, token, '|')) session.isCompleted = (token == "1");
    if (std::getline(iss, token, '|')) session.transferredBytes = std::stoull(token);
    
    return session;
}