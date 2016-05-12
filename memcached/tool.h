/*
 * (C) 2007-2015 No_company Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * tool.h -- 
 *
 * Version: 1.0  2016年02月15日 16时30分27秒
 *
 * Authors:
 *     Reazon (changgongxiaorong), cgxryy@gmail.com
 *
 */

#ifndef MEM_TOOL_H_
#define MEM_TOOL_H_

#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

//from char[] to uint64_t
bool safe_strtoull(const char* str, uint64_t* out);
//from char[] to int64_t
bool safe_strtoll(const char* str, int64_t* out);
//from char[] to uint32_t
bool safe_strtoul(const char* str, uint32_t* out);
//from char[] to int32_t
bool safe_strtol(const char* str, int32_t* out);

#endif
