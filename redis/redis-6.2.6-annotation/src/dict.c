/* Hash Tables Implementation.
 *
 * This file implements in memory hash tables with insert/del/replace/find/
 * get-random-element operations. Hash tables will auto resize if needed
 * tables of power of two in size are used, collisions are handled by
 * chaining. See the source code for more information... :)
 *
 * Copyright (c) 2006-2012, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "fmacros.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <sys/time.h>

#include "dict.h"
#include "zmalloc.h"
#include "redisassert.h"

/* Using dictEnableResize() / dictDisableResize() we make possible to
 * enable/disable resizing of the hash table as needed. This is very important
 * for Redis, as we use copy-on-write and don't want to move too much memory
 * around when there is a child performing saving operations.
 *
 * 通过dictEnableResize() / dictDisableResize()可以手动允许/禁止hash表进行rehash
 *
 * Note that even when dict_can_resize is set to 0, not all resizes are
 * prevented: a hash table is still allowed to grow if the ratio between
 * the number of elements and the buckets > dict_force_resize_ratio.
 * 
 * dictDisableResize()不能禁止所有的rehash
 * 如果已使用节点数量/bucket数量>dict_force_resize_ratio，则会强制rehash
 */
// 字典是否使用rehash，1表示enable，0表示disable
static int dict_can_resize = 1;
// 强制rehash的比例
static unsigned int dict_force_resize_ratio = 5;

/* -------------------------- private prototypes ---------------------------- */

static int _dictExpandIfNeeded(dict *ht);
static unsigned long _dictNextPower(unsigned long size);
static long _dictKeyIndex(dict *ht, const void *key, uint64_t hash, dictEntry **existing);
static int _dictInit(dict *ht, dictType *type, void *privDataPtr);

/* -------------------------- hash functions -------------------------------- */

static uint8_t dict_hash_function_seed[16];

// 设置hash函数种子
void dictSetHashFunctionSeed(uint8_t *seed) {
    memcpy(dict_hash_function_seed,seed,sizeof(dict_hash_function_seed));
}

// 取hash函数种子
uint8_t *dictGetHashFunctionSeed(void) {
    return dict_hash_function_seed;
}

/* The default hashing function uses SipHash implementation
 * in siphash.c. */
/* 默认hash函数使用SipHash（siphash.c） */
uint64_t siphash(const uint8_t *in, const size_t inlen, const uint8_t *k);
uint64_t siphash_nocase(const uint8_t *in, const size_t inlen, const uint8_t *k);

uint64_t dictGenHashFunction(const void *key, int len) {
    return siphash(key,len,dict_hash_function_seed);
}

uint64_t dictGenCaseHashFunction(const unsigned char *buf, int len) {
    return siphash_nocase(buf,len,dict_hash_function_seed);
}

/* ----------------------------- API implementation ------------------------- */

/* Reset a hash table already initialized with ht_init().
 * NOTE: This function should only be called by ht_destroy().
 * 上述注释已经过期了
 */
/* 重置/初始化hash表的各个成员 */
static void _dictReset(dictht *ht)
{
    ht->table = NULL;
    ht->size = 0;
    ht->sizemask = 0;
    ht->used = 0;
}

/* Create a new hash table */
/* 新建hash表 */
dict *dictCreate(dictType *type,
        void *privDataPtr)
{
    dict *d = zmalloc(sizeof(*d));

	// 初始化字典
    _dictInit(d,type,privDataPtr);
    return d;
}

/* Initialize the hash table */
/* 初始化hash表 */
int _dictInit(dict *d, dictType *type,
        void *privDataPtr)
{
	// 初始化dict中2个hash表的各个成员
    _dictReset(&d->ht[0]);
    _dictReset(&d->ht[1]);

	// 设置字典类型
    d->type = type;
	// 设置私有数据
    d->privdata = privDataPtr;
	// 设置hash表rehash状态，不进行rehash时，为-1
    d->rehashidx = -1;
    d->pauserehash = 0;
    return DICT_OK;
}

/* Resize the table to the minimal size that contains all the elements,
 * but with the invariant of a USED/BUCKETS ratio near to <= 1 */
/* 调整字典大小，使已用节点数和字典大小之间的比例接近1:1 */
int dictResize(dict *d)
{
	// 字段数组最小长度
    unsigned long minimal;

	// 如果在不允许resize或dict正在rehash的时候调用，返回DICT_ERR
    if (!dict_can_resize || dictIsRehashing(d)) return DICT_ERR;

	// 获取hash表元素数量
    minimal = d->ht[0].used;
	// 如果元素数量小于最小初始化大小，则使用DICT_HT_INITIAL_SIZE
    if (minimal < DICT_HT_INITIAL_SIZE)
        minimal = DICT_HT_INITIAL_SIZE;

	// 根据minimal调整字典大小
    return dictExpand(d, minimal);
}

/* Expand or create the hash table,
 * when malloc_failed is non-NULL, it'll avoid panic if malloc fails (in which case it'll be set to 1).
 * Returns DICT_OK if expand was performed, and DICT_ERR if skipped. */
/* 创建或者扩展hash表
 * 扩展思路：
 * 根据要求的size，计算新hash表容量大小，创建新hash表，
 * 并将新hash表放到ht[1]中，并设置rehashidx=0（即从第0个桶开始做rehash迁移）
 * 此处新的hash表数组的大小为2^n，方便使用&进行hash取模，比%取模有性能优势
 % 如果hash表ht[0]没有初始化，则直接初始化到ht[0]
 * T = O(N)
 */
int _dictExpand(dict *d, unsigned long size, int* malloc_failed)
{
    if (malloc_failed) *malloc_failed = 0;

    /* the size is invalid if it is smaller than the number of
     * elements already inside the hash table */
	// 如果字典正在rehash，或者已有节点数大于扩容数组的大小，则返回DICT_ERR
    if (dictIsRehashing(d) || d->ht[0].used > size)
        return DICT_ERR;
	
    dictht n; /* the new hash table */
    // 计算要扩容的容量，为大于size的第一个2^n
	unsigned long realsize = _dictNextPower(size);

    /* Detect overflows */
	// 避免溢出
    if (realsize < size || realsize * sizeof(dictEntry*) < realsize)
        return DICT_ERR;

    /* Rehashing to the same table size is not useful. */
	// 如果要扩容的容量和原来一样，则返回DICT_ERR
    if (realsize == d->ht[0].size) return DICT_ERR;

    /* Allocate the new hash table and initialize all pointers to NULL */
	// 设置新的容量和对应掩码
    n.size = realsize;
    n.sizemask = realsize-1;
	// 选择在分配失败时报错还是直接终止程序
    if (malloc_failed) {
		// 尝试分配hash数组
        n.table = ztrycalloc(realsize*sizeof(dictEntry*));
        // 设置是否分配失败，n.table == NULL即分配失败
		*malloc_failed = n.table == NULL;
		// 如果分配失败，则返回错误
        if (*malloc_failed)
            return DICT_ERR;
    } else
		// 分配hash数组，分配失败就终止程序
        n.table = zcalloc(realsize*sizeof(dictEntry*));

	// 设置已用节点数为0
    n.used = 0;

    /* Is this the first initialization? If so it's not really a rehashing
     * we just set the first hash table so that it can accept keys. */
	// 如果字典的hash表为NULL，则表示这是首次初始化
    if (d->ht[0].table == NULL) {
		// 将hash表放到hash数组的第0位中
        d->ht[0] = n;
        return DICT_OK;
    }

    /* Prepare a second hash table for incremental rehashing */
	// 如果不是首次初始化，则将hash表放到hash数组的第1位，用于rehash
    d->ht[1] = n;
	// 设置从hash表中的第0位进行迁移
    d->rehashidx = 0;
    return DICT_OK;
}

/* return DICT_ERR if expand was not performed */
/* 如果扩展失败，报DICT_ERR */
int dictExpand(dict *d, unsigned long size) {
    return _dictExpand(d, size, NULL);
}

