#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <memory>
#include "debug_log.h"
#include "network_interface.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

// 假设 passwd 是私钥密码，这里简单定义，实际使用时可按需修改
const char* passwd = NULL; 
// 假设 ca_cert、server_cert 和 key 分别是 CA 证书、服务端证书和私钥文件路径，实际使用时可按需修改
const char* ca_cert = "ca-cert.pem"; 
const char* server_cert = "server-cert.pem"; 
const char* key = "key.pem"; 

Network_Interface *Network_Interface::m_network_interface = NULL;

Network_Interface::Network_Interface():
        epollfd_(0),
        listenfd_(0),
        websocket_handler_map_(),
        ctx(nullptr)
{
    // 初始化
    SSLeay_add_ssl_algorithms();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    ctx = create_context();
    configure_context(ctx);
    if(0 != init())
        exit(1);
}

Network_Interface::~Network_Interface()
{
    if (ctx) {
        SSL_CTX_free(ctx);
    }
}

SSL_CTX* Network_Interface::create_context()
{
    SSL_CTX *ctx;
    // 我们使用SSL V3,V2
    ctx = SSL_CTX_new(SSLv23_method());
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    return ctx;
}

// 自定义删除器，用于智能指针管理 EVP_PKEY
struct EVPPKeyDeleter {
    void operator()(EVP_PKEY* pkey) {
        if (pkey) {
            EVP_PKEY_free(pkey);
        }
    }
};

// 错误处理函数
void handle_openssl_error(const std::string& message) {
    ERR_print_errors_fp(stderr);
    fprintf(stderr, "%s\n", message.c_str());
    exit(EXIT_FAILURE);
}

void Network_Interface::configure_context(SSL_CTX *ctx)
{
    SSL_CTX_set_ecdh_auto(ctx, 1);

    // 要求校验对方证书
    SSL_CTX_set_verify(ctx, SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);

    // 加载CA的证书
    if (SSL_CTX_load_verify_locations(ctx, ca_cert, NULL) != 1) {
        handle_openssl_error("Failed to load CA certificate");
    }

    // 加载自己的证书链
    if (SSL_CTX_use_certificate_chain_file(ctx, server_cert) <= 0) {
        handle_openssl_error("Failed to load server certificate chain");
    }

    // 打开私钥文件
    FILE *key_file = fopen(key, "r");
    if (!key_file) {
        perror("Failed to open private key file");
        exit(EXIT_FAILURE);
    }

    // 使用智能指针管理 EVP_PKEY
    std::unique_ptr<EVP_PKEY, EVPPKeyDeleter> pkey(PEM_read_PrivateKey(key_file, NULL, NULL, (void*)passwd));
    fclose(key_file);
    if (!pkey) {
        handle_openssl_error("Failed to read private key");
    }

    // 验证私钥算法是否为 RSA
    if (EVP_PKEY_id(pkey.get()) != EVP_PKEY_RSA) {
        fprintf(stderr, "Private key is not an RSA key\n");
        exit(EXIT_FAILURE);
    }

    // 将私钥设置到 SSL 上下文中
    if (SSL_CTX_use_PrivateKey(ctx, pkey.get()) <= 0) {
        handle_openssl_error("Failed to set private key");
    }

    // 判定私钥是否正确  
    if (!SSL_CTX_check_private_key(ctx)) {
        fprintf(stderr, "Private key does not match the public certificate\n");
        exit(EXIT_FAILURE);
    }

    // 设置验证深度
    SSL_CTX_set_verify_depth(ctx, 1);
}

int Network_Interface::init()
{
    listenfd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd_ == -1){
        DEBUG_LOG("创建套接字失败!");
        return -1;
    }
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(sockaddr_in));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);
    if(-1 == bind(listenfd_, (struct sockaddr *)(&server_addr), sizeof(server_addr))){
        DEBUG_LOG("绑定套接字失败!");
        return -1;
    }
    if(-1 == listen(listenfd_, 5)){
        DEBUG_LOG("监听失败!");
        return -1;
    }
    epollfd_ = epoll_create(MAXEVENTSSIZE);

    ctl_event(listenfd_, true);	
    DEBUG_LOG("服务器启动成功!");
    return 0;
}

