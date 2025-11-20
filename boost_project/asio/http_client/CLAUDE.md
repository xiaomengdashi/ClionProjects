# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a high-performance HTTP/HTTPS client library built with C++17 using Boost ASIO. The library uses template metaprogramming to support both plain HTTP (tcp::socket) and HTTPS (ssl::stream) with zero virtual function overhead.

## Build Commands

```bash
cd /home/ubuntu/github/ClionProjects/boost_project/asio/http_client

# Build the project
mkdir -p build && cd build
cmake ..
make

# Clean build
rm -rf build && mkdir build && cd build && cmake .. && make

# Release build with optimizations
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

## Test Commands

```bash
cd /home/ubuntu/github/ClionProjects/boost_project/asio/http_client/build

# Run basic unit tests (URL parsing, factory class)
./bin/test_http

# Run network tests (requires internet connection)
./bin/example_client_demo
```

## Project Structure

```
http_client/
├── include/                      # Header files
│   ├── HttpClient.h             # Template client class (main public API)
│   ├── HttpConnection.h         # Template connection handler (async I/O)
│   ├── HttpRequest.h            # Request object with method/headers/body
│   ├── HttpResponse.h           # Response object with status/headers/body
│   ├── UrlParser.h              # URL parsing utilities
│   ├── SslStreamWrapper.h       # SSL/TLS certificate verification
│   ├── HttpClientFactory.h      # Factory for creating HTTP/HTTPS clients
│   └── HttpException.h          # Exception definitions
├── src/                          # Implementation files
│   ├── HttpClient.cpp           # Client implementation (manages io_context, threads)
│   ├── HttpConnection.cpp       # Async connection implementation (ASIO async handlers)
│   ├── HttpRequest.cpp
│   ├── HttpResponse.cpp
│   ├── UrlParser.cpp
│   ├── SslStreamWrapper.cpp
│   └── HttpClientFactory.cpp
├── test/                         # Test files
│   ├── main.cpp                 # Basic unit tests (URL parsing, factory class)
│   └── test_network.cpp         # Network integration tests
├── CMakeLists.txt               # Build configuration
├── README.md                     # Detailed documentation
└── CLAUDE.md                     # This file
```

## Architecture Overview

### Template-Based Design

The library uses C++ templates to support multiple socket types without virtual functions:

```cpp
// HTTP version (plain socket)
using HttpClientPlain = HttpClient<asio::ip::tcp::socket>;

