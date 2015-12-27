/*
 * (C) 2007-2015 No_company Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * socket.h -- 
 *
 * Version: 1.0  2015年12月01日 16时07分04秒
 *
 * Authors:
 *     Reazon (changgongxiaorong), cgxryy@gmail.com
 *
 *
 */

#ifndef MEM_SOCKET_H_
#define MEM_SOCKET_H_

#include "settings.h"

class Socket {
public:
    static  int  set_reuse_linger_keepalive(int socketfd, int* flag);
    static  bool set_nonblock(int* socketfd);
    
    Socket(int net_or_local, struct settings* setting) {
        if (net_or_local) {
            socket_tcp(setting);
        }
        else {
            socket_unix(setting);
        }
    }
    
    Socket() {
        struct settings setting;
        setting.port = 11211;
        socket_tcp(&setting);
    }

    int c_socket() { return sfd; }

private:
    int sfd;
    char* unix_file;

    void socket_unix(struct settings* setting);
    void socket_tcp(struct settings* setting);
};

#endif
