from functools import wraps
from flask import request, jsonify
import jwt
from datetime import datetime, timezone, timedelta
from models.models import User

def token_required(f):
    """JWT认证装饰器"""
    @wraps(f)
    def decorated(*args, **kwargs):
        token = request.headers.get('Authorization')
        if not token:
            return jsonify({'error': '缺少认证令牌'}), 401
        
        try:
            if token.startswith('Bearer '):
                token = token[7:]
            
            # 简化处理：允许demo-token用于测试
            if token == 'demo-token':
                current_user = User.query.filter_by(username='demo').first()
                if not current_user:
                    return jsonify({'error': '用户不存在'}), 401
            else:
                from app import app
                data = jwt.decode(token, app.config['SECRET_KEY'], algorithms=['HS256'])
                current_user = User.query.get(data['user_id'])
                if not current_user or not current_user.is_active:
                    return jsonify({'error': '用户不存在或已被禁用'}), 401
        except jwt.ExpiredSignatureError:
            return jsonify({'error': '令牌已过期'}), 401
        except jwt.InvalidTokenError:
            return jsonify({'error': '无效令牌'}), 401
        
        return f(current_user, *args, **kwargs)
    return decorated

def admin_required(f):
    """管理员权限装饰器"""
    @wraps(f)
    def decorated(*args, **kwargs):
        token = request.headers.get('Authorization')
        if not token:
            return jsonify({'error': '缺少认证令牌'}), 401
        
        try:
            if token.startswith('Bearer '):
                token = token[7:]
            
            if token == 'demo-token':
                current_user = User.query.filter_by(username='demo').first()
            else:
                from app import app
                data = jwt.decode(token, app.config['SECRET_KEY'], algorithms=['HS256'])
                current_user = User.query.get(data['user_id'])
            
            if not current_user or not current_user.is_active:
                return jsonify({'error': '用户不存在或已被禁用'}), 401
            
            if current_user.role != 'admin':
                return jsonify({'error': '需要管理员权限'}), 403
                
        except jwt.ExpiredSignatureError:
            return jsonify({'error': '令牌已过期'}), 401
        except jwt.InvalidTokenError:
            return jsonify({'error': '无效令牌'}), 401
        
        return f(current_user, *args, **kwargs)
    return decorated

def generate_jwt_token(username, user_id, secret_key):
    """生成JWT令牌"""
    return jwt.encode({
        'username': username,
        'user_id': user_id,
        'exp': datetime.now(timezone.utc) + timedelta(hours=24)
    }, secret_key, algorithm='HS256')