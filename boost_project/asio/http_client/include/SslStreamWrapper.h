#ifndef HTTP_CLIENT_SSL_STREAM_WRAPPER_H
#define HTTP_CLIENT_SSL_STREAM_WRAPPER_H

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <memory>
#include <string>

namespace asio = boost::asio;
namespace ssl = asio::ssl;

class SslStreamWrapper {
public:
    using SslContext = ssl::context;
    using SslStream = ssl::stream<asio::ip::tcp::socket>;

    /**
     * Get shared SSL context for HTTPS client (singleton)
     */
    static std::shared_ptr<SslContext> GetSslContext();

    /**
     * Create SSL context for HTTPS client
     */
    static std::shared_ptr<SslContext> CreateSslContext();

    /**
     * Verify certificate callback
     */
    static bool VerifyCertificate(bool preverified,
                                  ssl::verify_context& ctx);

private:
    static std::string GetCertificatePath();
    static std::shared_ptr<SslContext> ssl_context_;  // Singleton instance
};

#endif // HTTP_CLIENT_SSL_STREAM_WRAPPER_H