/* return DICT_ERR if expand failed due to memory allocation failure */
/* 如果因为内存分配失败而扩展失败，返回DICT_ERR */
int dictTryExpand(dict *d, unsigned long size) {
    int malloc_failed;
    _dictExpand(d, size, &malloc_failed);
    return malloc_failed? DICT_ERR : DICT_OK;
}

/* Performs N steps of incremental rehashing. Returns 1 if there are still
 * keys to move from the old to the new hash table, otherwise 0 is returned.
 *
 * Note that a rehashing step consists in moving a bucket (that may have more
 * than one key as we use chaining) from the old to the new hash table, however
 * since part of the hash table may be composed of empty spaces, it is not
 * guaranteed that this function will rehash even a single bucket, since it
 * will visit at max N*10 empty buckets in total, otherwise the amount of
 * work it does would be unbound and the function may block for a long time. */
/* 字典rehash过程，n表示迁移桶的数量 */
int dictRehash(dict *d, int n) {
	// 最多遍历桶的数量
    int empty_visits = n*10; /* Max number of empty buckets to visit. */
    // 如果字典不是rehash状态，返回0
	if (!dictIsRehashing(d)) return 0;

	// 如果迁移桶的数量不为0，且hash表没迁移完
    while(n-- && d->ht[0].used != 0) {
        dictEntry *de, *nextde;

        /* Note that rehashidx can't overflow as we are sure there are more
         * elements because ht[0].used != 0 */
		// 断言hash表桶的个数>已经rehash的桶索引，即rehashidx <= ht[0].size - 1
        assert(d->ht[0].size > (unsigned long)d->rehashidx);
		// 如果要迁移的桶为空，则准备迁移下一个桶（这步去除空桶）
        while(d->ht[0].table[d->rehashidx] == NULL) {
            d->rehashidx++;
			// 如果已经超过规定处理的最大空桶数量，则本次迁移完毕
            if (--empty_visits == 0) return 1;
        }
		// 获取要迁移的桶的链表的首节点
        de = d->ht[0].table[d->rehashidx];
        /* Move all the keys in this bucket from the old to the new hash HT */
		// 遍历要迁移的桶的链表
        while(de) {
            uint64_t h;

			// 获取下一个节点
            nextde = de->next;
            /* Get the index in the new hash table */
			// 计算当前节点在新hash表中的hash值（桶idx）
            h = dictHashKey(d, de->key) & d->ht[1].sizemask;
			// 将当前节点迁移到新hash表中
            de->next = d->ht[1].table[h];
            d->ht[1].table[h] = de;
			// 旧hash表数量减少
            d->ht[0].used--;
			// 新hash表数量增加
            d->ht[1].used++;
			// 处理下一个节点
            de = nextde;
        }
		// 清空旧节点的hash桶
        d->ht[0].table[d->rehashidx] = NULL;
		// 设置下一个要迁移的hash桶
        d->rehashidx++;
    }

    /* Check if we already rehashed the whole table... */
	// 判断hash表是否迁移完成
    if (d->ht[0].used == 0) {
		// 迁移完成，回收旧hash表
        zfree(d->ht[0].table);
		// 用新hash表替换旧hash表
        d->ht[0] = d->ht[1];
		// 重置临时的hash表
        _dictReset(&d->ht[1]);
		// 清空rehashidx，回到未rehash状态
        d->rehashidx = -1;
        return 0;
    }

    /* More to rehash... */
	// 1表示未迁移完成
    return 1;
}

// 获取当前毫秒时间
long long timeInMilliseconds(void) {
    struct timeval tv;

    gettimeofday(&tv,NULL);
    return (((long long)tv.tv_sec)*1000)+(tv.tv_usec/1000);
}

/* Rehash in ms+"delta" milliseconds. The value of "delta" is larger 
 * than 0, and is smaller than 1 in most cases. The exact upper bound 
 * depends on the running time of dictRehash(d,100).*/
/* 指定时间rehash */
int dictRehashMilliseconds(dict *d, int ms) {
    // 如果暂停rehash，直接返回，不处理
	if (d->pauserehash > 0) return 0;

	// 获取当前时间戳
    long long start = timeInMilliseconds();
    // 统计rehash桶的个数
	int rehashes = 0;

	// rehash 100个桶
    while(dictRehash(d,100)) {
        // 统计rehash的桶的个数
		rehashes += 100;
        // 超出给定时间，退出rehash过程
		if (timeInMilliseconds()-start > ms) break;
    }
	// 返回本次rehash桶的个数
    return rehashes;
}

/* This function performs just a step of rehashing, and only if hashing has
 * not been paused for our hash table. When we have iterators in the
 * middle of a rehashing we can't mess with the two hash tables otherwise
 * some element can be missed or duplicated.
 *
 * This function is called by common lookup or update operations in the
 * dictionary so that the hash table automatically migrates from H1 to H2
 * while it is actively used. */
/* 执行一步incremental rehash */
static void _dictRehashStep(dict *d) {
    // 没有暂停rehash的情况执行
	if (d->pauserehash == 0) dictRehash(d,1);
}

/* Add an element to the target hash table */
/* 尝试增加k-v对
 * 在key不存在的时候添加操作才会成功
 * 添加成功 -- 返回DICT_OK
 * 添加失败 -- 返回DICT_ERR
 */
int dictAdd(dict *d, void *key, void *val)
{
	// 添加一个节点
    dictEntry *entry = dictAddRaw(d,key,NULL);

	// 如果添加失败（key已存在），返回DICT_ERR
    if (!entry) return DICT_ERR;
	// key不存在，添加成功，则将值设置到entry中
    dictSetVal(d, entry, val);
    return DICT_OK;
}

/* Low level add or find:
 * This function adds the entry but instead of setting a value returns the
 * dictEntry structure to the user, that will make sure to fill the value
 * field as they wish.
 *
 * This function is also directly exposed to the user API to be called
 * mainly in order to store non-pointers inside the hash value, example:
 *
 * entry = dictAddRaw(dict,mykey,NULL);
 * if (entry != NULL) dictSetSignedIntegerVal(entry,1000);
 *
 * Return values:
 *
 * If key already exists NULL is returned, and "*existing" is populated
 * with the existing entry if existing is not NULL.
 *
 * If key was added, the hash entry is returned to be manipulated by the caller.
 */
/* 尝试将键插入到字典中
 * 
 * 如果key已经存在于字典中，则返回NULL
 * 如果key不存在，则创建新的hash节点，将节点与key关联，并插入字典，返回值为dictEntry本身
 */
dictEntry *dictAddRaw(dict *d, void *key, dictEntry **existing)
{
    long index;
    dictEntry *entry;
    dictht *ht;

	// 如果当前字典d正在rehash，帮忙做rehash的工作
    if (dictIsRehashing(d)) _dictRehashStep(d);

    /* Get the index of the new element, or -1 if
     * the element already exists. */
	// 获取key对应的桶idx，如果key已经存在，返回NULL
    if ((index = _dictKeyIndex(d, key, dictHashKey(d,key), existing)) == -1)
        return NULL;

    /* Allocate the memory and store the new entry.
     * Insert the element in top, with the assumption that in a database
     * system it is more likely that recently added entries are accessed
     * more frequently. */
	// 获取hash表，如果正在rehash，用新hash表ht[1]，否则用旧hash表ht[0]
    ht = dictIsRehashing(d) ? &d->ht[1] : &d->ht[0];
    // 分配dictEntry内存
	entry = zmalloc(sizeof(*entry));
    // 将节点添加到当前桶，并作为桶中节点的链表头
	entry->next = ht->table[index];
    ht->table[index] = entry;
	// 更新节点数量
    ht->used++;

    /* Set the hash entry fields. */
	// 将hash值设置到节点（entry）中
    dictSetKey(d, entry, key);
    return entry;
}

/* Add or Overwrite:
 * Add an element, discarding the old value if the key already exists.
 * Return 1 if the key was added from scratch, 0 if there was already an
 * element with such key and dictReplace() just performed a value update
 * operation. */
