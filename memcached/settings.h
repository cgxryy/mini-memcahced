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
    //...
    int     num_threads;
    int     port;
    char*   unix_filename;
    int     net_or_local;
    int     backlog;//listen backlog
};

#endif
