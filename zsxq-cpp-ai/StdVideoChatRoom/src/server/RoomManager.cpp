#include "RoomManager.h"
#include <algorithm>
#include <iostream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <random>
#include <fstream>
#include <filesystem>
#include <uuid/uuid.h>

bool RoomManager::joinRoom(const std::string& roomId, const UserInfo& userInfo) {
    std::lock_guard<std::mutex> lock(globalMutex_);
    
    std::cout << "尝试加入房间: " << roomId << "，用户: " << userInfo.userName 
              << "，连接ID: " << userInfo.connectionId << std::endl;
    
    // 检查用户是否已经在其他房间
    auto it = connectionToRoom_.find(userInfo.connectionId);
    if (it != connectionToRoom_.end()) {
        std::cout << "用户已在房间 " << it->second << " 中，先离开" << std::endl;
        // 先离开当前房间
        leaveRoom(it->second, userInfo.userId);
    }
    
    // 查找或创建房间
    auto roomIt = rooms_.find(roomId);
    if (roomIt == rooms_.end()) {
        // 创建新房间
        auto room = std::make_shared<RoomInfo>();
        room->roomId = roomId;
        rooms_[roomId] = room;
        roomIt = rooms_.find(roomId);
        std::cout << "创建新房间: " << roomId << std::endl;
    } else {
        std::cout << "找到现有房间: " << roomId << "，当前用户数: " << roomIt->second->users.size() << std::endl;
    }
    
    // 加入房间
    {
        std::lock_guard<std::mutex> roomLock(roomIt->second->mutex);
        roomIt->second->users[userInfo.userId] = userInfo;
        connectionToRoom_[userInfo.connectionId] = roomId;
    }
    
    std::cout << "用户 " << userInfo.userName << " 成功加入房间 " << roomId 
              << "，房间内用户数: " << roomIt->second->users.size() << std::endl;
    return true;
}

