import gevent.monkey
gevent.monkey.patch_all()  # 应用 gevent 补丁

from ws4py.server.geventserver import WSGIServer
from ws4py.server.wsgiutils import WebSocketWSGIApplication
from ws4py.websocket import WebSocket
import logging
import ssl
import os

# 配置日志
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class EchoWebSocket(WebSocket):
    def received_message(self, message):
        """
        当接收到客户端消息时触发此方法
        :param message: 客户端发送的消息对象
        """
        logger.info(f"收到消息: {message}")
        # 回显消息给客户端
        self.send(message.data, message.is_binary)

    def closed(self, code, reason=None):
        """
        当连接关闭时触发此方法
        :param code: 关闭连接的状态码
        :param reason: 关闭连接的原因
        """
        logger.info(f"连接关闭，状态码: {code}, 原因: {reason}")

if __name__ == '__main__':
    # 创建 WebSocket 应用
    app = WebSocketWSGIApplication(handler_cls=EchoWebSocket)

    # 创建 SSL 上下文并加载证书
    ssl_context = ssl.create_default_context(ssl.Purpose.CLIENT_AUTH)

    # 检查证书文件是否存在
    cert_path = 'server-cert.pem'
    key_path = 'server-key.pem'
    if not os.path.exists(cert_path) or not os.path.exists(key_path):
        logger.error("证书文件或密钥文件不存在，请检查路径。")
    else:
        try:
            ssl_context.load_cert_chain(certfile=cert_path, keyfile=key_path)
            logger.info("证书和密钥加载成功")
        except Exception as e:
            logger.error(f"加载证书和密钥时出错: {e}")

    # 创建 WebSocket 服务器，监听本地 8080 端口
    server = WSGIServer(
        ('0.0.0.0', 8080), 
        app,
        ssl_context=ssl_context
    )
    logger.info("WebSocket 服务器 (wss) 已启动，监听端口 8080...")
    try:
        # 启动服务器
        server.serve_forever()
    except KeyboardInterrupt:
        logger.info("正在关闭 WebSocket 服务器...")
        server.stop()
        logger.info("WebSocket 服务器已关闭")
