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

/* �ֵ����״̬
 * 0 -- �ɹ�
 * 1 -- ʧ��
 */
#define DICT_OK 0
#define DICT_ERR 1

/* Unused arguments generate annoying warnings... */
/* ���ֵ��˽�����ݲ�ʹ��ʱ�����ڱ��������warning�ĺ� */
#define DICT_NOTUSED(V) ((void) V)

/* hash��Ľڵ�
 * ����k-v�ԣ��Լ���һ���ڵ��ָ��
 * hash��ڵ�����һ����������*/
typedef struct dictEntry {
	// ��
    void *key;
	// ֵ
	// u�����г�Առ��ͬһ���ڴ棬ͬһʱ��ֻ�ܱ���һ����Ա��ֵ
	// �ڵ��ֵv����Ϊָ�룬uint64������int64������������
    union {
        void *val;
        uint64_t u64;
        int64_t s64;
        double d;
    } v;
	// ָ����һ��hash��ڵ�
    struct dictEntry *next;
} dictEntry;

/* �ֵ�����
 * �ṩ��غ���ָ�룬������չ
 * �൱�ڶ�̬��ʵ��
 */
typedef struct dictType {
	// ��ϣ�����������ϣֵ
    uint64_t (*hashFunction)(const void *key);
	// ����key�ĺ���
    void *(*keyDup)(void *privdata, const void *key);
	// ����val�ĺ���
    void *(*valDup)(void *privdata, const void *obj);
	// �Ƚ�key�ĺ���
    int (*keyCompare)(void *privdata, const void *key1, const void *key2);
	// key����������
    void (*keyDestructor)(void *privdata, void *key);
	// val����������
    void (*valDestructor)(void *privdata, void *obj);
	// �������ݺ�������ڴ�͸��������ж��Ƿ��������
    int (*expandAllowed)(size_t moreMem, double usedRatio);
} dictType;

/* This is our hash table structure. Every dictionary has two of this as we
 * implement incremental rehashing, for the old to the new table. */
/* hash��
 * ÿ���ֵ䶼ʹ��2����ϣ���Ӷ�ʵ��incremental rehash
 */
typedef struct dictht {
	// hash���ָ������,dictEntry*��ʾhash�ڵ��ָ�룬dictEntry**��ʾ�����׵�ַ
    dictEntry **table;
	// hash�����С��һ��Ϊ2^n�����������Ҫͨ��ȡģ����ȡhash���������ȡģ������&�������ܲ�һЩ��
    unsigned long size;
	// hash���鳤�����룬һ��sizemask = 2^n - 1
    unsigned long sizemask;
	// hash��k-v�Եĸ���
    unsigned long used;
} dictht;

/* �ֵ� */
typedef struct dict {
	// �����ض�����
    dictType *type;
	// ˽������
    void *privdata;
    // ��ϣ���ֵ��а���2��hash��
	// ���õ�hash��Ϊht[0]����rehashʱ����ʹ��ht[1]��incremental rehash
	dictht ht[2];
	// rehash��������rehash���ڽ���ʱ��rehashidx == -1
    long rehashidx; /* rehashing not in progress if rehashidx == -1 */
	// pauserehash > 0 ��ʾrehash��ͣ��= 0 ��ʾ����rehash
	// ���ã���Ϊ��ȫ��������ɨ�����ͬʱ���ڶ��������pauserehash��ֵ���� > 1
    int16_t pauserehash; /* If >0 rehashing is paused (<0 indicates coding error) */
} dict;

/* If safe is set to 1 this is a safe iterator, that means, you can call
 * dictAdd, dictFind, and other functions against the dictionary even while
 * iterating. Otherwise it is a non safe iterator, and only dictNext()
 * should be called while iterating. */
