/*
 * (C) 2007-2015 No_company Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * slab.cpp -- 
 *
 * Version: 1.0  2015年12月10日 17时36分50秒
 *
 * Authors:
 *     Reazon (changgongxiaorong), cgxryy@gmail.com
 *
 */
#include "slab.h"
#include <iostream>

//这里传递参数的原因是不知道item是否存在
bool base_item::item_size_ok(const size_t nkey, const int flag, const int nbytes) {
    size_t ntotal = sizeof(base_item) + nkey + 1 + nbytes;

    return slab->slabs_clsid(ntotal) != 0;
}

size_t base_item::item_total() {
    //结构体大小+key大小('\0'+1)+value大小
    return (sizeof(base_item) + nkey + 1 + nbytes);
}

void base_item::set_cas(uint64_t v) {
    if (item_flag & Slab::ITEM_CAS) {
        cas = v;
    }
}

uint64_t base_item::get_cas() {
    return ((item_flag & Slab::ITEM_CAS) ? cas : (uint64_t)0);
}

slabclass::slabclass() {
    size = 0;
    perslab = 0;
    slots = 0;//NULL
    sl_curr = 0;
    slabs = 0;
    slab_list = 0;//NULL
    list_size = 0;
    killing = 0;
    requested = 0;
}

Slab::Slab(size_t limit, double factor, struct settings* mem_setting) :
    mem_limit(limit),
    mem_malloced(0) {

    int i = POWER_SMALLEST - 1;
    unsigned int size = sizeof(base_item) + mem_setting->chunk_size;
    
    slabclass temp;//only fill mem_base[0], no use
    mem_base.push_back(temp);

    while (++i < POWER_LARGEST && size <= mem_setting->item_size_max/factor) {
        //保证刚好字节对齐
        if (size % CHUNK_ALION_BYTES)
            size += CHUNK_ALION_BYTES - (size % CHUNK_ALION_BYTES);
        
        slabclass temp_slab;
        temp_slab.size = size;
        temp_slab.perslab = mem_setting->item_size_max / temp_slab.size;
        
        mem_base.push_back(temp_slab);

        size *= factor;
        if (mem_setting->verbose > 1) {
            std::cerr << "slab class " << i << ": chunk size " << temp_slab.size << " perslab " << temp_slab.perslab << std::endl; 
        }
    }

    power_largest = i;
    slabclass temp_slab;
    temp_slab.size = mem_setting->item_size_max;
    temp_slab.perslab = 1;
    mem_base.push_back(temp_slab);
    
    if (mem_setting->verbose > 1) {
        std::cerr << "slab class " << i << ": chunk size " << temp_slab.size << " perslab " << temp_slab.perslab << std::endl; 
    }

    //可以关闭
    slabs_preallocate(POWER_LARGEST);
}

unsigned int Slab::slabs_clsid(const size_t size) {
    int res = POWER_SMALLEST;

    if (size == 0)
        return 0;

    while (size > mem_base[res].size)
        if (res++ == power_largest)
            return 0;
    return res;
}

void Slab::slabs_preallocate(const unsigned int maxslabs) {
    int i;
    unsigned int prealloc = 0;

    for ( i = POWER_SMALLEST; i < power_largest; i++) {
        if (++prealloc > maxslabs)
            return;
        if (do_slabs_newslab(i) == 0) {
            std::cerr << "Error while preallocating slab class "<< i << " memory!\n"; 
            exit(1);
        }
    }
}

void* Slab::slabs_alloc(size_t size, unsigned int id) {
    void* ret;
    slabs_lock.lock();
    ret = do_slabs_alloc(size, id);
    slabs_lock.unlock();

    return ret;
}

