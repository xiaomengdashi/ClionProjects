#include <iostream>
#include <thread>
#include <chrono>
#include "HttpClient.h"
#include "HttpClientFactory.h"

/**
 * Simple HTTP request test (using httpbin.org)
 */
void TestSimpleHttp() {
    std::cout << "\n=== Testing Simple HTTP GET ===" << std::endl;

    try {
        auto client = std::make_shared<HttpClientPlain>();

        std::cout << "Sending GET request to http://httpbin.org/get ..." << std::endl;
        auto future = client->Get("http://httpbin.org/get", 10000);  // 10 second timeout

        auto response = future.get();

        std::cout << "Status Code: " << response.GetStatusCode() << std::endl;
        std::cout << "Response Time: " << response.GetResponseTimeMs() << " ms" << std::endl;

        if (response.HasError()) {
            std::cerr << "Error: " << response.GetErrorMessage() << std::endl;
        } else if (response.GetStatusCode() == 200) {
            std::cout << "✓ Success! Response body length: " << response.GetBody().length() << " bytes" << std::endl;
            std::cout << "Headers:" << std::endl;
            for (const auto& [key, value] : response.GetHeaders()) {
                std::cout << "  " << key << ": " << value << std::endl;
            }
        }

        client->Stop();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

/**
 * Simple HTTPS request test
 */
void TestSimpleHttps() {
    std::cout << "\n=== Testing Simple HTTPS GET ===" << std::endl;

    try {
        auto client = std::make_shared<HttpClientSecure>();

        std::cout << "Sending GET request to https://httpbin.org/get ..." << std::endl;
        auto future = client->Get("https://httpbin.org/get", 10000);  // 10 second timeout

        auto response = future.get();

        std::cout << "Status Code: " << response.GetStatusCode() << std::endl;
        std::cout << "Response Time: " << response.GetResponseTimeMs() << " ms" << std::endl;

        if (response.HasError()) {
            std::cerr << "Error: " << response.GetErrorMessage() << std::endl;
        } else if (response.GetStatusCode() == 200) {
            std::cout << "✓ Success! Response body length: " << response.GetBody().length() << " bytes" << std::endl;
        }

        client->Stop();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

/**
 * POST request test
 */
void TestPost() {
    std::cout << "\n=== Testing POST Request ===" << std::endl;

    try {
        auto client = std::make_shared<HttpClientPlain>();

        std::string json_data = R"({"name":"test","value":123})";

        std::cout << "Sending POST request with JSON data..." << std::endl;
        auto future = client->Post(
            "http://httpbin.org/post",
            json_data,
            10000
        );

        auto response = future.get();

        std::cout << "Status Code: " << response.GetStatusCode() << std::endl;
        std::cout << "Response Time: " << response.GetResponseTimeMs() << " ms" << std::endl;

        if (response.HasError()) {
            std::cerr << "Error: " << response.GetErrorMessage() << std::endl;
        } else if (response.GetStatusCode() == 200) {
            std::cout << "✓ Success! Response body length: " << response.GetBody().length() << " bytes" << std::endl;
        }

        client->Stop();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

/**
 * Multiple concurrent requests
 */
void TestConcurrentRequests() {
    std::cout << "\n=== Testing Concurrent Requests ===" << std::endl;

    try {
        auto io_ctx = std::make_shared<asio::io_context>();

        // Create two clients with shared io_context
        auto http_client = std::make_shared<HttpClientPlain>(io_ctx);
        auto https_client = std::make_shared<HttpClientSecure>(io_ctx);

        std::cout << "Sending multiple concurrent requests..." << std::endl;

        auto future1 = http_client->Get("http://httpbin.org/delay/1");
        auto future2 = https_client->Get("https://httpbin.org/get");

        auto resp1 = future1.get();
        auto resp2 = future2.get();

        std::cout << "Request 1 - Status: " << resp1.GetStatusCode()
                  << ", Time: " << resp1.GetResponseTimeMs() << "ms" << std::endl;
        std::cout << "Request 2 - Status: " << resp2.GetStatusCode()
                  << ", Time: " << resp2.GetResponseTimeMs() << "ms" << std::endl;

        http_client->Stop();
        https_client->Stop();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

/**
 * HEAD request test
 */
void TestHead() {
    std::cout << "\n=== Testing HEAD Request ===" << std::endl;

    try {
        auto client = std::make_shared<HttpClientPlain>();

        std::cout << "Sending HEAD request..." << std::endl;
        auto future = client->Head("http://httpbin.org/get", 10000);

        auto response = future.get();

        std::cout << "Status Code: " << response.GetStatusCode() << std::endl;
        std::cout << "Response Time: " << response.GetResponseTimeMs() << " ms" << std::endl;

        if (response.HasError()) {
            std::cerr << "Error: " << response.GetErrorMessage() << std::endl;
        } else {
            std::cout << "✓ Success!" << std::endl;
            std::cout << "Content-Type: " << response.GetHeader("Content-Type") << std::endl;
            std::cout << "Content-Length: " << response.GetHeader("Content-Length") << std::endl;
        }

        client->Stop();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "HTTP/HTTPS Client Network Test Program" << std::endl;
    std::cout << "=====================================" << std::endl;
    std::cout << "Note: These tests require internet connectivity to httpbin.org" << std::endl;

    // Run tests
    TestSimpleHttp();
    TestSimpleHttps();
    TestHead();
    TestPost();
    TestConcurrentRequests();

    std::cout << "\n=== All Tests Completed ===" << std::endl;

    return 0;
}
