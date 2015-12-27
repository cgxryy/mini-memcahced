/*
 * (C) 2007-2015 No_company Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * conn.h -- 
 *
 * Version: 1.0  2015年12月04日 21时23分45秒
 *
 * Authors:
 *     Reazon (changgongxiaorong), cgxryy@gmail.com
 *
 */
#ifndef MEM_CONN_H_
#define MEM_CONN_H_

#include <ev++.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <vector>
#include <regex>
#include <new>    //为了缓冲区扩展

#include "slab.h"

#define READBUF_LEN 2048
#define WRITEBUF_LEN 2048

#define NREAD_ADD       1
#define NREAD_SET       2
#define NREAD_REPLACE   3
#define NREAD_APPEND    4
#define NREAD_PREPEND   5
#define NREAD_CAS       6

#define WRITE_DATA_ALL  1
#define WRITE_DATA_PART 2
#define WRITE_ERROR     3

class SlabItem;
class Slab;

enum conn_states {
    conn_listening,
//  conn_new_cmd,
    conn_parse_cmd,
    conn_waiting,
    conn_read,
    conn_write,
    conn_nread,
    conn_closed,
//  conn_nwrite
//  ...
};

//队列中的元素，为了工作线程产生Conn而不是在监听线程产生
class Item {
public:
    struct ev_loop* base_loop;
    int sfd;
    enum conn_states state;
    int flag;
    
    Item(struct ev_loop* loop, int socketfd, enum conn_states state_init, int event_flag);
    Item () = default;
    Item& operator= (Item& temp);
    ~Item() = default;
};

class Conn{
public:    
    static void dispatch_conn_new(int sfd, enum conn_states new_state, int event_flag);
    
    int             sfd;
    struct ev_loop* base_loop;
    conn_states     state;
    int             flag;
    bool            noreply;
    bool            flag_stop;

    char*       rbuf;
    char*       rbuf_end;
    char*       rbuf_now;
    int         rbuf_len;
    int         rbuf_rlen;
    
    char*       rnbuf;
    char*       rnbuf_end;
    char*       rnbuf_now;
    int         rnbuf_len;
    int         rnbuf_rlen;
    
    SlabItem*   item;
    short       nread_cmd;

    char*       wbuf;
    char*       wbuf_end;
    char*       wbuf_now;
    int         wbuf_len;
    int         wbuf_wlen;

    ev_io write_watcher;
    ev_io read_watcher;

    Conn() = delete;
    Conn(struct ev_loop* loop, int socketfd, enum conn_states state_init, int event_flag, int read_len);

    ~Conn()
    {
        delete[] rbuf;
        delete[] wbuf;
    }

    void conn_state_set(enum conn_states new_state);
    void out_string(const char* w_str);
    
    void drive_machine();
    int  try_read_command();
    void process_command();//返回值为value的长度
    int  try_read_tcp();
    void read_value_stored();
    int  write_activate_read();
    void prepare_write_buf();
    void command_one_para(std::vector<std::string>& tokens);
    void command_two_para(std::vector<std::string>& tokens);
    void command_three_para(std::vector<std::string>& tokens);
    void command_five_para(std::vector<std::string>& tokens);
    void conn_close(); //析构函数取代
    void conn_shake(char* end_cmd, int value_len);
};

extern Slab slablist;

#endif
