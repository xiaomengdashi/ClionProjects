const https = require('https');
const WebSocket = require('ws');
const fs = require('fs');

// 读取证书和私钥
const serverOptions = {
  cert: fs.readFileSync('server-cert.pem'),
  key: fs.readFileSync('server-key.pem')
};

// 创建HTTPS服务器
const server = https.createServer(serverOptions);

// 创建 WebSocket 服务器
const wss = new WebSocket.Server({ server });

wss.on('connection', (ws) => {
  ws.send('Secure Connection Established');
  
  ws.on('message', (message) => {
    console.log('Received:', message.toString());
    ws.send(`Echo: ${message}`);
  });
});

server.listen(8080, '0.0.0.0', () => {
  console.log('Secure WebSocket Server running on wss://localhost:8080');
});
