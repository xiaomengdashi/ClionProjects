#pragma once

#include "arpa/inet.h"

int initSocket();

int initSocketAddress(struct sockaddr_in *addr, unsigned port, const char* ip);

int setListenSocket(int lfd, unsigned port);

int acceptSocket(int lfd, struct sockaddr* addr);

int connectToSocket(int fd, unsigned port, const char* ip);