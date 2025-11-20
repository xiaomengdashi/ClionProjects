#ifndef HTTP_CLIENT_HTTP_CLIENT_H
#define HTTP_CLIENT_HTTP_CLIENT_H

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <memory>
#include <future>
#include <thread>
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "HttpConnection.h"

namespace asio = boost::asio;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;

/**
 * Generic HTTP/HTTPS client template
 * SocketType can be either tcp::socket or ssl::stream<tcp::socket>
 */
template<typename SocketType>
class HttpClient {
public:
    explicit HttpClient();
    explicit HttpClient(std::shared_ptr<asio::io_context> io_context);
    ~HttpClient();

    /**
     * Send GET request
     */
    std::future<HttpResponse> Get(const std::string& url, int timeout_ms = 5000);

    /**
     * Send POST request
     */
    std::future<HttpResponse> Post(const std::string& url,
                                   const std::string& body,
                                   int timeout_ms = 5000);

    /**
     * Send PUT request
     */
    std::future<HttpResponse> Put(const std::string& url,
                                  const std::string& body,
                                  int timeout_ms = 5000);

    /**
     * Send DELETE request
     */
    std::future<HttpResponse> Delete(const std::string& url, int timeout_ms = 5000);

    /**
     * Send HEAD request
     */
    std::future<HttpResponse> Head(const std::string& url, int timeout_ms = 5000);

    /**
     * Send custom HTTP request
     */
    std::future<HttpResponse> SendRequest(const HttpRequest& request);

    /**
     * Set default timeout for all requests
     */
    void SetDefaultTimeout(int timeout_ms) { default_timeout_ms_ = timeout_ms; }

    /**
     * Add default header for all requests
     */
    void AddDefaultHeader(const std::string& key, const std::string& value);

    /**
     * Get the IO context
     */
    std::shared_ptr<asio::io_context> GetIOContext() const { return io_context_; }

    /**
     * Stop the client
     */
    void Stop();

private:
    std::shared_ptr<asio::io_context> io_context_;
    std::shared_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_;
    std::shared_ptr<std::thread> io_thread_;
    std::map<std::string, std::string> default_headers_;
    int default_timeout_ms_ = 5000;
    bool running_ = false;

    /**
     * Send request with method
     */
    std::future<HttpResponse> SendRequestWithMethod(
        const std::string& url,
        HttpRequest::Method method,
        const std::string& body = "",
        int timeout_ms = -1);

    /**
     * Run io_context in background thread
     */
    void RunIOContext();
};

// Type aliases for convenience
using HttpClientPlain = HttpClient<asio::ip::tcp::socket>;
using HttpClientSecure = HttpClient<ssl::stream<tcp::socket>>;

#endif // HTTP_CLIENT_HTTP_CLIENT_H
