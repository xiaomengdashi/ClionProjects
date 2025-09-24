#include "WebSocketHandler.h"
#include "RoomManager.h"
#include <iostream>
#include <random>
#include <sstream>
#include <iomanip>
#include <atomic>
#include <chrono>

WebSocketHandler::WebSocketHandler() : connection_id_counter_(0) {
    // 设置日志级别
    ws_server_.set_access_channels(websocketpp::log::alevel::all);
    ws_server_.clear_access_channels(websocketpp::log::alevel::frame_payload);

    // 初始化Asio
    ws_server_.init_asio();

    // 设置回调函数
    ws_server_.set_tls_init_handler(std::bind(&WebSocketHandler::on_tls_init, this, std::placeholders::_1));
    ws_server_.set_open_handler(std::bind(&WebSocketHandler::on_open, this, std::placeholders::_1));
    ws_server_.set_close_handler(std::bind(&WebSocketHandler::on_close, this, std::placeholders::_1));
    ws_server_.set_message_handler(std::bind(&WebSocketHandler::on_message, this, 
                                            std::placeholders::_1, std::placeholders::_2));
}

WebSocketHandler::~WebSocketHandler() {
    stop();
}

context_ptr WebSocketHandler::on_tls_init(websocketpp::connection_hdl hdl) {
    context_ptr ctx = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);

    try {
        ctx->set_options(boost::asio::ssl::context::default_workarounds |
                        boost::asio::ssl::context::no_sslv2 |
                        boost::asio::ssl::context::no_sslv3 |
                        boost::asio::ssl::context::single_dh_use);
        
        ctx->use_certificate_chain_file("certificates/server.crt");
        ctx->use_private_key_file("certificates/server.key", boost::asio::ssl::context::pem);
        ctx->use_tmp_dh_file("certificates/dh.pem");
        
    } catch (std::exception& e) {
        std::cout << "TLS初始化错误: " << e.what() << std::endl;
    }
    
    return ctx;
}

void WebSocketHandler::start(uint16_t port) {
    try {
        ws_server_.listen(port);
        ws_server_.start_accept();
        std::cout << "WebSocket服务器启动在端口 " << port << std::endl;
        ws_server_.run();
    } catch (websocketpp::exception const & e) {
        std::cout << "WebSocket服务器启动失败: " << e.what() << std::endl;
    }
}

void WebSocketHandler::stop() {
    ws_server_.stop();
}

void WebSocketHandler::on_open(websocketpp::connection_hdl hdl) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_.insert(hdl);
    
    // 生成唯一连接ID
    uint64_t id = connection_id_counter_.fetch_add(1);
    std::string connId = "conn_" + std::to_string(id);
    connection_map_[connId] = hdl;
    
    std::cout << "新的WebSocket连接: " << connId << " (总连接数: " << connections_.size() << ")" << std::endl;
}

void WebSocketHandler::on_close(websocketpp::connection_hdl hdl) {
    std::string connId = getConnectionId(hdl);
    std::cout << "WebSocket连接关闭: " << connId << std::endl;
    
    // 查找用户所在的房间
    std::string roomId = RoomManager::getInstance().findRoomByConnection(connId);
    if (!roomId.empty()) {
        std::cout << "连接 " << connId << " 在房间 " << roomId << " 中" << std::endl;
        
        // 查找用户ID
        std::string userId = "";
        std::string userName = "";
        auto users = RoomManager::getInstance().getRoomUsers(roomId);
        for (const auto& user : users) {
            if (user.connectionId == connId) {
                userId = user.userId;
                userName = user.userName;
                break;
            }
        }
        
        if (!userId.empty()) {
            std::cout << "找到用户 " << userName << " (ID: " << userId << ")，准备移除" << std::endl;
            
            // 通知其他用户，该用户已断开连接
            json userDisconnected = {
                {"type", messageTypeToString(MessageType::USER_DISCONNECTED)},
                {"userId", userId}
            };
            
            broadcastToRoom(roomId, userDisconnected.dump(), connId);
            
            // 从房间移除用户
            bool removeResult = RoomManager::getInstance().leaveRoom(roomId, userId);
            std::cout << "移除用户结果: " << (removeResult ? "成功" : "失败") << std::endl;
        } else {
            std::cout << "警告: 未找到连接ID对应的用户: " << connId << std::endl;
        }
    } else {
        std::cout << "连接 " << connId << " 不在任何房间中" << std::endl;
    }
    
    // 从连接映射中移除
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto erased = connection_map_.erase(connId);
        connections_.erase(hdl);
        
        std::cout << "清理连接映射: " << (erased > 0 ? "成功" : "失败") 
                  << "，当前连接数: " << connections_.size() << std::endl;
    }
}

