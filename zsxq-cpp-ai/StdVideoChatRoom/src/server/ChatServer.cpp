#include "ChatServer.h"
#include "RoomManager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <regex>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

ChatServer::ChatServer(const std::string& cert_path, const std::string& key_path) 
    : public_dir_("public") {
    https_server_ = new httplib::SSLServer(cert_path.c_str(), key_path.c_str());
    ws_handler_.reset(new WebSocketHandler());
    
    if (!https_server_->is_valid()) {
        throw std::runtime_error("HTTPS服务器初始化失败");
    }
    
    setupRoutes();
}

ChatServer::~ChatServer() {
    stop();
    delete https_server_;
}

void ChatServer::start(int http_port, int ws_port) {
    // 启动WebSocket服务器在单独的线程
    ws_thread_.reset(new std::thread([this, ws_port]() {
        ws_handler_->start(ws_port);
    }));
    
    std::cout << "HTTPS服务器启动在端口 " << http_port << std::endl;
    std::cout << "WebSocket服务器启动在端口 " << ws_port << std::endl;
    
    // 启动HTTPS服务器（阻塞）
    https_server_->listen("0.0.0.0", http_port);
}

void ChatServer::stop() {
    if (https_server_) {
        https_server_->stop();
    }
    
    if (ws_handler_) {
        ws_handler_->stop();
    }
    
    if (ws_thread_ && ws_thread_->joinable()) {
        ws_thread_->join();
    }
}

void ChatServer::setupRoutes() {
    // 主页路由
    https_server_->Get("/", [this](const httplib::Request& req, httplib::Response& res) {
        std::string file_path = public_dir_ + "/index.html";
        std::ifstream file(file_path, std::ios::binary);
        
        if (file) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            res.set_content(buffer.str(), "text/html");
        } else {
            res.status = 404;
            res.set_content("File not found", "text/plain");
        }
    });
    
    // ===== API路由必须在静态文件路由之前 =====
    
    // 文件上传路由
    https_server_->Post("/api/upload", [this](const httplib::Request& req, httplib::Response& res) {
        handleFileUpload(req, res);
    });
    
    // 文件下载路由
    https_server_->Get(R"(/api/download/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        handleFileDownload(req, res);
    });
    
    // 文件列表路由
    https_server_->Get(R"(/api/files/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        handleFileList(req, res);
    });
    
    // ===== 静态文件服务放在最后，避免拦截API请求 =====
    
    // 静态文件服务 - 必须放在API路由之后
    https_server_->Get(R"(/(.+))", [this](const httplib::Request& req, httplib::Response& res) {
        std::string path = req.matches[1];
        std::string file_path = public_dir_ + "/" + path;
        
        // 安全检查，防止目录遍历
        if (path.find("..") != std::string::npos) {
            res.status = 403;
            res.set_content("Forbidden", "text/plain");
            return;
        }
        
        std::ifstream file(file_path, std::ios::binary);
        if (file) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            res.set_content(buffer.str(), getMimeType(file_path));
        } else {
            res.status = 404;
            res.set_content("File not found", "text/plain");
        }
    });
    
    // OPTIONS 预检请求处理
    https_server_->Options(".*", [](const httplib::Request& req, httplib::Response& res) {
        return;
    });
    
    // CORS设置
    https_server_->set_post_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, X-Room-Id, X-User-Id");
    });
}

std::string ChatServer::getMimeType(const std::string& path) {
    size_t dot = path.rfind('.');
    if (dot == std::string::npos) {
        return "application/octet-stream";
    }
    
    std::string ext = path.substr(dot + 1);
    
    if (ext == "html" || ext == "htm") return "text/html";
    if (ext == "css") return "text/css";
    if (ext == "js") return "application/javascript";
    if (ext == "json") return "application/json";
    if (ext == "png") return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "gif") return "image/gif";
    if (ext == "svg") return "image/svg+xml";
    if (ext == "ico") return "image/x-icon";
    
    return "application/octet-stream";
}

