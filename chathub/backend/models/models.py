from flask_sqlalchemy import SQLAlchemy
from datetime import datetime
from werkzeug.security import generate_password_hash, check_password_hash

db = SQLAlchemy()

class User(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    username = db.Column(db.String(80), unique=True, nullable=False)
    email = db.Column(db.String(120), unique=True, nullable=False)
    password_hash = db.Column(db.String(255), nullable=False)
    role = db.Column(db.String(20), default='user')  # 'admin' or 'user'
    api_key = db.Column(db.String(100), unique=True, nullable=True)
    subscription_type = db.Column(db.String(20), default='free')
    usage_count = db.Column(db.Integer, default=0)
    usage_limit = db.Column(db.Integer, default=100)
    is_active = db.Column(db.Boolean, default=True)
    created_at = db.Column(db.DateTime, default=datetime.utcnow)
    
    def set_password(self, password):
        """设置密码"""
        self.password_hash = generate_password_hash(password)
    
    def check_password(self, password):
        """验证密码"""
        return check_password_hash(self.password_hash, password)
    
    def to_dict(self, include_sensitive=False):
        from utils.time_utils import to_beijing_iso
        data = {
            'id': self.id,
            'username': self.username,
            'email': self.email,
            'role': self.role,
            'subscription_type': self.subscription_type,
            'usage_count': self.usage_count,
            'usage_limit': self.usage_limit,
            'is_active': self.is_active,
            'created_at': to_beijing_iso(self.created_at)
        }
        if include_sensitive and self.api_key:
            data['api_key'] = self.api_key
        return data

class Conversation(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    user_id = db.Column(db.Integer, db.ForeignKey('user.id'), nullable=False)
    conversation_id = db.Column(db.String(100), unique=True, nullable=False)
    title = db.Column(db.String(200), default='新对话')
    model = db.Column(db.String(50), nullable=False)
    created_at = db.Column(db.DateTime, default=datetime.utcnow)
    updated_at = db.Column(db.DateTime, default=datetime.utcnow)
    
    def to_dict(self):
        from utils.time_utils import to_beijing_iso
        return {
            'id': self.id,
            'user_id': self.user_id,
            'conversation_id': self.conversation_id,
            'title': self.title,
            'model': self.model,
            'created_at': to_beijing_iso(self.created_at),
            'updated_at': to_beijing_iso(self.updated_at)
        }

class Message(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    conversation_id = db.Column(db.String(100), nullable=False)
    role = db.Column(db.String(20), nullable=False)  # 'user' or 'assistant'
    content = db.Column(db.Text, nullable=False)
    reasoning = db.Column(db.Text, nullable=True)  # 推理过程字段
    timestamp = db.Column(db.DateTime, default=datetime.utcnow)
    
    def to_dict(self):
        from utils.time_utils import to_beijing_iso
        return {
            'id': self.id,
            'conversation_id': self.conversation_id,
            'role': self.role,
            'content': self.content,
            'reasoning': self.reasoning,
            'timestamp': to_beijing_iso(self.timestamp)
        }

class ApiKey(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    model_provider = db.Column(db.String(100), nullable=False)
    api_key = db.Column(db.String(500), nullable=False)
    base_url = db.Column(db.String(500))
    is_active = db.Column(db.Boolean, default=True)
    created_at = db.Column(db.DateTime, default=datetime.utcnow)
    updated_at = db.Column(db.DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)
    
    def to_dict(self):
        from utils.time_utils import to_beijing_iso
        return {
            'id': self.id,
            'model_provider': self.model_provider,
            'api_key': self.api_key,
            'base_url': self.base_url,
            'is_active': self.is_active,
            'created_at': to_beijing_iso(self.created_at),
            'updated_at': to_beijing_iso(self.updated_at)
        }

class Model(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    model_name = db.Column(db.String(100), nullable=False)  # 模型名称，如 gpt-4, claude-3-sonnet
    display_name = db.Column(db.String(100), nullable=False)  # 显示名称，如 GPT-4, Claude-3 Sonnet
    model_provider = db.Column(db.String(100), nullable=False)  # 模型提供商，如 openai, anthropic
    model_type = db.Column(db.String(50), default='chat')  # 模型类型：chat, completion, embedding
    max_tokens = db.Column(db.Integer, default=4096)  # 最大token数
    supports_streaming = db.Column(db.Boolean, default=True)  # 是否支持流式输出
    supports_function_calling = db.Column(db.Boolean, default=False)  # 是否支持函数调用
    supports_vision = db.Column(db.Boolean, default=False)  # 是否支持视觉输入
    input_price_per_1k = db.Column(db.Float, default=0.0)  # 输入价格（每1k tokens）
    output_price_per_1k = db.Column(db.Float, default=0.0)  # 输出价格（每1k tokens）
    description = db.Column(db.Text)  # 模型描述
    is_active = db.Column(db.Boolean, default=True)  # 是否启用
    sort_order = db.Column(db.Integer, default=0)  # 排序顺序
    created_at = db.Column(db.DateTime, default=datetime.utcnow)
    updated_at = db.Column(db.DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)
    
    def to_dict(self):
        from utils.time_utils import to_beijing_iso
        return {
            'id': self.id,
            'model_name': self.model_name,
            'display_name': self.display_name,
            'model_provider': self.model_provider,
            'model_type': self.model_type,
            'max_tokens': self.max_tokens,
            'supports_streaming': self.supports_streaming,
            'supports_function_calling': self.supports_function_calling,
            'supports_vision': self.supports_vision,
            'input_price_per_1k': self.input_price_per_1k,
            'output_price_per_1k': self.output_price_per_1k,
            'description': self.description,
            'is_active': self.is_active,
            'sort_order': self.sort_order,
            'created_at': to_beijing_iso(self.created_at),
            'updated_at': to_beijing_iso(self.updated_at)
        }