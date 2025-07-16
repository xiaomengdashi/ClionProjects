#include "network_utils.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <regex>

int NetworkUtils::createTCPSocket() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cerr << "创建TCP套接字失败: " << strerror(errno) << std::endl;
        return -1;
    }
    return sockfd;
}

int NetworkUtils::createUDPSocket() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "创建UDP套接字失败: " << strerror(errno) << std::endl;
        return -1;
    }
    return sockfd;
}

int NetworkUtils::createRawSocket() {
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (sockfd < 0) {
        std::cerr << "创建原始套接字失败: " << strerror(errno) << std::endl;
        std::cerr << "注意：原始套接字需要root权限" << std::endl;
        return -1;
    }
    
    // 设置IP_HDRINCL选项，允许手动构造IP头部
    int one = 1;
    if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        std::cerr << "设置IP_HDRINCL失败: " << strerror(errno) << std::endl;
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

bool NetworkUtils::bindSocket(int sockfd, const std::string& ip, uint16_t port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (ip.empty() || ip == "0.0.0.0") {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_aton(ip.c_str(), &addr.sin_addr) == 0) {
            std::cerr << "无效的IP地址: " << ip << std::endl;
            return false;
        }
    }
    
    if (bind(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "绑定套接字失败: " << strerror(errno) << std::endl;
        return false;
    }
    
    return true;
}

bool NetworkUtils::connectSocket(int sockfd, const std::string& ip, uint16_t port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (inet_aton(ip.c_str(), &addr.sin_addr) == 0) {
        std::cerr << "无效的IP地址: " << ip << std::endl;
        return false;
    }
    
    if (connect(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "连接失败: " << strerror(errno) << std::endl;
        return false;
    }
    
    return true;
}

bool NetworkUtils::listenSocket(int sockfd, int backlog) {
    if (listen(sockfd, backlog) < 0) {
        std::cerr << "监听失败: " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

int NetworkUtils::acceptConnection(int sockfd, std::string& client_ip, uint16_t& client_port) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    int client_sockfd = accept(sockfd, reinterpret_cast<struct sockaddr*>(&client_addr), &addr_len);
    if (client_sockfd < 0) {
        std::cerr << "接受连接失败: " << strerror(errno) << std::endl;
        return -1;
    }
    
    client_ip = inet_ntoa(client_addr.sin_addr);
    client_port = ntohs(client_addr.sin_port);
    
    return client_sockfd;
}

ssize_t NetworkUtils::sendData(int sockfd, const std::vector<uint8_t>& data) {
    ssize_t sent = send(sockfd, data.data(), data.size(), 0);
    if (sent < 0) {
        std::cerr << "发送数据失败: " << strerror(errno) << std::endl;
    }
    return sent;
}

ssize_t NetworkUtils::receiveData(int sockfd, std::vector<uint8_t>& buffer, size_t max_size) {
    buffer.resize(max_size);
    ssize_t received = recv(sockfd, buffer.data(), max_size, 0);
    if (received < 0) {
        std::cerr << "接收数据失败: " << strerror(errno) << std::endl;
        buffer.clear();
    } else {
        buffer.resize(received);
    }
    return received;
}

void NetworkUtils::closeSocket(int sockfd) {
    if (sockfd >= 0) {
        close(sockfd);
    }
}

bool NetworkUtils::setNonBlocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0) {
        std::cerr << "获取套接字标志失败: " << strerror(errno) << std::endl;
        return false;
    }
    
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        std::cerr << "设置非阻塞模式失败: " << strerror(errno) << std::endl;
        return false;
    }
    
    return true;
}

bool NetworkUtils::setReuseAddr(int sockfd) {
    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        std::cerr << "设置地址重用失败: " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

std::string NetworkUtils::getLocalIP() {
    struct ifaddrs *ifaddrs_ptr, *ifa;
    std::string local_ip;
    
    if (getifaddrs(&ifaddrs_ptr) == -1) {
        std::cerr << "获取网络接口失败: " << strerror(errno) << std::endl;
        return "127.0.0.1";
    }
    
    for (ifa = ifaddrs_ptr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) continue;
        
        if (ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in* addr_in = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
            std::string ip = inet_ntoa(addr_in->sin_addr);
            
            // 跳过回环地址，选择第一个非回环地址
            if (ip != "127.0.0.1" && local_ip.empty()) {
                local_ip = ip;
            }
        }
    }
    
    freeifaddrs(ifaddrs_ptr);
    
    return local_ip.empty() ? "127.0.0.1" : local_ip;
}

bool NetworkUtils::isValidIP(const std::string& ip) {
    std::regex ip_regex(
        R"(^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)"
    );
    return std::regex_match(ip, ip_regex);
}