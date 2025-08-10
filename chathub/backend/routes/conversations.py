from flask import Blueprint, request, jsonify
from sqlalchemy import inspect, text
from datetime import datetime
from models.models import db, Conversation, Message, User
from utils.auth import token_required

conversations_bp = Blueprint('conversations', __name__)

@conversations_bp.route('/api/conversations', methods=['GET'])
@token_required
def get_conversations(current_user):
    """获取用户的对话列表"""
    from sqlalchemy import func
    
    user_id = request.args.get('user_id', 1, type=int)
    
    # 获取对话列表并统计每个对话的用户消息数量（只统计用户发出的问题）
    conversations_with_count = db.session.query(
        Conversation,
        func.count(Message.id).label('message_count')
    ).outerjoin(
        Message, 
        db.and_(
            Conversation.conversation_id == Message.conversation_id,
            Message.role == 'user'
        )
    ).filter(
        Conversation.user_id == user_id
    ).group_by(
        Conversation.id
    ).order_by(
        Conversation.updated_at.desc()
    ).all()
    
    result = []
    for conv, message_count in conversations_with_count:
        conv_dict = conv.to_dict()
        conv_dict['message_count'] = message_count
        result.append(conv_dict)
    
    return jsonify(result)

@conversations_bp.route('/api/conversations/<conversation_id>/messages', methods=['GET'])
@token_required
def get_messages(current_user, conversation_id):
    """获取对话的消息历史"""
    messages = Message.query.filter_by(conversation_id=conversation_id)\
                           .order_by(Message.timestamp.asc())\
                           .all()
    # 兼容旧数据：将历史消息中拼接在content内的"推理过程"拆分到reasoning字段
    changed = False
    marker = "\n\n[推理过程]\n"
    for msg in messages:
        if msg.role == 'assistant' and (msg.reasoning is None or (isinstance(msg.reasoning, str) and not msg.reasoning.strip())):
            content = msg.content or ''
            if marker in content or '[推理过程]' in content:
                if marker in content:
                    head, _, tail = content.partition(marker)
                else:
                    idx = content.find('[推理过程]')
                    head = content[:idx]
                    tail = content[idx + len('[推理过程]'):]
                reasoning_text = (tail or '').strip()
                msg.content = head.rstrip()
                msg.reasoning = reasoning_text if reasoning_text else None
                changed = True
    if changed:
        try:
            db.session.commit()
        except Exception as e:
            db.session.rollback()
            print(f"迁移旧消息写入数据库失败: {e}")
    return jsonify([msg.to_dict() for msg in messages])

@conversations_bp.route('/api/conversations/<conversation_id>', methods=['PUT'])
@token_required
def update_conversation(current_user, conversation_id):
    """更新对话信息（如重命名）"""
    try:
        data = request.get_json()
        conversation = Conversation.query.filter_by(conversation_id=conversation_id).first()
        
        if not conversation:
            return jsonify({'error': '对话不存在'}), 404
        
        # 更新标题
        if 'title' in data:
            conversation.title = data['title']
            conversation.updated_at = datetime.utcnow()
        
        db.session.commit()
        return jsonify({'message': '对话已更新', 'conversation': conversation.to_dict()})
        
    except Exception as e:
        db.session.rollback()
        return jsonify({'error': f'更新对话时发生错误: {str(e)}'}), 500

@conversations_bp.route('/api/conversations/<conversation_id>', methods=['DELETE'])
@token_required
def delete_conversation(current_user, conversation_id):
    """删除对话"""
    try:
        # 删除消息
        Message.query.filter_by(conversation_id=conversation_id).delete()
        # 删除对话
        Conversation.query.filter_by(conversation_id=conversation_id).delete()
        
        db.session.commit()
        return jsonify({'message': '对话已删除'})
        
    except Exception as e:
        db.session.rollback()
        return jsonify({'error': f'删除对话时发生错误: {str(e)}'}), 500

@conversations_bp.route('/api/stats', methods=['GET'])
@token_required
def get_stats(current_user):
    """获取用户统计信息"""
    import uuid
    from sqlalchemy import func
    
    user_id = request.args.get('user_id', 1, type=int)
    
    user = User.query.get(user_id)
    if not user:
        # 创建默认用户
        user = User(
            username='demo_user',
            email='demo@example.com',
            api_key=str(uuid.uuid4()),
            subscription_type='pro',
            usage_count=45,
            usage_limit=1000
        )
        db.session.add(user)
        db.session.commit()
    
    # 统计对话数量
    total_conversations = Conversation.query.filter_by(user_id=user_id).count()
    
    # 获取用户的所有对话ID
    user_conversations = Conversation.query.filter_by(user_id=user_id).all()
    conversation_ids = [conv.conversation_id for conv in user_conversations]
    
    # 统计用户消息数量（只统计用户发出的问题）
    if conversation_ids:
        total_messages = Message.query.filter(
            Message.conversation_id.in_(conversation_ids),
            Message.role == 'user'
        ).count()
    else:
        total_messages = 0
    
    # 计算活跃天数（有对话的天数）
    if conversation_ids:
        active_days_result = db.session.query(
            func.count(func.distinct(func.date(Conversation.created_at)))
        ).filter(Conversation.user_id == user_id).scalar()
        days_active = active_days_result or 0
    else:
        days_active = 0
    
    # 计算使用百分比
    usage_percentage = (user.usage_count / user.usage_limit) * 100 if user.usage_limit > 0 else 0
    
    return jsonify({
        'user': user.to_dict(),
        'total_conversations': total_conversations,
        'total_messages': total_messages,
        'tokens_used': user.usage_count,
        'days_active': days_active,
        'usage_percentage': usage_percentage,
        'monthly_usage': user.usage_count,
        'quota_limit': user.usage_limit
    })