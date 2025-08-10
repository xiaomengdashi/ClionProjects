from flask import Blueprint, request, jsonify
from datetime import datetime, timezone, timedelta
import jwt
import uuid
from models.models import User, db
from utils.auth import token_required, admin_required
from models.config import Config

auth_bp = Blueprint('auth', __name__)

@auth_bp.route('/api/auth/register', methods=['POST'])
def register():
    """普通用户注册"""
    try:
        data = request.get_json()
        username = data.get('username')
        email = data.get('email')
        password = data.get('password')
        
        if not username or not email or not password:
            return jsonify({'error': '用户名、邮箱和密码不能为空'}), 400
        
        # 验证用户名长度
        if len(username) < 3 or len(username) > 20:
            return jsonify({'error': '用户名长度必须在3-20个字符之间'}), 400
        
        # 验证密码长度
        if len(password) < 6:
            return jsonify({'error': '密码长度至少6个字符'}), 400
        
        # 检查用户名是否已存在
        if User.query.filter_by(username=username).first():
            return jsonify({'error': '用户名已存在'}), 400
        
        # 检查邮箱是否已存在
        if User.query.filter_by(email=email).first():
            return jsonify({'error': '邮箱已存在'}), 400
        
        # 创建新用户（普通用户角色）
        new_user = User(
            username=username,
            email=email,
            role='user',
            api_key=str(uuid.uuid4()),  # 为每个用户生成唯一的API key
            subscription_type='free',
            usage_count=0,
            usage_limit=100,
            is_active=True
        )
        new_user.set_password(password)
        
        db.session.add(new_user)
        db.session.commit()
        
        return jsonify({
            'success': True,
            'message': '注册成功！请使用您的账户登录',
            'user': {
                'id': new_user.id,
                'username': new_user.username,
                'email': new_user.email,
                'role': new_user.role
            }
        })
    except Exception as e:
        db.session.rollback()
        return jsonify({'error': str(e)}), 500

@auth_bp.route('/api/auth/admin/register', methods=['POST'])
@admin_required
def admin_register(current_user):
    """管理员注册新用户"""
    try:
        data = request.get_json()
        username = data.get('username')
        email = data.get('email')
        password = data.get('password')
        role = data.get('role', 'user')
        
        if not username or not email or not password:
            return jsonify({'error': '用户名、邮箱和密码不能为空'}), 400
        
        # 检查用户名是否已存在
        if User.query.filter_by(username=username).first():
            return jsonify({'error': '用户名已存在'}), 400
        
        # 检查邮箱是否已存在
        if User.query.filter_by(email=email).first():
            return jsonify({'error': '邮箱已存在'}), 400
        
        # 创建新用户
        new_user = User(
            username=username,
            email=email,
            role=role,
            api_key=str(uuid.uuid4()) if role == 'admin' else None
        )
        new_user.set_password(password)
        
        db.session.add(new_user)
        db.session.commit()
        
        return jsonify({
            'success': True,
            'message': '用户注册成功',
            'user': new_user.to_dict()
        })
    except Exception as e:
        db.session.rollback()
        return jsonify({'error': str(e)}), 500

@auth_bp.route('/api/auth/login', methods=['POST'])
def login():
    """用户登录"""
    try:
        data = request.get_json()
        username = data.get('username')
        password = data.get('password')
        
        if not username or not password:
            return jsonify({'error': '用户名和密码不能为空'}), 400
        
        # 从数据库查找用户
        user = User.query.filter_by(username=username).first()
        
        # 验证用户和密码
        if user and user.is_active and user.check_password(password):
            # 生成JWT令牌
            token = jwt.encode({
                'username': username,
                'user_id': user.id,
                'role': user.role,
                'exp': datetime.now(timezone.utc) + timedelta(hours=24)
            }, Config.SECRET_KEY, algorithm='HS256')
            
            return jsonify({
                'success': True,
                'token': token,
                'user': user.to_dict(include_sensitive=(user.role == 'admin')),
                'message': '登录成功'
            })
        else:
            return jsonify({
                'success': False,
                'message': '用户名或密码错误，或账户已被禁用'
            }), 401
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@auth_bp.route('/api/auth/verify', methods=['GET'])
@token_required
def verify_token(current_user):
    """验证令牌有效性"""
    return jsonify({
        'valid': True,
        'user': current_user.to_dict(include_sensitive=(current_user.role == 'admin'))
    })

@auth_bp.route('/api/auth/logout', methods=['POST'])
@token_required
def logout(current_user):
    """用户登出"""
    return jsonify({'message': '登出成功'})