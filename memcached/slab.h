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
#include <mutex>
#include <atomic>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "conn.h"
#include "settings.h"
//#include "stat.h"

/*
struct base_item;
class LRU_list;
class Slab;
struct slabclass;
class HashTable;
*/
class Conn;
struct Stats;
struct settings;

struct base_item {
    //item大小是否符合要求
    static bool item_size_ok(const size_t nkey, const int flag, const int nbytes);
    //item各项总计大小
    size_t item_total();
    void set_cas(uint64_t v);
    uint64_t get_cas();
    inline static uint64_t get_cas_id(void) {
        static uint64_t cas_id = 0;
        return ++cas_id;
    }
    inline char* real_data_addr() {
        return data + nkey + 1;
    }
    struct base_item    *next;
    struct base_item    *prev;      
    struct base_item    *h_next;    //记录HashTable的下一个item的地址
    uint64_t            cas;        //cas命令所需序列号
    unsigned int        realtime;
    unsigned int        exptime;
    int                 nbytes;     //value大小
    std::atomic<unsigned int> refcount;
    uint8_t             item_flag;
    uint8_t             slabs_clsid;//slab class编号
    uint8_t             nkey;       //key大小
    char               data[0];//key 和 value 存在了一起
    //不写在一起是为了内存对齐
};

class LRU_list {
private:
    const int TAIL_REPAIR_TIME = 3 * 3600;

public:
    static const int NOT_STORED = 0;
    static const int STORED = 1;
    static const int EXISTS = 2;
    static const int NOT_FOUND = 3;
    //更新间隔
    static const int ITEM_UPDATE_INTERVAL = 60;
    std::vector<base_item*> heads;
    std::vector<base_item*> tails;
    std::vector<unsigned int>sizes;

    LRU_list();
    void item_link_q(base_item* it);
    void item_unlink_q(base_item* it);
    base_item* do_item_get(const char* key, const size_t nkey, const uint32_t hv);
    base_item* do_item_alloc(const char* key, const size_t nkey, const int flags, const unsigned int exptime, const int nbytes, const uint32_t cur_hv);
    base_item* do_item_touch(const char* key, const size_t nkey, const uint32_t exptime, const uint32_t hv);
    int  do_item_link(base_item* item, const uint32_t hv);
    void do_item_unlink(base_item* item, const uint32_t hv);
    void do_item_remove(base_item* item);
    void do_item_update(base_item* item);
    int  do_item_replace(base_item* it, base_item* new_it, const uint32_t hv);
    void do_item_unlink_nolock(base_item* it, const uint32_t hv);
    int  do_store_item(base_item* item, Conn* c, uint32_t hv);

    //thread use
    int  item_link(base_item* item);
    void item_unlink(base_item* item);
    void item_update(base_item* item);
    void item_remove(base_item* item);
    void item_free(base_item* it);
    int  item_replace(base_item* old_it, base_item* new_it, const uint32_t hv);
    base_item* item_alloc(const char *key, size_t nkey, int flags, unsigned int exptime, int bytes);
    base_item* item_get(const char* key, const size_t nkey);
    base_item* item_touch(const char* key, size_t nkey, uint32_t exptime);
    int store_item(base_item* item, Conn* c);
};


inline int mutex_lock(std::mutex* mutex) {
    while(mutex->try_lock() == false)
        ;
    return 0;
}

class HashTable {
private:
    std::vector<struct base_item*> primary_hashtable;
    
public:
    static uint32_t hash(const void* key, size_t length);

    std::vector<std::mutex*> item_locks;
    int item_lock_count;
    int hashpower;

    inline unsigned long int hashsize(int n) {
        return ((unsigned long int)1<<n);
    }
    inline unsigned long int hashmask(unsigned long int n) {
        return (hashsize(n)-1);
    }

    HashTable(const int hashtable_init);
    HashTable() : HashTable(16) {}
    base_item* hash_find(const char* key, const size_t nkey, const uint32_t hv);
    int hash_insert(base_item* it, const uint32_t hv);
    void hash_delete(const char* key, const size_t nkey, const uint32_t hv);
    //不断的trylock区域锁
    void hash_lock(uint32_t hv);
    //揭开对应区域锁
    void hash_unlock(uint32_t hv);
    //区别于hash_lock只尝试一次区域锁
    std::mutex*  item_trylock(uint32_t hv);
    //和hash_unlock一样，只是为了名字匹配
    void item_trylock_unlock(std::mutex* lock);
};

struct slabclass {
    unsigned int size;              //最多存储多大base_item
    unsigned int perslab;           //多少base_item

    void* slots;                    //freelist链表头部
    unsigned int sl_curr;           //freelist中的个数

    unsigned int slabs;             //已申请的slabs个数
    void**  slab_list;              //slab内存链表
    unsigned int list_size;         //数组前面的大小

    unsigned int killing;           //失效的slab

    //为什么slab存储请求的bytes数
    size_t requested;
    slabclass();
};

class Slab {
public:
    unsigned int power_largest;

    static const uint8_t ITEM_LINKED = 1;
    static const uint8_t ITEM_CAS = 2;
    static const uint8_t ITEM_SLABBED = 4;
    static const uint8_t ITEM_FETCHED = 8;

    static const int POWER_SMALLEST = 1;
    static const int POWER_LARGEST = 200;
    static const int CHUNK_ALION_BYTES = 8;
    
    std::mutex slabs_lock;
    std::vector<slabclass> mem_base;
    size_t mem_limit;      //内存最大限值
    size_t mem_malloced;
    size_t mem_avail;

    Slab(struct settings* mem_setting) : Slab(10000000, 1.2, mem_setting){}
    Slab(size_t limit, double factor, struct settings* mem_setting);

    unsigned int slabs_clsid(const size_t size);
    void  slabs_preallocate(const unsigned int maxslabs);
    void* slabs_alloc(size_t size, unsigned int id);
    void  slabs_free(void* ptr, size_t size, unsigned int id);
    void* do_slabs_alloc(size_t size, unsigned int id);
    int   do_slabs_newslab(unsigned int id);
    char* memory_allocate(size_t len);
    void  split_slab_page_into_freelist(char* ptr, unsigned int id);
    void  do_slabs_free(void *ptr, size_t size, unsigned int id);
    int   grow_slab_list(unsigned int id);
    void  slabs_adjust_mem_requested(unsigned int id, size_t old, size_t ntotal);
};

extern settings mem_setting;

extern volatile unsigned int current_time();
extern std::mutex cache_lock;
extern HashTable hashtable;
extern Slab* slab;
extern unsigned int hashitems;
extern LRU_list lru_list;
#endif