void* Slab::do_slabs_alloc(size_t size, unsigned int id) {
    void* ret = 0;
    struct base_item *it = 0;
    slabclass& p = mem_base[id];

    if (id < POWER_SMALLEST || id > POWER_LARGEST) {
        return (void*)0;
    }

    //查看是否freelist上没有东西了
    if (! (p.sl_curr != 0 || do_slabs_newslab(id) != 0) ) {
        return (void*)0;
    }
    else if (p.sl_curr != 0) {
        //从freelist取出一个给他
        it = (struct base_item*)(p.slots);
        p.slots = it->next;
        if (it->next) 
            it->next->prev = 0;
        p.sl_curr--;
        ret = (void*)it;
    }

    if (ret) {
        p.requested += size;
    }

    return ret;
}

int Slab::grow_slab_list(unsigned int id) {
    slabclass& p = mem_base[id];
    if (p.slabs == p.list_size) {
        size_t new_size = (p.list_size != 0) ? p.list_size * 2 : 16;
        void*  new_list = realloc(p.slab_list, new_size * sizeof(void*));
        if (new_list == 0) {
            std::cerr << "new list realloc " << new_size*sizeof(void*) << "failed\n";
            return 0;
        }
        p.list_size = new_size;
        p.slab_list = (void**)new_list;
    }
    return 1;
}

int Slab::do_slabs_newslab(unsigned int id) {
    slabclass& p = mem_base[id];
    int len = p.size * p.perslab;
    char* ptr;

    //异常情况
    //内存限制不为0,申请大小总计已超过限制，并且已经申请了一些slab数
    if ((mem_limit && mem_malloced+len > mem_limit && p.slabs > 0) ||
           (grow_slab_list(id) == 0)  ||
            ((ptr = memory_allocate((size_t)len)) == 0)) {
        return 0;
    }
    
    memset(ptr, 0, (size_t)len);
    //把内存分解为chunk
    split_slab_page_into_freelist(ptr, id);
    p.slab_list[p.slabs++] = ptr;
    mem_malloced += len;
    
    return 1;
}

//no use
char* Slab::memory_allocate(size_t len) {

    char* ret = 0;
//    if (mem_base == 0) {
    ret = (char*)malloc(len);
//    }else {
//    }

    return ret;
}


void Slab::split_slab_page_into_freelist(char* ptr, unsigned int id) {
    slabclass& p = mem_base[id];
    int i;
    for (i = 0; i < p.perslab; i++) {
        do_slabs_free(ptr, 0, id);
        ptr += p.size;
    }
}

void Slab::slabs_free(void* ptr, size_t size, unsigned int id) {
    slabs_lock.lock();
    do_slabs_free(ptr, size, id);
    slabs_lock.unlock();
}
 

void Slab::do_slabs_free(void* ptr, size_t size, unsigned int id) {
    if (((struct base_item*)ptr)->slabs_clsid != 0 || 
            (id < POWER_SMALLEST || id > POWER_LARGEST))
        return;

    slabclass& p = mem_base[id];

    struct base_item* it = (struct base_item*)ptr;
    it->item_flag |= ITEM_SLABBED;
    it->prev = 0;
    it->next = (struct base_item*)(p.slots);
    if (it->next)
        it->next->prev = it;
    p.slots = it;
    //slab空闲链表长度增大
    p.sl_curr++;
    //使用的slab大小变小
    p.requested -= size;
    return;
}

//只是改一下slabclass里面每个块对应的存储数值
void Slab::slabs_adjust_mem_requested(unsigned int id, size_t old, size_t ntotal) {
    slabs_lock.lock();
    slabclass& p = mem_base[id];
    p.requested = p.requested - old + ntotal;
    slabs_lock.unlock();
}

HashTable::HashTable(const int hashtable_init) {
    if (hashtable_init) {
        hashpower = hashtable_init;
    }
    //is it correct?
    primary_hashtable.reserve(hashsize(hashpower));
    primary_hashtable.resize(hashsize(hashpower));
    
    //stats.stat_lock.lock();
    //stats.hash_power_level = hashpower;
    //stats.hash_bytes = hashsize(hashpower) * sizeof(void*);
    //stats.stat_lock.unlock();

    item_lock_count = hashsize(hashpower);
    item_locks.reserve(item_lock_count);
    item_locks.resize(item_lock_count);
    for( int i = 0; i < item_lock_count; i++) {
        item_locks[i] = new std::mutex;
    }
}