/* 增加k-v */
int dictReplace(dict *d, void *key, void *val)
{
    dictEntry *entry, *existing, auxentry;

    /* Try to add the element. If the key
     * does not exists dictAdd will succeed. */
	// 尝试直接将k-v对添加到字典
	// 如果键key不存在的话，添加成功
    entry = dictAddRaw(d,key,&existing);
    if (entry) {
		// 将值设置到节点中
        dictSetVal(d, entry, val);
        return 1;
    }

    /* Set the new value and free the old one. Note that it is important
     * to do that in this order, as the value may just be exactly the same
     * as the previous one. In this context, think to reference counting,
     * you want to increment (set), and then decrement (free), and not the
     * reverse. */
	// 浅拷贝，auxentry里包含原有dictEntry的void * key，val，dictEntry* next
    auxentry = *existing;
	// 设置新的值
    dictSetVal(d, existing, val);
	// 释放旧的值
    dictFreeVal(d, &auxentry);
    return 0;
}

/* Add or Find:
 * dictAddOrFind() is simply a version of dictAddRaw() that always
 * returns the hash entry of the specified key, even if the key already
 * exists and can't be added (in that case the entry of the already
 * existing key is returned.)
 *
 * See dictAddRaw() for more information. */
/* 添加节点，如果已存在，则直接返回已存在的节点 */
dictEntry *dictAddOrFind(dict *d, void *key) {
    dictEntry *entry, *existing;
	// 如果key已经存在于字典中，entry = NULL，existing返回找到对应于key的dictEntry
	// 如果key不存在，则创建新的hash节点，将节点与key关联，并插入字典，返回值为dictEntry本身
    entry = dictAddRaw(d,key,&existing);
    return entry ? entry : existing;
}

/* Search and remove an element. This is an helper function for
 * dictDelete() and dictUnlink(), please check the top comment
 * of those functions. */
/* 查找并删除包含给定key的节点 
 * 
 * 参数nofree决定是否调用键和值的释放函数
 * 0 -- 调用
 * 1 -- 不调用
 * 
 * 找到并成功删除返回被删除的节点，没找到则返回NULL
 */
static dictEntry *dictGenericDelete(dict *d, const void *key, int nofree) {
    uint64_t h, idx;
    dictEntry *he, *prevHe;
    int table;

    if (d->ht[0].used == 0 && d->ht[1].used == 0) return NULL;

	// 如果正在rehash，帮忙做一步rehash操作
    if (dictIsRehashing(d)) _dictRehashStep(d);

	// 计算hash值
    h = dictHashKey(d, key);

	// 遍历hash表
    for (table = 0; table <= 1; table++) {
		// 计算桶索引
        idx = h & d->ht[table].sizemask;
		// 取桶上的链表（dictEntry）头结点
        he = d->ht[table].table[idx];
        prevHe = NULL;
        while(he) {
			// 如果找到目标节点
            if (key==he->key || dictCompareKeys(d, key, he->key)) {
                /* Unlink the element from the list */
				// 将该节点从链表删除
                if (prevHe)
                    prevHe->next = he->next;
                else
                    d->ht[table].table[idx] = he->next;
				// nofree == 0则调用键和值的释放函数，并释放节点本身
                if (!nofree) {
                    dictFreeKey(d, he);
                    dictFreeVal(d, he);
                    zfree(he);
                }
				// 更新已使用节点数量
                d->ht[table].used--;
				// 返回删除的节点（内存已释放，he为原dictEntry的地址，非空）
                return he;
            }
            prevHe = he;
            he = he->next;
        }
		// 如果字典不是正在rehash，退出；否则需要再遍历ht[1]
        if (!dictIsRehashing(d)) break;
    }
	// 没找到，返回NULL
    return NULL; /* not found */
}

/* Remove an element, returning DICT_OK on success or DICT_ERR if the
 * element was not found. */
/* 删除指定key对应节点，回收内存
 * 成功 -- 返回DICT_OK
 * 失败 -- 返回DICT_ERR
 */
int dictDelete(dict *ht, const void *key) {
    return dictGenericDelete(ht,key,0) ? DICT_OK : DICT_ERR;
}

/* Remove an element from the table, but without actually releasing
 * the key, value and dictionary entry. The dictionary entry is returned
 * if the element was found (and unlinked from the table), and the user
 * should later call `dictFreeUnlinkedEntry()` with it in order to release it.
 * Otherwise if the key is not found, NULL is returned.
 *
 * This function is useful when we want to remove something from the hash
 * table but want to use its value before actually deleting the entry.
 * Without this function the pattern would require two lookups:
 *
 *  entry = dictFind(...);
 *  // Do something with entry
 *  dictDelete(dictionary,entry);
 *
 * Thanks to this function it is possible to avoid this, and use
 * instead:
 *
 * entry = dictUnlink(dictionary,entry);
 * // Do something with entry
 * dictFreeUnlinkedEntry(entry); // <- This does not need to lookup again.
 */
/* 从字典中移除某个节点，但不删除（回收内存）
 * 返回值为该节点
 */
dictEntry *dictUnlink(dict *ht, const void *key) {
    return dictGenericDelete(ht,key,1);
}

/* You need to call this function to really free the entry after a call
 * to dictUnlink(). It's safe to call this function with 'he' = NULL. */
/* 回收节点的内存 */
void dictFreeUnlinkedEntry(dict *d, dictEntry *he) {
	// 节点为空，直接返回
    if (he == NULL) return;
	// 节点不为空，回收节点的key，value，内存
    dictFreeKey(d, he);
    dictFreeVal(d, he);
    zfree(he);
}

/* Destroy an entire dictionary */
/* 清空字典 */
int _dictClear(dict *d, dictht *ht, void(callback)(void *)) {
    unsigned long i;

    /* Free all the elements */
	// 如果hash表的hash数组已经初始化并且hash表中有元素，开始遍历
    for (i = 0; i < ht->size && ht->used > 0; i++) {
        dictEntry *he, *nextHe;

		// 如果有传callback函数, 则每清空65535个槽, 则执行回调方法一次
		// 为什么要这么处理这个回调呢 ? 主要为了避免hash表太大, 一直删除会阻塞, 通过回调方法, 删除阻塞过程中能够处理新的请求
	    // https://www.modb.pro/db/72930
        if (callback && (i & 65535) == 0) callback(d->privdata);

		// 桶内没有节点，则不处理，遍历下一个桶
        if ((he = ht->table[i]) == NULL) continue;
		// 桶内存在节点
        while(he) {
            nextHe = he->next;
			// 清空当前节点
            dictFreeKey(d, he);
            dictFreeVal(d, he);
            zfree(he);
			// 更新已使用节点
            ht->used--;
			// 获取下一个节点
            he = nextHe;
        }
    }
    /* Free the table and the allocated cache structure */
	// 释放hash表
    zfree(ht->table);
    /* Re-initialize the table */
	// 重新初始化字典
    _dictReset(ht);
    return DICT_OK; /* never fails */
}

/* Clear & Release the hash table */
/* 清空并释放字典 */
void dictRelease(dict *d)
{
	// 清空ht[0]和ht[1]
    _dictClear(d,&d->ht[0],NULL);
    _dictClear(d,&d->ht[1],NULL);
	// 回收字典内存
    zfree(d);
}

/* 返回节点中包含key的节点
 * 找到 -- 返回节点
 * 找不到 -- 返回NULL
 */
dictEntry *dictFind(dict *d, const void *key)
{
    dictEntry *he;
    uint64_t h, idx, table;

	// 字典为空，返回NULL
    if (dictSize(d) == 0) return NULL; /* dict is empty */

	// 字典正在rehash，打工
    if (dictIsRehashing(d)) _dictRehashStep(d);
    
	// 计算key对应的hash值
	h = dictHashKey(d, key);
    // 在字典中查找key
	for (table = 0; table <= 1; table++) {
		// 通过hash值计算桶索引
        idx = h & d->ht[table].sizemask;
        // 找到桶上首节点
		he = d->ht[table].table[idx];
        // 遍历桶上所有节点
		while(he) {
			// 找到key，则返回该节点
            if (key==he->key || dictCompareKeys(d, key, he->key))
                return he;
            he = he->next;
        }
		// 如果程序在ht[0]中没找到节点，并且字典正在rehash，取ht[1]继续找
        if (!dictIsRehashing(d)) return NULL;
    }
	// 运行至此，说明在ht[0]和ht[1]中都没找到包含key的节点，返回NULL
    return NULL;
}

