const WebSocket = require('ws');
const readline = require('readline');
const fs = require('fs');

// 创建控制台接口
const rl = readline.createInterface({
  input: process.stdin,
  output: process.stdout
});

// 忽略自签名证书错误（仅用于测试环境）
process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';

// 创建WebSocket客户端
const client = new WebSocket('wss://localhost:8080');

client.on('open', () => {
  console.log('已连接到服务器');
  promptForMessage();
});

client.on('message', (data) => {
  console.log(`\n收到服务器消息: ${data}`);
  promptForMessage();
});

client.on('error', (error) => {
  console.error('连接错误:', error.message);
});

client.on('close', () => {
  console.log('连接已关闭');
  rl.close();
});

function promptForMessage() {
  rl.question('输入要发送的消息 (输入 exit 退出): ', (message) => {
    if (message.toLowerCase() === 'exit') {
      client.close();
      rl.close();
      return;
    }
    
    client.send(message);
  });
}
