from flask import Blueprint, request, jsonify
from datetime import datetime
from models.models import db, ApiKey
from utils.auth import admin_required

apikeys_bp = Blueprint('apikeys', __name__)

@apikeys_bp.route('/api/apikeys', methods=['GET'])
@admin_required
def get_api_keys(current_user):
    """获取所有API密钥（仅管理员）"""
    try:
        api_keys = ApiKey.query.all()
        return jsonify([key.to_dict() for key in api_keys])
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@apikeys_bp.route('/api/apikeys', methods=['POST'])
@admin_required
def create_api_key(current_user):
    """创建或更新API密钥"""
    try:
        data = request.get_json()
        model_provider = data.get('model_provider')
        api_key = data.get('api_key')
        base_url = data.get('base_url', '')
        
        if not model_provider or not api_key:
            return jsonify({'error': '模型提供商和API密钥不能为空'}), 400
        
        # 检查是否已存在
        existing_key = ApiKey.query.filter_by(model_provider=model_provider).first()
        if existing_key:
            # 更新现有密钥
            existing_key.api_key = api_key
            existing_key.base_url = base_url
            existing_key.updated_at = datetime.utcnow()
        else:
            # 创建新密钥
            new_key = ApiKey(
                model_provider=model_provider,
                api_key=api_key,
                base_url=base_url
            )
            db.session.add(new_key)
        
        db.session.commit()
        return jsonify({'message': 'API密钥保存成功'})
    except Exception as e:
        db.session.rollback()
        return jsonify({'error': str(e)}), 500

@apikeys_bp.route('/api/apikeys/<int:key_id>', methods=['PUT'])
@admin_required
def update_api_key(current_user, key_id):
    """更新API密钥（仅管理员）"""
    try:
        api_key = ApiKey.query.get_or_404(key_id)
        data = request.get_json()
        
        if 'api_key' in data:
            api_key.api_key = data['api_key']
        if 'base_url' in data:
            api_key.base_url = data['base_url']
        if 'is_active' in data:
            api_key.is_active = data['is_active']
        
        api_key.updated_at = datetime.utcnow()
        db.session.commit()
        
        return jsonify({'message': 'API密钥更新成功'})
    except Exception as e:
        db.session.rollback()
        return jsonify({'error': str(e)}), 500

@apikeys_bp.route('/api/apikeys/<int:key_id>', methods=['DELETE'])
@admin_required
def delete_api_key(current_user, key_id):
    """删除API密钥（仅管理员）"""
    try:
        api_key = ApiKey.query.get_or_404(key_id)
        db.session.delete(api_key)
        db.session.commit()
        
        return jsonify({'message': 'API密钥删除成功'})
    except Exception as e:
        db.session.rollback()
        return jsonify({'error': str(e)}), 500

@apikeys_bp.route('/api/apikeys/<int:key_id>/toggle', methods=['POST'])
@admin_required
def toggle_api_key(current_user, key_id):
    """切换API密钥状态（仅管理员）"""
    try:
        api_key = ApiKey.query.get_or_404(key_id)
        api_key.is_active = not api_key.is_active
        api_key.updated_at = datetime.utcnow()
        db.session.commit()
        
        status = '启用' if api_key.is_active else '禁用'
        return jsonify({'message': f'API密钥已{status}'})
    except Exception as e:
        db.session.rollback()
        return jsonify({'error': str(e)}), 500