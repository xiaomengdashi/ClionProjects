from flask import Blueprint, request, jsonify
from models.models import User, db
from utils.auth import admin_required, token_required
import uuid

users_bp = Blueprint('users', __name__)

@users_bp.route('/api/users', methods=['GET'])
@admin_required
def get_users(current_user):
    """获取所有用户列表（仅管理员）"""
    try:
        page = request.args.get('page', 1, type=int)
        per_page = request.args.get('per_page', 10, type=int)
        search = request.args.get('search', '')
        role = request.args.get('role', '')
        
        query = User.query
        
        if search:
            query = query.filter(
                (User.username.contains(search)) | 
                (User.email.contains(search))
            )
        
        if role:
            query = query.filter(User.role == role)
        
        pagination = query.paginate(
            page=page, 
            per_page=per_page, 
            error_out=False
        )
        
        users = [user.to_dict() for user in pagination.items]
        
        return jsonify({
            'users': users,
            'total': pagination.total,
            'pages': pagination.pages,
            'current_page': page,
            'per_page': per_page
        })
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@users_bp.route('/api/users/<int:user_id>', methods=['GET'])
@admin_required
def get_user(current_user, user_id):
    """获取单个用户信息（仅管理员）"""
    try:
        user = User.query.get_or_404(user_id)
        return jsonify({'user': user.to_dict(include_sensitive=True)})
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@users_bp.route('/api/users/<int:user_id>', methods=['PUT'])
@admin_required
def update_user(current_user, user_id):
    """更新用户信息（仅管理员）"""
    try:
        user = User.query.get_or_404(user_id)
        data = request.get_json()
        
        # 更新基本信息
        if 'username' in data:
            # 检查用户名是否已被其他用户使用
            existing_user = User.query.filter_by(username=data['username']).first()
            if existing_user and existing_user.id != user_id:
                return jsonify({'error': '用户名已存在'}), 400
            user.username = data['username']
        
        if 'email' in data:
            # 检查邮箱是否已被其他用户使用
            existing_user = User.query.filter_by(email=data['email']).first()
            if existing_user and existing_user.id != user_id:
                return jsonify({'error': '邮箱已存在'}), 400
            user.email = data['email']
        
        if 'role' in data:
            old_role = user.role
            user.role = data['role']
            # 如果从普通用户升级为管理员，生成API密钥
            if old_role == 'user' and data['role'] == 'admin' and not user.api_key:
                user.api_key = str(uuid.uuid4())
            # 如果从管理员降级为普通用户，清除API密钥
            elif old_role == 'admin' and data['role'] == 'user':
                user.api_key = None
        
        if 'subscription_type' in data:
            user.subscription_type = data['subscription_type']
        
        if 'usage_limit' in data:
            user.usage_limit = data['usage_limit']
        
        if 'is_active' in data:
            user.is_active = data['is_active']
        
        # 如果提供了新密码，更新密码
        if 'password' in data and data['password']:
            user.set_password(data['password'])
        
        db.session.commit()
        
        return jsonify({
            'success': True,
            'message': '用户信息更新成功',
            'user': user.to_dict(include_sensitive=True)
        })
    except Exception as e:
        db.session.rollback()
        return jsonify({'error': str(e)}), 500

@users_bp.route('/api/users/<int:user_id>', methods=['DELETE'])
@admin_required
def delete_user(current_user, user_id):
    """删除用户（仅管理员）"""
    try:
        if user_id == current_user.id:
            return jsonify({'error': '不能删除自己的账户'}), 400
        
        user = User.query.get_or_404(user_id)
        
        # 软删除：设置为非活跃状态
        user.is_active = False
        db.session.commit()
        
        return jsonify({
            'success': True,
            'message': '用户已被禁用'
        })
    except Exception as e:
        db.session.rollback()
        return jsonify({'error': str(e)}), 500

@users_bp.route('/api/users/<int:user_id>/reset-password', methods=['POST'])
@admin_required
def reset_user_password(current_user, user_id):
    """重置用户密码（仅管理员）"""
    try:
        user = User.query.get_or_404(user_id)
        
        # 自动生成新密码（8位随机字符串）
        import random
        import string
        new_password = ''.join(random.choices(string.ascii_letters + string.digits, k=8))
        
        user.set_password(new_password)
        db.session.commit()
        
        return jsonify({
            'success': True,
            'message': '密码重置成功',
            'new_password': new_password
        })
    except Exception as e:
        db.session.rollback()
        return jsonify({'error': str(e)}), 500

@users_bp.route('/api/users/profile', methods=['GET'])
@token_required
def get_profile(current_user):
    """获取当前用户信息"""
    try:
        return jsonify({
            'user': current_user.to_dict(include_sensitive=(current_user.role == 'admin'))
        })
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@users_bp.route('/api/users/profile', methods=['PUT'])
@token_required
def update_profile(current_user):
    """更新当前用户信息"""
    try:
        data = request.get_json()
        
        # 普通用户只能更新基本信息
        if 'email' in data:
            # 检查邮箱是否已被其他用户使用
            existing_user = User.query.filter_by(email=data['email']).first()
            if existing_user and existing_user.id != current_user.id:
                return jsonify({'error': '邮箱已存在'}), 400
            current_user.email = data['email']
        
        # 如果提供了新密码，更新密码
        if 'password' in data and data['password']:
            current_user.set_password(data['password'])
        
        db.session.commit()
        
        return jsonify({
            'success': True,
            'message': '个人信息更新成功',
            'user': current_user.to_dict(include_sensitive=(current_user.role == 'admin'))
        })
    except Exception as e:
        db.session.rollback()
        return jsonify({'error': str(e)}), 500