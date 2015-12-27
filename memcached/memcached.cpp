/*
 * (C) 2007-2015 No_company Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * memcached.cpp -- 
 *
 * Version: 1.0  2015年11月02日 15时58分57秒
 *
 * Authors:
 *     Reazon (changgongxiaorong), cgxryy@gmail.com
 *
 */
#include "memcached.h"

void main_ev_init(WorkThread& main_thread);
void thread_init(int num_worker_thread, WorkThread& dispatch_thread);
void socket_init(WorkThread& main_thread, struct settings* setting);

int main(int argc, char** argv) {
    
    int result = 0;
 
    //全局主体数据或配置数据
    setting.num_threads = 1;
    setting.port = 11211;
    setting.net_or_local = MODEL_NET;
    setting.backlog = 1024;

    WorkThread dispatch_thread;         //分发任务并accept的主线程

    //丢弃root权限
    
    //守护进程
    
    //主loop初始化
    main_ev_init(dispatch_thread);

    //连接数组初始化

    //slab初始化

    //启动工作线程
    thread_init(setting.num_threads, dispatch_thread);
    
    //配置套接字
    //0.Unix本地套接字
    //1.Tcp套接字
    socket_init(dispatch_thread, &setting);
    
    //存储线程
    
    ev_run(dispatch_thread.base_loop);
    return result;
}

void main_ev_init(WorkThread& main_thread) {
    main_thread.base_loop = ev_loop_new(EVBACKEND_EPOLL);
}

void thread_init(int num_worker_thread, WorkThread& dispatch_thread) {
    for ( int i = 0; i < num_worker_thread; i++) {
        WorkThread *t = new WorkThread();
        workthreads.push_back(t);
    }

    if (pipe(dispatch_thread.fds)) {
        perror("Can't create notify pipe");
        exit(1);
    }

    std::vector<WorkThread*>::iterator iter = workthreads.begin();
    for( ; iter != workthreads.end(); iter++) {
        (*iter)->fds[0] = dispatch_thread.fds[0];
        (*iter)->fds[1] = dispatch_thread.fds[1];
        (*iter)->setup_thread();
    }

    iter = workthreads.begin();
    for( ; iter != workthreads.end(); iter++) {
        (*iter)->work_libev();
    }
}

void socket_init(WorkThread& main_thread, struct settings *setting) {
    
    if (setting->net_or_local) {
        Socket* tcp_socket = new Socket(MODEL_NET, setting);
        Conn* listen_conn = new Conn(main_thread.base_loop, tcp_socket->c_socket(), conn_listening, EV_READ, 1);
    }
    else {
        Socket* unix_socket = new Socket(MODEL_LOCAL, setting);
        Conn* listen_conn = new Conn(main_thread.base_loop, unix_socket->c_socket(), conn_listening, EV_READ, 1);
    }
}
