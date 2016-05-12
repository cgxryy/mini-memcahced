/*
 * (C) 2007-2015 No_company Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * tool.cpp -- 
 *
 * Version: 1.0  2016年02月15日 16时35分56秒
 *
 * Authors:
 *     Reazon (changgongxiaorong), cgxryy@gmail.com
 *
 */
#include "tool.h"

/* 
 * string to unsigned long long int
 * */
bool safe_strtoull(const char* str, uint64_t *out) {
    errno = 0;
    *out = 0;
    char *endptr;
    unsigned long long ull = strtoull(str, &endptr, 10);
    if ((errno == ERANGE) || (str == endptr))
        return false;

    if ((*endptr == '\0') && endptr != str) {
        if ((long long)ull < 0) {
            if (strchr(str, '-') != 0) {
                return false;
            }
        }
        *out = ull;
        return true;
    }
    return false;
}


/*
 * string to long long int
 */
bool safe_strtoll(const char* str, int64_t* out) {
    errno = 0;
    *out = 0;
    char* endptr;
    long long ll = strtoll(str, &endptr, 10);
    if ((errno == ERANGE) || (str == endptr))
        return false;

    if ((*endptr == '\0') && endptr != str) {
        *out = ll;
        return true;
    }
    return false;
}

/*
 * string to unsigned long int
 */
bool safe_strtoul(const char* str, uint32_t* out) {
    char* endptr = 0;
    unsigned long l = 0;
    *out = 0;
    errno = 0;

    l = strtoul(str, &endptr, 10);
    if ((errno == ERANGE) || (str == endptr)) {
        return false;
    }

    if ((*endptr == '\0') && endptr != str) { 
        if ((long)l < 0) {
            if (strchr(str, '-') != 0)
                return false;
        }
        *out = l;
        return true;
    }
    return false;
}

/*
 * string to long int
 */
bool safe_strtol(const char* str, int32_t* out) {
    errno = 0;
    *out = 0;
    char* endptr;
    long l = strtol(str, &endptr, 10);
    if ((errno == ERANGE) || (str == endptr)) {
        return false;
    }

    if ((*endptr == '\0') && (endptr != str)) {
        *out = l;
        return true;
    }
    return false;
}
