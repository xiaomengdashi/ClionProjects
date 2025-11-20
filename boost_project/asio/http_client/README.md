# HTTP/HTTPS 客户端库 - Boost ASIO 实现

使用 C++17 模板和纯 ASIO 实现的高性能 HTTP/HTTPS 客户端库。

## 特性

✅ **模板设计** - 支持 HTTP 和 HTTPS，编译期自动适配
✅ **纯 ASIO** - 完全基于 Boost ASIO，无额外虚基类
✅ **异步非阻塞** - 使用 std::future 返回结果
✅ **自动 SSL/TLS** - HTTPS 自动处理证书验证
✅ **连接管理** - 支持连接建立、超时控制
✅ **完整的 HTTP 支持** - GET, POST, PUT, DELETE, HEAD, OPTIONS
✅ **URL 解析** - 自动提取 host, port, path, scheme

## 项目结构

```
http_client/
├── include/
│   ├── HttpClient.h              # 模板客户端类
│   ├── HttpConnection.h          # 模板连接类
│   ├── HttpRequest.h             # 请求类
│   ├── HttpResponse.h            # 响应类
│   ├── UrlParser.h               # URL 解析器
│   ├── SslStreamWrapper.h        # SSL 辅助类
│   ├── HttpClientFactory.h       # 工厂类
│   └── HttpException.h           # 异常定义
├── src/
│   ├── HttpClient.cpp
│   ├── HttpConnection.cpp
│   ├── HttpRequest.cpp
│   ├── HttpResponse.cpp
│   ├── UrlParser.cpp
│   ├── SslStreamWrapper.cpp
│   ├── HttpClientFactory.cpp
│   ├── main.cpp                  # 基础测试
│   └── test_network.cpp          # 网络测试
├── CMakeLists.txt
└── README.md
```

## 编译

### 前置需求
- C++17 编译器（GCC 7+ 或 Clang 5+）
- Boost 库（system 组件）
- OpenSSL 库

### Linux/Mac 编译

```bash
cd http_client
mkdir build
cd build
cmake ..
make
```

### 编译输出
- `bin/test_http` - 单元与网络测试
- `libhttp_client.a` - 静态库

## 使用示例

### 基本 HTTP GET 请求

```cpp
#include "HttpClient.h"

// 创建 HTTP 客户端
auto client = std::make_shared<HttpClient<boost::asio::ip::tcp::socket>>();

// 发送 GET 请求
auto future = client->Get("http://example.com", 5000);  // 5秒超时

// 等待响应
auto response = future.get();

std::cout << "Status: " << response.GetStatusCode() << std::endl;
std::cout << "Body: " << response.GetBody() << std::endl;

client->Stop();
```

### HTTPS GET 请求

```cpp
#include "HttpClient.h"

// 创建 HTTPS 客户端
auto client = std::make_shared<HttpClient<ssl::stream<tcp::socket>>>();

auto future = client->Get("https://api.github.com/users/torvalds");
auto response = future.get();

if (response.IsSuccess()) {
    std::cout << "请求成功!" << std::endl;
    std::cout << response.GetBody() << std::endl;
}

client->Stop();
```

### POST 请求

```cpp
auto client = std::make_shared<HttpClientPlain>();

std::string json_data = R"({"username":"admin","password":"123"})";
auto future = client->Post("http://example.com/login", json_data);

auto response = future.get();
if (response.IsSuccess()) {
    // 处理响应
}
```

### 使用工厂类自动选择 HTTP/HTTPS

```cpp
#include "HttpClientFactory.h"

auto http_client = HttpClientFactory::CreateHttpClient();
auto https_client = HttpClientFactory::CreateHttpsClient();

auto future1 = http_client->Get("http://example.com");
auto future2 = https_client->Get("https://example.com");
```

### 并发请求

```cpp
auto io_ctx = std::make_shared<asio::io_context>();

auto http_client = std::make_shared<HttpClientPlain>(io_ctx);
auto https_client = std::make_shared<HttpClientSecure>(io_ctx);

// 发送多个请求
auto f1 = http_client->Get("http://example1.com");
auto f2 = https_client->Get("https://example2.com");
auto f3 = http_client->Post("http://example3.com", "data");

// 等待所有响应
auto r1 = f1.get();
auto r2 = f2.get();
auto r3 = f3.get();
```

### 自定义请求头

```cpp
auto client = std::make_shared<HttpClientPlain>();

client->AddDefaultHeader("User-Agent", "MyApp/1.0");
client->AddDefaultHeader("Accept", "application/json");

auto future = client->Get("http://api.example.com/data");
auto response = future.get();
```

## API 参考

### HttpClient<SocketType>

#### 方法

```cpp
std::future<HttpResponse> Get(const std::string& url, int timeout_ms = 5000);
std::future<HttpResponse> Post(const std::string& url, const std::string& body, int timeout_ms = 5000);
std::future<HttpResponse> Put(const std::string& url, const std::string& body, int timeout_ms = 5000);
std::future<HttpResponse> Delete(const std::string& url, int timeout_ms = 5000);
std::future<HttpResponse> Head(const std::string& url, int timeout_ms = 5000);
std::future<HttpResponse> SendRequest(const HttpRequest& request);

void SetDefaultTimeout(int timeout_ms);
void AddDefaultHeader(const std::string& key, const std::string& value);
void Stop();
```

### HttpRequest