/* 获取包含给定key的节点的值(val)
 * 节点不为空 -- 返回节点的值
 * 节点为空   -- NULL
 */
void *dictFetchValue(dict *d, const void *key) {
    dictEntry *he;

    he = dictFind(d,key);
    return he ? dictGetVal(he) : NULL;
}

/* A fingerprint is a 64 bit number that represents the state of the dictionary
 * at a given time, it's just a few dict properties xored together.
 * When an unsafe iterator is initialized, we get the dict fingerprint, and check
 * the fingerprint again when the iterator is released.
 * If the two fingerprints are different it means that the user of the iterator
 * performed forbidden operations against the dictionary while iterating. */
/* 计算字典指纹（用于校验），64bit */
long long dictFingerprint(dict *d) {
    long long integers[6], hash = 0;
    int j;

    integers[0] = (long) d->ht[0].table;
    integers[1] = d->ht[0].size;
    integers[2] = d->ht[0].used;
    integers[3] = (long) d->ht[1].table;
    integers[4] = d->ht[1].size;
    integers[5] = d->ht[1].used;

    /* We hash N integers by summing every successive integer with the integer
     * hashing of the previous sum. Basically:
     *
     * Result = hash(hash(hash(int1)+int2)+int3) ...
     *
     * This way the same set of integers in a different order will (likely) hash
     * to a different number. */
    for (j = 0; j < 6; j++) {
        hash += integers[j];
        /* For the hashing step we use Tomas Wang's 64 bit integer hash. */
        hash = (~hash) + (hash << 21); // hash = (hash << 21) - hash - 1;
        hash = hash ^ (hash >> 24);
        hash = (hash + (hash << 3)) + (hash << 8); // hash * 265
        hash = hash ^ (hash >> 14);
        hash = (hash + (hash << 2)) + (hash << 4); // hash * 21
        hash = hash ^ (hash >> 28);
        hash = hash + (hash << 31);
    }
    return hash;
}

/* 创建并返回给定字典的不安全迭代器 */
dictIterator *dictGetIterator(dict *d)
{
	// 分配迭代器内存
    dictIterator *iter = zmalloc(sizeof(*iter));

	// 迭代器初始化
    iter->d = d;
    iter->table = 0;
    iter->index = -1;
    iter->safe = 0;
    iter->entry = NULL;
    iter->nextEntry = NULL;
    return iter;
}

/* 创建并返回给定字典的安全迭代器*/
dictIterator *dictGetSafeIterator(dict *d) {
	// 获取迭代器
    dictIterator *i = dictGetIterator(d);

	// 设置安全迭代器标识
    i->safe = 1;
    return i;
}

/* 获取迭代器的下一个节点
 * 如果迭代完成，返回NULL
 */
dictEntry *dictNext(dictIterator *iter)
{
    while (1) {
		// 如果当前节点为空
        if (iter->entry == NULL) {
			// 获取要遍历的hash表
            dictht *ht = &iter->d->ht[iter->table];
			// 如果时初次迭代
            if (iter->index == -1 && iter->table == 0) {
				// 如果是安全迭代器，暂停rehash
                if (iter->safe)
                    dictPauseRehashing(iter->d);
				// 如果是不安全迭代器，则给字典一个fingerprint
                else
                    iter->fingerprint = dictFingerprint(iter->d);
            }
			// 迭代器索引+1
            iter->index++;

			// 如果迭代器索引>当前被迭代的hash表的大小，则说明已经迭代完毕
            if (iter->index >= (long) ht->size) {
				// 判断当前数组是否在rehash，并且迭代器当前还在遍历ht[0]
                if (dictIsRehashing(iter->d) && iter->table == 0) {
					// 标识迭代器在遍历下一个hash表ht[1]
                    iter->table++;
					// 重置hash表索引
                    iter->index = 0;
					// 获取第二个hash表ht[1]
                    ht = &iter->d->ht[1];
				// 如果字典没有在rehash，或者现在已经遍历完ht[1]，则退出
                } else {
                    break;
                }
            }
			// 走到这里说明还没有迭代完当前hash表ht
			// 更新节点指针，指向下一个桶的表头
            iter->entry = ht->table[iter->index];
        } else {
			// 执行到这里，说明iter不为空（即当前正在迭代某个桶中的链表节点）
			// 获取下一个节点
            iter->entry = iter->nextEntry;
        }
		// 如果当前节点不为空，那么记录该节点的下一个节点
		// 我们需要在此保存next，因为使用iterator的人可能会删除这个返回的节点
        if (iter->entry) {
            /* We need to save the 'next' here, the iterator user
             * may delete the entry we are returning. */
            iter->nextEntry = iter->entry->next;
			// 返回当前节点
            return iter->entry;
        }
    }
	// 迭代完成（没有下一个节点），返回NULL
    return NULL;
}

/* 释放给定的字典迭代器 */
void dictReleaseIterator(dictIterator *iter)
{
    if (!(iter->index == -1 && iter->table == 0)) {
		// 释放安全迭代器
        if (iter->safe)
			// 重新开始该字典的rehash
            dictResumeRehashing(iter->d);
		// 释放不安全迭代器（验证指纹是否变化）
        else
            assert(iter->fingerprint == dictFingerprint(iter->d));
    }
    zfree(iter);
}

/* Return a random entry from the hash table. Useful to
 * implement randomized algorithms */
/* 随机返回字典中任意一个节点
 * 可用于实现随机化算法
 * 如果字典为空，返回NULL
 * 实现思路：先随机取一个非空的桶，再从桶中随机选一个节点
 *
 * 取每个节点的概率不均，因为每个桶的链表长度可能不同
 */
dictEntry *dictGetRandomKey(dict *d)
{
    dictEntry *he, *orighe;
    unsigned long h;
    int listlen, listele;

	// 字典为空
    if (dictSize(d) == 0) return NULL;
    
	// 正在rehash，帮忙打工
	if (dictIsRehashing(d)) _dictRehashStep(d);
    
	// 正在rehash，需要考虑ht[0]和ht[1]
	if (dictIsRehashing(d)) {
		// 随机循环获取，知道桶里有节点存在
        do {
            /* We are sure there are no elements in indexes from 0
             * to rehashidx-1 */
			// 0~rehashidx-1的桶中没有元素（已迁移至ht[1]）
			// 因此要保证桶的idx>=rehashidx
            h = d->rehashidx + (randomULong() % (dictSlots(d) - d->rehashidx));
			// 根据h的大小确定取ht[1]或ht[0]的桶
            he = (h >= d->ht[0].size) ? d->ht[1].table[h - d->ht[0].size] :
                                      d->ht[0].table[h];
			// 直到取到一个非空桶的首节点
        } while(he == NULL);
	// 不在rehash，只需考虑ht[0]
    } else {
        do {
			// 生成随机数，计算对应桶idx
            h = randomULong() & d->ht[0].sizemask;
            // 取该桶的首节点
			he = d->ht[0].table[h];
			// 直到取到一个非空桶的首节点
        } while(he == NULL);
    }

    /* Now we found a non empty bucket, but it is a linked
     * list and we need to get a random element from the list.
     * The only sane way to do so is counting the elements and
     * select a random index. */
	// 此处我们获得了一个非空桶的首节点，其后连接着一个链表，我们需要从中随机选一个元素
    listlen = 0;
    orighe = he;
    // 计算链表长度
	while(he) {
        he = he->next;
        listlen++;
    }
	// 随机选择节点在链表中的idx
    listele = random() % listlen;
    he = orighe;
	// 找到目标节点
    while(listele--) he = he->next;
    // 返回目标节点
	return he;
}

