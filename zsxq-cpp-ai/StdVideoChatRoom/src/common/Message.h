#ifndef MESSAGE_H
#define MESSAGE_H

#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * WebSocket消息类型枚举
 */
enum class MessageType {
    JOIN_ROOM,        // 加入房间
    LEAVE_ROOM,       // 离开房间
    USER_JOINED,      // 用户加入通知
    USER_LEFT,        // 用户离开通知
    ROOM_USERS,       // 房间用户列表
    OFFER,            // WebRTC offer
    ANSWER,           // WebRTC answer
    ICE_CANDIDATE,    // ICE候选
    ERROR,            // 错误消息
    USER_DISCONNECTED,// 用户意外断开连接
    PING,             // 心跳检测
    TEXT_MESSAGE,     // 文字消息
    TYPING_START,     // 开始输入
    TYPING_END,       // 停止输入
    FILE_UPLOADED,    // 文件上传通知
    FILE_LIST         // 文件列表
};

/**
 * 消息类型转字符串
 */
inline std::string messageTypeToString(MessageType type) {
    switch (type) {
        case MessageType::JOIN_ROOM: return "join_room";
        case MessageType::LEAVE_ROOM: return "leave_room";
        case MessageType::USER_JOINED: return "user_joined";
        case MessageType::USER_LEFT: return "user_left";
        case MessageType::ROOM_USERS: return "room_users";
        case MessageType::OFFER: return "offer";
        case MessageType::ANSWER: return "answer";
        case MessageType::ICE_CANDIDATE: return "ice_candidate";
        case MessageType::ERROR: return "error";
        case MessageType::USER_DISCONNECTED: return "user_disconnected";
        case MessageType::PING: return "ping";
        case MessageType::TEXT_MESSAGE: return "text_message";
        case MessageType::TYPING_START: return "typing_start";
        case MessageType::TYPING_END: return "typing_end";
        case MessageType::FILE_UPLOADED: return "file_uploaded";
        case MessageType::FILE_LIST: return "file_list";
        default: return "unknown";
    }
}

/**
 * 字符串转消息类型
 */
inline MessageType stringToMessageType(const std::string& str) {
    if (str == "join_room") return MessageType::JOIN_ROOM;
    if (str == "leave_room") return MessageType::LEAVE_ROOM;
    if (str == "user_joined") return MessageType::USER_JOINED;
    if (str == "user_left") return MessageType::USER_LEFT;
    if (str == "room_users") return MessageType::ROOM_USERS;
    if (str == "offer") return MessageType::OFFER;
    if (str == "answer") return MessageType::ANSWER;
    if (str == "ice_candidate") return MessageType::ICE_CANDIDATE;
    if (str == "error") return MessageType::ERROR;
    if (str == "user_disconnected") return MessageType::USER_DISCONNECTED;
    if (str == "ping") return MessageType::PING;
    if (str == "text_message") return MessageType::TEXT_MESSAGE;
    if (str == "typing_start") return MessageType::TYPING_START;
    if (str == "typing_end") return MessageType::TYPING_END;
    if (str == "file_uploaded") return MessageType::FILE_UPLOADED;
    if (str == "file_list") return MessageType::FILE_LIST;
    return MessageType::ERROR;
}

#endif // MESSAGE_H 