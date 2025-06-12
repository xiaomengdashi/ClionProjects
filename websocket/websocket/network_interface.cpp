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
    ERR_load_BIO_strings();

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
	struct sockaddr_in client_addr;
	socklen_t clilen;
	int nfds = 0;
	int fd = 0;
	int bufflen = 0;
	struct epoll_event events[MAXEVENTSSIZE];
	while(true){
		nfds = epoll_wait(epollfd_, events, MAXEVENTSSIZE, TIMEWAIT);
		for(int i = 0; i < nfds; i++){
			if(events[i].data.fd == listenfd_){
				fd = accept(listenfd_, (struct sockaddr *)&client_addr, &clilen);
				ctl_event(fd, true);
			}
			else if(events[i].events & EPOLLIN){
				if((fd = events[i].data.fd) < 0)
					continue;
				Websocket_Handler *handler = websocket_handler_map_[fd];
				if(handler == NULL)
					continue;
				if((bufflen = read(fd, handler->getbuff(), BUFFLEN)) <= 0){
					ctl_event(fd, false);
				}
				else{
					handler->process();
				}
			}
		}
	}

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
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = fd;
    if(flag){
        if(-1 == epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &ev)){
            DEBUG_LOG("添加事件失败!");
        }
        if(fd != listenfd_){
            // 传入 SSL_CTX* ctx 参数
            websocket_handler_map_[fd] = new Websocket_Handler(fd, ctx);
        }
    }else{
        if(-1 == epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, &ev)){
            DEBUG_LOG("删除事件失败!");
        }
        if(websocket_handler_map_.find(fd) != websocket_handler_map_.end()){
            delete websocket_handler_map_[fd];
            websocket_handler_map_.erase(fd);
        }
    }
}

void Network_Interface::run(){
	epoll_loop();
}
