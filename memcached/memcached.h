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

class WorkThread;
/*
class WorkThread;
class HashTable;
class Slab;
class LRU_list;
struct Stats;
*/

//全局变量
//extern settings mem_setting;
//extern time_t process_started;

struct settings mem_setting;
Slab* slab = new Slab(&mem_setting);
std::vector<WorkThread*> workthreads;
int last_threadid = -1;
 
//Stats stats;

//append
HashTable hashtable;
LRU_list lru_list;
time_t process_started = 0;
volatile unsigned int current_time() {
    time_t now;
    time(&now);
    return (unsigned int)now;
}
std::mutex cache_lock;
unsigned int hashitems;

#endif
