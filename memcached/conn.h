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

#define KEY_MAX_LENGTH 250

//最大淘汰时间一个月
#define REALTIME_MAXDELTA 60*60*24*30

#define READBUF_LEN 2048
#define WRITEBUF_LEN 2048

#define NREAD_ADD       1
#define NREAD_SET       2
#define NREAD_REPLACE   3
#define NREAD_APPEND    4
#define NREAD_PREPEND   5
#define NREAD_CAS       6
#define NREAD_ERROR     7

#define WRITE_DATA_ALL  1
#define WRITE_DATA_PART 2
#define WRITE_ERROR     3


struct base_item;
class LRU_list;
class Slab;
struct slabclass;
class HashTable;
struct settings;

enum conn_states : unsigned int {
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
    int             flag;       //
    bool            noreply;
    bool            flag_stop;  //退出状态机drive_machine
    bool            flag_precmd;//为真时，上个命令是存储类命令
    bool            flag_shake; //当一次读入过多，导致把键值都读了，或者读了两次命令，这个用来区分两者

    //缓存区:相关第一行命令,关于key的读取
    char*       rbuf;       //缓存区地址
    char*       rbuf_end;   //空间尾部
    char*       rbuf_now;   //当前读入位置
    int         rbuf_len;   //总长度
    int         rbuf_rlen;  //已读长度
    
    //缓存区:存储时用到的第二行命令,关于value读取
    char*       rnbuf;
    char*       rnbuf_end;
    char*       rnbuf_now;
    int         rnbuf_len;
    int         rnbuf_rlen;
    

    base_item*   item;
    short       cmd;

    //CAS操作
    uint64_t cas;

    //缓存区:返回客户端的写入缓存区
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
    void nread_stat_set(std::string& command);

    void drive_machine();
    int  try_read_command();
    void process_command();//返回值为value的长度
    int  try_read_tcp();
    void read_value_stored();
    int  write_activate_read();
    void prepare_write_buf();

    void get_command(std::vector<std::string>& tokens);
    void delete_command(std::vector<std::string>& tokens);
    void command_one_para(std::vector<std::string>& tokens);
    void command_two_para(std::vector<std::string>& tokens);
    void command_three_para(std::vector<std::string>& tokens);
    void command_five_six_para(std::vector<std::string>& tokens, bool cas);
    void conn_close(); //析构函数取代
    void conn_shake(char* end_cmd, char* temp_end, int value_len);
};

//辅助工具函数
unsigned int realtime(const time_t exptime);

//用到的数据
extern Slab* slab;
extern LRU_list lru_list;

extern time_t process_started;
#endif
