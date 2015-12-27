/*
 * (C) 2007-2015 No_company Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * conn.cpp -- 
 *
 * Version: 1.0  2015年12月04日 21时23分54秒
 *
 * Authors:
 *     Reazon (changgongxiaorong), cgxryy@gmail.com
 *
 */
#include "conn.h"
#include "thread.h"

#include <cstdlib>
//class WorkThread;

/*
 *  线程队列元素相关
 */
Item::Item(struct ev_loop* loop, int socketfd, enum conn_states state_init, int event_flag) : 
    base_loop(loop), 
    sfd(socketfd), 
    state(state_init), 
    flag(event_flag)
{}

Item& Item::operator= (Item& temp) {
    sfd = temp.sfd;
    state = temp.state;
    flag = temp.flag;
    base_loop = temp.base_loop;
    return temp;
}


/*
 *  连接相关
 */
static void conn_handler(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    Conn* conn = (Conn*)(watcher->data);
    if (watcher->fd != conn->sfd) {
        delete conn;
        return;
    }
    conn->drive_machine();
}


void Conn::conn_state_set(enum conn_states new_state) {
    state = new_state;
}

Conn::Conn(struct ev_loop* loop, int socketfd, enum conn_states state_init, int event_flag, int read_len) : 
        sfd(socketfd),
        base_loop(loop), 
        state(state_init), 
        flag(event_flag), 
        rbuf_len(read_len) {

    wbuf_len = WRITEBUF_LEN;
    
    rbuf = new char[rbuf_len];
    rbuf_now = rbuf;
    rbuf_end = rbuf + rbuf_len;
    rbuf_rlen = 0;

    rnbuf_rlen = 0;
    rnbuf_len = 0;

    wbuf = new char[wbuf_len];
    wbuf_now = wbuf;
    wbuf_end = wbuf + wbuf_len;
    wbuf_wlen = 0;
    flag_stop = false;

    //套接字已经在Socket构造函数中设置为非阻塞
    read_watcher.data = (void*)this;
    write_watcher.data = (void*)this;
    ev_io_init(&(this->read_watcher), conn_handler, sfd, EV_READ);
    ev_io_init(&(this->write_watcher), conn_handler, sfd, EV_WRITE);
    ev_io_start(loop, &read_watcher);
}

void Conn::out_string(const char* w_str) {

    if (noreply) {
        this->noreply = false;
        conn_state_set(conn_read);
    }

    int len = strlen(w_str);
    if (len + 2 >= wbuf_len) {
        w_str = "SERVER_ERROR output line too long";
        len = strlen(w_str);
    }
    
    memcpy(wbuf, w_str, len);
    memcpy(wbuf + len, "\r\n", 2);
    wbuf_len = len + 2;
    wbuf_now = wbuf;

    conn_state_set(conn_write);
}


//请求处理状态机
void Conn::drive_machine(){
    socklen_t addrlen;
    struct sockaddr_storage addr;
    int accept_sfd;
    int fd_flags = 1;
    int res, ret;

    while(!flag_stop) {
        switch(state) {
            case conn_listening:                
                addrlen = sizeof(addr); 

                if ((accept_sfd = accept(this->sfd, (struct sockaddr*)&addr, &addrlen)) == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        flag_stop = true;
                    }
                    else if (errno == EMFILE) {
                        //省略accept_new_conn
                        flag_stop = true;
                    }
                    else {
                        flag_stop = true;
                    }
                    break;
                }
                
                if (!Socket::set_nonblock(&accept_sfd)) {
                    close(sfd);
                    break;
                }

                //删除了对连接的范围控制
                dispatch_conn_new(accept_sfd, conn_read, EV_READ);
                
                flag_stop = true;
                break;
           
            case conn_parse_cmd:
                //尝试读命令
                //这里有一个防止恶意get的方法
                if (try_read_command() == 0) {
                    conn_state_set(conn_waiting);
                }
                flag_stop = true;    

                break;
            
            case conn_read:
                //这个阶段是读取命令，不是读取值，所以，读事件不需要注销
                res = try_read_tcp();

                switch (res) {
                    case READ_NO_DATA_RECEIVED: 
                        conn_state_set(conn_waiting);
                        break;
                    case READ_DATA_RECEIVED:
                        conn_state_set(conn_parse_cmd);
                        break;
                    case READ_ERROR:
                        conn_state_set(conn_closed);
                        break;
                    case READ_MEMORY_ERROR:
                        break;
                }

                break;
            //conn_write写入缓冲区的内容
            case conn_write:
                ret = write_activate_read();
                //写入完成后，注销写事件，激活读事件
                switch (ret) {
                    case WRITE_DATA_PART:
                        break;
                    case WRITE_DATA_ALL:
                        conn_state_set(conn_waiting);
                        break;
                    case WRITE_ERROR:
                        conn_state_set(conn_closed);
                        break;
                }
                    
                break;

            case conn_nread:
                //读取完成，注销读事件，激活写事件，进入write
                read_value_stored();
                break;

            case conn_waiting:
                flag_stop = true;
                break;

            case conn_closed:
                conn_close();
                //调用析构函数
                break;
/*
 *          case conn_nwrite:

                break;
*/
        }
    }
}

