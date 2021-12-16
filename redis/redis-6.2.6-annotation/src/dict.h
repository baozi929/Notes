/* Hash Tables Implementation.
 *
 * This file implements in-memory hash tables with insert/del/replace/find/
 * get-random-element operations. Hash tables will auto-resize if needed
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

#ifndef __DICT_H
#define __DICT_H

#include "mt19937-64.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

/* 字典操作状态
 * 0 -- 成功
 * 1 -- 失败
 */
#define DICT_OK 0
#define DICT_ERR 1

/* Unused arguments generate annoying warnings... */
/* 在字典的私有数据不使用时，用于避免编译器warning的宏 */
#define DICT_NOTUSED(V) ((void) V)

/* hash表的节点
 * 包含k-v对，以及下一个节点的指针
 * hash表节点会组成一个单向链表*/
typedef struct dictEntry {
	// 键
    void *key;
	// 值
	// u的所有成员占用同一段内存，同一时刻只能保存一个成员的值
	// 节点的值v，可为指针，uint64整数，int64整数，浮点数
    union {
        void *val;
        uint64_t u64;
        int64_t s64;
        double d;
    } v;
	// 指向下一个hash表节点
    struct dictEntry *next;
} dictEntry;

/* 字典类型
 * 提供相关函数指针，用于扩展
 * 相当于多态的实现
 */
typedef struct dictType {
	// 哈希函数，计算哈希值
    uint64_t (*hashFunction)(const void *key);
	// 复制key的函数
    void *(*keyDup)(void *privdata, const void *key);
	// 复制val的函数
    void *(*valDup)(void *privdata, const void *obj);
	// 比较key的函数
    int (*keyCompare)(void *privdata, const void *key1, const void *key2);
	// key的析构函数
    void (*keyDestructor)(void *privdata, void *key);
	// val的析构函数
    void (*valDestructor)(void *privdata, void *obj);
	// 根据扩容后数组的内存和负载因子判断是否可以扩容
    int (*expandAllowed)(size_t moreMem, double usedRatio);
} dictType;

/* This is our hash table structure. Every dictionary has two of this as we
 * implement incremental rehashing, for the old to the new table. */
/* hash表
 * 每个字典都使用2个哈希表，从而实现incremental rehash
 */
typedef struct dictht {
	// hash表的指针数组,dictEntry*表示hash节点的指针，dictEntry**表示数组首地址
    dictEntry **table;
	// hash数组大小，一般为2^n（如果不是需要通过取模来获取hash表槽索引，取模操作比&操作性能差一些）
    unsigned long size;
	// hash数组长度掩码，一般sizemask = 2^n - 1
    unsigned long sizemask;
	// hash表k-v对的个数
    unsigned long used;
} dictht;

/* 字典 */
typedef struct dict {
	// 类型特定函数
    dictType *type;
	// 私有数据
    void *privdata;
    // 哈希表，字典中包含2个hash表
	// 常用的hash表为ht[0]，当rehash时，会使用ht[1]做incremental rehash
	dictht ht[2];
	// rehash索引，当rehash不在进行时，rehashidx == -1
    long rehashidx; /* rehashing not in progress if rehashidx == -1 */
	// pauserehash > 0 表示rehash暂停；= 0 表示可以rehash
	// 作用：因为安全迭代器和扫描可能同时存在多个，导致pauserehash的值可能 > 1
    int16_t pauserehash; /* If >0 rehashing is paused (<0 indicates coding error) */
} dict;

/* If safe is set to 1 this is a safe iterator, that means, you can call
 * dictAdd, dictFind, and other functions against the dictionary even while
 * iterating. Otherwise it is a non safe iterator, and only dictNext()
 * should be called while iterating. */
/* 字典迭代器 */
typedef struct dictIterator {
	// 被迭代的字典的指针
    dict *d;
	// 迭代器当前指向的哈希表索引下标
    long index;
	// table -- 正在迭代的hash表数组ht的索引，值为0或1
	// save  -- 标识该迭代器是否安全
    int table, safe;
	// entry     -- 当前已返回的节点
	// nextEntry -- 下一个节点
    dictEntry *entry, *nextEntry;
    /* unsafe iterator fingerprint for misuse detection. */
	// 字典当前状态的签名，hash值
    long long fingerprint;
} dictIterator;

typedef void (dictScanFunction)(void *privdata, const dictEntry *de);
typedef void (dictScanBucketFunction)(void *privdata, dictEntry **bucketref);

/* This is the initial size of every hash table */
/* 哈希表初始大小 */
#define DICT_HT_INITIAL_SIZE     4

/* ------------------------------- Macros ------------------------------------*/
// 释放给定字典节点的值
#define dictFreeVal(d, entry) \
    if ((d)->type->valDestructor) \
        (d)->type->valDestructor((d)->privdata, (entry)->v.val)

// 设置给定字典节点的值
#define dictSetVal(d, entry, _val_) do { \
    if ((d)->type->valDup) \
        (entry)->v.val = (d)->type->valDup((d)->privdata, _val_); \
    else \
        (entry)->v.val = (_val_); \
} while(0)

