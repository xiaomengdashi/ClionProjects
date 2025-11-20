#include "HttpClient.h"
#include "UrlParser.h"
#include "SslStreamWrapper.h"
#include <iostream>
#include <thread>

template<typename SocketType>
HttpClient<SocketType>::HttpClient()
    : io_context_(std::make_shared<asio::io_context>()),
      work_guard_(std::make_shared<asio::executor_work_guard<asio::io_context::executor_type>>(io_context_->get_executor())),
      running_(true) {
    RunIOContext();
}

template<typename SocketType>
HttpClient<SocketType>::HttpClient(std::shared_ptr<asio::io_context> io_context)
    : io_context_(io_context),
      running_(true) {
    // If shared, assume caller manages the io_context
}

template<typename SocketType>
HttpClient<SocketType>::~HttpClient() {
    Stop();
}

template<typename SocketType>
std::future<HttpResponse> HttpClient<SocketType>::Get(
    const std::string& url,
    int timeout_ms) {
    return SendRequestWithMethod(url, HttpRequest::Method::GET, "", timeout_ms);
}

template<typename SocketType>
std::future<HttpResponse> HttpClient<SocketType>::Post(
    const std::string& url,
    const std::string& body,
    int timeout_ms) {
    return SendRequestWithMethod(url, HttpRequest::Method::POST, body, timeout_ms);
}

template<typename SocketType>
std::future<HttpResponse> HttpClient<SocketType>::Put(
    const std::string& url,
    const std::string& body,
    int timeout_ms) {
    return SendRequestWithMethod(url, HttpRequest::Method::PUT, body, timeout_ms);
}

template<typename SocketType>
std::future<HttpResponse> HttpClient<SocketType>::Delete(
    const std::string& url,
    int timeout_ms) {
    return SendRequestWithMethod(url, HttpRequest::Method::DELETE, "", timeout_ms);
}

template<typename SocketType>
std::future<HttpResponse> HttpClient<SocketType>::Head(
    const std::string& url,
    int timeout_ms) {
    return SendRequestWithMethod(url, HttpRequest::Method::HEAD, "", timeout_ms);
}

template<typename SocketType>
std::future<HttpResponse> HttpClient<SocketType>::SendRequest(const HttpRequest& request) {
    auto promise = std::make_shared<std::promise<HttpResponse>>();
    auto future = promise->get_future();

    io_context_->post([this, request, promise]() {
        auto connection = std::make_shared<HttpConnection<SocketType>>(io_context_, request.IsHttps());

        connection->AsyncSendRequest(request, [promise](const HttpResponse& response) {
            promise->set_value(response);
        });
    });

    return future;
}

template<typename SocketType>
void HttpClient<SocketType>::AddDefaultHeader(const std::string& key, const std::string& value) {
    default_headers_[key] = value;
}

template<typename SocketType>
void HttpClient<SocketType>::Stop() {
    running_ = false;
    if (work_guard_) {
        work_guard_->reset();  // Allow io_context to finish
    }
    if (io_context_) {
        io_context_->stop();
    }
    if (io_thread_ && io_thread_->joinable()) {
        io_thread_->join();
    }
}

template<typename SocketType>
std::future<HttpResponse> HttpClient<SocketType>::SendRequestWithMethod(
    const std::string& url,
    HttpRequest::Method method,
    const std::string& body,
    int timeout_ms) {

    auto promise = std::make_shared<std::promise<HttpResponse>>();
    auto future = promise->get_future();

    io_context_->post([this, url, method, body, timeout_ms, promise]() {
        HttpRequest request = UrlParser::ParseUrl(url);
        request.SetMethod(method);

        if (!body.empty()) {
            request.SetBody(body);
        }

        // Apply default headers
        for (const auto& [key, value] : default_headers_) {
            if (request.GetHeaders().find(key) == request.GetHeaders().end()) {
                request.SetHeader(key, value);
            }
        }

        // Apply timeout
        int timeout = (timeout_ms < 0) ? default_timeout_ms_ : timeout_ms;
        request.SetTimeoutMs(timeout);

        auto connection = std::make_shared<HttpConnection<SocketType>>(io_context_, request.IsHttps());

        connection->AsyncSendRequest(request, [promise](const HttpResponse& response) {
            promise->set_value(response);
        });
    });

    return future;
}

template<typename SocketType>
void HttpClient<SocketType>::RunIOContext() {
    if (io_thread_) {
        return;  // Already running
    }

    io_thread_ = std::make_shared<std::thread>([this]() {
        try {
            io_context_->run();
        } catch (const std::exception& e) {
            std::cerr << "IO Context error: " << e.what() << std::endl;
        }
    });
}

// Explicit template instantiation for TCP socket
template class HttpClient<tcp::socket>;

// Explicit template instantiation for SSL stream
template class HttpClient<ssl::stream<tcp::socket>>;