void Conn::conn_close() {
    ev_io_stop(base_loop, &read_watcher);
    ev_io_stop(base_loop, &write_watcher);
    close(sfd);
}


void Conn::dispatch_conn_new(int sfd, enum conn_states new_state, int event_flag) {
    char buf[1];

    int threadid = (last_threadid + 1) % setting.num_threads;

    WorkThread* thread_chosen = workthreads[threadid];

    //每次申请新空间，比原始做法增加了消耗
    //原做法是构造item，在回调函数再构造conn
    Item item(thread_chosen->base_loop, sfd, new_state, event_flag);
    thread_chosen->item_queue.push(item);

    buf[0] = 'c';
    if (write(thread_chosen->fds[1], buf, 1) != 1) {
        std::cerr << "Writting to thread botify pipe";
    }
}

int Conn::try_read_command() {
    char *endflag;
    char* endp;
    if (rbuf_rlen == 0)
        return 0;
    
    endflag = (char*)memchr((void*)rbuf, '\n', rbuf_rlen);
    if (!endflag) {
        if (rbuf_rlen > 1024) {
            char* ptr = rbuf_now;
            while (*ptr == ' ') {
                ++ptr;
            }

            if (ptr - rbuf_now > 100 || ((strncmp(ptr, "get ", 4) && strncmp(ptr, "gets ", 5)))) {
                conn_state_set(conn_closed);
                return -1;
            }
        }
        return 0;
    }
    
    endp = endflag + 1;
    if (endflag - rbuf_now > 1 && *(endflag - 1) == '\r') {
        endflag--;
    }
    *endflag = '\0';
    
    //检查是否多read了value的值，以及是否读完
    process_command();
    
   // conn_shake(endflag, rbuf_rlen - (endflag - rbuf));
    rbuf_rlen -= (endp - wbuf_now);
    rbuf_now = endp;

    return 1;
}

void Conn::conn_shake(char* end_cmd, int value_len) {
 
    char* end_value = (char*)memchr((void*)end_cmd, value_len);
    if (value_len == )

}

void Conn::process_command() {
    //利用编译原理解析命令
    //也可以直接正则，原理一样，这里使用正则
    std::string s(rbuf);
    std::string pattern("[^\\s+]+");
    std::regex r(pattern);
    std::sregex_iterator it(s.begin(), s.end(), r);
    std::sregex_iterator end;
    
    std::vector<std::string> tokens;
    while (it != end) {
        tokens.push_back(it->str());
        it++;
    }

    switch (tokens.size()) {
        case 1:
            /*
             * 1.quit
             * 2.version
             * 3.stats
             */
            command_one_para(tokens);
            break;
        case 2:
            /*
             * 1.get key
             * 2.flush_all number
             */
            command_two_para(tokens);
            break;
        case 3:
            /*
             * 1.delete key time
             * 2.incr/decr key value
             */
            command_three_para(tokens);
            break;
        case 5:
            /*
             * 1.set/add/replace/append/prepend/cas
             */
            command_five_para(tokens);
            break;
    }
    //暂时不管具体
    //使用模拟返回

}

void Conn::command_one_para(std::vector<std::string>& tokens) {
}
void Conn::command_two_para(std::vector<std::string>& tokens) {
}
void Conn::command_three_para(std::vector<std::string>& tokens) {
}
void Conn::command_five_para(std::vector<std::string>& tokens) {
    //这里代码是暂时的，先用set试试
    if (tokens[0] == std::string("set")) {
        SlabItem* temp = new SlabItem();
        temp->key = tokens[1];
        temp->flag = tokens[2];
        temp->exptime = tokens[3];
        temp->bytes = atoi(tokens[4].c_str());
        slablist.slab_list.push_back(temp);
        conn_state_set(conn_nread);
        nread_cmd = NREAD_SET;
        item = temp;

        rnbuf_len = temp->bytes;
        rnbuf = new (std::nothrow) char[rnbuf_len+3];//value + "\r\n" + \0
        if (rnbuf == 0) {
            std::cerr << "rnbuf malloc error";
        }
        rnbuf_end = rnbuf + rnbuf_len;
        rnbuf_now = rnbuf;
    }
}

