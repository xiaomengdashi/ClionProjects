### 生成密钥

`openssl req -x509 -newkey rsa:4096 -keyout server-key.pem -out server-cert.pem -days 365 -nodes`

server-key.pem：服务器的私钥
server-cert.pem：自签名证书（同时作为 CA 证书）
