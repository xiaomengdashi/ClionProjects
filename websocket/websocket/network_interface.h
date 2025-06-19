#ifndef __NETWORK_INTERFACE__
#define __NETWORK_INTERFACE__

#include "websocket_handler.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/inotify.h>  // 添加 inotify 头文件

#define PORT 9000
#define TIMEWAIT 100
#define BUFFLEN 2048
#define MAXEVENTSSIZE 20

typedef std::map<int, Websocket_Handler *> WEB_SOCKET_HANDLER_MAP;

class Network_Interface {
private:
	Network_Interface();
	~Network_Interface();
	int init();
	int epoll_loop();
	int set_noblock(int fd);
	void ctl_event(int fd, bool flag);
	SSL_CTX* create_context();
	void configure_context(SSL_CTX* ctx);
	void reload_certificates();  // 新增重新加载证书方法
	void init_inotify();  // 新增初始化 inotify 方法
	void handle_inotify_event();  // 新增处理 inotify 事件方法

public:
	void run();
	static Network_Interface *get_share_network_interface();
private:
	int epollfd_;
	int listenfd_;
	WEB_SOCKET_HANDLER_MAP websocket_handler_map_;
	static Network_Interface *m_network_interface;
	SSL_CTX* ctx;
	int inotify_fd_;  // 新增 inotify 文件描述符
	int inotify_wd_ca_;  // 新增 CA 证书监听描述符
	int inotify_wd_server_;  // 新增服务端证书监听描述符
	int inotify_wd_key_;  // 新增私钥文件监听描述符
};

#define NETWORK_INTERFACE Network_Interface::get_share_network_interface()

#endif