void WebSocketHandler::on_message(websocketpp::connection_hdl hdl, message_ptr msg) {
    try {
        json data = json::parse(msg->get_payload());
        
        if (!data.contains("type")) {
            std::cerr << "消息缺少type字段" << std::endl;
            return;
        }
        
        MessageType type = stringToMessageType(data["type"]);
        
        switch (type) {
            case MessageType::JOIN_ROOM:
                handleJoinRoom(hdl, data);
                break;
                
            case MessageType::LEAVE_ROOM:
                handleLeaveRoom(hdl, data);
                break;
                
            case MessageType::OFFER:
            case MessageType::ANSWER:
            case MessageType::ICE_CANDIDATE:
                handleWebRTCSignaling(hdl, data, type);
                break;
                
            case MessageType::PING:
                // 处理心跳消息，更新最后活动时间
                if (data.contains("userId") && data.contains("roomId")) {
                    updateUserActivity(hdl, data["userId"], data["roomId"]);
                }
                break;
                
            case MessageType::TEXT_MESSAGE:
                // 处理文字消息
                handleTextMessage(hdl, data);
                break;
                
            case MessageType::TYPING_START:
                // 处理开始输入
                handleTypingStatus(hdl, data, true);
                break;
                
            case MessageType::TYPING_END:
                // 处理停止输入
                handleTypingStatus(hdl, data, false);
                break;
                
            default:
                std::cerr << "未知的消息类型: " << data["type"] << std::endl;
                break;
        }
        
    } catch (json::exception& e) {
        std::cerr << "JSON解析错误: " << e.what() << std::endl;
    }
}

void WebSocketHandler::handleJoinRoom(websocketpp::connection_hdl hdl, const json& data) {
    if (!data.contains("roomId")) {
        json error = {
            {"type", messageTypeToString(MessageType::ERROR)},
            {"message", "缺少roomId字段"}
        };
        sendMessage(hdl, error.dump());
        return;
    }
    
    std::string roomId = data["roomId"];
    std::string userId = generateRandomUsername();
    std::string userName = "用户" + userId;
    std::string connId = getConnectionId(hdl);
    
    UserInfo userInfo = {userId, userName, connId};
    
    // 加入房间
    if (RoomManager::getInstance().joinRoom(roomId, userInfo)) {
        // 获取房间内所有用户
        auto users = RoomManager::getInstance().getRoomUsers(roomId);
        
        // 发送房间用户列表给新加入的用户
        json roomUsers = {
            {"type", messageTypeToString(MessageType::ROOM_USERS)},
            {"userId", userId},
            {"userName", userName},
            {"users", json::array()}
        };
        
        for (const auto& user : users) {
            if (user.userId != userId) {
                roomUsers["users"].push_back({
                    {"userId", user.userId},
                    {"userName", user.userName}
                });
            }
        }
        
        sendMessage(hdl, roomUsers.dump());
        
        // 发送消息历史给新加入的用户
        auto messages = RoomManager::getInstance().getMessages(roomId, 20); // 获取最近20条消息
        if (!messages.empty()) {
            json historyJson = {
                {"type", "message_history"},
                {"messages", json::array()}
            };
            
            for (const auto& msg : messages) {
                historyJson["messages"].push_back({
                    {"messageId", msg.messageId},
                    {"userId", msg.userId},
                    {"userName", msg.userName},
                    {"content", msg.content},
                    {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                        msg.timestamp.time_since_epoch()).count()}
                });
            }
            
            sendMessage(hdl, historyJson.dump());
        }
        
        // 发送文件列表给新加入的用户
        auto files = RoomManager::getInstance().getFiles(roomId);
        if (!files.empty()) {
            json fileListJson = {
                {"type", messageTypeToString(MessageType::FILE_LIST)},
                {"files", json::array()}
            };
            
            for (const auto& file : files) {
                fileListJson["files"].push_back({
                    {"fileId", file.fileId},
                    {"filename", file.originalName},
                    {"size", file.fileSize},
                    {"mimeType", file.mimeType},
                    {"uploaderUserId", file.uploaderUserId},
                    {"uploaderUserName", file.uploaderUserName},
                    {"uploadTime", std::chrono::duration_cast<std::chrono::milliseconds>(
                        file.uploadTime.time_since_epoch()).count()}
                });
            }
            
            sendMessage(hdl, fileListJson.dump());
        }
        
        // 通知其他用户有新用户加入
        json userJoined = {
            {"type", messageTypeToString(MessageType::USER_JOINED)},
            {"userId", userId},
            {"userName", userName}
        };
        
        broadcastToRoom(roomId, userJoined.dump(), connId);
        
    } else {
        json error = {
            {"type", messageTypeToString(MessageType::ERROR)},
            {"message", "加入房间失败"}
        };
        sendMessage(hdl, error.dump());
    }
}