bool RoomManager::leaveRoom(const std::string& roomId, const std::string& userId) {
    std::lock_guard<std::mutex> lock(globalMutex_);
    
    auto roomIt = rooms_.find(roomId);
    if (roomIt == rooms_.end()) {
        return false;
    }
    
    std::string connectionId;
    std::vector<std::string> filesToDelete;
    bool shouldDestroyRoom = false;
    
    {
        std::lock_guard<std::mutex> roomLock(roomIt->second->mutex);
        
        auto userIt = roomIt->second->users.find(userId);
        if (userIt == roomIt->second->users.end()) {
            return false;
        }
        
        connectionId = userIt->second.connectionId;
        std::cout << "用户 " << userIt->second.userName << " 离开房间 " << roomId << std::endl;
        
        roomIt->second->users.erase(userIt);
        
        // 如果房间为空，准备销毁房间
        if (roomIt->second->users.empty()) {
            std::cout << "房间 " << roomId << " 已空，准备销毁房间" << std::endl;
            shouldDestroyRoom = true;
            
            // 收集要删除的文件ID
            for (const auto& pair : roomIt->second->files) {
                filesToDelete.push_back(pair.first);
            }
        }
    }
    
    // 在房间锁外处理房间销毁，避免死锁
    if (shouldDestroyRoom) {
        std::cout << "销毁房间 " << roomId << std::endl;
        
        // 先删除房间对象
        rooms_.erase(roomIt);
        
        // 删除文件映射
        for (const auto& fileId : filesToDelete) {
            fileToRoom_.erase(fileId);
        }
        
        // 删除物理文件目录（不需要房间锁）
        std::string uploadDir = "uploads/" + roomId;
        try {
            if (std::filesystem::exists(uploadDir)) {
                std::filesystem::remove_all(uploadDir);
                std::cout << "已删除房间 " << roomId << " 的文件目录" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "删除房间 " << roomId << " 文件目录失败: " << e.what() << std::endl;
        }
    }
    
    // 移除连接映射
    connectionToRoom_.erase(connectionId);
    
    return true;
}

std::vector<UserInfo> RoomManager::getRoomUsers(const std::string& roomId) {
    std::lock_guard<std::mutex> lock(globalMutex_);
    
    auto roomIt = rooms_.find(roomId);
    if (roomIt == rooms_.end()) {
        return {};
    }
    
    std::vector<UserInfo> users;
    {
        std::lock_guard<std::mutex> roomLock(roomIt->second->mutex);
        for (const auto& pair : roomIt->second->users) {
            users.push_back(pair.second);
        }
    }
    
    return users;
}

std::string RoomManager::findRoomByConnection(const std::string& connectionId) {
    std::lock_guard<std::mutex> lock(globalMutex_);
    
    auto it = connectionToRoom_.find(connectionId);
    if (it != connectionToRoom_.end()) {
        return it->second;
    }
    
    return "";
}

void RoomManager::removeUserByConnection(const std::string& connectionId) {
    std::lock_guard<std::mutex> lock(globalMutex_);
    
    auto it = connectionToRoom_.find(connectionId);
    if (it == connectionToRoom_.end()) {
        return;
    }
    
    std::string roomId = it->second;
    auto roomIt = rooms_.find(roomId);
    if (roomIt != rooms_.end()) {
        std::vector<std::string> filesToDelete;
        bool shouldDestroyRoom = false;
        
        {
            std::lock_guard<std::mutex> roomLock(roomIt->second->mutex);
            
            // 查找并移除用户
            for (auto userIt = roomIt->second->users.begin(); userIt != roomIt->second->users.end(); ++userIt) {
                if (userIt->second.connectionId == connectionId) {
                    std::cout << "用户 " << userIt->second.userName << " 断开连接，离开房间 " << roomId << std::endl;
                    roomIt->second->users.erase(userIt);
                    break;
                }
            }
            
            // 如果房间为空，准备销毁房间
            if (roomIt->second->users.empty()) {
                std::cout << "房间 " << roomId << " 已空，准备销毁房间" << std::endl;
                shouldDestroyRoom = true;
                
                // 收集要删除的文件ID
                for (const auto& pair : roomIt->second->files) {
                    filesToDelete.push_back(pair.first);
                }
            }
        }
        
        // 在房间锁外处理房间销毁，避免死锁
        if (shouldDestroyRoom) {
            std::cout << "销毁房间 " << roomId << std::endl;
            
            // 先删除房间对象
            rooms_.erase(roomIt);
            
            // 删除文件映射
            for (const auto& fileId : filesToDelete) {
                fileToRoom_.erase(fileId);
            }
            
            // 删除物理文件目录（不需要房间锁）
            std::string uploadDir = "uploads/" + roomId;
            try {
                if (std::filesystem::exists(uploadDir)) {
                    std::filesystem::remove_all(uploadDir);
                    std::cout << "已删除房间 " << roomId << " 的文件目录" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "删除房间 " << roomId << " 文件目录失败: " << e.what() << std::endl;
            }
        }
    }
    
    connectionToRoom_.erase(connectionId);
}

bool RoomManager::addMessage(const std::string& roomId, const ChatMessage& message) {
    std::lock_guard<std::mutex> lock(globalMutex_);
    
    auto roomIt = rooms_.find(roomId);
    if (roomIt == rooms_.end()) {
        return false;
    }
    
    {
        std::lock_guard<std::mutex> roomLock(roomIt->second->mutex);
        roomIt->second->messages.push_back(message);
        
        // 限制消息历史数量，保持最新的100条消息
        if (roomIt->second->messages.size() > 100) {
            roomIt->second->messages.pop_front();
        }
    }
    
    std::cout << "用户 " << message.userName << " 在房间 " << roomId << " 发送消息: " << message.content << std::endl;
    return true;
}

std::vector<ChatMessage> RoomManager::getMessages(const std::string& roomId, size_t limit) {
    std::lock_guard<std::mutex> lock(globalMutex_);
    
    auto roomIt = rooms_.find(roomId);
    if (roomIt == rooms_.end()) {
        return {};
    }
    
    std::vector<ChatMessage> messages;
    {
        std::lock_guard<std::mutex> roomLock(roomIt->second->mutex);
        
        size_t start = 0;
        if (roomIt->second->messages.size() > limit) {
            start = roomIt->second->messages.size() - limit;
        }
        
        for (size_t i = start; i < roomIt->second->messages.size(); ++i) {
            messages.push_back(roomIt->second->messages[i]);
        }
    }
    
    return messages;
}

bool RoomManager::setUserTyping(const std::string& roomId, const std::string& userId, bool isTyping) {
    std::lock_guard<std::mutex> lock(globalMutex_);
    
    auto roomIt = rooms_.find(roomId);
    if (roomIt == rooms_.end()) {
        return false;
    }
    
    {
        std::lock_guard<std::mutex> roomLock(roomIt->second->mutex);
        
        if (isTyping) {
            roomIt->second->typingUsers.insert(userId);
        } else {
            roomIt->second->typingUsers.erase(userId);
        }
    }
    
    return true;
}

std::vector<std::string> RoomManager::getTypingUsers(const std::string& roomId) {
    std::lock_guard<std::mutex> lock(globalMutex_);
    
    auto roomIt = rooms_.find(roomId);
    if (roomIt == rooms_.end()) {
        return {};
    }
    
    std::vector<std::string> typingUsers;
    {
        std::lock_guard<std::mutex> roomLock(roomIt->second->mutex);
        for (const auto& userId : roomIt->second->typingUsers) {
            typingUsers.push_back(userId);
        }
    }
    
    return typingUsers;
}

bool RoomManager::addFile(const std::string& roomId, const FileInfo& fileInfo) {
    std::lock_guard<std::mutex> lock(globalMutex_);
    
    auto roomIt = rooms_.find(roomId);
    if (roomIt == rooms_.end()) {
        return false;
    }
    
    {
        std::lock_guard<std::mutex> roomLock(roomIt->second->mutex);
        roomIt->second->files[fileInfo.fileId] = fileInfo;
        fileToRoom_[fileInfo.fileId] = roomId;
    }
    
    std::cout << "文件 " << fileInfo.originalName << " 已上传到房间 " << roomId << std::endl;
    return true;
}

std::vector<FileInfo> RoomManager::getFiles(const std::string& roomId) {
    std::lock_guard<std::mutex> lock(globalMutex_);
    
    auto roomIt = rooms_.find(roomId);
    if (roomIt == rooms_.end()) {
        return {};
    }
    
    std::vector<FileInfo> files;
    {
        std::lock_guard<std::mutex> roomLock(roomIt->second->mutex);
        for (const auto& pair : roomIt->second->files) {
            files.push_back(pair.second);
        }
    }
    
    // 按上传时间排序（最新的在前）
    std::sort(files.begin(), files.end(), 
        [](const FileInfo& a, const FileInfo& b) {
            return a.uploadTime > b.uploadTime;
        });
    
    return files;
}

std::shared_ptr<FileInfo> RoomManager::getFileById(const std::string& fileId) {
    std::lock_guard<std::mutex> lock(globalMutex_);
    
    auto it = fileToRoom_.find(fileId);
    if (it == fileToRoom_.end()) {
        return nullptr;
    }
    
    std::string roomId = it->second;
    auto roomIt = rooms_.find(roomId);
    if (roomIt == rooms_.end()) {
        return nullptr;
    }
    
    std::lock_guard<std::mutex> roomLock(roomIt->second->mutex);
    auto fileIt = roomIt->second->files.find(fileId);
    if (fileIt == roomIt->second->files.end()) {
        return nullptr;
    }
    
    return std::make_shared<FileInfo>(fileIt->second);
}

bool RoomManager::hasFilePermission(const std::string& fileId, const std::string& connectionId) {
    std::lock_guard<std::mutex> lock(globalMutex_);
    
    // 检查用户是否在有权限的房间内
    auto connIt = connectionToRoom_.find(connectionId);
    if (connIt == connectionToRoom_.end()) {
        return false;
    }
    
    // 检查文件是否属于该房间
    auto fileIt = fileToRoom_.find(fileId);
    if (fileIt == fileToRoom_.end()) {
        return false;
    }
    
    return connIt->second == fileIt->second;
}

bool RoomManager::deleteRoomFiles(const std::string& roomId) {
    std::lock_guard<std::mutex> lock(globalMutex_);
    
    auto roomIt = rooms_.find(roomId);
    if (roomIt == rooms_.end()) {
        return false;
    }
    
    std::vector<std::string> fileIds;
    {
        std::lock_guard<std::mutex> roomLock(roomIt->second->mutex);
        for (const auto& pair : roomIt->second->files) {
            fileIds.push_back(pair.first);
        }
        roomIt->second->files.clear();
    }
    
    // 删除文件映射
    for (const auto& fileId : fileIds) {
        fileToRoom_.erase(fileId);
    }
    
    // 删除物理文件目录
    std::string uploadDir = "uploads/" + roomId;
    try {
        if (std::filesystem::exists(uploadDir)) {
            std::filesystem::remove_all(uploadDir);
            std::cout << "已删除房间 " << roomId << " 的文件目录" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "删除文件目录失败: " << e.what() << std::endl;
        return false;
    }
    
    return true;
}

std::string RoomManager::generateFileId() {
    uuid_t uuid;
    char uuid_str[37];
    uuid_generate(uuid);
    uuid_unparse(uuid, uuid_str);
    return std::string(uuid_str);
}

std::string RoomManager::getMimeType(const std::string& filename) {
    size_t dot = filename.rfind('.');
    if (dot == std::string::npos) {
        return "application/octet-stream";
    }
    
    std::string ext = filename.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    // 常见文件类型映射
    static const std::unordered_map<std::string, std::string> mimeTypes = {
        {"txt", "text/plain"},
        {"pdf", "application/pdf"},
        {"doc", "application/msword"},
        {"docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
        {"xls", "application/vnd.ms-excel"},
        {"xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
        {"ppt", "application/vnd.ms-powerpoint"},
        {"pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"png", "image/png"},
        {"gif", "image/gif"},
        {"bmp", "image/bmp"},
        {"svg", "image/svg+xml"},
        {"mp4", "video/mp4"},
        {"avi", "video/x-msvideo"},
        {"mkv", "video/x-matroska"},
        {"mp3", "audio/mpeg"},
        {"wav", "audio/wav"},
        {"zip", "application/zip"},
        {"rar", "application/x-rar-compressed"},
        {"7z", "application/x-7z-compressed"},
        {"tar", "application/x-tar"},
        {"gz", "application/gzip"},
        {"json", "application/json"},
        {"xml", "application/xml"},
        {"html", "text/html"},
        {"css", "text/css"},
        {"js", "application/javascript"}
    };
    
    auto it = mimeTypes.find(ext);
    if (it != mimeTypes.end()) {
        return it->second;
    }
    
    return "application/octet-stream";
}

bool RoomManager::isFileTypeAllowed(const std::string& filename) {
    size_t dot = filename.rfind('.');
    if (dot == std::string::npos) {
        return false; // 没有扩展名，不允许
    }
    
    std::string ext = filename.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    // 允许的文件类型白名单
    static const std::unordered_set<std::string> allowedTypes = {
        "txt", "pdf", "doc", "docx", "xls", "xlsx", "ppt", "pptx",
        "jpg", "jpeg", "png", "gif", "bmp", "svg",
        "mp4", "avi", "mkv", "mp3", "wav",
        "zip", "rar", "7z", "tar", "gz",
        "json", "xml", "html", "css", "js"
    };
    
    // 禁止的危险文件类型
    static const std::unordered_set<std::string> dangerousTypes = {
        "exe", "bat", "cmd", "com", "pif", "scr", "vbs", "jar", "app", "deb", "rpm"
    };
    
    if (dangerousTypes.count(ext) > 0) {
        return false;
    }
    
    return allowedTypes.count(ext) > 0;
} 