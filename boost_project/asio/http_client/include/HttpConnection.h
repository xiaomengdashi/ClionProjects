#ifndef HTTP_CLIENT_HTTP_CONNECTION_H
#define HTTP_CLIENT_HTTP_CONNECTION_H

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/system/error_code.hpp>
#include <memory>
#include <functional>
#include <string>
#include "HttpRequest.h"
#include "HttpResponse.h"

namespace asio = boost::asio;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;
using error_code = boost::system::error_code;

/**
 * Generic HTTP/HTTPS connection template
 * SocketType can be either tcp::socket or ssl::stream<tcp::socket>
 */
template<typename SocketType>
class HttpConnection : public std::enable_shared_from_this<HttpConnection<SocketType>> {
public:
    using ResponseCallback = std::function<void(const HttpResponse&)>;
    using SelfPtr = std::shared_ptr<HttpConnection<SocketType>>;

    explicit HttpConnection(std::shared_ptr<asio::io_context> io_context,
                          bool is_https = false);
    ~HttpConnection();

    /**
     * Send HTTP request asynchronously
     */
    void AsyncSendRequest(const HttpRequest& request,
                         ResponseCallback callback);

    /**
     * Check if connection is still open
     */
    bool IsConnected() const { return connected_; }

    /**
     * Close the connection
     */
    void Close();

private:
    std::shared_ptr<asio::io_context> io_context_;
    SocketType socket_;
    bool is_https_;
    bool connected_ = false;

    // Buffers for receiving data
    asio::streambuf receive_buffer_;
    std::string response_buffer_;

    // Request info for response processing
    HttpRequest current_request_;
    ResponseCallback response_callback_;
    std::chrono::steady_clock::time_point request_start_time_;

    // Timer for timeout
    asio::deadline_timer timeout_timer_;

    /**
     * Internal method to resolve host and connect
     */
    void DoResolveAndConnect(const HttpRequest& request);

    /**
     * Internal method for connection establishment
     */
    void OnConnect(const HttpRequest& request,
                   const error_code& ec);

    /**
     * SSL handshake for HTTPS
     */
    void DoSslHandshake(const HttpRequest& request);

    /**
     * Send the HTTP request after connection is established
     */
    void SendHttpRequest(const HttpRequest& request);

    /**
     * Read response header
     */
    void ReadResponseHeader();

    /**
     * Handle response header read
     */
    void OnResponseHeaderRead(const error_code& ec,
                             std::size_t bytes_transferred);

    /**
     * Parse HTTP status line and headers
     */
    void ParseResponseHeader(const std::string& header_text,
                            HttpResponse& response);

    /**
     * Read response body
     */
    void ReadResponseBody(HttpResponse& response);

    /**
     * Handle response body read
     */
    void OnResponseBodyRead(HttpResponse& response,
                           const error_code& ec,
                           std::size_t bytes_transferred);

    /**
     * Handle timeout
     */
    void OnTimeout(const error_code& ec);

    /**
     * Get the underlying socket for operations
     */
    auto& GetSocket() {
        if constexpr (std::is_same_v<SocketType, tcp::socket>) {
            return socket_;
        } else {
            return socket_.lowest_layer();
        }
    }

    /**
     * Helper to enable SSL if needed (specialization for ssl::stream)
     */
    void SetupSslIfNeeded(const std::string& host);

    /**
     * Calculate response time
     */
    long long GetElapsedTimeMs() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now - request_start_time_).count();
    }
};

#endif // HTTP_CLIENT_HTTP_CONNECTION_H
