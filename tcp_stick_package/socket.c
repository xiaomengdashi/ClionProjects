//
// Created by Kolane on 2025/1/19.
//

#include "socket.h"

int initSocket() {
    return 0;
}

int initSocketAddress(struct sockaddr_in *addr, unsigned int port, const char *ip) {
    return 0;
}

int setListenSocket(int lfd, unsigned int port) {
    return 0;
}

int acceptSocket(int lfd, struct sockaddr *addr) {
    return 0;
}

int connectToSocket(int fd, unsigned int port, const char *ip) {
    struct sockaddr_in addr;
    initSocketAddress(&addr, port, ip);



    return 0;
}
