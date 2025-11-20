#include "HttpConnection.h"
#include "UrlParser.h"
#include "SslStreamWrapper.h"
#include <iostream>
#include <sstream>

template<typename SocketType>
HttpConnection<SocketType>::HttpConnection(
    std::shared_ptr<asio::io_context> io_context,
    bool is_https)
    : io_context_(io_context),
      socket_(*io_context),
      is_https_(is_https),
      timeout_timer_(*io_context) {
}

// Specialization for SSL stream - requires SSL context
template<>
HttpConnection<ssl::stream<tcp::socket>>::HttpConnection(
    std::shared_ptr<asio::io_context> io_context,
    bool is_https)
    : io_context_(io_context),
      socket_(*io_context, *SslStreamWrapper::GetSslContext()),
      is_https_(is_https),
      timeout_timer_(*io_context) {
}

template<typename SocketType>
HttpConnection<SocketType>::~HttpConnection() {
    Close();
}

template<typename SocketType>
void HttpConnection<SocketType>::AsyncSendRequest(
    const HttpRequest& request,
    ResponseCallback callback) {
    current_request_ = request;
    response_callback_ = callback;
    request_start_time_ = std::chrono::steady_clock::now();

    DoResolveAndConnect(request);
}

template<typename SocketType>
void HttpConnection<SocketType>::Close() {
    boost::system::error_code ec;
    GetSocket().close(ec);
    connected_ = false;
}

template<typename SocketType>
void HttpConnection<SocketType>::DoResolveAndConnect(const HttpRequest& request) {
    auto self = this->shared_from_this();

    // Use io_context_.post to ensure resolver stays alive during async operation
    // Lambda captures resolver by value (shared_ptr-like behavior through shared_from_this)
    io_context_->post([this, self, request]() {
        auto resolver = std::make_unique<tcp::resolver>(*io_context_);

        resolver->async_resolve(
            request.GetHost(),
            request.GetPort(),
            [this, self, request, resolver_ptr = resolver.release()](
                const boost::system::error_code& ec,
                const tcp::resolver::results_type& results) mutable {
                std::unique_ptr<tcp::resolver> resolver(resolver_ptr);

                if (ec) {
                    HttpResponse response;
                    response.SetErrorMessage("Resolve failed: " + ec.message());
                    response.SetResponseTimeMs(GetElapsedTimeMs());
                    response_callback_(response);
                    return;
                }

                asio::async_connect(
                    GetSocket(),
                    results,
                    [this, self, request](const boost::system::error_code& ec,
                                         const tcp::resolver::endpoint_type&) {
                        OnConnect(request, ec);
                    });
            });
    });
}

template<typename SocketType>
void HttpConnection<SocketType>::OnConnect(
    const HttpRequest& request,
    const error_code& ec) {
    if (ec) {
        HttpResponse response;
        response.SetErrorMessage("Connect failed: " + ec.message());
        response.SetResponseTimeMs(GetElapsedTimeMs());
        response_callback_(response);
        return;
    }

    connected_ = true;

    // For HTTPS, perform SSL handshake
    if (is_https_) {
        DoSslHandshake(request);
    } else {
        // For HTTP, send request immediately
        SendHttpRequest(request);
    }
}

template<typename SocketType>
void HttpConnection<SocketType>::DoSslHandshake(const HttpRequest& request) {
    auto self = this->shared_from_this();

    // This specialization is only for SSL streams
    if constexpr (std::is_same_v<SocketType, ssl::stream<tcp::socket>>) {
        try {
            SetupSslIfNeeded(request.GetHost());

            socket_.async_handshake(
                ssl::stream_base::client,
                [this, self, request](const boost::system::error_code& ec) {
                    if (ec) {
                        HttpResponse response;
                        response.SetErrorMessage("SSL handshake failed: " + ec.message());
                        response.SetResponseTimeMs(GetElapsedTimeMs());
                        response_callback_(response);
                        return;
                    }

                    SendHttpRequest(request);
                });
        } catch (const std::exception& e) {
            HttpResponse response;
            response.SetErrorMessage(std::string("SSL setup failed: ") + e.what());
            response.SetResponseTimeMs(GetElapsedTimeMs());
            response_callback_(response);
        }
    }
}

template<typename SocketType>
void HttpConnection<SocketType>::SetupSslIfNeeded(const std::string& host) {
    if constexpr (std::is_same_v<SocketType, ssl::stream<tcp::socket>>) {
        // Set SNI (Server Name Indication)
        if (!SSL_set_tlsext_host_name(socket_.native_handle(), host.c_str())) {
            boost::system::error_code ec(static_cast<int>(::ERR_get_error()),
                                        boost::asio::error::get_ssl_category());
            throw boost::system::system_error(ec);
        }
    }
}

template<typename SocketType>
void HttpConnection<SocketType>::SendHttpRequest(const HttpRequest& request) {
    auto self = this->shared_from_this();

    std::string http_request = request.BuildRequestHeader();
    if (!request.GetBody().empty()) {
        http_request += request.GetBody();
    }

    asio::async_write(
        socket_,
        asio::buffer(http_request),
        [this, self, request](const boost::system::error_code& ec,
                             std::size_t bytes_transferred) {
            if (ec) {
                HttpResponse response;
                response.SetErrorMessage("Send failed: " + ec.message());
                response.SetResponseTimeMs(GetElapsedTimeMs());
                response_callback_(response);
                return;
            }

            ReadResponseHeader();
        });
}