int Network_Interface::epoll_loop(){
    // 定义客户端地址结构体，用于存储客户端的地址信息
    struct sockaddr_in client_addr;
    // 客户端地址结构体的长度
    socklen_t clilen;
    // 存储 epoll_wait 返回的就绪事件数量
    int nfds = 0;
    // 存储文件描述符
    int fd = 0;
    // 存储读取到的数据长度
    int bufflen = 0;
    // 定义 epoll_event 数组，用于存储就绪的事件
    struct epoll_event events[MAXEVENTSSIZE];
    // 进入无限循环，持续监听事件
    while(true){
        // 调用 epoll_wait 等待事件发生，TIMEWAIT 为超时时间
        // epollfd_ 是 epoll 实例的文件描述符
        // events 数组用于存储就绪的事件
        // MAXEVENTSSIZE 是 events 数组的最大长度
        // 返回就绪事件的数量
        nfds = epoll_wait(epollfd_, events, MAXEVENTSSIZE, TIMEWAIT);
        // 遍历所有就绪的事件
        for(int i = 0; i < nfds; i++){
            // 如果就绪事件的文件描述符是监听套接字
            if(events[i].data.fd == listenfd_){
                // 接受新的客户端连接，返回新的客户端套接字描述符
                // client_addr 存储客户端地址信息
                // clilen 存储客户端地址结构体的长度
                fd = accept(listenfd_, (struct sockaddr *)&client_addr, &clilen);
                // 调用 ctl_event 函数将新的客户端套接字添加到 epoll 实例中进行监听
                ctl_event(fd, true);
            }
            // 如果就绪事件是读事件
            else if(events[i].events & EPOLLIN){
                // 获取发生读事件的文件描述符
                if((fd = events[i].data.fd) < 0)
                    // 如果文件描述符无效，跳过本次循环
                    continue;
                // 从 websocket_handler_map_ 中获取对应的 Websocket_Handler 对象
                Websocket_Handler *handler = websocket_handler_map_[fd];
                if(handler == NULL)
                    // 如果没有对应的处理对象，跳过本次循环
                    continue;
                // 从客户端套接字读取数据到 handler 的缓冲区中
                // BUFFLEN 是缓冲区的最大长度
                // 返回实际读取到的数据长度
                if((bufflen = read(fd, handler->getbuff(), BUFFLEN)) <= 0){
                    // 如果读取到的数据长度小于等于 0，说明客户端关闭连接或发生错误
                    // 调用 ctl_event 函数将该客户端套接字从 epoll 实例中移除
                    ctl_event(fd, false);
                }
                else{
                    // 如果读取到有效数据，调用 handler 的 process 方法处理数据
                    handler->process();
                }
            }
        }
    }
    // 理论上不会执行到这里，返回 0 表示正常结束
    return 0;
}

int Network_Interface::set_noblock(int fd){
	int flags;
    if ((flags = fcntl(fd, F_GETFL, 0)) == -1)
        flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

Network_Interface *Network_Interface::get_share_network_interface(){
	if(m_network_interface == NULL)
		m_network_interface = new Network_Interface();
	return m_network_interface;
}

void Network_Interface::ctl_event(int fd, bool flag)
{
    // 定义一个 epoll_event 结构体变量，用于存储事件信息
    struct epoll_event ev;
    // 设置事件类型为 EPOLLIN（可读事件）和 EPOLLET（边缘触发模式）
    ev.events = EPOLLIN | EPOLLET;
    // 将传入的文件描述符 fd 赋值给 ev.data.fd，方便后续识别事件对应的文件描述符
    ev.data.fd = fd;

    // 根据 flag 的值判断是添加事件还是删除事件
    if(flag){
        // 当 flag 为 true 时，将事件添加到 epoll 实例中
        // epoll_ctl 函数用于控制 epoll 实例，EPOLL_CTL_ADD 表示添加事件
        // epollfd_ 是 epoll 实例的文件描述符，fd 是要添加事件的文件描述符，&ev 是事件信息
        if(-1 == epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &ev)){
            // 若添加失败，输出调试日志
            DEBUG_LOG("添加事件失败!");
        }
        // 如果当前文件描述符不是监听套接字
        if(fd != listenfd_){
            // 为该文件描述符创建一个新的 Websocket_Handler 对象，并传入 SSL 上下文 ctx
            // 同时将该对象存入 websocket_handler_map_ 映射中，键为文件描述符 fd
            websocket_handler_map_[fd] = new Websocket_Handler(fd, ctx);
        }
    }else{
        // 当 flag 为 false 时，从 epoll 实例中删除事件
        // EPOLL_CTL_DEL 表示删除事件
        if(-1 == epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, &ev)){
            // 若删除失败，输出调试日志
            DEBUG_LOG("删除事件失败!");
        }
        // 检查 websocket_handler_map_ 映射中是否存在该文件描述符对应的 Websocket_Handler 对象
        if(websocket_handler_map_.find(fd) != websocket_handler_map_.end()){
            // 若存在，释放该对象的内存
            delete websocket_handler_map_[fd];
            // 从映射中移除该键值对
            websocket_handler_map_.erase(fd);
        }
    }
}

void Network_Interface::run(){
	epoll_loop();
}