#### 主要属性
- `method_` - HTTP 方法
- `url_` - 完整 URL
- `host_` - 解析后的主机名
- `port_` - 解析后的端口
- `path_` - 路径 + 查询字符串
- `headers_` - 请求头
- `body_` - 请求体
- `timeout_ms_` - 超时时间

#### 方法
```cpp
SetMethod(Method m)
SetUrl(const std::string& url)
SetHeader(const std::string& key, const std::string& value)
SetBody(const std::string& body)
SetTimeoutMs(int timeout)
GetMethodString() const
BuildRequestHeader() const
```

### HttpResponse

#### 主要属性
- `status_code_` - HTTP 状态码
- `headers_` - 响应头
- `body_` - 响应体
- `error_message_` - 错误信息
- `response_time_ms_` - 响应时间

#### 方法
```cpp
int GetStatusCode() const
const std::map<std::string, std::string>& GetHeaders() const
const std::string& GetBody() const
const std::string& GetErrorMessage() const
long long GetResponseTimeMs() const

bool IsSuccess() const          // 200-299
bool IsRedirect() const         // 300-399
bool IsClientError() const      // 400-499
bool IsServerError() const      // 500+
bool HasError() const
std::string GetHeader(const std::string& key) const
```

### UrlParser

```cpp
static HttpRequest ParseUrl(const std::string& url);
```

自动解析 URL 并填充 HttpRequest 对象。

## 设计理念

### 1. 模板元编程

使用 C++ 模板在编译期自动适配 HTTP 和 HTTPS：

```cpp
// HTTP
using HttpClientPlain = HttpClient<asio::ip::tcp::socket>;

// HTTPS
using HttpClientSecure = HttpClient<ssl::stream<tcp::socket>>;
```

### 2. 纯 ASIO 实现

完全基于 Boost ASIO 的异步 I/O：
- 不使用虚基类和虚函数
- 编译器可以充分优化
- 零运行时多态开销

### 3. Stream 抽象

ASIO 的 Stream 概念被 `tcp::socket` 和 `ssl::stream` 都支持：
- `async_connect()`
- `async_write()`
- `async_read()`
- `set_option()`

### 4. 异步模式

使用 `std::future` 和 `std::promise`：

```cpp
auto future = client->Get(url);
// ... 做其他事情 ...
auto response = future.get();  // 阻塞等待或返回已完成的结果
```

## SSL/TLS 支持

### 证书验证

默认设置：
- 验证服务器证书
- 允许自签名证书用于测试

在 `SslStreamWrapper::VerifyCertificate()` 中修改验证逻辑以满足实际生产需求。

### 系统 CA 自动加载

- 首选使用 OpenSSL 的系统默认路径加载 CA（`set_default_verify_paths`）
- 若失败，自动回退到常见 CA 路径（如 `/etc/ssl/certs/ca-certificates.crt` 等）
- 通过单例 SSL 上下文（`SslStreamWrapper::GetSslContext()`）避免重复加载、提升性能

### SNI (Server Name Indication)

自动设置 SNI，支持基于名称的虚拟主机。

## 编译配置

### 依赖项

```cmake
find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
```

### 链接库

- Boost.System
- OpenSSL (SSL + Crypto)
- pthread

## 测试

### 基础测试

```bash
./bin/http_client_test
```

测试内容：
- URL 解析
- 工厂类

### 网络测试

```bash
./bin/http_client_test_network
```

测试内容（需要网络连接）：
- HTTP GET 请求
- HTTPS GET 请求
- HEAD 请求
- POST 请求
- 并发请求

## 已知限制

1. **连接复用** - 当前版本每个请求创建新连接，不支持 Keep-Alive 复用
2. **重定向** - 不自动跟随 3xx 重定向
3. **分块编码** - 基础支持，但不是优化的
4. **代理支持** - 暂不支持 HTTP 代理
5. **Cookie** - 不维护 cookie 状态

## 后续改进

- [ ] 连接池和连接复用
- [ ] 自动重定向跟随
- [ ] 压缩编码支持（gzip, deflate）
- [ ] HTTP 代理支持
- [ ] Cookie 管理
- [ ] 自定义回调而不仅仅是 Future
- [ ] 请求体流式传输

## 许可证

MIT License

## 作者

基于 Boost ASIO 的 HTTP/HTTPS 客户端实现

---

## 故障排查

### 编译错误

如遇到 "not found" 错误，确保：
1. Boost 库已正确安装：`sudo apt-get install libboost-dev`
2. OpenSSL 已安装：`sudo apt-get install libssl-dev`
3. 使用 C++17 编译器

### SSL/TLS 握手失败

- 检查网络连接
- 验证系统 CA 是否可用（优先使用 `set_default_verify_paths`）
- 如失败，检查常见 CA 路径是否存在（例如 `/etc/ssl/certs/ca-certificates.crt`）
- 对于自签名证书，按需在 `VerifyCertificate()` 中放行（仅测试环境）

### IO 线程未运行 / run() 立即退出

- 现已添加 `executor_work_guard` 保持 `io_context` 存活
- 如出现立即退出，确认未提前 `reset()` 或 `stop()`
- 正确调用顺序：构造客户端 → 发送请求 → 获取结果 → 调用 `Stop()`

### 超时问题

增加超时时间：
```cpp
client->Get(url, 15000);  // 15 秒
```

### 内存泄漏

确保调用 `client->Stop()` 来优雅关闭：
```cpp
client->Stop();  // 重要！
```