template<typename SocketType>
void HttpConnection<SocketType>::ReadResponseHeader() {
    auto self = this->shared_from_this();

    asio::async_read_until(
        socket_,
        receive_buffer_,
        "\r\n\r\n",
        [this, self](const boost::system::error_code& ec,
                    std::size_t bytes_transferred) {
            OnResponseHeaderRead(ec, bytes_transferred);
        });
}

template<typename SocketType>
void HttpConnection<SocketType>::OnResponseHeaderRead(
    const error_code& ec,
    std::size_t bytes_transferred) {
    if (ec && ec != asio::error::eof) {
        HttpResponse response;
        response.SetErrorMessage("Read header failed: " + ec.message());
        response.SetResponseTimeMs(GetElapsedTimeMs());
        response_callback_(response);
        return;
    }

    // Extract header text
    std::istream is(&receive_buffer_);
    std::string header_line;
    std::string header_text;

    std::getline(is, header_line);
    header_text = header_line + "\r\n";

    while (std::getline(is, header_line)) {
        if (header_line == "\r") {
            break;
        }
        header_text += header_line + "\r\n";
    }

    HttpResponse response;
    ParseResponseHeader(header_text, response);

    ReadResponseBody(response);
}

template<typename SocketType>
void HttpConnection<SocketType>::ParseResponseHeader(
    const std::string& header_text,
    HttpResponse& response) {
    std::istringstream iss(header_text);
    std::string line;

    // Parse status line
    if (std::getline(iss, line)) {
        // Extract status code from "HTTP/1.1 200 OK"
        size_t pos = line.find(' ');
        if (pos != std::string::npos) {
            size_t second_space = line.find(' ', pos + 1);
            if (second_space != std::string::npos) {
                std::string status_str = line.substr(pos + 1, second_space - pos - 1);
                try {
                    response.SetStatusCode(std::stoi(status_str));
                } catch (...) {
                    response.SetStatusCode(0);
                }
            }
        }
    }

    // Parse headers
    while (std::getline(iss, line)) {
        if (line.empty() || line == "\r") {
            break;
        }
        // Remove trailing \r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        size_t pos = line.find(": ");
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 2);
            response.SetHeader(key, value);
        }
    }
}

template<typename SocketType>
void HttpConnection<SocketType>::ReadResponseBody(HttpResponse& response) {
    auto self = this->shared_from_this();

    // Create shared_ptr to keep response alive during async operation
    auto response_ptr = std::make_shared<HttpResponse>(response);

    // Extract any data already in the receive buffer
    auto already_read_buffer = std::make_shared<std::string>();
    if (receive_buffer_.size() > 0) {
        std::istream is(&receive_buffer_);
        already_read_buffer->assign(
            std::istreambuf_iterator<char>(is),
            std::istreambuf_iterator<char>());
    }

    // Check for Content-Length header
    auto headers = response.GetHeaders();
    auto content_length_it = headers.find("Content-Length");
    
    if (content_length_it != headers.end()) {
        // Read specific number of bytes
        size_t content_length = std::stoull(content_length_it->second);
        size_t already_read = already_read_buffer->size();
        
        if (already_read >= content_length) {
            // We already have all the data
            response_ptr->AppendBody(already_read_buffer->substr(0, content_length));
            response_ptr->SetResponseTimeMs(GetElapsedTimeMs());
            response_callback_(*response_ptr);
            Close();
            return;
        }
        
        size_t bytes_to_read = content_length - already_read;
        
        // Create a buffer for the remaining bytes
        auto remaining_buffer = std::make_shared<std::string>();
        remaining_buffer->resize(bytes_to_read);
        
        asio::async_read(
            socket_,
            asio::buffer(&(*remaining_buffer)[0], bytes_to_read),
            [this, self, response_ptr, remaining_buffer, already_read_buffer](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred) {
                
                if (ec && ec != asio::error::eof) {
                    response_ptr->SetErrorMessage("Read body failed: " + ec.message());
                } else {
                    // Combine already read data with newly read data
                    response_ptr->AppendBody(*already_read_buffer);
                    response_ptr->AppendBody(*remaining_buffer);
                }
                
                response_ptr->SetResponseTimeMs(GetElapsedTimeMs());
                response_callback_(*response_ptr);
                Close();
            });
    } else {
        // No Content-Length, read until EOF
        response_buffer_.clear();
        asio::async_read(
            socket_,
            asio::dynamic_buffer(response_buffer_),
            [this, self, response_ptr](const boost::system::error_code& ec,
                                   std::size_t bytes_transferred) {
                OnResponseBodyRead(*response_ptr, ec, bytes_transferred);
            });
    }
}

template<typename SocketType>
void HttpConnection<SocketType>::OnResponseBodyRead(
    HttpResponse& response,
    const error_code& ec,
    std::size_t bytes_transferred) {
    if (ec && ec != asio::error::eof) {
        response.SetErrorMessage("Read body failed: " + ec.message());
    } else if (ec == asio::error::eof) {
        // Normal end of response
    }

    response.AppendBody(response_buffer_);
    response.SetResponseTimeMs(GetElapsedTimeMs());

    response_callback_(response);
    Close();
}

template<typename SocketType>
void HttpConnection<SocketType>::OnTimeout(const error_code& ec) {
    if (!ec) {
        HttpResponse response;
        response.SetErrorMessage("Request timeout");
        response.SetResponseTimeMs(GetElapsedTimeMs());
        response_callback_(response);
        Close();
    }
}

// Explicit template instantiation for TCP socket
template class HttpConnection<tcp::socket>;

// Explicit template instantiation for SSL stream
template class HttpConnection<ssl::stream<tcp::socket>>;