/* This function samples the dictionary to return a few keys from random
 * locations.
 *
 * It does not guarantee to return all the keys specified in 'count', nor
 * it does guarantee to return non-duplicated elements, however it will make
 * some effort to do both things.
 *
 * Returned pointers to hash table entries are stored into 'des' that
 * points to an array of dictEntry pointers. The array must have room for
 * at least 'count' elements, that is the argument we pass to the function
 * to tell how many random elements we need.
 *
 * The function returns the number of items stored into 'des', that may
 * be less than 'count' if the hash table has less than 'count' elements
 * inside, or if not enough elements were found in a reasonable amount of
 * steps.
 *
 * Note that this function is not suitable when you need a good distribution
 * of the returned items, but only when you need to "sample" a given number
 * of continuous elements to run some kind of algorithm or to produce
 * statistics. However the function is much faster than dictGetRandomKey()
 * at producing N elements. */
/* 随机采集指定数量的节点，返回数量可能达不到count的个数
 * 如果需要返回一些随机key，这个函数比dictGetRandomKey快很多
 *
 * 实现思路：随机选一个桶，从这个桶开始取节点，直至取够
 */
unsigned int dictGetSomeKeys(dict *d, dictEntry **des, unsigned int count) {
    unsigned long j; /* internal hash table id, 0 or 1. */
	// hash表的数量（是否正在rehash决定）
    unsigned long tables; /* 1 or 2 tables? */
    // stored表示已经采集的节点数，maxsizemask为hash表最大size的掩码
	unsigned long stored = 0, maxsizemask;
	// 采集次数上限
    unsigned long maxsteps;

	// 最多返回字典的总节点数
    if (dictSize(d) < count) count = dictSize(d);
    // 采集次数上限为元素个数的10倍
	maxsteps = count*10;

    /* Try to do a rehashing work proportional to 'count'. */
    // 根据返回key的个数，帮忙做incremental rehashing
	for (j = 0; j < count; j++) {
        if (dictIsRehashing(d))
            _dictRehashStep(d);
        else
            break;
    }

	// 如果字典正在rehash，需要遍历2个hash表，否则遍历一个
    tables = dictIsRehashing(d) ? 2 : 1;
    maxsizemask = d->ht[0].sizemask;
	// 取2个hash表（如果存在的话）更大的size对应的sizemask
    if (tables > 1 && maxsizemask < d->ht[1].sizemask)
        maxsizemask = d->ht[1].sizemask;

    /* Pick a random point inside the larger table. */
	// 随机选择hash桶
    unsigned long i = randomULong() & maxsizemask;
	// 统计目前为止遍历了空hash桶的数量
    unsigned long emptylen = 0; /* Continuous empty entries so far. */
	// 如果采样的key已足够或者达到采样上限，退出循环
    while(stored < count && maxsteps--) {
		// 遍历所有hash表
        for (j = 0; j < tables; j++) {
            /* Invariant of the dict.c rehashing: up to the indexes already
             * visited in ht[0] during the rehashing, there are no populated
             * buckets, so we can skip ht[0] for indexes between 0 and idx-1. */
			// 如果有2个表，且正在遍历第一个表，而随机选的桶在已经迁移的范围
            if (tables == 2 && j == 0 && i < (unsigned long) d->rehashidx) {
                /* Moreover, if we are currently out of range in the second
                 * table, there will be no elements in both tables up to
                 * the current rehashing index, so we jump if possible.
                 * (this happens when going from big to small table). */
				// 如果id > hash表1的长度，说明rehash将一个大的hash表变成一个小的hash表
				// 因此不能在ht[1]中取，要在ht[0]中取
				// 但是又因为ht[0]中 < rehashidx的桶已被迁移，所以此处选择第一个未被迁移的桶
                if (i >= d->ht[1].size)
                    i = d->rehashidx;
                else
					// 选择的桶已被迁移，取ht[1]中找
                    continue;
            }
			// 如果随机hash桶索引大于当前表数组长度，不处理
            if (i >= d->ht[j].size) continue; /* Out of range for this table. */
			// 找到桶的首节点
            dictEntry *he = d->ht[j].table[i];

            /* Count contiguous empty buckets, and jump to other
             * locations if they reach 'count' (with a minimum of 5). */
			// 如果首节点为空
            if (he == NULL) {
				// 统计空的hash桶
                emptylen++;
				// 如果空桶数量大于一定数量（max(count,4)），则重新生成随机的hash桶idx
                if (emptylen >= 5 && emptylen > count) {
                    i = randomULong() & maxsizemask;
                    emptylen = 0;
                }
			// 如果首节点不为空
            } else {
				// 重置空的hash桶统计
                emptylen = 0;
				// 遍历链表
                while (he) {
                    /* Collect all the elements of the buckets found non
                     * empty while iterating. */
					// 取链表中的节点，放到des中，直至取够count个节点或者取完当前链表
                    *des = he;
                    des++;
                    he = he->next;
                    stored++;
                    if (stored == count) return stored;
                }
            }
        }
		// 获取下一个hash桶的位置
        i = (i+1) & maxsizemask;
    }
    return stored;
}

/* This is like dictGetRandomKey() from the POV of the API, but will do more
 * work to ensure a better distribution of the returned element.
 *
 * This function improves the distribution because the dictGetRandomKey()
 * problem is that it selects a random bucket, then it selects a random
 * element from the chain in the bucket. However elements being in different
 * chain lengths will have different probabilities of being reported. With
 * this function instead what we do is to consider a "linear" range of the table
 * that may be constituted of N buckets with chains of different lengths
 * appearing one after the other. Then we report a random element in the range.
 * In this way we smooth away the problem of different chain lengths. */
#define GETFAIR_NUM_ENTRIES 15
/* 公平地取一个随机key
 * 比dictGetRandomKey公平一点，但也不是绝对公平
 */
dictEntry *dictGetFairRandomKey(dict *d) {
    dictEntry *entries[GETFAIR_NUM_ENTRIES];
	// 先从字典中选取给定数量的节点
    unsigned int count = dictGetSomeKeys(d,entries,GETFAIR_NUM_ENTRIES);
    /* Note that dictGetSomeKeys() may return zero elements in an unlucky
     * run() even if there are actually elements inside the hash table. So
     * when we get zero, we call the true dictGetRandomKey() that will always
     * yield the element if the hash table has at least one. */
	// 如果没有得到任何一个节点，那就用dictGetRandomKey返回一个节点
    if (count == 0) return dictGetRandomKey(d);
	// 如果获得了count个节点，随机选一个返回
    unsigned int idx = rand() % count;
    return entries[idx];
}

/* Function to reverse bits. Algorithm from:
 * http://graphics.stanford.edu/~seander/bithacks.html#ReverseParallel */
/* 二进制的逆序操作
 * 举个例子，假设要反转32位整数，该算法的思路是：
 * 先对调相邻的16位, 再对调相邻的8位, 再对调相邻的4位, 再对调相邻的2位, 最终完成反转.
 */
static unsigned long rev(unsigned long v) {
	// CHAR_BIT一般为8，sizeof(v)为v的字节数
    unsigned long s = CHAR_BIT * sizeof(v); // bit size; must be power of 2
    // 如果是32位的话就是32个1
	unsigned long mask = ~0UL; // unsigned long 0 取反
    while ((s >>= 1) > 0) {
		// mask << s，mask高s位为1，第s位为0
		// mask ^= (mask << s)，mask低s位为1，高s位为0
        mask ^= (mask << s);
		// (v >> s) & mask，高s位移动到低s位，高位为0
		// (v << s) & ~mask，低s位移动到高s位，低位为0
		// |将二者拼起来，导致的结果是高s位和低s位互换
        v = ((v >> s) & mask) | ((v << s) & ~mask);
    }
    return v;
}

