在使用 OpenSSL 时，你可以按以下步骤生成 CA 证书、服务器证书和私钥。下面为你详细介绍生成过程。

## 1. 生成 CA 私钥
CA 私钥用于签发证书，使用 `openssl genpkey` 命令生成一个 2048 位的 RSA 私钥。


```bash
openssl genpkey -algorithm RSA -out ca.key -pkeyopt rsa_keygen_bits:2048
```
 - `genpkey`：用于生成私钥。
 - `-algorithm RSA`：指定使用 RSA 算法。
 - `-out ca.key`：指定私钥文件名为 ca.key。
 - `-pkeyopt rsa_keygen_bits:2048`：指定 RSA 密钥长度为 2048 位。

## 2. 生成 CA 自签名证书
使用上一步生成的私钥创建一个自签名的 CA 证书。

```bash
openssl req -x509 -new -nodes -key ca.key -sha256 -days 3650 -out ca.pem
```
 - `req`：处理证书请求。
 - `-x509`：生成自签名证书。
 - `-new`：创建新的证书请求。
 - `-nodes`：不加密私钥。
 - `-key ca.key`：指定使用的私钥文件。
 - `-sha256`：使用 SHA-256 哈希算法。
 - `-days 3650`：证书有效期为 10 年。
 - `-out ca.pem`：指定输出的 CA 证书文件名为 `ca.pem`。

执行该命令后，会提示你输入一些证书信息，如国家、组织、通用名称等，可按需填写。

## 3. 生成服务器私钥
同样使用 `openssl genpkey` 命令生成服务器私钥。


```bash
openssl genpkey -algorithm RSA -out server.key -pkeyopt rsa_keygen_bits:2048
```
此命令参数与生成 CA 私钥类似，生成的服务器私钥文件名为 `server.key`。

## 4. 生成服务器证书签名请求（CSR）
使用服务器私钥生成证书签名请求，后续由 CA 签发。


```bash
openssl req -new -key server.key -out server.csr
```
 - `-new`：创建新的证书请求。
 - `-key server.key`：指定使用的服务器私钥文件。
 - `-out server.csr`：指定输出的证书签名请求文件名为 `server.csr`。

执行该命令后，同样会提示输入证书信息，通用名称（Common Name）通常填写服务器的域名或 IP 地址。（**例如本地测试时可以设置为：127.0.0.1**）


## 5. 使用 CA 签发服务器证书
利用 CA 私钥和 CA 证书对服务器的 CSR 进行签名，生成服务器证书。


```bash
openssl x509 -req -in server.csr -CA ca.pem -CAkey ca.key -CAcreateserial -out server.crt -days 365 -sha256
```
 - `x509`：处理 X.509 格式的证书。
 - `-req`：处理证书签名请求。
 - `-in server.csr`：指定输入的证书签名请求文件。
 - `-CA ca.pem`：指定 CA 证书文件。
 - `-CAkey ca.key`：指定 CA 私钥文件。
 - `-CAcreateserial`：创建 CA 序列号文件。
 - `-out server.crt`：指定输出的服务器证书文件名为 server.crt。
 - `-days 365`：证书有效期为 1 年。
 - `-sha256`：使用 SHA-256 哈希算法。


## 总结
完成上述步骤后，你会得到以下 3 个文件：

 - `ca.pem`：CA 证书。
 - `server.crt`：服务器证书。
 - `server.key`：服务器私钥。
可将这些文件用于你的 WebSocket 服务器和客户端代码中，修改代码时注意替换相应的文件名。