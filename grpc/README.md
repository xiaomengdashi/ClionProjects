### 生成RPC源码

```bash
protoc --cpp_out=. --grpc_out=. --plugin=protoc-gen-grpc="/usr/local/bin/grpc_cpp_plugin" echoserver.proto 
```