// HTTPS version (SSL-wrapped socket)
using HttpClientSecure = HttpClient<ssl::stream<tcp::socket>>;
```

Both share the same `HttpClient<T>` template implementation, which adapts to the socket type at compile time. The socket types both conform to ASIO's Stream concept (async_connect, async_write, async_read).

### Request/Response Flow

1. **HttpClient** - User-facing API, manages:
   - Shared `io_context` for ASIO async operations
   - Background thread running `io_context.run()`
   - Default headers and timeout settings
   - Method convenience functions (Get, Post, Put, Delete, Head)

2. **HttpRequest** - Encapsulates:
   - HTTP method (GET, POST, PUT, DELETE, HEAD)
   - URL (automatically parsed by UrlParser)
   - Headers and body
   - Timeout in milliseconds

3. **HttpConnection<T>** - Handles async I/O:
   - Resolves DNS and connects to server
   - Performs SSL handshake if HTTPS (specialization for ssl::stream)
   - Writes HTTP request asynchronously
   - Reads response header and body
   - Manages timeout using ASIO deadline_timer
   - Returns response via callback

4. **HttpResponse** - Contains:
   - Status code and headers
   - Response body
   - Error message if connection failed
   - Response time in milliseconds

### Async Model

- All HTTP methods return `std::future<HttpResponse>`
- Operations run in background thread (managed by `io_context`)
- Caller blocks with `future.get()` when needed
- The `std::promise` is resolved in the ASIO async callback handler

### SSL/TLS Support

For HTTPS requests (`ssl::stream<tcp::socket>`):
- SNI (Server Name Indication) is automatically configured in `HttpConnection::SetupSslIfNeeded()`
- Certificate verification happens in `SslStreamWrapper::VerifyCertificate()`
- The `ssl::stream::handshake()` is called after TCP connect

## Key Classes and Methods

### HttpClient<SocketType>

**Constructor Options:**
- `HttpClient()` - Creates new io_context
- `HttpClient(shared_ptr<io_context>)` - Uses existing io_context for shared resources

**Main API:**
- `Get(url, timeout_ms)` → `future<HttpResponse>`
- `Post(url, body, timeout_ms)` → `future<HttpResponse>`
- `Put(url, body, timeout_ms)` → `future<HttpResponse>`
- `Delete(url, timeout_ms)` → `future<HttpResponse>`
- `Head(url, timeout_ms)` → `future<HttpResponse>`
- `SendRequest(HttpRequest&)` → `future<HttpResponse>`
- `AddDefaultHeader(key, value)` - Headers sent with every request
- `SetDefaultTimeout(ms)` - Default timeout for all requests
- `Stop()` - Gracefully shutdown (IMPORTANT: must be called before destruction)

### HttpConnection<SocketType>

**Core Method:**
- `AsyncSendRequest(request, callback)` - Internally used by HttpClient
  - Handles entire async flow: resolve → connect → (ssl handshake) → write → read → callback

**Template Specialization Points:**
- `GetSocket()` - Uses `if constexpr` to distinguish between `tcp::socket` and `ssl::stream<tcp::socket>`
  - For `tcp::socket`: returns `socket_` directly
  - For `ssl::stream<tcp::socket>`: returns `socket_.lowest_layer()`
- `SetupSslIfNeeded()` - Configured dynamically based on `is_https_` flag in constructor
- Constructor parameter `is_https` distinguishes behavior

**Internal Async Callbacks:**
- `OnConnect()` - Triggered after TCP connection established
- `DoSslHandshake()` - Called for HTTPS, performs TLS handshake
- `SendHttpRequest()` - Writes HTTP request to socket
- `OnResponseHeaderRead()` - Processes HTTP response headers
- `OnResponseBodyRead()` - Reads and processes response body
- `OnTimeout()` - Handles request timeout

The connection lifecycle is fully managed by shared_ptr (`enable_shared_from_this`), ensuring the connection object stays alive during all async operations.

### HttpRequest

**Key Fields:**
- `method_` - HttpRequest::Method enum
- `url_` - Full URL string
- `host_`, `port_`, `path_` - Parsed URL components
- `headers_` - map<string, string>
- `body_` - Request body
- `timeout_ms_` - Request timeout

**Methods:**
- `SetMethod(Method)`, `SetUrl(string)`, `SetHeader(k,v)`, `SetBody(string)`
- `BuildRequestHeader()` - Generates HTTP request line + headers string

### HttpResponse

**Status Check Methods:**
- `IsSuccess()` - 200-299
- `IsRedirect()` - 300-399
- `IsClientError()` - 400-499
- `IsServerError()` - 500+
- `HasError()` - Connection or parsing error

**Accessors:**
- `GetStatusCode()`, `GetHeaders()`, `GetBody()`, `GetHeader(key)`
- `GetErrorMessage()`, `GetResponseTimeMs()`

## Important Implementation Details

### Thread Safety

- `io_context` runs in dedicated background thread
- All ASIO operations are bound to this thread via `io_context.post()` or async handlers
- HttpClient methods are thread-safe from user's perspective
- HttpConnection objects are never shared between requests

### Memory Management

- Use `std::enable_shared_from_this<HttpConnection<T>>` to keep connection alive during async ops
- Promise/future ensures response data persists until user calls `future.get()`
- Critical: Always call `client->Stop()` before destruction to drain pending operations

### Timeout Handling

- `asio::deadline_timer` is used for each request
- On timeout, `OnTimeout()` cancels pending operations and returns error response
- Timer is relative to when request starts, not absolute clock

### URL Parsing

- `UrlParser::ParseUrl()` extracts:
  - scheme (http/https) - determines default port if not specified
  - host - domain or IP
  - port - explicit port or default (80 for HTTP, 443 for HTTPS)
  - path - includes query string

## Compilation Model

### Header-Only vs. Implementation

While the library uses `.h` and `.cpp` files, the template implementation relies on **explicit instantiation** rather than being fully header-only:

- **Header files** (`include/`) - Define template interfaces and non-template classes
- **Source files** (`src/`) - Implement templates and explicitly instantiate for `tcp::socket` and `ssl::stream<tcp::socket>`

This approach provides:
- Faster compilation for user code (templates already instantiated)
- Type safety with compile-time checking
- No header-only bloat in build times

### Linking

The CMakeLists.txt builds a static library `libhttp_client.a` containing:
- Non-template class implementations (HttpRequest, HttpResponse, UrlParser, etc.)
- Pre-compiled template instantiations

User code links against this library and includes only the headers.

## Template Instantiation

The library uses **explicit template instantiation** in `src/HttpClient.cpp`:

```cpp
template class HttpClient<tcp::socket>;
template class HttpClient<ssl::stream<tcp::socket>>;
```

This pre-compiles the template for both HTTP and HTTPS, reducing compilation time for user code. The same is done for `HttpConnection<T>`.

### Why Explicit Instantiation?

Without explicit instantiation, users must include full template implementations in headers, causing:
- Recompilation with every include
- Symbol duplication across compilation units
- Longer build times

With explicit instantiation, users only include type signatures and link against pre-compiled binaries.

### Adding New Socket Types

To support a new socket type (e.g., custom TLS wrapper):

1. Ensure it models the ASIO Stream concept (has `async_connect`, `async_write`, `async_read`)
2. Verify `GetSocket()` in HttpConnection works with your type (uses `if constexpr`)
3. Add explicit instantiation in `src/HttpClient.cpp`:
   ```cpp
   template class HttpClient<YourSocketType>;
   template class HttpConnection<YourSocketType>;
   ```

## Shared io_context Pattern

For concurrent requests using the same client infrastructure:

```cpp
auto io_ctx = std::make_shared<asio::io_context>();

