/*
 * (C) 2007-2015 No_company Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * thread.cpp -- 
 *
 * Version: 1.0  2015年11月02日 16时13分46秒
 *
 * Authors:
 *     Reazon (changgongxiaorong), cgxryy@gmail.com
 *
 */
#include <iostream>
#include <functional>

#include <unistd.h>
#include "thread.h"

enum conn_states;
class Item;
class Conn;

//兼容c++11的thread接口

//为了兼容libev的回调函数接口
static void conn_readhandler(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    Conn* read_conn = (Conn*)(watcher->data);
    if (watcher->fd != read_conn->sfd) {
        delete read_conn;
        return;
    }
    read_conn->drive_machine();
}

static void workthread_thread_libev_process(struct ev_loop* loop, struct ev_io *watcher, int revents) {
    WorkThread* this_thread = (WorkThread*)(watcher->data);
    this_thread->thread_libev_process();
}

/*
 * 工作线程相关
 */
void WorkThread::setup_thread() {
    base_loop = ev_loop_new(EVBACKEND_EPOLL);
    this->read_watcher.data = (void*)this;
    ev_io_init(&(this->read_watcher), workthread_thread_libev_process, fds[0], EV_READ);//监视fds[0]  notify_send_fd
    ev_io_start(base_loop, &read_watcher);
}

void WorkThread::work_thread(struct ev_loop* loop) {
    ev_run(loop);
}

void WorkThread::work_libev() {
    std::thread t(&WorkThread::work_thread, this, base_loop);
    t.detach();
}

void WorkThread::thread_libev_process() {
    char buf[1];
    Item item;//tags 涉及是否允许默认构造函数，或者使用conn_queue中存储指针
    if (read(fds[0], buf, 1) != 1)
        std::cerr << "Can't read from libev pipe" << std::endl;

    if (buf[0] == 'c') {
        item_queue.wait_and_pop(item);
        Conn* conn = new Conn(item.base_loop, item.sfd, item.state, item.flag, READBUF_LEN);
    }
}
