import os
from datetime import timedelta

class Config:
    # 数据库配置
    basedir = os.path.abspath(os.path.dirname(__file__))
    database_dir = os.path.join(os.path.dirname(basedir), "database")
    os.makedirs(database_dir, exist_ok=True)
    db_path = os.path.join(database_dir, "chathub.db")
    
    SQLALCHEMY_DATABASE_URI = f'sqlite:///{db_path}?timeout=20'
    SQLALCHEMY_TRACK_MODIFICATIONS = False
    SQLALCHEMY_ENGINE_OPTIONS = {
        'pool_timeout': 20,
        'pool_recycle': -1,
        'pool_pre_ping': True
    }
    
    # JWT配置
    SECRET_KEY = 'chathub-secret-key-2024'
    JWT_EXPIRATION_DELTA = timedelta(hours=24)
    
    # CORS配置
    CORS_ORIGINS = ['http://192.168.31.21:3000', 'http://localhost:3000']
    CORS_SUPPORTS_CREDENTIALS = True
    
    # 服务器配置
    HOST = '0.0.0.0'
    PORT = 5001
    DEBUG = True