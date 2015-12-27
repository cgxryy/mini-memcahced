/*
 * (C) 2007-2015 No_company Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * thread.h -- 
 *
 * Version: 1.0  2015年11月04日 20时55分29秒
 *
 * Authors:
 *     Reazon (changgongxiaorong), cgxryy@gmail.com
 *
 */
#ifndef MEM_THREAD_H_
#define MEM_THREAD_H_

#include <thread>
#include <ev++.h>
#include <vector>
#include <iostream>
#include <unistd.h>

#include "threadsafe_queue.h"
#include "settings.h"
#include "socket.h"
#include "conn.h"

#define READ_DATA_RECEIVED      0
#define READ_NO_DATA_RECEIVED   1
#define READ_ERROR              2
#define READ_MEMORY_ERROR       3

enum conn_states;

class WorkThread {
public:
    WorkThread(){}
    ~WorkThread(){}
//    WorkThread(WorkThread const &) = delete;
//    WorkThread& operator=(WorkThread const &) = delete;

    void setup_thread(); //初始化线程数据
    void thread_libev_process();    //当线程队列中有可用任务的处理函数
    void work_libev();              //多线程执行实体
    void work_thread(struct ev_loop* loop);

    /* 
     * fds[0]   notify_send_fd
     * fds[1]   notify_receive_fd
     * */
    int fds[2];
    struct ev_loop *base_loop;
    threadsafe_queue<Item> item_queue;

private:
    ev_io read_watcher;
};

extern std::vector<WorkThread*> workthreads;
extern int last_threadid;
extern struct settings setting;

#endif
