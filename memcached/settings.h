/*
 * (C) 2007-2015 No_company Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * settings.h -- 
 *
 * Version: 1.0  2015年11月22日 17时51分50秒
 *
 * Authors:
 *     Reazon (changgongxiaorong), cgxryy@gmail.com
 *
 */
#ifndef MEM_SETTINGS_H_
#define MEM_SETTINGS_H_

#define MODEL_NET 1
#define MODEL_LOCAL 0

struct settings {
    //基本参数
    int verbose;    //显示等级，越高，信息越详细

    //网络架构参数
    int     num_threads;
    int     port;
    char*   unix_filename;
    int     net_or_local;
    int     backlog;//listen backlog

    //slab相关参数
    unsigned int chunk_size;
    unsigned int item_size_max;
    unsigned int oldest_live;

    //LRU
    int evict_to_free;
    
    settings() {
        item_size_max = 1024 * 1024;
        verbose = 2;
        net_or_local = MODEL_NET;
        chunk_size = 48;
        backlog = 1024;
        port = 11211;
        oldest_live = 0;
        num_threads = 4;
        evict_to_free = 1;
    }
};

#endif
