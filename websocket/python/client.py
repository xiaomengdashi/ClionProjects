from ws4py.client.threadedclient import WebSocketClient
import logging
import ssl
import threading

# 配置日志
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class EchoClient(WebSocketClient):
    def opened(self):
        """
        当与服务端成功建立连接时触发此方法
        """
        logger.info("已连接到服务器")
        # 启动输入线程
        input_thread = threading.Thread(target=self.input_loop)
        input_thread.daemon = True
        input_thread.start()

    def input_loop(self):
        """
        监听用户输入并发送消息到服务端的循环
        """
        while True:
            try:
                message = input("请输入要发送的消息（输入 'quit' 退出）: ")
                if message.lower() == 'quit':
                    self.close()
                    break
                self.send(message)
            except Exception as e:
                logger.error(f"发送消息出错: {e}")
                break

    def received_message(self, message):
        """
        当接收到服务端消息时触发此方法
        :param message: 服务端发送的消息对象
        """
        logger.info(f"收到服务器消息: {message}")

    def closed(self, code, reason=None):
        """
        当连接关闭时触发此方法
        :param code: 关闭连接的状态码
        :param reason: 关闭连接的原因
        """
        logger.info(f"连接关闭，状态码: {code}, 原因: {reason}")

if __name__ == '__main__':
    try:
        # 创建 SSL 上下文，忽略自签名证书验证（生产环境不建议）
        ssl_context = ssl.create_default_context()
        ssl_context.check_hostname = False
        ssl_context.verify_mode = ssl.CERT_NONE

        # 创建 WebSocket 客户端实例，连接到服务端
        client = EchoClient('wss://localhost:8080', ssl_options={'ssl_context': ssl_context})
        client.connect()
        client.run_forever()
    except KeyboardInterrupt:
        logger.info("正在关闭客户端...")
        client.close()
        logger.info("客户端已关闭")
    except Exception as e:
        logger.error(f"发生错误: {e}")