/* dictScan() is used to iterate over the elements of a dictionary.
 *
 * Iterating works the following way:
 *
 * 1) Initially you call the function using a cursor (v) value of 0.
 * 2) The function performs one step of the iteration, and returns the
 *    new cursor value you must use in the next call.
 * 3) When the returned cursor is 0, the iteration is complete.
 *
 * The function guarantees all elements present in the
 * dictionary get returned between the start and end of the iteration.
 * However it is possible some elements get returned multiple times.
 *
 * For every element returned, the callback argument 'fn' is
 * called with 'privdata' as first argument and the dictionary entry
 * 'de' as second argument.
 *
 * HOW IT WORKS.
 *
 * The iteration algorithm was designed by Pieter Noordhuis.
 * The main idea is to increment a cursor starting from the higher order
 * bits. That is, instead of incrementing the cursor normally, the bits
 * of the cursor are reversed, then the cursor is incremented, and finally
 * the bits are reversed again.
 *
 * This strategy is needed because the hash table may be resized between
 * iteration calls.
 *
 * dict.c hash tables are always power of two in size, and they
 * use chaining, so the position of an element in a given table is given
 * by computing the bitwise AND between Hash(key) and SIZE-1
 * (where SIZE-1 is always the mask that is equivalent to taking the rest
 *  of the division between the Hash of the key and SIZE).
 *
 * For example if the current hash table size is 16, the mask is
 * (in binary) 1111. The position of a key in the hash table will always be
 * the last four bits of the hash output, and so forth.
 *
 * WHAT HAPPENS IF THE TABLE CHANGES IN SIZE?
 *
 * If the hash table grows, elements can go anywhere in one multiple of
 * the old bucket: for example let's say we already iterated with
 * a 4 bit cursor 1100 (the mask is 1111 because hash table size = 16).
 *
 * If the hash table will be resized to 64 elements, then the new mask will
 * be 111111. The new buckets you obtain by substituting in ??1100
 * with either 0 or 1 can be targeted only by keys we already visited
 * when scanning the bucket 1100 in the smaller hash table.
 *
 * By iterating the higher bits first, because of the inverted counter, the
 * cursor does not need to restart if the table size gets bigger. It will
 * continue iterating using cursors without '1100' at the end, and also
 * without any other combination of the final 4 bits already explored.
 *
 * Similarly when the table size shrinks over time, for example going from
 * 16 to 8, if a combination of the lower three bits (the mask for size 8
 * is 111) were already completely explored, it would not be visited again
 * because we are sure we tried, for example, both 0111 and 1111 (all the
 * variations of the higher bit) so we don't need to test it again.
 *
 * WAIT... YOU HAVE *TWO* TABLES DURING REHASHING!
 *
 * Yes, this is true, but we always iterate the smaller table first, then
 * we test all the expansions of the current cursor into the larger
 * table. For example if the current cursor is 101 and we also have a
 * larger table of size 16, we also test (0)101 and (1)101 inside the larger
 * table. This reduces the problem back to having only one table, where
 * the larger one, if it exists, is just an expansion of the smaller one.
 *
 * LIMITATIONS
 *
 * This iterator is completely stateless, and this is a huge advantage,
 * including no additional memory used.
 *
 * The disadvantages resulting from this design are:
 *
 * 1) It is possible we return elements more than once. However this is usually
 *    easy to deal with in the application level.
 * 2) The iterator must return multiple elements per call, as it needs to always
 *    return all the keys chained in a given bucket, and all the expansions, so
 *    we are sure we don't miss keys moving during rehashing.
 * 3) The reverse cursor is somewhat hard to understand at first, but this
 *    comment is supposed to help.
 */
/* 字典扫描
 * 作用：迭代给定字典的元素
 *
 * 迭代进行逻辑：
 * 1. 初始情况：用0作为cursor（v）调用函数
 * 2. 函数执行一步迭代操作，并返回下次迭代使用的新cursor
 * 3. 当函数返回cursor为0时，迭代完成
 *
 * 该函数保证在迭代从开始到结束期间，一直存在于字典的元素会被迭代到
 * 但是存在一个元素被返回多次的可能
 *
 * 每当一个元素被返回时，dictScanFunc *fn会被调用
 * fn函数的第一个参数时privdata，第二个参数时字典节点
 *
 * 工作原理：
 * 在二进制高位上对cursor进行加法计算
 * （将cursor的二进制位反转，然后对反转后的值进行加法计算，最后对加法的结果翻转）
 *
 * 采用这个策略是因为调用迭代的时候，hash表可能正在调整大小。
 *
 * dict.c的hash表大小总为2^n，并且使用链表，所以在一个给定表中一个元素的位置时通过计算key的hash值和size-1的按位与操作获得。
 * 即 key的hash值 & size-1 = key的hash值 % size
 *
 * 如果hash表大小发生了变化会如何？
 * 1. 如果hash表变大
 * 元素会分散到原来桶倍数的地方，比如我们迭代一个4bit的cursor 1100（hash表大小16，对应掩码1111）
 * 如果hash表扩容到64，新的掩码为63（111111），要获取新的桶用什么代替原来的1100？即??1100
 * 其中，?表示0或1，由我们已经访问过在小的hash表中的桶位置1100决定
 *
 * 通过有限迭代高位的比特位，因为翻转的计数，当hash表变大时，cursor不需要重新开始，可以继续迭代cursor，
 * 而不需要访问1100结尾和其他已经访问过的最后4位bit组合的位置
 * 
 * 举个例子，如果从高位开始迭代，1100变成了0011，扩展了1位就是00011。
 * 因为初始化的时候是从0开始，已经访问过0000,1000,0100,1100,0010,1010,0110,1110,0001,1001,0101,1101,0011（当前位置）
 * 接下来需要访问1011,0111,1111
 * 如果扩展了，那么我们将要访问01100，因为是从高位开始计数，对于访问过的元素，最高位添加0，全部小于00011，
 * 所以之前访问的位置无需再次访问，接下来要访问的是00011,10011,01011,11011,00111,10111,01111,11111,000000
 *
 * 2. 如果hash表缩小
 * 举个例子，hash表大小从16到8，如果一个低3bits的组合已经被访问过（size = 8，掩码111），那么它将不会再被访问
 * 举例：0111和1111只有最高位不一样，所以我们不需要再次访问它
 * 
 * 具体说明：
 * 如果从高位开始迭代，1100变为0011，如果收缩了，会变成011
 * 初始化从0开始，0000,1000,0100,1100,0010,1010,0110,1110,0001,1001,0101,1101,0011（当前位置），接下来需要访问1011,0111,1111
 * 如果表缩小了，那么即将访问的是011,111,0（不会出现重复元素）
 * 如果表缩小前，访问到1011，则需要访问011，但0011节点已经访问过，所以会重复访问011
 *
 * 迁移过程有2张表怎么办？
 * 从小表开始迭代，通过当前cursor扩展到大的表。
 * 举例：当前cursor为101，然后存在大小为16的表，在大表中访问(0)101和(1)101
 *
 * Limitations：
 * 这个迭代器时没有状态的，因此没有额外的内存消耗
 * 设计的缺陷：
 * 1. 可能返回重复元素（可在应用层解决）
 * 2. 为了不错过任何元素，迭代器需要返回给定桶上的所有键，以及因为扩展hash表而产生的新表，
 *    所以迭代器必须在一次迭代中返回多个元素
 * 3. 对游标进行翻转的原因看起来不好理解
 */
