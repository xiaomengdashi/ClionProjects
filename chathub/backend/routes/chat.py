from flask import Blueprint, request, jsonify, Response, current_app
import uuid
import json
from datetime import datetime
from models.models import db, Conversation, Message, ApiKey
from utils.auth import token_required
from utils.ai_client import AIClient

chat_bp = Blueprint('chat', __name__)

@chat_bp.route('/api/chat', methods=['POST'])
@token_required
def chat(current_user):
    """处理聊天请求"""
    data = request.get_json()
    
    # 验证请求数据
    if not data or 'message' not in data or 'model' not in data:
        return jsonify({'error': '缺少必要参数'}), 400
    
    message = data['message']
    model = data['model']
    conversation_id = data.get('conversation_id')
    if not conversation_id:
        conversation_id = str(uuid.uuid4())
    user_id = data.get('user_id', 1)  # 简化处理，实际应该从认证中获取
    
    print(f"接收到的数据: message={message}, model={model}, conversation_id={conversation_id}, user_id={user_id}")
    
    try:
        # 查找或创建对话
        conversation = Conversation.query.filter_by(conversation_id=conversation_id).first()
        if not conversation:
            conversation = Conversation(
                user_id=user_id,
                conversation_id=conversation_id,
                model=model,
                title=message[:50] + '...' if len(message) > 50 else message
            )
            db.session.add(conversation)
        
        # 保存用户消息
        user_message = Message(
            conversation_id=conversation_id,
            role='user',
            content=message
        )
        db.session.add(user_message)
        
        # 统一的模型API调用逻辑
        print(f"正在调用模型: {model}")
        
        # 查询所有API密钥进行调试
        all_keys = ApiKey.query.all()
        print(f"数据库中的所有API密钥: {[(key.model_provider, key.is_active) for key in all_keys]}")
        
        # 尝试查找对应的API密钥（优先查找siliconflow）
        api_key_record = ApiKey.query.filter_by(model_provider='siliconflow', is_active=True).first()
        print(f"查询结果: {api_key_record}")
        
        if api_key_record and api_key_record.api_key:
            print(f"找到API密钥，开始调用API...")
            ai_response = AIClient.call_siliconflow_api(model, message, api_key_record.api_key)
            print(f"API调用完成，响应长度: {len(ai_response)}")
        else:
            print("未找到有效的API密钥")
            ai_response = "请先在API密钥管理中配置相应的API密钥。"
        
        # 保存AI响应（无推理流的模型仅保存内容）
        ai_message = Message(
            conversation_id=conversation_id,
            role='assistant',
            content=ai_response,
            reasoning=None
        )
        db.session.add(ai_message)
        
        # 更新对话时间
        conversation.updated_at = datetime.utcnow()
        db.session.commit()
        
        from utils.time_utils import to_beijing_iso
        return jsonify({
            'conversation_id': conversation_id,
            'response': ai_response,
            'model': model,
            'timestamp': to_beijing_iso(datetime.utcnow())
        })
        
    except Exception as e:
        import traceback
        error_details = traceback.format_exc()
        print(f"聊天API错误: {error_details}")
        db.session.rollback()
        return jsonify({'error': f'处理请求时发生错误: {str(e)}'}), 500

@chat_bp.route('/api/chat/stream', methods=['POST'])
@token_required
def chat_stream(current_user):
    """处理流式聊天请求"""
    data = request.get_json()
    
    # 验证请求数据
    if not data or 'message' not in data or 'model' not in data:
        return jsonify({'error': '缺少必要参数'}), 400
    
    message = data['message']
    model = data['model']
    conversation_id = data.get('conversation_id')
    if not conversation_id:
        conversation_id = str(uuid.uuid4())
    user_id = data.get('user_id', 1)
    
    print(f"流式聊天请求: message={message}, model={model}, conversation_id={conversation_id}")
    
    # 在路由函数中获取应用实例
    app = current_app._get_current_object()
    
    def generate_stream():
        with app.app_context():
            try:
                # 查找或创建对话
                conversation = Conversation.query.filter_by(conversation_id=conversation_id).first()
                if not conversation:
                    conversation = Conversation(
                        user_id=user_id,
                        conversation_id=conversation_id,
                        model=model,
                        title=message[:50] + '...' if len(message) > 50 else message
                    )
                    db.session.add(conversation)
                
                # 保存用户消息
                user_message = Message(
                    conversation_id=conversation_id,
                    role='user',
                    content=message
                )
                db.session.add(user_message)
                db.session.commit()
                
                # 发送初始响应
                yield f"data: {json.dumps({'type': 'start', 'conversation_id': conversation_id})}\n\n"
                
                # 统一的流式模型API调用逻辑
                print(f"开始流式调用模型: {model}")
                api_key_record = ApiKey.query.filter_by(model_provider='siliconflow', is_active=True).first()
                
                if api_key_record and api_key_record.api_key:
                    print(f"开始流式调用API...")
                    
                    # 获取流式响应
                    stream_response = AIClient.call_siliconflow_api_stream(model, message, api_key_record.api_key)
                    
                    if stream_response:
                        complete_response = ""
                        complete_reasoning = ""
                        
                        # 逐步接收并处理响应
                        for chunk in stream_response:
                            if not chunk.choices:
                                continue
                            
                            # 处理普通内容 - delta.content
                            if chunk.choices[0].delta.content:
                                content = chunk.choices[0].delta.content
                                complete_response += content
                                yield f"data: {json.dumps({'type': 'content', 'content': content})}\n\n"
                            
                            # 处理推理内容 - delta.reasoning_content
                            if chunk.choices[0].delta.reasoning_content:
                                reasoning = chunk.choices[0].delta.reasoning_content
                                complete_reasoning += reasoning
                                yield f"data: {json.dumps({'type': 'reasoning', 'content': reasoning})}\n\n"
                        
                        # 在新的应用上下文中保存数据
                        with app.app_context():
                            # 保存AI响应，推理过程和结果分别存储
                            ai_message = Message(
                                conversation_id=conversation_id,
                                role='assistant',
                                content=complete_response,
                                reasoning=complete_reasoning if complete_reasoning else None
                            )
                            db.session.add(ai_message)
                            
                            # 更新对话时间
                            conversation = Conversation.query.filter_by(conversation_id=conversation_id).first()
                            if conversation:
                                conversation.updated_at = datetime.utcnow()
                            db.session.commit()
                        
                        yield f"data: {json.dumps({'type': 'end', 'complete_response': complete_response, 'complete_reasoning': complete_reasoning})}\n\n"
                    else:
                        error_msg = "流式API调用失败"
                        yield f"data: {json.dumps({'type': 'error', 'content': error_msg})}\n\n"
                else:
                    error_msg = "请先在API密钥管理中配置相应的API密钥。"
                    yield f"data: {json.dumps({'type': 'error', 'content': error_msg})}\n\n"
                    
            except Exception as e:
                import traceback
                error_details = traceback.format_exc()
                print(f"流式聊天错误: {error_details}")
                try:
                    with app.app_context():
                        db.session.rollback()
                except:
                    pass  # 忽略rollback错误
                yield f"data: {json.dumps({'type': 'error', 'content': f'处理请求时发生错误: {str(e)}'})}.\n\n"
    
    return Response(
        generate_stream(),
        mimetype='text/event-stream',
        headers={
            'Cache-Control': 'no-cache',
            'Connection': 'keep-alive',
            'Access-Control-Allow-Origin': '*',
            'Access-Control-Allow-Headers': 'Content-Type,Authorization'
        }
    )
