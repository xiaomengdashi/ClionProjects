下面为你详细介绍使用 OpenSSL 生成 CA 证书、服务端证书和私钥的步骤。

### 1. 生成 CA 私钥
首先，生成一个用于 CA 的私钥文件，可使用 `openssl genpkey` 命令。以下命令会生成一个 2048 位的 RSA 私钥文件 `ca-key.pem`。


```bash
openssl genpkey -algorithm RSA -out ca-key.pem -pkeyopt rsa_keygen_bits:2048
```
### 2. 生成 CA 自签名证书
借助生成的私钥，创建一个自签名的 CA 证书 `ca-cert.pem`。在执行命令过程中，会要求输入一些证书相关信息，如国家、组织、通用名称等，可按需填写。


```bash
openssl req -new -x509 -days 3650 -key ca-key.pem -out ca-cert.pem
```
参数说明：

- new：创建一个新的证书请求。
- x509：生成自签名的 X.509 证书。
- days 3650：设置证书有效期为 10 年。
- key ca-key.pem：指定使用的私钥文件。
- out ca-cert.pem：指定输出的证书文件。
### 3. 生成服务端私钥
使用相同的方法生成服务端私钥 `server-key.pem`。


```bash
openssl genpkey -algorithm RSA -out server-key.pem -pkeyopt rsa_keygen_bits:2048
```

### 4. 生成服务端证书签名请求（CSR）
基于服务端私钥生成证书签名请求 `server-csr.pem`，执行命令时同样需要输入证书相关信息，通用名称（Common Name）建议填写服务器的域名或 IP 地址。


```bash
openssl req -new -key server-key.pem -out server-csr.pem
```

### 5. 使用 CA 证书和私钥签发服务端证书
利用之前生成的 CA 证书和私钥，对服务端的证书签名请求进行签发，生成服务端证书 `server-cert.pem`。


```bash
openssl x509 -req -in server-csr.pem -CA ca-cert.pem -CAkey ca-key.pem -CAcreateserial -out server-cert.pem -days 365
```
参数说明：

- -req：表示处理证书签名请求。
- -in server-csr.pem：指定输入的证书签名请求文件。
- -CA ca-cert.pem：指定使用的 CA 证书。
- -CAkey ca-key.pem：指定使用的 CA 私钥。
- -CAcreateserial：创建 CA 证书的序列号文件。
- -out server-cert.pem：指定输出的服务端证书文件。
- -days 365：设置服务端证书有效期为 1 年。

### 总结
通过以上步骤，你可以生成：

- CA 私钥：`ca-key.pem`
- CA 自签名证书：`ca-cert.pem`
- 服务端私钥：`server-key.pem`
- 服务端证书：`server-cert.pem`

注意事项
- 实际生产环境中，建议使用更长的密钥长度（如 4096 位）来提高安全性。
- 要妥善保管好私钥文件，防止泄露。
- 证书有效期不宜设置过长，需定期更新证书。