unsigned long dictScan(dict *d,
                       unsigned long v,
                       dictScanFunction *fn,
                       dictScanBucketFunction* bucketfn,
                       void *privdata)
{
    dictht *t0, *t1;
    const dictEntry *de, *next;
    unsigned long m0, m1;

	// 字典为空，不处理，返回0
    if (dictSize(d) == 0) return 0;

    /* This is needed in case the scan callback tries to do dictFind or alike. */
	// 如果正在rehash，暂停rehash状态+1
    dictPauseRehashing(d);

	// 如果字典没有在rehash
    if (!dictIsRehashing(d)) {
		// 获取ht[0]的地址
        t0 = &(d->ht[0]);
		// 获取ht[0]的size掩码
        m0 = t0->sizemask;

        /* Emit entries at cursor */
		// 如果桶扫描函数bucketfn存在，则用bucketfnchuli要获取的桶
        if (bucketfn) bucketfn(privdata, &t0->table[v & m0]);
		// 获取桶上首节点
        de = t0->table[v & m0];
		// 如果节点存在，遍历链表上的节点，并用fn函数处理每一个节点
        while (de) {
            next = de->next;
            fn(privdata, de);
            de = next;
        }

        /* Set unmasked bits so incrementing the reversed cursor
         * operates on the masked bits */
		// 假设hash表size为8，掩码m0为29位0，3位1，即...000111
		// ~m0 = ...111000
		// v |= ~m0，即保留低位的数据，即高29位为1，低3位为原有v的实际数据xxx
		// 假设v=3ul，则经过算v |= ~m0后，v=111...111xxx(111...111011)
        v |= ~m0;

        /* Increment the reverse cursor */
		// 翻转cursor，v = xxx111...111(110111...111)
        v = rev(v);
		// xx(x+1)000...000(111000...000)
        v++;
		// 再次翻转回原来的数据(000...000111)，即011->111
        v = rev(v);
	// 如果字典正在rehash
    } else {
		// 取出2个hash表的地址
        t0 = &d->ht[0];
        t1 = &d->ht[1];

        /* Make sure t0 is the smaller and t1 is the bigger table */
        // 保证t0比t1小
		if (t0->size > t1->size) {
            t0 = &d->ht[1];
            t1 = &d->ht[0];
        }

		// 获取size掩码
        m0 = t0->sizemask;
        m1 = t1->sizemask;

        /* Emit entries at cursor */
		// 对表0元素进行处理，与非rehash的情况一致
        if (bucketfn) bucketfn(privdata, &t0->table[v & m0]);
        de = t0->table[v & m0];
        while (de) {
            next = de->next;
            fn(privdata, de);
            de = next;
        }

        /* Iterate over indices in larger table that are the expansion
         * of the index pointed to by the cursor in the smaller table */
		// 通过小表的cursor扩展大表的cursor
		// 因为m1>m0，所以小表中一个桶idx会扩展到大表中的(m1+1)/(m0+1)个桶
        do {
            /* Emit entries at cursor */
            if (bucketfn) bucketfn(privdata, &t1->table[v & m1]);
			// m1 > m0，此处获得的桶的值与小表一致
            de = t1->table[v & m1];
            while (de) {
                next = de->next;
                fn(privdata, de);
                de = next;
            }

            /* Increment the reverse cursor not covered by the smaller mask.*/
            v |= ~m1;
            v = rev(v);
            v++;
            v = rev(v);

            /* Continue while bits covered by mask difference is non-zero */
			// v的二进制表示中，若m1比m0高的若干位都为0，则退出循环
			// 举个例子，小表4位，大表6位，小表遍历到0100
			// 大表从000100开始遍历，100100 -> 010100 -> 110100 -> 001100
			// 最高的2位再次变为0，进位了，低4位也获得了下一个桶的位置（1100）
        } while (v & (m0 ^ m1));
    }

	// 重新开始rehash
    dictResumeRehashing(d);

    return v;
}

/* ------------------------- private functions ------------------------------ */

/* Because we may need to allocate huge memory chunk at once when dict
 * expands, we will check this allocation is allowed or not if the dict
 * type has expandAllowed member function. */
/* 在扩展字典的时候可能需要分配很多内存
 * 在字典类型包含expandAllowed的情况下，我们需要检查这个分配是否被允许
 */
static int dictTypeExpandAllowed(dict *d) {
	// 没有expandAllowed函数，返回1，表示允许
    if (d->type->expandAllowed == NULL) return 1;
	// 否则需要根据扩容后数组的内存和负载因子判断是否允许扩容
    return d->type->expandAllowed(
                    _dictNextPower(d->ht[0].used + 1) * sizeof(dictEntry*),
                    (double)d->ht[0].used / d->ht[0].size);
}

/* Expand the hash table if needed */
/* 在需要时扩展hash表 */
static int _dictExpandIfNeeded(dict *d)
{
    /* Incremental rehashing already in progress. Return. */
	// 正在rehash，返回DICT_OK
    if (dictIsRehashing(d)) return DICT_OK;

    /* If the hash table is empty expand it to the initial size. */
	// hash表为空，扩展到默认大小
    if (d->ht[0].size == 0) return dictExpand(d, DICT_HT_INITIAL_SIZE);

    /* If we reached the 1:1 ratio, and we are allowed to resize the hash
     * table (global setting) or we should avoid it but the ratio between
     * elements/buckets is over the "safe" threshold, we resize doubling
     * the number of buckets. */
	// 如果used/size>=1，expandAllowed允许扩容，且允许自动resize flag = 1或者used/size>=dict_force_resize_ratio，则扩展空间
    if (d->ht[0].used >= d->ht[0].size &&
        (dict_can_resize ||
         d->ht[0].used/d->ht[0].size > dict_force_resize_ratio) &&
        dictTypeExpandAllowed(d))
    {
        return dictExpand(d, d->ht[0].used + 1);
    }
    return DICT_OK;
}

/* Our hash table capability is a power of two */
/* 计算第一个大于等于size的2^n */
static unsigned long _dictNextPower(unsigned long size)
{
    unsigned long i = DICT_HT_INITIAL_SIZE;

    if (size >= LONG_MAX) return LONG_MAX + 1LU;
    while(1) {
        if (i >= size)
            return i;
        i *= 2;
    }
}

/* Returns the index of a free slot that can be populated with
 * a hash entry for the given 'key'.
 * If the key already exists, -1 is returned
 * and the optional output parameter may be filled.
 *
 * Note that if we are in the process of rehashing the hash table, the
 * index is always returned in the context of the second (new) hash table. */
/* 返回可以将key插入到hash表的索引位置
 * 如果key已存在，返回-1
 *
 * 如果字典正在rehash，那么总是返回1号hash表的索引
 * 因为在字典进行rehash时，新节点总是插入到1号hash表
 */
static long _dictKeyIndex(dict *d, const void *key, uint64_t hash, dictEntry **existing)
{
    unsigned long idx, table;
    dictEntry *he;
    if (existing) *existing = NULL;

    /* Expand the hash table if needed */
	// 如果需要可扩展hash表，则扩展hash表
    if (_dictExpandIfNeeded(d) == DICT_ERR)
        return -1;

    for (table = 0; table <= 1; table++) {
		// 计算索引值
        idx = hash & d->ht[table].sizemask;
        /* Search if this slot does not already contain the given key */
		// 检查key是否存在
        he = d->ht[table].table[idx];
        while(he) {
			// 找到key
            if (key==he->key || dictCompareKeys(d, key, he->key)) {
				// 把找到的节点通过*existing返回
                if (existing) *existing = he;
                return -1;
            }
			// 遍历桶内下一个节点
            he = he->next;
        }

		// 运行到此处，说明ht[0]所有节点不包含key
		// 因此检查是否在rehash，如果正在rehash，继续对ht[1]进行操作
        if (!dictIsRehashing(d)) break;
    }
    return idx;
}

/* 清空字典上的所有hash表，以及其中节点
 * 并重置字典属性
 */
void dictEmpty(dict *d, void(callback)(void*)) {
    _dictClear(d,&d->ht[0],callback);
    _dictClear(d,&d->ht[1],callback);
    d->rehashidx = -1;
    d->pauserehash = 0;
}

/* 开启rehash */
void dictEnableResize(void) {
    dict_can_resize = 1;
}

/* 关闭rehash */
void dictDisableResize(void) {
    dict_can_resize = 0;
}

/* 获取在字典d中对应于key的hash值 */
uint64_t dictGetHash(dict *d, const void *key) {
    return dictHashKey(d, key);
}

/* Finds the dictEntry reference by using pointer and pre-calculated hash.
 * oldkey is a dead pointer and should not be accessed.
 * the hash value should be provided using dictGetHash.
 * no string / key comparison is performed.
 * return value is the reference to the dictEntry if found, or NULL if not found. */