void ChatServer::handleFileUpload(const httplib::Request& req, httplib::Response& res) {
    try {
        // 验证必要的header
        if (!req.has_header("X-Room-Id") || !req.has_header("X-User-Id")) {
            res.status = 400;
            json error = {{"error", "缺少必要的头部信息"}};
            res.set_content(error.dump(), "application/json");
            return;
        }
        
        std::string roomId = req.get_header_value("X-Room-Id");
        std::string userId = req.get_header_value("X-User-Id");
        
        // 验证用户权限
        std::string validatedUserId;
        if (!validateUserPermission(req, roomId, validatedUserId) || validatedUserId != userId) {
            res.status = 403;
            json error = {{"error", "无权限访问该房间"}};
            res.set_content(error.dump(), "application/json");
            return;
        }
        
        // 检查是否有文件
        if (!req.has_file("file")) {
            res.status = 400;
            json error = {{"error", "没有上传文件"}};
            res.set_content(error.dump(), "application/json");
            return;
        }
        
        auto file = req.get_file_value("file");
        if (file.filename.empty() || file.content.empty()) {
            res.status = 400;
            json error = {{"error", "文件为空"}};
            res.set_content(error.dump(), "application/json");
            return;
        }
        
        // 验证文件大小（10MB限制）
        const size_t MAX_FILE_SIZE = 10 * 1024 * 1024; // 10MB
        if (file.content.size() > MAX_FILE_SIZE) {
            res.status = 413;
            json error = {{"error", "文件大小超过限制（最大10MB）"}};
            res.set_content(error.dump(), "application/json");
            return;
        }
        
        // 验证文件类型
        if (!RoomManager::getInstance().isFileTypeAllowed(file.filename)) {
            res.status = 400;
            json error = {{"error", "不允许的文件类型"}};
            res.set_content(error.dump(), "application/json");
            return;
        }
        
        // 生成文件ID和存储路径
        std::string fileId = RoomManager::getInstance().generateFileId();
        std::string uploadDir = "uploads/" + roomId;
        std::string filePath = uploadDir + "/" + fileId + "_" + file.filename;
        
        // 创建上传目录
        std::filesystem::create_directories(uploadDir);
        
        // 保存文件
        std::ofstream outFile(filePath, std::ios::binary);
        if (!outFile) {
            res.status = 500;
            json error = {{"error", "文件保存失败"}};
            res.set_content(error.dump(), "application/json");
            return;
        }
        
        outFile.write(file.content.data(), file.content.size());
        outFile.close();
        
        // 获取用户信息
        auto users = RoomManager::getInstance().getRoomUsers(roomId);
        std::string userName = "";
        for (const auto& user : users) {
            if (user.userId == userId) {
                userName = user.userName;
                break;
            }
        }
        
        // 创建文件信息并添加到房间
        FileInfo fileInfo(
            fileId,
            file.filename,
            userId,
            userName,
            file.content.size(),
            filePath,
            RoomManager::getInstance().getMimeType(file.filename)
        );
        
        if (RoomManager::getInstance().addFile(roomId, fileInfo)) {
            // 广播文件上传通知给房间内的其他用户
            if (ws_handler_) {
                ws_handler_->broadcastFileUpload(roomId, fileInfo);
            }
            
            // 返回成功响应
            json response = {
                {"fileId", fileId},
                {"filename", file.filename},
                {"size", file.content.size()},
                {"uploadTime", std::chrono::duration_cast<std::chrono::milliseconds>(
                    fileInfo.uploadTime.time_since_epoch()).count()}
            };
            
            res.set_content(response.dump(), "application/json");
        } else {
            // 删除已保存的文件
            std::filesystem::remove(filePath);
            res.status = 500;
            json error = {{"error", "文件信息保存失败"}};
            res.set_content(error.dump(), "application/json");
        }
        
    } catch (const std::exception& e) {
        res.status = 500;
        json error = {{"error", "服务器内部错误"}};
        res.set_content(error.dump(), "application/json");
    }
}

void ChatServer::handleFileDownload(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string fileId = req.matches[1];
        
        // 获取文件信息
        auto fileInfo = RoomManager::getInstance().getFileById(fileId);
        if (!fileInfo) {
            res.status = 404;
            res.set_content("文件不存在", "text/plain");
            return;
        }
        
        // 验证用户权限（通过Connection ID）
        std::string connectionId = req.get_header_value("X-Connection-Id", "");
        if (!connectionId.empty() && !RoomManager::getInstance().hasFilePermission(fileId, connectionId)) {
            res.status = 403;
            res.set_content("无权限访问该文件", "text/plain");
            return;
        }
        
        // 检查文件是否存在
        if (!std::filesystem::exists(fileInfo->filePath)) {
            res.status = 404;
            res.set_content("文件不存在", "text/plain");
            return;
        }
        
        // 读取文件内容
        std::ifstream file(fileInfo->filePath, std::ios::binary);
        if (!file) {
            res.status = 500;
            res.set_content("无法读取文件", "text/plain");
            return;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        
        // 设置响应头
        res.set_content(buffer.str(), fileInfo->mimeType);
        res.set_header("Content-Disposition", 
            "attachment; filename=\"" + fileInfo->originalName + "\"");
        
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content("服务器内部错误", "text/plain");
    }
}

void ChatServer::handleFileList(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string roomId = req.matches[1];
        
        // 验证用户权限
        std::string userId;
        if (!validateUserPermission(req, roomId, userId)) {
            res.status = 403;
            json error = {{"error", "无权限访问该房间"}};
            res.set_content(error.dump(), "application/json");
            return;
        }
        
        // 获取文件列表
        auto files = RoomManager::getInstance().getFiles(roomId);
        
        json fileList = json::array();
        for (const auto& file : files) {
            fileList.push_back({
                {"fileId", file.fileId},
                {"filename", file.originalName},
                {"size", file.fileSize},
                {"mimeType", file.mimeType},
                {"uploaderName", file.uploaderUserName},
                {"uploadTime", std::chrono::duration_cast<std::chrono::milliseconds>(
                    file.uploadTime.time_since_epoch()).count()}
            });
        }
        
        json response = {
            {"files", fileList}
        };
        
        res.set_content(response.dump(), "application/json");
        
    } catch (const std::exception& e) {
        res.status = 500;
        json error = {{"error", "服务器内部错误"}};
        res.set_content(error.dump(), "application/json");
    }
}

bool ChatServer::validateUserPermission(const httplib::Request& req, const std::string& roomId, std::string& userId) {
    // 这是一个简化的权限验证，实际应用中应该有更严格的身份验证机制
    // 这里我们通过User-Agent或其他方式来模拟连接验证
    
    if (req.has_header("X-User-Id")) {
        userId = req.get_header_value("X-User-Id");
        
        // 检查房间是否存在且用户在房间内
        auto users = RoomManager::getInstance().getRoomUsers(roomId);
        for (const auto& user : users) {
            if (user.userId == userId) {
                return true;
            }
        }
    }
    
    return false;
} 