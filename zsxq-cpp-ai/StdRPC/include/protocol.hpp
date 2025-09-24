#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include "serializer.hpp"

namespace stdrpc {

/**
 * RPC消息类型枚举
 */
enum class MessageType : uint8_t {
    REQUEST = 0x01,   // RPC请求
    RESPONSE = 0x02,  // RPC响应
    ERROR = 0x03,     // 错误消息
    HEARTBEAT = 0x04  // 心跳包
};

/**
 * RPC状态码
 */
enum class StatusCode : uint16_t {
    OK = 0,                     // 成功
    METHOD_NOT_FOUND = 1,       // 方法未找到
    INVALID_PARAMS = 2,         // 无效参数
    INTERNAL_ERROR = 3,         // 内部错误
    SERIALIZATION_ERROR = 4,    // 序列化错误
    NETWORK_ERROR = 5,          // 网络错误
    TIMEOUT = 6                 // 超时
};

/**
 * RPC消息头结构
 * 包含消息的基本元数据
 */
struct MessageHeader {
    uint32_t magic = 0x52504343;    // 魔数 'RPCC' (RPC Client)
    uint8_t version = 1;             // 协议版本
    MessageType type;                // 消息类型
    uint32_t request_id;             // 请求ID，用于匹配请求和响应
    uint32_t body_size;              // 消息体大小

    /**
     * 序列化消息头
     * @param serializer 序列化器
     */
    void serialize(Serializer& serializer) const {
        serializer.write(magic);
        serializer.write(version);
        serializer.write(static_cast<uint8_t>(type));
        serializer.write(request_id);
        serializer.write(body_size);
    }

    /**
     * 反序列化消息头
     * @param serializer 序列化器
     * @return 如果成功返回true，否则返回false
     */
    bool deserialize(Serializer& serializer) {
        if (serializer.remainingBytes() < getHeaderSize()) {
            return false;
        }
        magic = serializer.read<uint32_t>();
        if (magic != 0x52504343) {
            return false;  // 魔数不匹配
        }
        version = serializer.read<uint8_t>();
        type = static_cast<MessageType>(serializer.read<uint8_t>());
        request_id = serializer.read<uint32_t>();
        body_size = serializer.read<uint32_t>();
        return true;
    }

    /**
     * 获取消息头大小
     * @return 消息头大小（字节）
     */
    static constexpr size_t getHeaderSize() {
        return sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint8_t) +
               sizeof(uint32_t) + sizeof(uint32_t);
    }
};

/**
 * RPC请求消息
 */
struct RequestMessage {
    std::string method_name;              // 方法名
    std::vector<uint8_t> params_data;     // 参数数据（序列化后）

    /**
     * 序列化请求消息
     * @param serializer 序列化器
     */
    void serialize(Serializer& serializer) const {
        serializer.write(method_name);
        serializer.write(static_cast<uint32_t>(params_data.size()));
        serializer.writeRaw(params_data.data(), params_data.size());
    }

    /**
     * 反序列化请求消息
     * @param serializer 序列化器
     */
    void deserialize(Serializer& serializer) {
        method_name = serializer.readString();
        uint32_t size = serializer.read<uint32_t>();
        params_data.resize(size);
        serializer.readRaw(params_data.data(), size);
    }
};

/**
 * RPC响应消息
 */
struct ResponseMessage {
    StatusCode status;                    // 状态码
    std::vector<uint8_t> result_data;     // 结果数据（序列化后）
    std::string error_message;            // 错误消息（仅在出错时使用）

    /**
     * 序列化响应消息
     * @param serializer 序列化器
     */
    void serialize(Serializer& serializer) const {
        serializer.write(static_cast<uint16_t>(status));
        serializer.write(static_cast<uint32_t>(result_data.size()));
        serializer.writeRaw(result_data.data(), result_data.size());
        serializer.write(error_message);
    }

    /**
     * 反序列化响应消息
     * @param serializer 序列化器
     */
    void deserialize(Serializer& serializer) {
        status = static_cast<StatusCode>(serializer.read<uint16_t>());
        uint32_t size = serializer.read<uint32_t>();
        result_data.resize(size);
        serializer.readRaw(result_data.data(), size);
        error_message = serializer.readString();
    }
};

/**
 * RPC完整消息
 * 包含消息头和消息体
 */
class Message {
private:
    MessageHeader header_;
    std::vector<uint8_t> body_;

public:
    /**
     * 默认构造函数
     */
    Message() = default;

    /**
     * 构造请求消息
     * @param request_id 请求ID
     * @param request 请求内容
     */
    Message(uint32_t request_id, const RequestMessage& request) {
        header_.type = MessageType::REQUEST;
        header_.request_id = request_id;

        // 序列化请求体
        Serializer serializer;
        request.serialize(serializer);
        body_ = serializer.getData();
        header_.body_size = static_cast<uint32_t>(body_.size());
    }

    /**
     * 构造响应消息
     * @param request_id 请求ID
     * @param response 响应内容
     */
    Message(uint32_t request_id, const ResponseMessage& response) {
        header_.type = MessageType::RESPONSE;
        header_.request_id = request_id;

        // 序列化响应体
        Serializer serializer;
        response.serialize(serializer);
        body_ = serializer.getData();
        header_.body_size = static_cast<uint32_t>(body_.size());
    }

    /**
     * 获取消息头
     */
    const MessageHeader& getHeader() const { return header_; }
    MessageHeader& getHeader() { return header_; }

    /**
     * 获取消息体
     */
    const std::vector<uint8_t>& getBody() const { return body_; }
    std::vector<uint8_t>& getBody() { return body_; }

    /**
     * 设置消息体
     */
    void setBody(const std::vector<uint8_t>& body) {
        body_ = body;
        header_.body_size = static_cast<uint32_t>(body.size());
    }

    /**
     * 序列化整个消息
     * @return 序列化后的字节流
     */
    std::vector<uint8_t> serialize() const {
        Serializer serializer;
        header_.serialize(serializer);
        serializer.writeRaw(body_.data(), body_.size());
        return serializer.getData();
    }

    /**
     * 从字节流反序列化消息
     * @param data 字节流
     * @param size 数据大小
     * @return 如果成功返回消息对象，否则返回nullptr
     */
    static std::unique_ptr<Message> deserialize(const uint8_t* data, size_t size) {
        if (size < MessageHeader::getHeaderSize()) {
            return nullptr;
        }

        auto msg = std::make_unique<Message>();
        Serializer serializer(data, MessageHeader::getHeaderSize());

        if (!msg->header_.deserialize(serializer)) {
            return nullptr;
        }

        // 检查消息体大小
        if (size < MessageHeader::getHeaderSize() + msg->header_.body_size) {
            return nullptr;
        }

        // 读取消息体
        msg->body_.resize(msg->header_.body_size);
        std::memcpy(msg->body_.data(),
                   data + MessageHeader::getHeaderSize(),
                   msg->header_.body_size);

        return msg;
    }
};

} // namespace stdrpc

#endif // PROTOCOL_HPP