/* 根据指针和提前计算好的hash值寻找entry
 * oldkey是一个不能被修改的常量指针
 * hash值是通过dictGetHash计算得到的
 * 不需要比较string/key
 * 返回：
 * 找到   -- 返回dictEntry的引用
 * 没找到 -- NULL
 */
dictEntry **dictFindEntryRefByPtrAndHash(dict *d, const void *oldptr, uint64_t hash) {
    dictEntry *he, **heref;
    unsigned long idx, table;

	// 字典为空不需要查找
    if (dictSize(d) == 0) return NULL; /* dict is empty */
    for (table = 0; table <= 1; table++) {
		// 找桶
        idx = hash & d->ht[table].sizemask;
		// 找桶首节点的地址
        heref = &d->ht[table].table[idx];
		// 首节点
        he = *heref;
		// 找链表中的节点key的地址是否与oldptr相同
        while(he) {
            if (oldptr==he->key)
                return heref;
            heref = &he->next;
            he = *heref;
        }
		// 如果没有正在hash，不需要查找ht[1]
        if (!dictIsRehashing(d)) return NULL;
    }
    return NULL;
}

/* ------------------------------- Debugging ---------------------------------*/

#define DICT_STATS_VECTLEN 50
size_t _dictGetStatsHt(char *buf, size_t bufsize, dictht *ht, int tableid) {
    unsigned long i, slots = 0, chainlen, maxchainlen = 0;
    unsigned long totchainlen = 0;
    unsigned long clvector[DICT_STATS_VECTLEN];
    size_t l = 0;

    if (ht->used == 0) {
        return snprintf(buf,bufsize,
            "No stats available for empty dictionaries\n");
    }

    /* Compute stats. */
    for (i = 0; i < DICT_STATS_VECTLEN; i++) clvector[i] = 0;
    for (i = 0; i < ht->size; i++) {
        dictEntry *he;

        if (ht->table[i] == NULL) {
            clvector[0]++;
            continue;
        }
        slots++;
        /* For each hash entry on this slot... */
        chainlen = 0;
        he = ht->table[i];
        while(he) {
            chainlen++;
            he = he->next;
        }
        clvector[(chainlen < DICT_STATS_VECTLEN) ? chainlen : (DICT_STATS_VECTLEN-1)]++;
        if (chainlen > maxchainlen) maxchainlen = chainlen;
        totchainlen += chainlen;
    }

    /* Generate human readable stats. */
    l += snprintf(buf+l,bufsize-l,
        "Hash table %d stats (%s):\n"
        " table size: %lu\n"
        " number of elements: %lu\n"
        " different slots: %lu\n"
        " max chain length: %lu\n"
        " avg chain length (counted): %.02f\n"
        " avg chain length (computed): %.02f\n"
        " Chain length distribution:\n",
        tableid, (tableid == 0) ? "main hash table" : "rehashing target",
        ht->size, ht->used, slots, maxchainlen,
        (float)totchainlen/slots, (float)ht->used/slots);

    for (i = 0; i < DICT_STATS_VECTLEN-1; i++) {
        if (clvector[i] == 0) continue;
        if (l >= bufsize) break;
        l += snprintf(buf+l,bufsize-l,
            "   %s%ld: %ld (%.02f%%)\n",
            (i == DICT_STATS_VECTLEN-1)?">= ":"",
            i, clvector[i], ((float)clvector[i]/ht->size)*100);
    }

    /* Unlike snprintf(), return the number of characters actually written. */
    if (bufsize) buf[bufsize-1] = '\0';
    return strlen(buf);
}

void dictGetStats(char *buf, size_t bufsize, dict *d) {
    size_t l;
    char *orig_buf = buf;
    size_t orig_bufsize = bufsize;

    l = _dictGetStatsHt(buf,bufsize,&d->ht[0],0);
    buf += l;
    bufsize -= l;
    if (dictIsRehashing(d) && bufsize > 0) {
        _dictGetStatsHt(buf,bufsize,&d->ht[1],1);
    }
    /* Make sure there is a NULL term at the end. */
    if (orig_bufsize) orig_buf[orig_bufsize-1] = '\0';
}

/* ------------------------------- Benchmark ---------------------------------*/

#ifdef REDIS_TEST

uint64_t hashCallback(const void *key) {
    return dictGenHashFunction((unsigned char*)key, strlen((char*)key));
}

int compareCallback(void *privdata, const void *key1, const void *key2) {
    int l1,l2;
    DICT_NOTUSED(privdata);

    l1 = strlen((char*)key1);
    l2 = strlen((char*)key2);
    if (l1 != l2) return 0;
    return memcmp(key1, key2, l1) == 0;
}

void freeCallback(void *privdata, void *val) {
    DICT_NOTUSED(privdata);

    zfree(val);
}

char *stringFromLongLong(long long value) {
    char buf[32];
    int len;
    char *s;

    len = sprintf(buf,"%lld",value);
    s = zmalloc(len+1);
    memcpy(s, buf, len);
    s[len] = '\0';
    return s;
}

dictType BenchmarkDictType = {
    hashCallback,
    NULL,
    NULL,
    compareCallback,
    freeCallback,
    NULL,
    NULL
};

#define start_benchmark() start = timeInMilliseconds()
#define end_benchmark(msg) do { \
    elapsed = timeInMilliseconds()-start; \
    printf(msg ": %ld items in %lld ms\n", count, elapsed); \
} while(0)

/* ./redis-server test dict [<count> | --accurate] */
int dictTest(int argc, char **argv, int accurate) {
    long j;
    long long start, elapsed;
    dict *dict = dictCreate(&BenchmarkDictType,NULL);
    long count = 0;

    if (argc == 4) {
        if (accurate) {
            count = 5000000;
        } else {
            count = strtol(argv[3],NULL,10);
        }
    } else {
        count = 5000;
    }

    start_benchmark();
    for (j = 0; j < count; j++) {
        int retval = dictAdd(dict,stringFromLongLong(j),(void*)j);
        assert(retval == DICT_OK);
    }
    end_benchmark("Inserting");
    assert((long)dictSize(dict) == count);

    /* Wait for rehashing. */
    while (dictIsRehashing(dict)) {
        dictRehashMilliseconds(dict,100);
    }

    start_benchmark();
    for (j = 0; j < count; j++) {
        char *key = stringFromLongLong(j);
        dictEntry *de = dictFind(dict,key);
        assert(de != NULL);
        zfree(key);
    }
    end_benchmark("Linear access of existing elements");

    start_benchmark();
    for (j = 0; j < count; j++) {
        char *key = stringFromLongLong(j);
        dictEntry *de = dictFind(dict,key);
        assert(de != NULL);
        zfree(key);
    }
    end_benchmark("Linear access of existing elements (2nd round)");

    start_benchmark();
    for (j = 0; j < count; j++) {
        char *key = stringFromLongLong(rand() % count);
        dictEntry *de = dictFind(dict,key);
        assert(de != NULL);
        zfree(key);
    }
    end_benchmark("Random access of existing elements");

    start_benchmark();
    for (j = 0; j < count; j++) {
        dictEntry *de = dictGetRandomKey(dict);
        assert(de != NULL);
    }
    end_benchmark("Accessing random keys");

    start_benchmark();
    for (j = 0; j < count; j++) {
        char *key = stringFromLongLong(rand() % count);
        key[0] = 'X';
        dictEntry *de = dictFind(dict,key);
        assert(de == NULL);
        zfree(key);
    }
    end_benchmark("Accessing missing");

    start_benchmark();
    for (j = 0; j < count; j++) {
        char *key = stringFromLongLong(j);
        int retval = dictDelete(dict,key);
        assert(retval == DICT_OK);
        key[0] += 17; /* Change first number to letter. */
        retval = dictAdd(dict,key,(void*)j);
        assert(retval == DICT_OK);
    }
    end_benchmark("Removing and adding");
    dictRelease(dict);
    return 0;
}
#endif
