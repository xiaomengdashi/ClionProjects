from flask import Blueprint, jsonify

misc_bp = Blueprint('misc', __name__)

@misc_bp.route('/api/health', methods=['GET'])
def health_check():
    """健康检查"""
    return jsonify({'status': 'healthy', 'message': 'AI API服务运行正常'})