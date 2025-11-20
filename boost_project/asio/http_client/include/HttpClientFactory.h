#ifndef HTTP_CLIENT_HTTP_CLIENT_FACTORY_H
#define HTTP_CLIENT_HTTP_CLIENT_FACTORY_H

#include <memory>
#include <string>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace asio = boost::asio;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;

// Forward declarations
template<typename SocketType> class HttpClient;
class HttpResponse;

/**
 * Factory for creating HTTP/HTTPS clients
 * Automatically selects the appropriate client type based on URL scheme
 */
class HttpClientFactory {
public:
    /**
     * Create an HTTP client
     */
    static std::shared_ptr<HttpClient<tcp::socket>>
    CreateHttpClient(std::shared_ptr<asio::io_context> io_context = nullptr);

    /**
     * Create an HTTPS client
     */
    static std::shared_ptr<HttpClient<ssl::stream<tcp::socket>>>
    CreateHttpsClient(std::shared_ptr<asio::io_context> io_context = nullptr);

    /**
     * Create client based on URL scheme
     * Returns a void pointer that can be cast to appropriate type
     * Or use the typed versions above
     */
    static bool IsHttpsUrl(const std::string& url);
};

#endif // HTTP_CLIENT_HTTP_CLIENT_FACTORY_H
