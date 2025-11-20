#include "SslStreamWrapper.h"
#include <iostream>
#include <openssl/x509.h>
#include <mutex>

// Static member initialization
std::shared_ptr<SslStreamWrapper::SslContext> SslStreamWrapper::ssl_context_ = nullptr;

std::shared_ptr<SslStreamWrapper::SslContext> SslStreamWrapper::GetSslContext() {
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!ssl_context_) {
        ssl_context_ = CreateSslContext();
    }
    
    return ssl_context_;
}

std::shared_ptr<SslStreamWrapper::SslContext> SslStreamWrapper::CreateSslContext() {
    auto ctx = std::make_shared<SslContext>(ssl::context::tlsv12_client);

    try {
        // Set options
        ctx->set_options(
            ssl::context::default_workarounds |
            ssl::context::no_sslv2 |
            ssl::context::single_dh_use);

        // Try to load system CA certificates
        boost::system::error_code ec;
        
        // First, try set_default_verify_paths() which uses OpenSSL's default locations
        ctx->set_default_verify_paths(ec);
        if (ec) {
            std::cerr << "Warning: set_default_verify_paths failed: " << ec.message() << std::endl;
            
            // Fallback: try to load from known certificate paths
            std::string cert_path = GetCertificatePath();
            if (!cert_path.empty()) {
                ctx->load_verify_file(cert_path, ec);
                if (ec) {
                    std::cerr << "Warning: Failed to load CA from " << cert_path << ": " << ec.message() << std::endl;
                } else {
                    std::cout << "Loaded CA certificates from: " << cert_path << std::endl;
                }
            } else {
                std::cerr << "Warning: No CA certificate bundle found" << std::endl;
            }
        }

        // Set verify mode - VERIFY_PEER will verify server certificate
        ctx->set_verify_mode(ssl::context::verify_peer);

        // Set verification callback
        ctx->set_verify_callback([](bool preverified, ssl::verify_context& ctx) {
            return VerifyCertificate(preverified, ctx);
        });

    } catch (std::exception& e) {
        std::cerr << "SSL Context Error: " << e.what() << std::endl;
    }

    return ctx;
}

bool SslStreamWrapper::VerifyCertificate(bool preverified,
                                        ssl::verify_context& ctx) {
    // Get the certificate being verified
    X509_STORE_CTX* store_ctx = ctx.native_handle();
    int depth = X509_STORE_CTX_get_error_depth(store_ctx);
    int err = X509_STORE_CTX_get_error(store_ctx);
    X509* cert = X509_STORE_CTX_get_current_cert(store_ctx);

    // Get certificate subject name for logging
    char subject_name[256];
    X509_NAME_oneline(X509_get_subject_name(cert), subject_name, sizeof(subject_name));

    if (!preverified) {
        // Certificate verification failed
        std::cerr << "Certificate verification failed at depth " << depth << std::endl;
        std::cerr << "  Subject: " << subject_name << std::endl;
        std::cerr << "  Error: " << X509_verify_cert_error_string(err) << " (" << err << ")" << std::endl;

        // For self-signed certificates in testing, you can allow them
        // Uncomment the following lines to allow self-signed certificates:
        /*
        if (err == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN ||
            err == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT) {
            std::cerr << "  (Allowing self-signed certificate for testing)" << std::endl;
            return true;
        }
        */
    }

    // Return the preverified status
    return preverified;
}

std::string SslStreamWrapper::GetCertificatePath() {
    // Common paths for CA bundle
    const char* paths[] = {
        "/etc/ssl/certs/ca-certificates.crt",      // Ubuntu/Debian
        "/etc/pki/tls/certs/ca-bundle.crt",        // CentOS/RHEL
        "/usr/local/share/certs/ca-root-nss.crt",  // OpenBSD
        "/etc/ssl/cert.pem",                        // OpenSSL default
        nullptr
    };

    for (const char** p = paths; *p; ++p) {
        FILE* f = fopen(*p, "r");
        if (f) {
            fclose(f);
            return *p;
        }
    }

    return "";
}
