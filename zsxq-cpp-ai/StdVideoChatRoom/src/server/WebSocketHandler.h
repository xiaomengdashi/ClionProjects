#ifndef WEBSOCKET_HANDLER_H
#define WEBSOCKET_HANDLER_H

#include <websocketpp/config/asio.hpp>
#include <websocketpp/server.hpp>
#include <memory>
#include <set>
#include <mutex>
#include <nlohmann/json.hpp>
#include "../common/Message.h"

// 前向声明
struct FileInfo;

using json = nlohmann::json;
typedef websocketpp::server<websocketpp::config::asio_tls> server;
typedef server::message_ptr message_ptr;
typedef std::shared_ptr<boost::asio::ssl::context> context_ptr;

/**
 * WebSocket处理器类
 * 负责处理WebSocket连接和消息
 */
class WebSocketHandler {
public:
    WebSocketHandler();
    ~WebSocketHandler();

    /**
     * 启动WebSocket服务器
     * @param port 端口号
     */
    void start(uint16_t port);

    /**
     * 停止WebSocket服务器
     */
    void stop();

    /**
     * 向指定连接发送消息
     * @param hdl 连接句柄
     * @param message 消息内容
     */
    void sendMessage(websocketpp::connection_hdl hdl, const std::string& message);

    /**
     * 向房间内所有用户广播消息
     * @param roomId 房间ID
     * @param message 消息内容
     * @param excludeConnection 排除的连接ID（可选）
     */
    void broadcastToRoom(const std::string& roomId, const std::string& message, 
                        const std::string& excludeConnection = "");

    /**
     * 更新用户活动时间
     * @param hdl WebSocket连接句柄
     * @param userId 用户ID
     * @param roomId 房间ID
     */
    void updateUserActivity(websocketpp::connection_hdl hdl, const std::string& userId, const std::string& roomId);

    /**
     * 获取连接ID
     * @param hdl WebSocket连接句柄
     * @return 连接ID
     */
    std::string getConnectionId(websocketpp::connection_hdl hdl);

    /**
     * 广播文件上传通知
     * @param roomId 房间ID
     * @param fileInfo 文件信息
     */
    void broadcastFileUpload(const std::string& roomId, const FileInfo& fileInfo);

private:
    /**
     * 处理TLS初始化
     */
    context_ptr on_tls_init(websocketpp::connection_hdl hdl);

    /**
     * 处理新连接
     */
    void on_open(websocketpp::connection_hdl hdl);

    /**
     * 处理连接关闭
     */
    void on_close(websocketpp::connection_hdl hdl);

    /**
     * 处理接收到的消息
     */
    void on_message(websocketpp::connection_hdl hdl, message_ptr msg);

    /**
     * 处理加入房间请求
     */
    void handleJoinRoom(websocketpp::connection_hdl hdl, const json& data);

    /**
     * 处理离开房间请求
     */
    void handleLeaveRoom(websocketpp::connection_hdl hdl, const json& data);

    /**
     * 处理WebRTC信令
     */
    void handleWebRTCSignaling(websocketpp::connection_hdl hdl, const json& data, MessageType type);

    /**
     * 处理文字消息
     */
    void handleTextMessage(websocketpp::connection_hdl hdl, const json& data);

    /**
     * 处理输入状态
     */
    void handleTypingStatus(websocketpp::connection_hdl hdl, const json& data, bool isTyping);

    /**
     * 生成随机用户名
     */
    std::string generateRandomUsername();

    /**
     * 生成消息ID
     */
    std::string generateMessageId();

private:
    server ws_server_;
    std::set<websocketpp::connection_hdl, std::owner_less<websocketpp::connection_hdl>> connections_;
    std::mutex connections_mutex_;
    std::map<std::string, websocketpp::connection_hdl> connection_map_;
    std::atomic<uint64_t> connection_id_counter_;  // 连接ID计数器
};

#endif // WEBSOCKET_HANDLER_H 