uint32_t HashTable::hash(const void* key, size_t length) {
    char* key_s = (char*)key;
    uint32_t hash, i;
    for( hash = i = 0; i < length; i++) {
        hash += key_s[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);

    return hash;
}

struct base_item* HashTable::hash_find(const char* key, const size_t nkey, const uint32_t hv) {
    struct base_item *it;
    it = primary_hashtable[hv & hashmask(hashpower)];
    
    int depth = 0;//循环的深度
    struct base_item *ret = 0;
    while (it) {
        if ((nkey == it->nkey) && (memcmp(key, it->data, nkey) == 0)) {
            ret = it;
            break;
        }
        it = it->h_next;
        ++depth;
    }
    //如果一直没找到直接返回ret初始值NULL
    return ret;
}

int HashTable::hash_insert(base_item* it, const uint32_t hv) {

    it->h_next = primary_hashtable[hv & hashmask(hashpower)];
    primary_hashtable[hv & hashmask(hashpower)] = it;
 
    //这个是自己加的锁，原本没有
    hashitems++;

    return 1;
}

void HashTable::hash_delete(const char* key, const size_t nkey, const uint32_t hv) {
    base_item* &p = primary_hashtable[hv & hashmask(hashpower)];
    base_item** pos;

    //因为c++引用和指针处理不同所以要先处理一下
    if (p && ((nkey != (p->nkey)) || memcmp(key, p->data, nkey))) {
        pos = &(p->h_next);
        while (*pos && ((nkey != ((*pos)->nkey)) || memcmp(key, (*pos)->data, nkey))) {
            pos = &((*pos)->h_next);
        }
    }
    else {
        p = 0;
        return;
    }
    
    //想改变的是指针的位置，所以必须用二级指针
    if (*pos) {
        base_item* nxt;
        hashitems--;
        nxt = (*pos)->h_next;
        (*pos)->h_next = 0;
        *pos = nxt;
        return;
    }
}

void HashTable::hash_lock(uint32_t hv) {
    mutex_lock(item_locks[(hv & hashmask(hashpower)) % item_lock_count]);
}

void HashTable::hash_unlock(uint32_t hv) {
    item_locks[(hv & hashmask(hashpower)) % item_lock_count]->unlock();
}

std::mutex* HashTable::item_trylock(uint32_t hv) {
    std::mutex* lock = item_locks[(hv & hashmask(hashpower)) % item_lock_count];
    //抢到锁证明这个区域目前没有其他线程增改存
    if (lock->try_lock() == true) {
        return lock;
    }
    return 0;
}

void HashTable::item_trylock_unlock(std::mutex* lock) {
    lock->unlock();
}

LRU_list::LRU_list() {
    heads.resize(Slab::POWER_LARGEST);
//    heads.reserve(Slab::POWER_LARGEST);
    tails.resize(Slab::POWER_LARGEST);
//    tails.reserve(Slab::POWER_LARGEST);
    sizes.resize(Slab::POWER_LARGEST);
//    sizes.reserve(Slab::POWER_LARGEST);
}

void LRU_list::item_link_q(base_item* it) {
    if (it->slabs_clsid > Slab::POWER_LARGEST || ((it->item_flag & Slab::ITEM_SLABBED) != 0)) {
        std::cerr << "item link error \n" << __FILE__ << ":" << __LINE__;
        return;
    }

    base_item* &it_head = heads[it->slabs_clsid];
    base_item* &it_tail = tails[it->slabs_clsid];

    if (it == it_head || !((it_head && it_tail) || (it_head == 0 && it_tail == 0))) {
        std::cerr << "item link error \n" << __FILE__ << ":" << __LINE__;
        return;
    }
    it->prev = 0;
    it->next = it_head;
    if (it->next)
        it->next->prev = it;
    it_head = it;
    if (it_tail == 0)
        it_tail = it;
    sizes[it->slabs_clsid]++;
    return;
}

void LRU_list::item_unlink_q(base_item* it) {
    if (it->slabs_clsid > Slab::POWER_LARGEST || ((it->item_flag & Slab::ITEM_SLABBED) != 0)) {
        std::cerr << "item link error \n" << __FILE__ << ":" << __LINE__;
        return;
    }

    base_item* &it_head = heads[it->slabs_clsid];
    base_item* &it_tail = tails[it->slabs_clsid];

    if (it_head == it) {
        if (it->prev != 0)
            it_head = it->next;
    }
    if (it_tail == it) {
        if (it->next != 0)
            it_tail = it->prev;
    }

    if (it->next)
        it->next->prev = it->prev;
    if (it->prev)
        it->prev->next = it->next;
    sizes[it->slabs_clsid]--;
    return;
}

//remove是把item从slab中释放掉(但是这个释放为提高效率也只是把它放到了slab中的空闲链表中)
void LRU_list::do_item_remove(base_item* it) {
    if ((it->item_flag & Slab::ITEM_SLABBED) != 0)
        return;

    //原子自减
    if (std::atomic_fetch_sub(&(it->refcount), 1u) == 0) {
        item_free(it);
    }
}

//unlink是把item从hashtable上和LRU上移除
void LRU_list::do_item_unlink(base_item* it, uint32_t hv) {
    mutex_lock(&cache_lock);
    if ((it->item_flag & Slab::ITEM_LINKED) != 0) {
        it->item_flag &= ~Slab::ITEM_LINKED;
        //stats.stat_lock.lock();
        //stats.curr_bytes -= sizeof(base_item);
        //stats.curr_items -= 1;
        //stats.stat_lock.unlock();
        hashtable.hash_delete(it->data, it->nkey, hv);
        item_unlink_q(it);
        do_item_remove(it);
    }
    cache_lock.unlock();
}

int LRU_list::do_item_link(base_item* item, const uint32_t hv) {
    if ((item->item_flag & (Slab::ITEM_LINKED | Slab::ITEM_SLABBED)) != 0){
        return 0;
    }
    mutex_lock(&cache_lock);

    item->item_flag |= Slab::ITEM_LINKED;
    item->realtime = current_time();
 
    //stats.stat_lock.lock();
    //stats.curr_bytes -= sizeof(base_item);
    //stats.curr_items -= 1;
    //stats.stat_lock.unlock();
    
    item->set_cas(base_item::get_cas_id());
    hashtable.hash_insert(item, hv);
    item_link_q(item);
    std::atomic_fetch_add(&(item->refcount), 1u);

    cache_lock.unlock();
    return 1;
}

void LRU_list::do_item_update(base_item* item) {
    if (item->realtime < current_time() - ITEM_UPDATE_INTERVAL) {
        if ((item->item_flag & Slab::ITEM_SLABBED) != 0) {
            return;
        }
        mutex_lock(&cache_lock);
        if ((item->item_flag & Slab::ITEM_LINKED) != 0){
            item_unlink_q(item);
            item->realtime = current_time();
            item_link_q(item);
        }
        cache_lock.unlock();
    }
}

int LRU_list::do_item_replace(base_item* it, base_item* new_it, const uint32_t hv) {
    if ((it->item_flag & Slab::ITEM_SLABBED) != 0)
        return 0;
    do_item_unlink(it, hv);
    return do_item_link(new_it, hv);
}

void LRU_list::do_item_unlink_nolock(base_item* it, const uint32_t hv) {
    if ((it->item_flag & Slab::ITEM_LINKED) != 0){
        it->item_flag &= ~(Slab::ITEM_LINKED);
        //省去状态更迭
        hashtable.hash_delete(it->data, it->nkey, hv);
        item_unlink_q(it);
        do_item_remove(it);
    }
}

base_item* LRU_list::do_item_get(const char* key, const size_t nkey, const uint32_t hv) {
    base_item* it = hashtable.hash_find(key, nkey, hv);
    if (it != NULL) {
        //原子增
        std::atomic_fetch_add(&(it->refcount), 1u);
        /*
         * 这里有一段相关item忙等的东西，不知道和扩容相关不
        if (slab_rebalance_signal &&
            ((void *)it >= slab_rebal.slab_start && (void *)it < slab_rebal.slab_end)) {
            do_item_unlink_nolock(it, hv);
            do_item_remove(it);
            it = NULL;
        } 
         */
    }
    int was_found = 0;
    //输出必要信息
    if (mem_setting.verbose > 2) {
        int ii;
        if (it == 0) {
            std::cerr << "> NOT FOUND ";
        }
        else {
            std::cerr << "> FOUND KEY ";
            was_found++;
        }
        for( ii = 0; ii < nkey; ++ii) {
            std::cerr << key[ii];
        }
    }
    
    if (it != 0) {
        //如果存活时间不为0或者小于规定时间，删除(LRU算法)
        //但此时仍不知何时设置时间的
    /*    if (mem_setting.oldest_live != 0 && mem_setting.oldest_live <= current_time && 
                it->realtime <= mem_setting.oldest_live) {
            do_item_unlink(it, hv);
            do_item_remove(it);
            it = 0;
            if (was_found) {
                std::cerr << "-nuked by flush\n";
            }
        } else 
     */ if (it->exptime != 0 && it->exptime <= current_time()) {
            do_item_unlink(it, hv);
            do_item_remove(it);
            it = 0;
            if (was_found) {
                std::cerr << "-nuked by expire\n";
            }
            
        }else {
            it->item_flag |= Slab::ITEM_FETCHED;
        }
    }
    
    if (mem_setting.verbose > 2) {
        std::cerr << '\n';
    }

    return it;
}

base_item* LRU_list::do_item_alloc(const char* key, const size_t nkey, const int flags, const unsigned int exptime, const int nbytes, const uint32_t cur_hv) {
    base_item *it = 0;
    //base_item nkey nbytes
    int ntotal = sizeof(base_item) + nkey + 1 + nbytes;
    std::cout << slab->mem_base.size() << std::endl;
    unsigned int id = slab->slabs_clsid(ntotal);
    if (id == 0)
        return 0;

    mutex_lock(&cache_lock);

    //尝试从LRU链尾中找5次，看是否有合适的item被替换(没有被锁定&&淘汰时间到了的)
    int tries = 5;
    int tried_alloc = 0;
    base_item* search = tails[id];
    base_item* next_it;
    std::mutex* hold_lock;

    for( ; tries > 0 && search != 0; tries--, search = next_it) {
        next_it = search->prev;
        //不满足条件的item跳过
        if (search->nbytes == 0 && search->nkey == 0 && search->item_flag == 1) {
            tries++;
            continue;
        }

        //算出哈希值
        uint32_t hv = hashtable.hash(search->data, search->nkey);
        //此处有很大争议
        //if (hv == cur_hv || (hold_lock = item_trylock(hv)))
        //if (hv != cur_hv && (hold_lock = item_trylock(hv)))
        //是否被锁定
        if ((hold_lock = hashtable.item_trylock(hv)) == 0)
            continue;

        if (std::atomic_fetch_add(&(search->refcount), 1u) != 2) {
            std::atomic_fetch_sub(&(search->refcount), 1u);
                if (search->realtime + TAIL_REPAIR_TIME < current_time())  {
                    search->refcount = 1;
                    do_item_unlink_nolock(search, hv);
                }
                if (hold_lock)
                    hashtable.item_trylock_unlock(hold_lock);
                continue;
        }

        if ((search->exptime != 0 && search->exptime < current_time())
              //少了一个和最老时间的比较
                ) {
            it = search;
            //这里的item_total与ntotal区别在?
            slab->slabs_adjust_mem_requested(it->slabs_clsid, it->item_total(), ntotal);
            do_item_unlink_nolock(it, hv);
            it->slabs_clsid = 0;
        } else if ((it = (base_item*)(slab->slabs_alloc(ntotal, id))) == 0) {
            //这个if语句都是来记录item各种状态的
            tried_alloc = 1;
            if (mem_setting.evict_to_free == 0) {
                //记录超出内存
            }
            else {
                if (search->exptime != 0)
            

            it = search;
            slab->slabs_adjust_mem_requested(it->slabs_clsid, it->item_total(), ntotal);
            do_item_unlink_nolock(it, hv);
            it->slabs_clsid = 0;
            //if (mem_setting.slab_automove == 2)
            }
        }
        //解除原子引用锁定
        std::atomic_fetch_sub(&(search->refcount), 1u);
        if (hold_lock)
            hashtable.item_trylock_unlock(hold_lock);
        break;
    }

    if (!tried_alloc && (tries == 0 || search == 0))
        it = (base_item*)slab->slabs_alloc(ntotal, id);

    cache_lock.unlock();

    return it;
}

//修改item的时间
base_item* LRU_list::do_item_touch(const char* key, const size_t nkey, const uint32_t exptime, const uint32_t hv) {
    base_item *it = do_item_get(key, nkey, hv);
    if (it != NULL) {
        it->exptime = exptime;
    }
    return it;
}

int LRU_list::do_store_item(base_item* it, Conn* c, uint32_t hv) {
    char* key = it->data;
    
    //获取旧的数据项
    base_item* old_it = do_item_get(key, it->nkey, hv);
    int store_stat = LRU_list::NOT_STORED;

    base_item* new_it = 0;
    int flags = 0;

    //已经有该项item存在
    if (old_it != 0 && c->cmd == NREAD_ADD) {
        /* 
         * 更新当前item目的
         * 1.更新时间,重建LRU链
         * 2.后面执行do_item_remove，每次remove会把refcount引用计数减一
         *   如果引用计数=1则被删除，重建之后refcount为2
         * */
        do_item_update(old_it);
    //旧的item不存在
    }else if (!old_it && (c->cmd == NREAD_REPLACE || c->cmd == NREAD_APPEND 
                || c->cmd == NREAD_PREPEND)) {
        //什么也不做,因为只有replace替换已有值
    }else if (c->cmd == NREAD_CAS) {

        //不存在此项
        if (old_it == 0) {
            store_stat = LRU_list::NOT_FOUND;
        }
        if (it->get_cas() == old_it->get_cas()) {
            item_replace(old_it, it, hv);
            store_stat = LRU_list::STORED;
        }
        else {
            if (mem_setting.verbose > 1) {
                std::cerr << "CAS: failure: expected " << old_it->get_cas() << " but got " 
                    << it->get_cas();
            }
            store_stat = LRU_list::EXISTS;
        }
    }
    else {
        //与上面第二个判断不同，这里是旧的item存在的replace append prepend set命令
        if (c->cmd == NREAD_APPEND || c->cmd == NREAD_PREPEND) {
            if (it->get_cas() != 0) {
                if (it->get_cas() != old_it->get_cas()) {
                    store_stat = LRU_list::EXISTS;
                }
            }

            if (store_stat == LRU_list::NOT_STORED) {
                new_it = do_item_alloc(key, it->nkey, flags, old_it->exptime, 
                        it->nbytes + old_it->nbytes - 2, hv);
                //分配失败
                if (new_it == 0) {
                    if (old_it != 0) 
                        do_item_remove(old_it);
                    return LRU_list::NOT_STORED;
                }

                //copy数据
                if (c->cmd == NREAD_APPEND) {
                    memcpy(new_it->real_data_addr(), old_it->real_data_addr(), 
                            old_it->nbytes);
                    memcpy(new_it->real_data_addr() + old_it->nbytes - 2/*\r\n*/, 
                            it->real_data_addr(), it->nbytes);
                }
                else {
                    //NREAD_PREPEND
                    memcpy(new_it->real_data_addr(), it->real_data_addr(), it->nbytes);
                    memcpy(new_it->real_data_addr() + it->nbytes - 2, 
                            old_it->real_data_addr(), old_it->nbytes);
                }
                it = new_it;
            }
        }

        if (store_stat == LRU_list::NOT_STORED) {
            if (old_it != 0)
                item_replace(old_it, it, hv);
            else 
                //set a new key-value
                do_item_link(it, hv);

            c->cas = it->get_cas();
            store_stat = LRU_list::STORED;
        }
    }

    if (old_it != 0)
        do_item_remove(old_it);
    if (new_it != 0)
        do_item_remove(new_it);

    if (store_stat == LRU_list::STORED) {
        c->cas = it->get_cas();
    }

    return store_stat;
}

int LRU_list::item_link(base_item* item) {
    int ret;
    uint32_t hv;

    hv = HashTable::hash(item->data, item->nkey);
    //哈希局部锁
    hashtable.hash_lock(hv);
    //调用实现的函数
    ret = do_item_link(item, hv);
    hashtable.hash_unlock(hv);

    return ret;
}

void LRU_list::item_unlink(base_item* item) {
    uint32_t hv;

    hv = HashTable::hash(item->data, item->nkey);
    //哈希局部锁
    hashtable.hash_lock(hv);
    do_item_unlink(item, hv);
    hashtable.hash_unlock(hv);
}

void LRU_list::item_update(base_item* item) {
    uint32_t hv;

    hv = HashTable::hash(item->data, item->nkey);
    //哈希局部锁
    hashtable.hash_lock(hv);
    do_item_update(item);
    hashtable.hash_unlock(hv);
}

void LRU_list::item_remove(base_item* item) {
    uint32_t hv;

    hv = HashTable::hash(item->data, item->nkey);
    //哈希局部锁
    hashtable.hash_lock(hv);
    do_item_remove(item);
    hashtable.hash_unlock(hv);
}

void LRU_list::item_free(base_item* it) {
    size_t ntotal = it->item_total();
    unsigned int clsid;
    if ( !((it->item_flag & Slab::ITEM_LINKED) == 0 && 
            it != heads[it->slabs_clsid] &&
            it != tails[it->slabs_clsid] &&
            it->refcount == 0)) {
        std::cerr << "item free error \n" << __FILE__ << ":" << __LINE__;
        return;
    }

    clsid = it->slabs_clsid;
    it->slabs_clsid = 0;
    slab->slabs_free(it, ntotal, clsid);
}

int LRU_list::item_replace(base_item* old_it, base_item* new_it, const uint32_t hv) {
    return do_item_replace(old_it, new_it, hv);
}

base_item* LRU_list::item_alloc(const char* key, size_t nkey, int flags, unsigned int exptime, int bytes) {
    base_item* it;
    it = do_item_alloc(key, nkey, flags, exptime, bytes, 0);
    return it;
}

base_item* LRU_list::item_get(const char* key, const size_t nkey) {
    base_item *it;
    uint32_t hv;
    hv = HashTable::hash(key, nkey);
    hashtable.hash_lock(hv);
    it = do_item_get(key, nkey, hv);
    hashtable.hash_unlock(hv);
    return it;
}

base_item* LRU_list::item_touch(const char* key, size_t nkey, uint32_t exptime) {
    base_item *it;
    uint32_t hv;
    hv = HashTable::hash(key, nkey);
    hashtable.hash_lock(hv);
    it = do_item_touch(key, nkey, exptime, hv);
    hashtable.hash_unlock(hv);
    return it;
}

int LRU_list::store_item(base_item* item, Conn* c) {
    int ret;
    uint32_t hv;

    hv = hashtable.hash(item->data, item->nkey);
    //区域锁
    hashtable.hash_lock(hv);
    ret = do_store_item(item, c, hv);
    hashtable.hash_unlock(hv); 
    
    return ret;
}
