# ChatHub RESTful API 文档

ChatHub 后端现在提供纯粹的 RESTful API 服务，不包含前端页面。

## 基础信息

- **基础URL**: `http://127.0.0.1:5001`
- **认证方式**: Bearer Token (在请求头中添加 `Authorization: Bearer <token>`)
- **测试Token**: `demo-token` (用于开发测试)
- **数据库**: SQLite数据库文件位于 `../database/chathub.db`

## 项目结构

```
chathub/
├── backend/           # 后端API服务
│   ├── app.py        # 主应用文件
│   └── requirements.txt
├── database/         # 数据库文件目录
│   └── chathub.db   # SQLite数据库文件
└── frontend/        # 前端文件（独立）
```

## API 端点

### 1. 健康检查

**GET** `/api/health`

检查API服务状态

**响应示例**:
```json
{
  "status": "healthy",
  "message": "AI API服务运行正常"
}
```

### 2. 获取模型列表

**GET** `/api/models`

获取所有可用的AI模型

**响应示例**:
```json
[
  {
    "id": "gpt-4",
    "name": "GPT-4",
    "provider": "OpenAI",
    "description": "最先进的语言模型，适合复杂任务",
    "cost_per_message": 0.03
  }
]
```

### 3. 用户认证

#### 登录
**POST** `/api/auth/login`

**请求体**:
```json
{
  "username": "admin",
  "password": "kolane"
}
```

**响应示例**:
```json
{
  "success": true,
  "token": "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9...",
  "user": {
    "id": 1,
    "username": "admin",
    "email": "admin@chathub.com",
    "subscription_type": "premium",
    "role": "admin"
  },
  "message": "登录成功"
}
```

#### 验证Token
**GET** `/api/auth/verify`

需要认证。验证当前token的有效性。

### 4. 聊天功能

#### 发送消息
**POST** `/api/chat`

需要认证。发送消息给AI模型。

**请求体**:
```json
{
  "message": "你好，请介绍一下自己",
  "model": "Qwen/Qwen2.5-72B-Instruct",
  "conversation_id": "可选，不提供则自动生成",
  "user_id": 1
}
```

**响应示例**:
```json
{
  "conversation_id": "uuid-string",
  "response": "AI的回复内容",
  "model": "Qwen/Qwen2.5-72B-Instruct",
  "timestamp": "2024-01-01T12:00:00"
}
```

#### 流式聊天
**POST** `/api/chat/stream`

需要认证。支持Server-Sent Events的流式聊天。

**请求体**: 同上

**响应**: SSE流，每个事件包含：
```json
{
  "type": "content",
  "content": "部分回复内容"
}
```

### 5. 对话管理

#### 获取对话列表
**GET** `/api/conversations`

需要认证。获取用户的所有对话。

#### 获取对话消息
**GET** `/api/conversations/{conversation_id}/messages`

需要认证。获取特定对话的所有消息。

#### 删除对话
**DELETE** `/api/conversations/{conversation_id}`

需要认证。删除指定对话。

### 6. API密钥管理

#### 保存API密钥
**POST** `/api/apikeys`

需要认证。保存第三方API密钥。

**请求体**:
```json
{
  "model_provider": "siliconflow",
  "api_key": "your-api-key",
  "base_url": "https://api.siliconflow.cn/v1/chat/completions"
}
```

#### 获取API密钥列表
**GET** `/api/apikeys`

需要认证。获取已保存的API密钥列表（密钥会被部分隐藏）。

#### 更新API密钥
**PUT** `/api/apikeys/{key_id}`

需要认证。更新指定的API密钥。

#### 删除API密钥
**DELETE** `/api/apikeys/{key_id}`

需要认证。删除指定的API密钥。

### 7. 统计信息

**GET** `/api/stats`

需要认证。获取用户的使用统计信息。

## 错误响应

所有错误响应都遵循以下格式：

```json
{
  "error": "错误描述信息"
}
```

常见HTTP状态码：
- `200`: 成功
- `400`: 请求参数错误
- `401`: 未认证或认证失败
- `404`: 资源不存在
- `500`: 服务器内部错误

## 使用示例

### 使用curl测试API

```bash
# 健康检查
curl -X GET http://127.0.0.1:5001/api/health

# 获取模型列表
curl -X GET http://127.0.0.1:5001/api/models

# 登录获取token
curl -X POST http://127.0.0.1:5001/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"kolane"}'

# 发送聊天消息（需要替换token）
curl -X POST http://127.0.0.1:5001/api/chat \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer demo-token" \
  -d '{"message":"你好","model":"gpt-3.5-turbo","user_id":1}'
```

## 注意事项

1. 使用硅基流动模型（Qwen、DeepSeek）需要先配置相应的API密钥
2. 测试环境可以使用 `demo-token` 作为认证token
3. 生产环境建议使用JWT token进行认证
4. 所有时间戳都使用ISO 8601格式
5. 流式聊天需要客户端支持Server-Sent Events