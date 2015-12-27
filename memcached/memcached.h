/*
 * (C) 2007-2015 No_company Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * memcached.h -- 
 *
 * Version: 1.0  2015年11月02日 15时55分50秒
 *
 * Authors:
 *     Reazon (changgongxiaorong), cgxryy@gmail.com
 *
 */
#ifndef MEM_MEMCACHED_H_
#define MEM_MEMCACHED_H_

#include <unistd.h>
#include <ev++.h>
#include "thread.h"
#include "socket.h"

//全局变量
Slab slablist;
std::vector<WorkThread*> workthreads;
int last_threadid = -1;
struct settings setting;

#endif