// Multiple clients share the same io_context
auto http_client = std::make_shared<HttpClientPlain>(io_ctx);
auto https_client = std::make_shared<HttpClientSecure>(io_ctx);

// All requests run on the same background thread
auto f1 = http_client->Get("http://example1.com");
auto f2 = https_client->Get("https://example2.com");

auto r1 = f1.get();
auto r2 = f2.get();

http_client->Stop();  // Stops the shared io_context
```

This is useful for resource-constrained environments or when you want all HTTP operations on a single thread.

## Supported HTTP Methods

`HttpRequest::Method` enum supports:
- GET
- POST
- PUT
- DELETE
- HEAD
- OPTIONS
- PATCH

All methods are accessible via `HttpClient`:
```cpp
client->Get(url);
client->Post(url, body);
client->Put(url, body);
client->Delete(url);
client->Head(url);
client->SendRequest(request);  // For OPTIONS, PATCH, or custom methods
```

For PATCH and OPTIONS, use `SendRequest()` with a custom `HttpRequest`:

```cpp
HttpRequest request = UrlParser::ParseUrl(url);
request.SetMethod(HttpRequest::Method::PATCH);
request.SetBody(patch_data);
auto future = client->SendRequest(request);
```

## Request Builder Pattern

`HttpRequest` supports method chaining for fluent API:

```cpp
HttpRequest request("https://api.example.com/data");
request.SetMethod(HttpRequest::Method::POST)
       .SetHeader("Content-Type", "application/json")
       .SetBody(json_data)
       .SetTimeoutMs(10000);

auto response = client->SendRequest(request).get();
```

## Debug Build

To build with debug symbols and without optimizations:

```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

This enables better debugging with gdb/lldb while keeping assertions active.

## Development Workflow

### Typical Edit-Build-Test Cycle

```bash
# Build once
cd build && cmake .. && make

# Run basic tests (no network required)
./bin/test_http

# Run full tests (requires internet)
./bin/example_client_demo
```

### Debugging a Single Test

```bash
# Build with debug symbols
cd build && cmake -DCMAKE_BUILD_TYPE=Debug .. && make

# Debug unit tests
gdb ./bin/test_http

# Debug network tests
gdb ./bin/example_client_demo
```

