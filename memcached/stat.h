/*
 * (C) 2007-2015 No_company Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * stat.h -- 
 *
 * Version: 1.0  2016年02月03日 21时56分23秒
 *
 * Authors:
 *     Reazon (changgongxiaorong), cgxryy@gmail.com
 *
 */
#ifndef MEM_STAT_H_
#define MEM_STAT_H_

#include <stdint.h>
#include <mutex>

struct Stats{
    std::mutex stat_lock;
    unsigned int hash_power_level;
    unsigned int curr_items;
    uint64_t hash_bytes;
    uint64_t curr_bytes;
    Stats() :
        hash_power_level(0),
        hash_bytes(0)
    {}
};

#endif
