#ifndef SUBSCRIBER_H
#define SUBSCRIBER_H

#include <string>
#include <memory>
#include "message.h"

/**
 * 订阅者接口 - 所有订阅者必须实现此接口
 * 用于接收和处理消息队列中的消息
 */
class ISubscriber {
public:
    virtual ~ISubscriber() = default;

    // 处理接收到的消息
    virtual void onMessage(const std::shared_ptr<Message>& message) = 0;

    // 获取订阅者名称
    virtual std::string getName() const = 0;

    // 获取订阅者ID
    virtual std::string getId() const = 0;
};

#endif // SUBSCRIBER_H