int Conn::try_read_tcp() {
    int read_state = READ_NO_DATA_RECEIVED;
    int num_alloc = 0;//记录重新申请的次数
    int ret;

    //检查缓冲区位置是否正常
    if (rbuf_now != rbuf) {
        if (rbuf_rlen != 0) {
            memmove(rbuf, rbuf_now, rbuf_rlen);
        }
        rbuf_now = rbuf;
    }

    //循环读入，直到被IO复用切换走，或者读到尾，或者内存爆掉
    while (1) {
        //检测缓冲区是否够用，不够的话，扩展，malloc和new的取舍问题
        if (rbuf_rlen >= rbuf_len) {
            if (num_alloc == 4) {
                return read_state;
            }
            ++num_alloc;
            char* new_buf = new (std::nothrow) char[rbuf_len*2];
            if (new_buf == 0) {
                rbuf_rlen = 0;
                out_string("SERVER_ERROR out of memory reading request");
                return READ_MEMORY_ERROR;
            }
            memcpy(new_buf, rbuf, rbuf_len);
            delete[] rbuf;
            rbuf_len = rbuf_len*2;
            rbuf = new_buf;
            rbuf_now = rbuf;
        }

        int avail = rbuf_len - rbuf_rlen;
        ret = read(sfd, rbuf + rbuf_rlen, avail);
        if (ret > 0) {
            //stat状态变迁
            read_state = READ_DATA_RECEIVED;
            rbuf_now += ret;
            rbuf_rlen += ret;
            if (ret == avail) {
                continue;
            }
            else {
                break;
            }
        }
        if (ret == 0) {
            return READ_ERROR;
        }
        if (ret == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;//第一次从这里break是READ_NO_DATA_RECEIVED状态
            }
            return READ_ERROR;
        }
    }
    return read_state;
}

//协议解析的另一部分，以及和存储相关部分
void Conn::read_value_stored() {
    int ret;
    
    //检查缓冲区起始位置
    if (rnbuf_len - rnbuf_rlen > 0) {
        
    }

    ret = read(sfd, rnbuf, rnbuf_len- rnbuf_rlen);
    if (ret == 0) {
        if (rnbuf_rlen != rnbuf_len) {
            conn_state_set(conn_closed);
            return;
        }
    }
    else if (ret > 0) {
        rnbuf_rlen += ret;
    }
    else if (ret < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        }
    }

    if (rnbuf_len != rnbuf_rlen)
        return;

    //不同命令的分支实现
    switch (nread_cmd) {
        case NREAD_ADD:
            break;
        case NREAD_SET:
            memcpy(rnbuf + rnbuf_len, "\r\n", 3);
            item->value = rnbuf;
            memset(rnbuf, 0, rnbuf_len+3);
            memset(rbuf, 0, rbuf_len);
            rnbuf_len = rnbuf_rlen = 0;
            rbuf_len = rbuf_rlen = 0;
            if (rnbuf != 0)
                delete rnbuf;
            out_string("STORED\r\n");

            break;
        case NREAD_REPLACE:
            break;
        case NREAD_APPEND:
            break;
        case NREAD_PREPEND:
            break;
        case NREAD_CAS:
            break;
    }

    //根据结果把返回值定义好，并把读事件变为写事件
    ev_io_stop(base_loop, &(read_watcher));
    ev_io_set(&(write_watcher), sfd, EV_WRITE);
    ev_io_start(base_loop, &(write_watcher));
}

int Conn::write_activate_read() {
    int ret;
    int avail;
    int ret_state = WRITE_DATA_PART;

    while (1) {
    
        avail = wbuf_len - wbuf_wlen;
        //是否全部写入，若已全部写入则注销写事件
        if (avail == 0) {
            ev_io_stop(base_loop, &(write_watcher));
            ev_io_set(&(read_watcher), sfd, EV_READ);
            ev_io_start(base_loop, &(read_watcher));
            return WRITE_DATA_ALL;
        }
        ret = write(sfd, wbuf_now, avail);
        if (ret > 0) {
            wbuf_wlen += ret;
            wbuf_now += ret;
            ret_state = WRITE_DATA_PART;
            if (ret == avail) {
                ev_io_stop(base_loop, &(write_watcher));
                ev_io_set(&(read_watcher), sfd, EV_READ);
                ev_io_start(base_loop, &(read_watcher));
                return WRITE_DATA_ALL;
            }
        }
        if (ret == 0) {
            return WRITE_ERROR;
        }
        if (ret == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            return WRITE_ERROR;
        }
        //写入若成功，注销写事件，state回到conn_read
        //若不成功...
    }
    return ret_state;
}