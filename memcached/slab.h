/*
 * (C) 2007-2015 No_company Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * slab.h -- 
 *
 * Version: 1.0  2015年12月10日 17时36分42秒
 *
 * Authors:
 *     Reazon (changgongxiaorong), cgxryy@gmail.com
 *
 */
#ifndef MEM_SLAB_H_
#define MEM_SLAB_H_

#include <string>
#include <vector>

class SlabItem {
public:
    std::string key;
    std::string exptime;
    std::string value;
    std::string flag;
    int bytes;
};

class Slab {
public:
    std::vector<SlabItem*> slab_list;
};

#endif
