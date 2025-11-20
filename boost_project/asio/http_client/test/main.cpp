#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include "HttpClient.h"
#include "HttpClientFactory.h"
#include "UrlParser.h"


/**
 * Test HTTP request with actual server (www.baidu.com)
 * Requires internet connection
 */
void TestHttpRequest() {
    std::cout << "\n=== Testing HTTP GET Request ===" << std::endl;
    std::cout << "Connecting to www.baidu.com (public testing service)..." << std::endl;

    try {
        auto client = std::make_shared<HttpClient<asio::ip::tcp::socket>>();

        std::cout << "\nSending GET request to http://www.baidu.com/" << std::endl;
        auto future = client->Get("http://www.baidu.com/", 4000);  // 10 second timeout

        std::cout << "Waiting for response..." << std::endl;
        auto response = future.get();

        std::cout << "Status Code: " << response.GetStatusCode() << std::endl;
        std::cout << "Response Time: " << response.GetResponseTimeMs() << " ms" << std::endl;

        if (response.HasError()) {
            std::cout << "Error: " << response.GetErrorMessage() << std::endl;
        } else if (response.GetStatusCode() == 200) {
            std::cout << "✓ Success!" << std::endl;
            std::cout << "Body length: " << response.GetBody().length() << " bytes" << std::endl;
            std::cout << "Response headers:" << std::endl;
            for (const auto& [key, value] : response.GetHeaders()) {
                std::cout << "  " << key << ": " << value << std::endl;
            }
        } else {
            std::cout << "Unexpected status code: " << response.GetStatusCode() << std::endl;
        }

        client->Stop();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

/**
 * Test HTTPS request
 * Requires internet connection
 */
void TestHttpsRequest() {
    std::cout << "\n=== Testing HTTPS GET Request ===" << std::endl;
    std::cout << "Connecting to www.baidu.com (public testing service)..." << std::endl;

    try {
        auto client = std::make_shared<HttpClient<ssl::stream<tcp::socket>>>();

        std::cout << "\nSending GET request to https://www.baidu.com/" << std::endl;
        auto future = client->Get("https://www.baidu.com/", 10000);  // 10 second timeout

        std::cout << "Waiting for response..." << std::endl;
        auto response = future.get();

        std::cout << "Status Code: " << response.GetStatusCode() << std::endl;
        std::cout << "Response Time: " << response.GetResponseTimeMs() << " ms" << std::endl;

        if (response.HasError()) {
            std::cout << "Error: " << response.GetErrorMessage() << std::endl;
        } else if (response.GetStatusCode() == 200) {
            std::cout << "✓ Success!" << std::endl;
            std::cout << "Body length: " << response.GetBody().length() << " bytes" << std::endl;
        } else {
            std::cout << "Unexpected status code: " << response.GetStatusCode() << std::endl;
        }

        client->Stop();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

/**
 * Test POST request
 * Requires internet connection
 */
void TestPostRequest() {
    std::cout << "\n=== Testing POST Request ===" << std::endl;

    try {
        auto client = std::make_shared<HttpClient<asio::ip::tcp::socket>>();

        std::string json_body = R"({"username":"testuser","password":"testpass"})";
        std::cout << "\nSending POST request to http://httpbin.org/post" << std::endl;
        std::cout << "Body: " << json_body << std::endl;

        auto future = client->Post("http://httpbin.org/post", json_body, 10000);

        std::cout << "Waiting for response..." << std::endl;
        auto response = future.get();

        std::cout << "Status Code: " << response.GetStatusCode() << std::endl;
        std::cout << "Response Time: " << response.GetResponseTimeMs() << " ms" << std::endl;

        if (response.HasError()) {
            std::cout << "Error: " << response.GetErrorMessage() << std::endl;
        } else if (response.GetStatusCode() == 200) {
            std::cout << "✓ Success!" << std::endl;
            std::cout << "Response body length: " << response.GetBody().length() << " bytes" << std::endl;
        }

        client->Stop();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

int main()
{
    std::cout << "\n========================================" << std::endl;
    std::cout << "Network Tests (requires internet)" << std::endl;
    std::cout << "========================================" << std::endl;

    TestHttpRequest();
    TestHttpsRequest();
    TestPostRequest();

    std::cout << "\n========================================" << std::endl;
    std::cout << "All tests completed!" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}