void WebSocketHandler::handleLeaveRoom(websocketpp::connection_hdl hdl, const json& data) {
    if (!data.contains("roomId") || !data.contains("userId")) {
        return;
    }
    
    std::string roomId = data["roomId"];
    std::string userId = data["userId"];
    std::string connId = getConnectionId(hdl);
    
    if (RoomManager::getInstance().leaveRoom(roomId, userId)) {
        // 通知其他用户
        json userLeft = {
            {"type", messageTypeToString(MessageType::USER_LEFT)},
            {"userId", userId}
        };
        
        broadcastToRoom(roomId, userLeft.dump(), connId);
    }
}

void WebSocketHandler::handleWebRTCSignaling(websocketpp::connection_hdl hdl, const json& data, MessageType type) {
    if (!data.contains("targetUserId")) {
        return;
    }
    
    std::string connId = getConnectionId(hdl);
    std::string roomId = RoomManager::getInstance().findRoomByConnection(connId);
    
    if (roomId.empty()) {
        return;
    }
    
    // 转发WebRTC信令给目标用户
    auto users = RoomManager::getInstance().getRoomUsers(roomId);
    for (const auto& user : users) {
        if (user.userId == data["targetUserId"]) {
            auto it = connection_map_.find(user.connectionId);
            if (it != connection_map_.end()) {
                sendMessage(it->second, data.dump());
            }
            break;
        }
    }
}

void WebSocketHandler::sendMessage(websocketpp::connection_hdl hdl, const std::string& message) {
    try {
        ws_server_.send(hdl, message, websocketpp::frame::opcode::text);
    } catch (websocketpp::exception const & e) {
        std::cerr << "发送消息失败: " << e.what() << std::endl;
    }
}

void WebSocketHandler::broadcastToRoom(const std::string& roomId, const std::string& message, 
                                      const std::string& excludeConnection) {
    auto users = RoomManager::getInstance().getRoomUsers(roomId);
    
    for (const auto& user : users) {
        if (user.connectionId != excludeConnection) {
            auto it = connection_map_.find(user.connectionId);
            if (it != connection_map_.end()) {
                sendMessage(it->second, message);
            }
        }
    }
}

std::string WebSocketHandler::generateRandomUsername() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    
    return std::to_string(dis(gen));
}

std::string WebSocketHandler::getConnectionId(websocketpp::connection_hdl hdl) {
    // 在connection_map_中查找对应的连接ID
    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (const auto& pair : connection_map_) {
        if (!pair.second.expired()) {
            auto stored_hdl = pair.second.lock();
            auto current_hdl = hdl.lock();
            if (stored_hdl && current_hdl && stored_hdl.get() == current_hdl.get()) {
                return pair.first;
            }
        }
    }
    
    // 如果没有找到，说明连接可能已经关闭，返回临时ID
    auto con = hdl.lock();
    if (con) {
        std::stringstream ss;
        ss << "temp_" << con.get();
        return ss.str();
    }
    
    return "unknown";
}

/**
 * 更新用户活动时间
 */
void WebSocketHandler::updateUserActivity(websocketpp::connection_hdl hdl, const std::string& userId, const std::string& roomId) {
    // 这里可以实现存储用户最后活动时间的逻辑
    // 如果需要，可以创建一个定时任务检查用户活动状态，断开长时间不活跃的用户
    std::string connId = getConnectionId(hdl);
    // 记录日志，表示用户活跃
    std::cout << "用户 " << userId << " 在房间 " << roomId << " 活跃中" << std::endl;
}

