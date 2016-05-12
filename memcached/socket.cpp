/*
 * (C) 2007-2015 No_company Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * socket.cpp -- 
 *
 * Version: 1.0  2015年12月01日 16时13分01秒
 *
 * Authors:
 *     Reazon (changgongxiaorong), cgxryy@gmail.com
 *
 */
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/tcp.h>

#include "socket.h"
//#include "thread.h"

class Conn;

void Socket::socket_tcp(struct settings* mem_setting) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    addr.sin_family = PF_INET;
    addr.sin_port = htons(mem_setting->port);
    addr.sin_addr.s_addr = htons(INADDR_ANY);

    int error, flag = 1;

    if ((this->sfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        std::cerr << "tcp socket error";
        exit(1);
    }

    //设置非阻塞
    if (!Socket::set_nonblock(&(this->sfd)))
        exit(1);
/*  
    if (Socket::set_reuse_linger_keepalive(this->sfd, &flag) < 0)
        exit(1);
*/
    error = setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, (void*)&flag, sizeof(flag));
    if (error != 0)
        std::cerr << "set socket TCP_NODELAY error";

    if (bind(sfd, (struct sockaddr*)(&addr), sizeof(struct sockaddr)) == -1) {
        if (errno == EACCES)
            std::cerr << "EACCES ";

        if (errno == EADDRINUSE)
            std::cerr << "EADDRINUSE ";

        if (errno == EBADF)
            std::cerr << "EBADF ";

        if (errno == EINVAL)
            std::cerr << "EINVAL ";

        if (errno == ENOTSOCK)
            std::cerr << "ENOTSOCK ";

        std::cerr << "bind error";
        close(sfd);
        exit(1);
    }

    if (listen(sfd, mem_setting->backlog) == -1) {
        std::cerr << "listen error";
        close(sfd);
        exit(1);
    }
}

void Socket::socket_unix(struct settings* mem_setting) {
    int flag = 1;
    struct sockaddr_un uaddr;
    memset(&uaddr, 0, sizeof(uaddr));

    unlink(mem_setting->unix_filename);
    
    uaddr.sun_family = AF_UNIX;
    strncpy(uaddr.sun_path, mem_setting->unix_filename, strlen(mem_setting->unix_filename));

    if ((sfd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
        std::cerr << "unix socket error";
        exit(1);
    }

    if (Socket::set_reuse_linger_keepalive(this->sfd, &flag) < 0)
        exit(1);
        
    if (bind(sfd, (struct sockaddr*)&uaddr, sizeof(uaddr)) < 0) {
        std::cerr << "bind unix socket error";
        close(sfd);
    }

    if (listen(sfd, mem_setting->backlog) < 0) {
        std::cerr << "listen unix socket error";
        close(sfd);
    }
}

bool Socket::set_nonblock(int* socketfd) {
    int flag;
    
    if ((flag = fcntl(*socketfd, F_GETFL, 0)) < 0 || fcntl(*socketfd, F_SETFL, flag | O_NONBLOCK) < 0) {
        std::cerr << "set O_NONBLOCK errro";
        return false;
    }
    return true;
}

int Socket::set_reuse_linger_keepalive(int socketfd, int* flag) {
    int error;

    error = setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, (void*)flag, sizeof(flag));
    if (error != 0) {
        std::cerr << "set socket SO_REUSEADDR error";
        return -1;
    }
      
    error = setsockopt(socketfd, SOL_SOCKET, SO_KEEPALIVE, (void*)flag, sizeof(flag));
    if (error != 0) {
        std::cerr << "set socket SO_KEEPALIVE error";
        return -2;
    }
      
    error = setsockopt(socketfd, SOL_SOCKET, SO_LINGER, (void*)flag, sizeof(flag));
    if (error != 0) {
        std::cerr << "set socket SO_LINGER error"; 
        return -3;
    }


}
