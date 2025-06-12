#ifndef __NETWORK_INTERFACE__
#define __NETWORK_INTERFACE__

#include "websocket_handler.h"
#include <openssl/ssl.h>
#include <openssl/err.h>

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
public:
	void run();
	static Network_Interface *get_share_network_interface();
private:
	int epollfd_;
	int listenfd_;
	WEB_SOCKET_HANDLER_MAP websocket_handler_map_;
	static Network_Interface *m_network_interface;
	SSL_CTX* ctx;
};

#define NETWORK_INTERFACE Network_Interface::get_share_network_interface()

#endif