void WebSocketHandler::handleTextMessage(websocketpp::connection_hdl hdl, const json& data) {
    if (!data.contains("roomId") || !data.contains("userId") || !data.contains("content")) {
        json error = {
            {"type", messageTypeToString(MessageType::ERROR)},
            {"message", "消息格式错误"}
        };
        sendMessage(hdl, error.dump());
        return;
    }
    
    std::string roomId = data["roomId"];
    std::string userId = data["userId"];
    std::string content = data["content"];
    std::string connId = getConnectionId(hdl);
    
    // 验证用户是否在房间内
    std::string userRoom = RoomManager::getInstance().findRoomByConnection(connId);
    if (userRoom != roomId) {
        json error = {
            {"type", messageTypeToString(MessageType::ERROR)},
            {"message", "用户不在指定房间内"}
        };
        sendMessage(hdl, error.dump());
        return;
    }
    
    // 获取用户名
    auto users = RoomManager::getInstance().getRoomUsers(roomId);
    std::string userName = "";
    for (const auto& user : users) {
        if (user.userId == userId) {
            userName = user.userName;
            break;
        }
    }
    
    if (userName.empty()) {
        json error = {
            {"type", messageTypeToString(MessageType::ERROR)},
            {"message", "用户不存在"}
        };
        sendMessage(hdl, error.dump());
        return;
    }
    
    // 生成消息ID并创建消息
    std::string messageId = generateMessageId();
    ChatMessage chatMessage(messageId, userId, userName, content);
    
    // 保存消息到房间
    if (RoomManager::getInstance().addMessage(roomId, chatMessage)) {
        // 广播消息给房间内所有用户
        json messageJson = {
            {"type", messageTypeToString(MessageType::TEXT_MESSAGE)},
            {"messageId", messageId},
            {"userId", userId},
            {"userName", userName},
            {"content", content},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                chatMessage.timestamp.time_since_epoch()).count()}
        };
        
        broadcastToRoom(roomId, messageJson.dump());
    } else {
        json error = {
            {"type", messageTypeToString(MessageType::ERROR)},
            {"message", "发送消息失败"}
        };
        sendMessage(hdl, error.dump());
    }
}

void WebSocketHandler::handleTypingStatus(websocketpp::connection_hdl hdl, const json& data, bool isTyping) {
    if (!data.contains("roomId") || !data.contains("userId")) {
        return;
    }
    
    std::string roomId = data["roomId"];
    std::string userId = data["userId"];
    std::string connId = getConnectionId(hdl);
    
    // 验证用户是否在房间内
    std::string userRoom = RoomManager::getInstance().findRoomByConnection(connId);
    if (userRoom != roomId) {
        return;
    }
    
    // 更新输入状态
    if (RoomManager::getInstance().setUserTyping(roomId, userId, isTyping)) {
        // 广播输入状态给房间内其他用户
        json typingJson = {
            {"type", isTyping ? messageTypeToString(MessageType::TYPING_START) : messageTypeToString(MessageType::TYPING_END)},
            {"userId", userId}
        };
        
        broadcastToRoom(roomId, typingJson.dump(), connId);
    }
}

std::string WebSocketHandler::generateMessageId() {
    static std::atomic<uint64_t> counter{0};
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    std::stringstream ss;
    ss << timestamp << "_" << counter.fetch_add(1);
    return ss.str();
}

void WebSocketHandler::broadcastFileUpload(const std::string& roomId, const FileInfo& fileInfo) {
    json fileUploadJson = {
        {"type", messageTypeToString(MessageType::FILE_UPLOADED)},
        {"fileId", fileInfo.fileId},
        {"filename", fileInfo.originalName},
        {"size", fileInfo.fileSize},
        {"mimeType", fileInfo.mimeType},
        {"uploaderUserId", fileInfo.uploaderUserId},
        {"uploaderUserName", fileInfo.uploaderUserName},
        {"uploadTime", std::chrono::duration_cast<std::chrono::milliseconds>(
            fileInfo.uploadTime.time_since_epoch()).count()}
    };
    
    broadcastToRoom(roomId, fileUploadJson.dump());
} 