### Critical: Template Instantiation

The most important thing to remember when modifying templated classes (`HttpClient<T>`, `HttpConnection<T>`):

**Any changes to these templates MUST be followed by explicit instantiation in `src/HttpClient.cpp`:**

```cpp
// At the end of src/HttpClient.cpp
template class HttpClient<tcp::socket>;
template class HttpClient<ssl::stream<tcp::socket>>;
template class HttpConnection<tcp::socket>;
template class HttpConnection<ssl::stream<tcp::socket>>;
```

Without these explicit instantiations:
- Plain HTTP clients (`tcp::socket`) won't compile or will fail at link time
- HTTPS clients (`ssl::stream`) won't compile or will fail at link time
- The library won't work for users

**Always verify both instantiations are present after modifying template code.**

### Common Development Patterns

**When modifying request/response handling:**
- Edit files: `include/HttpRequest.h`, `include/HttpResponse.h`
- Rebuild: `cd build && make`
- Test with: `./bin/test_http` (fast) then `./bin/example_client_demo` (network)

**When modifying async I/O logic:**
- Edit files: `include/HttpConnection.h`, `src/HttpConnection.cpp`
- **IMPORTANT**: Update explicit instantiation in `src/HttpClient.cpp`
- Rebuild: `cd build && make`

**When adding HTTP methods:**
- Edit: `include/HttpRequest.h` (enum), `src/HttpRequest.cpp` (string mapping)
- Edit: `include/HttpClient.h` (convenience method), `src/HttpClient.cpp` (implementation)
- **IMPORTANT**: Ensure instantiations in `src/HttpClient.cpp` still exist
- Add test case in: `test/test_network.cpp`

## Common Development Tasks

### Adding a New HTTP Method

See the "Common Development Patterns" section above - it covers this workflow. Key files to edit:
1. `include/HttpRequest.h` - Add enum value
2. `src/HttpRequest.cpp` - Add string mapping in `GetMethodString()`
3. `include/HttpClient.h` - Add convenience method signature
4. `src/HttpClient.cpp` - Implement method and ensure explicit template instantiation
5. `test/test_network.cpp` - Add test case

### Modifying SSL Certificate Verification

Edit `SslStreamWrapper::VerifyCertificate()` in src/SslStreamWrapper.cpp to implement custom verification logic. Current implementation allows self-signed certs for testing.

For production, consider:
- Verifying the certificate chain
- Checking certificate expiration
- Pinning specific certificates

### Extending HttpResponse with Custom Data

1. Add fields to `HttpResponse` class in include/HttpResponse.h
2. Add getter/setter methods
3. Populate the field in `HttpConnection::OnResponseBodyRead()` in src/HttpConnection.cpp where the full response is available

### Running Tests

```bash
cd /home/ubuntu/github/ClionProjects/boost_project/asio/http_client/build

# Run basic unit tests (URL parsing, factory, etc.)
./bin/test_http

# Run network tests (requires internet)
./bin/example_client_demo
```

Both tests produce verbose output showing request/response details and timing information.

## Known Limitations

1. **No connection pooling** - Each request creates new connection (no Keep-Alive)
2. **No automatic redirects** - 3xx responses not followed
3. **No proxy support** - HTTP proxy not supported
4. **No cookie management** - No state maintained between requests
5. **Basic chunked encoding** - Not optimized for large responses
6. **Single-threaded per client** - Each client has one io_context thread

## Build Requirements

- C++17 compiler (GCC 7+, Clang 5+)
- Boost library with system component: `sudo apt-get install libboost-dev`
- OpenSSL development headers: `sudo apt-get install libssl-dev`

## Common Pitfalls

- **Forgetting `client->Stop()`** - Will hang on client destruction while waiting for io_thread
- **Reusing client after Stop()** - Client is not restartable after Stop()
- **Blocking on future.get()** - Will block caller's thread until response arrives
- **SSL verification failures** - Check `SslStreamWrapper::VerifyCertificate()` and CA cert path
- **Port number as wrong type** - URL parser expects integer port, validates range