/* �ֵ������ */
typedef struct dictIterator {
	// ���������ֵ��ָ��
    dict *d;
	// ��������ǰָ��Ĺ�ϣ�������±�
    long index;
	// table -- ���ڵ�����hash������ht��������ֵΪ0��1
	// save  -- ��ʶ�õ������Ƿ�ȫ
    int table, safe;
	// entry     -- ��ǰ�ѷ��صĽڵ�
	// nextEntry -- ��һ���ڵ�
    dictEntry *entry, *nextEntry;
    /* unsafe iterator fingerprint for misuse detection. */
	// �ֵ䵱ǰ״̬��ǩ����hashֵ
    long long fingerprint;
} dictIterator;

typedef void (dictScanFunction)(void *privdata, const dictEntry *de);
typedef void (dictScanBucketFunction)(void *privdata, dictEntry **bucketref);

/* This is the initial size of every hash table */
/* ��ϣ���ʼ��С */
#define DICT_HT_INITIAL_SIZE     4

/* ------------------------------- Macros ------------------------------------*/
// �ͷŸ����ֵ�ڵ��ֵ
#define dictFreeVal(d, entry) \
    if ((d)->type->valDestructor) \
        (d)->type->valDestructor((d)->privdata, (entry)->v.val)

// ���ø����ֵ�ڵ��ֵ
#define dictSetVal(d, entry, _val_) do { \
    if ((d)->type->valDup) \
        (entry)->v.val = (d)->type->valDup((d)->privdata, _val_); \
    else \
        (entry)->v.val = (_val_); \
} while(0)

// ��һ���з���������Ϊ�ڵ��ֵ
#define dictSetSignedIntegerVal(entry, _val_) \
    do { (entry)->v.s64 = _val_; } while(0)

// ��һ���޷���������Ϊ�ڵ��ֵ
#define dictSetUnsignedIntegerVal(entry, _val_) \
    do { (entry)->v.u64 = _val_; } while(0)

// ��һ��double��Ϊ�ڵ��ֵ
#define dictSetDoubleVal(entry, _val_) \
    do { (entry)->v.d = _val_; } while(0)

// �ͷŸ����ֵ�ڵ��key
#define dictFreeKey(d, entry) \
    if ((d)->type->keyDestructor) \
        (d)->type->keyDestructor((d)->privdata, (entry)->key)

// ���ø����ֵ�ڵ��key
#define dictSetKey(d, entry, _key_) do { \
    if ((d)->type->keyDup) \
        (entry)->key = (d)->type->keyDup((d)->privdata, _key_); \
    else \
        (entry)->key = (_key_); \
} while(0)

// �Ƚ�2���ֵ��key
#define dictCompareKeys(d, key1, key2) \
    (((d)->type->keyCompare) ? \
        (d)->type->keyCompare((d)->privdata, key1, key2) : \
        (key1) == (key2))

// �������key�Ĺ�ϣֵ
#define dictHashKey(d, key) (d)->type->hashFunction(key)
// ���ظ���hash�ڵ��key
#define dictGetKey(he) ((he)->key)
// ���ظ���hash�ڵ��value(v.val)
#define dictGetVal(he) ((he)->v.val)
// ���ظ���hash�ڵ���з�������(v.s64)
#define dictGetSignedIntegerVal(he) ((he)->v.s64)
// ���ظ���hash�ڵ���޷�������(v.u64)
#define dictGetUnsignedIntegerVal(he) ((he)->v.u64)
// ���ظ���hash�ڵ��double(v.d)
#define dictGetDoubleVal(he) ((he)->v.d)
// ���ظ����ֵ�Ĵ�С
#define dictSlots(d) ((d)->ht[0].size+(d)->ht[1].size)
// �����ֵ����нڵ������
#define dictSize(d) ((d)->ht[0].used+(d)->ht[1].used)
// �鿴�ڵ��Ƿ�����rehash
#define dictIsRehashing(d) ((d)->rehashidx != -1)
// ��ͣrehash
#define dictPauseRehashing(d) (d)->pauserehash++
// ���¿�ʼrehash
#define dictResumeRehashing(d) (d)->pauserehash--

/* If our unsigned long type can store a 64 bit number, use a 64 bit PRNG. */
/* ���unsigned long���Դ�64λ��ʹ��64λα����������� */
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
