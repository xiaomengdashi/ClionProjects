from flask import Blueprint, request, jsonify
from datetime import datetime
from models.models import db, Model
from utils.auth import admin_required

models_bp = Blueprint('models', __name__)

@models_bp.route('/api/models', methods=['GET'])
def get_models():
    """获取所有大模型列表"""
    try:
        # 获取查询参数
        active_only = request.args.get('active_only', 'false').lower() == 'true'
        provider = request.args.get('provider')
        model_type = request.args.get('type')
        
        # 构建查询
        query = Model.query
        
        if active_only:
            query = query.filter(Model.is_active == True)
        
        if provider:
            query = query.filter(Model.model_provider == provider)
            
        if model_type:
            query = query.filter(Model.model_type == model_type)
        
        # 按排序顺序和创建时间排序
        models = query.order_by(Model.sort_order.asc(), Model.created_at.desc()).all()
        
        return jsonify([model.to_dict() for model in models])
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@models_bp.route('/api/models', methods=['POST'])
@admin_required
def create_model(current_user):
    """创建新的大模型配置"""
    try:
        data = request.get_json()
        
        # 验证必填字段
        required_fields = ['model_name', 'display_name', 'model_provider']
        for field in required_fields:
            if not data.get(field):
                return jsonify({'error': f'{field} 不能为空'}), 400
        
        # 检查模型名称是否已存在
        existing_model = Model.query.filter_by(model_name=data['model_name']).first()
        if existing_model:
            return jsonify({'error': '模型名称已存在'}), 400
        
        # 创建新模型
        new_model = Model(
            model_name=data['model_name'],
            display_name=data['display_name'],
            model_provider=data['model_provider'],
            model_type=data.get('model_type', 'chat'),
            max_tokens=data.get('max_tokens', 4096),
            supports_streaming=data.get('supports_streaming', True),
            supports_function_calling=data.get('supports_function_calling', False),
            supports_vision=data.get('supports_vision', False),
            input_price_per_1k=data.get('input_price_per_1k', 0.0),
            output_price_per_1k=data.get('output_price_per_1k', 0.0),
            description=data.get('description', ''),
            is_active=data.get('is_active', True),
            sort_order=data.get('sort_order', 0)
        )
        
        db.session.add(new_model)
        db.session.commit()
        
        return jsonify({
            'message': '大模型创建成功',
            'model': new_model.to_dict()
        }), 201
    except Exception as e:
        db.session.rollback()
        return jsonify({'error': str(e)}), 500

@models_bp.route('/api/models/<int:model_id>', methods=['PUT'])
@admin_required
def update_model(current_user, model_id):
    """更新大模型配置"""
    try:
        model = Model.query.get_or_404(model_id)
        data = request.get_json()
        
        # 更新字段
        updatable_fields = [
            'display_name', 'model_provider', 'model_type', 'max_tokens',
            'supports_streaming', 'supports_function_calling', 'supports_vision',
            'input_price_per_1k', 'output_price_per_1k', 'description', 'is_active', 'sort_order'
        ]
        
        for field in updatable_fields:
            if field in data:
                setattr(model, field, data[field])
        
        # 如果更新模型名称，需要检查是否重复
        if 'model_name' in data and data['model_name'] != model.model_name:
            existing_model = Model.query.filter_by(model_name=data['model_name']).first()
            if existing_model:
                return jsonify({'error': '模型名称已存在'}), 400
            model.model_name = data['model_name']
        
        model.updated_at = datetime.utcnow()
        db.session.commit()
        
        return jsonify({
            'message': '大模型更新成功',
            'model': model.to_dict()
        })
    except Exception as e:
        db.session.rollback()
        return jsonify({'error': str(e)}), 500

@models_bp.route('/api/models/<int:model_id>', methods=['DELETE'])
@admin_required
def delete_model(current_user, model_id):
    """删除大模型配置"""
    try:
        model = Model.query.get_or_404(model_id)
        db.session.delete(model)
        db.session.commit()
        
        return jsonify({'message': '大模型删除成功'})
    except Exception as e:
        db.session.rollback()
        return jsonify({'error': str(e)}), 500

@models_bp.route('/api/models/<int:model_id>/toggle', methods=['POST'])
@admin_required
def toggle_model(current_user, model_id):
    """切换大模型启用状态"""
    try:
        model = Model.query.get_or_404(model_id)
        model.is_active = not model.is_active
        model.updated_at = datetime.utcnow()
        db.session.commit()
        
        status = '启用' if model.is_active else '禁用'
        return jsonify({'message': f'大模型已{status}'})
    except Exception as e:
        db.session.rollback()
        return jsonify({'error': str(e)}), 500

@models_bp.route('/api/models/providers', methods=['GET'])
def get_providers():
    """获取所有模型提供商列表"""
    try:
        providers = db.session.query(
            Model.model_provider
        ).filter(Model.is_active == True).distinct().all()
        
        # 提供商显示名称映射
        provider_display_map = {
            'openai': 'OpenAI',
            'anthropic': 'Anthropic',
            'google': 'Google',
            'baidu': '百度',
            'alibaba': '阿里巴巴',
            'zhipu': '智谱AI',
            'meta': 'Meta',
            'siliconflow': '硅基流动',
            'deepseek': 'DeepSeek',
            'moonshot': '月之暗面',
            'qwen': '通义千问'
        }
        
        provider_list = [
            {
                'provider': provider[0],
                'display_name': provider_display_map.get(provider[0], provider[0])
            }
            for provider in providers
        ]
        
        return jsonify(provider_list)
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@models_bp.route('/api/models/batch-update-order', methods=['POST'])
@admin_required
def batch_update_order(current_user):
    """批量更新模型排序"""
    try:
        data = request.get_json()
        model_orders = data.get('model_orders', [])
        
        for item in model_orders:
            model_id = item.get('id')
            sort_order = item.get('sort_order')
            
            if model_id and sort_order is not None:
                model = Model.query.get(model_id)
                if model:
                    model.sort_order = sort_order
                    model.updated_at = datetime.utcnow()
        
        db.session.commit()
        return jsonify({'message': '排序更新成功'})
    except Exception as e:
        db.session.rollback()
        return jsonify({'error': str(e)}), 500