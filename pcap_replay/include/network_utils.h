#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H

#include <string>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>

/**
 * 网络工具类
 */
class NetworkUtils {
public:
    /**
     * 创建TCP套接字
     * @return 套接字文件描述符，失败返回-1
     */
    static int createTCPSocket();
    
    /**
     * 创建UDP套接字
     * @return 套接字文件描述符，失败返回-1
     */
    static int createUDPSocket();
    
    /**
     * 创建原始套接字
     * @return 套接字文件描述符，失败返回-1
     */
    static int createRawSocket();
    
    /**
     * 绑定套接字到指定地址和端口
     * @param sockfd 套接字文件描述符
     * @param ip IP地址字符串
     * @param port 端口号
     * @return 是否成功
     */
    static bool bindSocket(int sockfd, const std::string& ip, uint16_t port);
    
    /**
     * 连接到指定地址和端口
     * @param sockfd 套接字文件描述符
     * @param ip IP地址字符串
     * @param port 端口号
     * @return 是否成功
     */
    static bool connectSocket(int sockfd, const std::string& ip, uint16_t port);
    
    /**
     * 监听套接字
     * @param sockfd 套接字文件描述符
     * @param backlog 监听队列长度
     * @return 是否成功
     */
    static bool listenSocket(int sockfd, int backlog = 5);
    
    /**
     * 接受连接
     * @param sockfd 监听套接字文件描述符
     * @param client_ip 输出客户端IP
     * @param client_port 输出客户端端口
     * @return 新的连接套接字文件描述符，失败返回-1
     */
    static int acceptConnection(int sockfd, std::string& client_ip, uint16_t& client_port);
    
    /**
     * 发送数据
     * @param sockfd 套接字文件描述符
     * @param data 要发送的数据
     * @return 实际发送的字节数，失败返回-1
     */
    static ssize_t sendData(int sockfd, const std::vector<uint8_t>& data);
    
    /**
     * 接收数据
     * @param sockfd 套接字文件描述符
     * @param buffer 接收缓冲区
     * @param max_size 最大接收字节数
     * @return 实际接收的字节数，失败返回-1
     */
    static ssize_t receiveData(int sockfd, std::vector<uint8_t>& buffer, size_t max_size);
    
    /**
     * 关闭套接字
     * @param sockfd 套接字文件描述符
     */
    static void closeSocket(int sockfd);
    
    /**
     * 设置套接字为非阻塞模式
     * @param sockfd 套接字文件描述符
     * @return 是否成功
     */
    static bool setNonBlocking(int sockfd);
    
    /**
     * 设置套接字重用地址选项
     * @param sockfd 套接字文件描述符
     * @return 是否成功
     */
    static bool setReuseAddr(int sockfd);
    
    /**
     * 获取本机IP地址
     * @return IP地址字符串
     */
    static std::string getLocalIP();
    
    /**
     * 检查IP地址格式是否有效
     * @param ip IP地址字符串
     * @return 是否有效
     */
    static bool isValidIP(const std::string& ip);
};

#endif // NETWORK_UTILS_H