// 将一个有符号整数设为节点的值
#define dictSetSignedIntegerVal(entry, _val_) \
    do { (entry)->v.s64 = _val_; } while(0)

// 将一个无符号整数设为节点的值
#define dictSetUnsignedIntegerVal(entry, _val_) \
    do { (entry)->v.u64 = _val_; } while(0)

// 将一个double设为节点的值
#define dictSetDoubleVal(entry, _val_) \
    do { (entry)->v.d = _val_; } while(0)

// 释放给定字典节点的key
#define dictFreeKey(d, entry) \
    if ((d)->type->keyDestructor) \
        (d)->type->keyDestructor((d)->privdata, (entry)->key)

// 设置给定字典节点的key
#define dictSetKey(d, entry, _key_) do { \
    if ((d)->type->keyDup) \
        (entry)->key = (d)->type->keyDup((d)->privdata, _key_); \
    else \
        (entry)->key = (_key_); \
} while(0)

// 比较2个字典的key
#define dictCompareKeys(d, key1, key2) \
    (((d)->type->keyCompare) ? \
        (d)->type->keyCompare((d)->privdata, key1, key2) : \
        (key1) == (key2))

// 计算给定key的哈希值
#define dictHashKey(d, key) (d)->type->hashFunction(key)
// 返回给定hash节点的key
#define dictGetKey(he) ((he)->key)
// 返回给定hash节点的value(v.val)
#define dictGetVal(he) ((he)->v.val)
// 返回给定hash节点的有符号整数(v.s64)
#define dictGetSignedIntegerVal(he) ((he)->v.s64)
// 返回给定hash节点的无符号整数(v.u64)
#define dictGetUnsignedIntegerVal(he) ((he)->v.u64)
// 返回给定hash节点的double(v.d)
#define dictGetDoubleVal(he) ((he)->v.d)
// 返回给定字典的大小
#define dictSlots(d) ((d)->ht[0].size+(d)->ht[1].size)
// 返回字典已有节点的数量
#define dictSize(d) ((d)->ht[0].used+(d)->ht[1].used)
// 查看节点是否正在rehash
#define dictIsRehashing(d) ((d)->rehashidx != -1)
// 暂停rehash
#define dictPauseRehashing(d) (d)->pauserehash++
// 重新开始rehash
#define dictResumeRehashing(d) (d)->pauserehash--

/* If our unsigned long type can store a 64 bit number, use a 64 bit PRNG. */
/* 如果unsigned long可以存64位，使用64位伪随机数生成器 */
#if ULONG_MAX >= 0xffffffffffffffff
#define randomULong() ((unsigned long) genrand64_int64())
#else
#define randomULong() random()
#endif

/* API */
dict *dictCreate(dictType *type, void *privDataPtr);
int dictExpand(dict *d, unsigned long size);
int dictTryExpand(dict *d, unsigned long size);
int dictAdd(dict *d, void *key, void *val);
dictEntry *dictAddRaw(dict *d, void *key, dictEntry **existing);
dictEntry *dictAddOrFind(dict *d, void *key);
int dictReplace(dict *d, void *key, void *val);
int dictDelete(dict *d, const void *key);
dictEntry *dictUnlink(dict *ht, const void *key);
void dictFreeUnlinkedEntry(dict *d, dictEntry *he);
void dictRelease(dict *d);
dictEntry * dictFind(dict *d, const void *key);
void *dictFetchValue(dict *d, const void *key);
int dictResize(dict *d);
dictIterator *dictGetIterator(dict *d);
dictIterator *dictGetSafeIterator(dict *d);
dictEntry *dictNext(dictIterator *iter);
void dictReleaseIterator(dictIterator *iter);
dictEntry *dictGetRandomKey(dict *d);
dictEntry *dictGetFairRandomKey(dict *d);
unsigned int dictGetSomeKeys(dict *d, dictEntry **des, unsigned int count);
void dictGetStats(char *buf, size_t bufsize, dict *d);
uint64_t dictGenHashFunction(const void *key, int len);
uint64_t dictGenCaseHashFunction(const unsigned char *buf, int len);
void dictEmpty(dict *d, void(callback)(void*));
void dictEnableResize(void);
void dictDisableResize(void);
int dictRehash(dict *d, int n);
int dictRehashMilliseconds(dict *d, int ms);
void dictSetHashFunctionSeed(uint8_t *seed);
uint8_t *dictGetHashFunctionSeed(void);
unsigned long dictScan(dict *d, unsigned long v, dictScanFunction *fn, dictScanBucketFunction *bucketfn, void *privdata);
uint64_t dictGetHash(dict *d, const void *key);
dictEntry **dictFindEntryRefByPtrAndHash(dict *d, const void *oldptr, uint64_t hash);

/* Hash table types */
extern dictType dictTypeHeapStringCopyKey;
extern dictType dictTypeHeapStrings;
extern dictType dictTypeHeapStringCopyKeyValue;

#ifdef REDIS_TEST
int dictTest(int argc, char *argv[], int accurate);
#endif

#endif /* __DICT_H */
