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
 * ͨ��dictEnableResize() / dictDisableResize()�����ֶ�����/��ֹhash�����rehash
 *
 * Note that even when dict_can_resize is set to 0, not all resizes are
 * prevented: a hash table is still allowed to grow if the ratio between
 * the number of elements and the buckets > dict_force_resize_ratio.
 * 
 * dictDisableResize()���ܽ�ֹ���е�rehash
 * �����ʹ�ýڵ�����/bucket����>dict_force_resize_ratio�����ǿ��rehash
 */
// �ֵ��Ƿ�ʹ��rehash��1��ʾenable��0��ʾdisable
static int dict_can_resize = 1;
// ǿ��rehash�ı���
static unsigned int dict_force_resize_ratio = 5;

/* -------------------------- private prototypes ---------------------------- */

static int _dictExpandIfNeeded(dict *ht);
static unsigned long _dictNextPower(unsigned long size);
static long _dictKeyIndex(dict *ht, const void *key, uint64_t hash, dictEntry **existing);
static int _dictInit(dict *ht, dictType *type, void *privDataPtr);

/* -------------------------- hash functions -------------------------------- */

static uint8_t dict_hash_function_seed[16];

// ����hash��������
void dictSetHashFunctionSeed(uint8_t *seed) {
    memcpy(dict_hash_function_seed,seed,sizeof(dict_hash_function_seed));
}

// ȡhash��������
uint8_t *dictGetHashFunctionSeed(void) {
    return dict_hash_function_seed;
}

/* The default hashing function uses SipHash implementation
 * in siphash.c. */
/* Ĭ��hash����ʹ��SipHash��siphash.c�� */
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
 * ����ע���Ѿ�������
 */
/* ����/��ʼ��hash��ĸ�����Ա */
static void _dictReset(dictht *ht)
{
    ht->table = NULL;
    ht->size = 0;
    ht->sizemask = 0;
    ht->used = 0;
}

/* Create a new hash table */
/* �½�hash�� */
dict *dictCreate(dictType *type,
        void *privDataPtr)
{
    dict *d = zmalloc(sizeof(*d));

	// ��ʼ���ֵ�
    _dictInit(d,type,privDataPtr);
    return d;
}

/* Initialize the hash table */
/* ��ʼ��hash�� */
int _dictInit(dict *d, dictType *type,
        void *privDataPtr)
{
	// ��ʼ��dict��2��hash��ĸ�����Ա
    _dictReset(&d->ht[0]);
    _dictReset(&d->ht[1]);

	// �����ֵ�����
    d->type = type;
	// ����˽������
    d->privdata = privDataPtr;
	// ����hash��rehash״̬��������rehashʱ��Ϊ-1
    d->rehashidx = -1;
    d->pauserehash = 0;
    return DICT_OK;
}

/* Resize the table to the minimal size that contains all the elements,
 * but with the invariant of a USED/BUCKETS ratio near to <= 1 */
/* �����ֵ��С��ʹ���ýڵ������ֵ��С֮��ı����ӽ�1:1 */
int dictResize(dict *d)
{
	// �ֶ�������С����
    unsigned long minimal;

	// ����ڲ�����resize��dict����rehash��ʱ����ã�����DICT_ERR
    if (!dict_can_resize || dictIsRehashing(d)) return DICT_ERR;

	// ��ȡhash��Ԫ������
    minimal = d->ht[0].used;
	// ���Ԫ������С����С��ʼ����С����ʹ��DICT_HT_INITIAL_SIZE
    if (minimal < DICT_HT_INITIAL_SIZE)
        minimal = DICT_HT_INITIAL_SIZE;

	// ����minimal�����ֵ��С
    return dictExpand(d, minimal);
}

/* Expand or create the hash table,
 * when malloc_failed is non-NULL, it'll avoid panic if malloc fails (in which case it'll be set to 1).
 * Returns DICT_OK if expand was performed, and DICT_ERR if skipped. */
/* ����������չhash��
 * ��չ˼·��
 * ����Ҫ���size��������hash��������С��������hash��
 * ������hash��ŵ�ht[1]�У�������rehashidx=0�����ӵ�0��Ͱ��ʼ��rehashǨ�ƣ�
 * �˴��µ�hash������Ĵ�СΪ2^n������ʹ��&����hashȡģ����%ȡģ����������
 % ���hash��ht[0]û�г�ʼ������ֱ�ӳ�ʼ����ht[0]
 * T = O(N)
 */
int _dictExpand(dict *d, unsigned long size, int* malloc_failed)
{
    if (malloc_failed) *malloc_failed = 0;

    /* the size is invalid if it is smaller than the number of
     * elements already inside the hash table */
	// ����ֵ�����rehash���������нڵ���������������Ĵ�С���򷵻�DICT_ERR
    if (dictIsRehashing(d) || d->ht[0].used > size)
        return DICT_ERR;
	
    dictht n; /* the new hash table */
    // ����Ҫ���ݵ�������Ϊ����size�ĵ�һ��2^n
	unsigned long realsize = _dictNextPower(size);

    /* Detect overflows */
	// �������
    if (realsize < size || realsize * sizeof(dictEntry*) < realsize)
        return DICT_ERR;

    /* Rehashing to the same table size is not useful. */
	// ���Ҫ���ݵ�������ԭ��һ�����򷵻�DICT_ERR
    if (realsize == d->ht[0].size) return DICT_ERR;

    /* Allocate the new hash table and initialize all pointers to NULL */
	// �����µ������Ͷ�Ӧ����
    n.size = realsize;
    n.sizemask = realsize-1;
	// ѡ���ڷ���ʧ��ʱ������ֱ����ֹ����
    if (malloc_failed) {
		// ���Է���hash����
        n.table = ztrycalloc(realsize*sizeof(dictEntry*));
        // �����Ƿ����ʧ�ܣ�n.table == NULL������ʧ��
		*malloc_failed = n.table == NULL;
		// �������ʧ�ܣ��򷵻ش���
        if (*malloc_failed)
            return DICT_ERR;
    } else
		// ����hash���飬����ʧ�ܾ���ֹ����
        n.table = zcalloc(realsize*sizeof(dictEntry*));

	// �������ýڵ���Ϊ0
    n.used = 0;

    /* Is this the first initialization? If so it's not really a rehashing
     * we just set the first hash table so that it can accept keys. */
	// ����ֵ��hash��ΪNULL�����ʾ�����״γ�ʼ��
    if (d->ht[0].table == NULL) {
		// ��hash��ŵ�hash����ĵ�0λ��
        d->ht[0] = n;
        return DICT_OK;
    }

    /* Prepare a second hash table for incremental rehashing */
	// ��������״γ�ʼ������hash��ŵ�hash����ĵ�1λ������rehash
    d->ht[1] = n;
	// ���ô�hash���еĵ�0λ����Ǩ��
    d->rehashidx = 0;
    return DICT_OK;
}

/* return DICT_ERR if expand was not performed */
/* �����չʧ�ܣ���DICT_ERR */
int dictExpand(dict *d, unsigned long size) {
    return _dictExpand(d, size, NULL);
}

/* return DICT_ERR if expand failed due to memory allocation failure */
/* �����Ϊ�ڴ����ʧ�ܶ���չʧ�ܣ�����DICT_ERR */
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
/* �ֵ�rehash���̣�n��ʾǨ��Ͱ������ */
int dictRehash(dict *d, int n) {
	// ������Ͱ������
    int empty_visits = n*10; /* Max number of empty buckets to visit. */
    // ����ֵ䲻��rehash״̬������0
	if (!dictIsRehashing(d)) return 0;

	// ���Ǩ��Ͱ��������Ϊ0����hash��ûǨ����
    while(n-- && d->ht[0].used != 0) {
        dictEntry *de, *nextde;

        /* Note that rehashidx can't overflow as we are sure there are more
         * elements because ht[0].used != 0 */
		// ����hash��Ͱ�ĸ���>�Ѿ�rehash��Ͱ��������rehashidx <= ht[0].size - 1
        assert(d->ht[0].size > (unsigned long)d->rehashidx);
		// ���ҪǨ�Ƶ�ͰΪ�գ���׼��Ǩ����һ��Ͱ���ⲽȥ����Ͱ��
        while(d->ht[0].table[d->rehashidx] == NULL) {
            d->rehashidx++;
			// ����Ѿ������涨���������Ͱ�������򱾴�Ǩ�����
            if (--empty_visits == 0) return 1;
        }
		// ��ȡҪǨ�Ƶ�Ͱ��������׽ڵ�
        de = d->ht[0].table[d->rehashidx];
        /* Move all the keys in this bucket from the old to the new hash HT */
		// ����ҪǨ�Ƶ�Ͱ������
        while(de) {
            uint64_t h;

			// ��ȡ��һ���ڵ�
            nextde = de->next;
            /* Get the index in the new hash table */
			// ���㵱ǰ�ڵ�����hash���е�hashֵ��Ͱidx��
            h = dictHashKey(d, de->key) & d->ht[1].sizemask;
			// ����ǰ�ڵ�Ǩ�Ƶ���hash����
            de->next = d->ht[1].table[h];
            d->ht[1].table[h] = de;
			// ��hash����������
            d->ht[0].used--;
			// ��hash����������
            d->ht[1].used++;
			// ������һ���ڵ�
            de = nextde;
        }
		// ��վɽڵ��hashͰ
        d->ht[0].table[d->rehashidx] = NULL;
		// ������һ��ҪǨ�Ƶ�hashͰ
        d->rehashidx++;
    }

    /* Check if we already rehashed the whole table... */
	// �ж�hash���Ƿ�Ǩ�����
    if (d->ht[0].used == 0) {
		// Ǩ����ɣ����վ�hash��
        zfree(d->ht[0].table);
		// ����hash���滻��hash��
        d->ht[0] = d->ht[1];
		// ������ʱ��hash��
        _dictReset(&d->ht[1]);
		// ���rehashidx���ص�δrehash״̬
        d->rehashidx = -1;
        return 0;
    }

    /* More to rehash... */
	// 1��ʾδǨ�����
    return 1;
}

// ��ȡ��ǰ����ʱ��
long long timeInMilliseconds(void) {
    struct timeval tv;

    gettimeofday(&tv,NULL);
    return (((long long)tv.tv_sec)*1000)+(tv.tv_usec/1000);
}

/* Rehash in ms+"delta" milliseconds. The value of "delta" is larger 
 * than 0, and is smaller than 1 in most cases. The exact upper bound 
 * depends on the running time of dictRehash(d,100).*/
/* ָ��ʱ��rehash */
int dictRehashMilliseconds(dict *d, int ms) {
    // �����ͣrehash��ֱ�ӷ��أ�������
	if (d->pauserehash > 0) return 0;

	// ��ȡ��ǰʱ���
    long long start = timeInMilliseconds();
    // ͳ��rehashͰ�ĸ���
	int rehashes = 0;

	// rehash 100��Ͱ
    while(dictRehash(d,100)) {
        // ͳ��rehash��Ͱ�ĸ���
		rehashes += 100;
        // ��������ʱ�䣬�˳�rehash����
		if (timeInMilliseconds()-start > ms) break;
    }
	// ���ر���rehashͰ�ĸ���
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
/* ִ��һ��incremental rehash */
static void _dictRehashStep(dict *d) {
    // û����ͣrehash�����ִ��
	if (d->pauserehash == 0) dictRehash(d,1);
}

/* Add an element to the target hash table */
/* ��������k-v��
 * ��key�����ڵ�ʱ����Ӳ����Ż�ɹ�
 * ��ӳɹ� -- ����DICT_OK
 * ���ʧ�� -- ����DICT_ERR
 */
int dictAdd(dict *d, void *key, void *val)
{
	// ���һ���ڵ�
    dictEntry *entry = dictAddRaw(d,key,NULL);

	// ������ʧ�ܣ�key�Ѵ��ڣ�������DICT_ERR
    if (!entry) return DICT_ERR;
	// key�����ڣ���ӳɹ�����ֵ���õ�entry��
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
/* ���Խ������뵽�ֵ���
 * 
 * ���key�Ѿ��������ֵ��У��򷵻�NULL
 * ���key�����ڣ��򴴽��µ�hash�ڵ㣬���ڵ���key�������������ֵ䣬����ֵΪdictEntry����
 */
dictEntry *dictAddRaw(dict *d, void *key, dictEntry **existing)
{
    long index;
    dictEntry *entry;
    dictht *ht;

	// �����ǰ�ֵ�d����rehash����æ��rehash�Ĺ���
    if (dictIsRehashing(d)) _dictRehashStep(d);

    /* Get the index of the new element, or -1 if
     * the element already exists. */
	// ��ȡkey��Ӧ��Ͱidx�����key�Ѿ����ڣ�����NULL
    if ((index = _dictKeyIndex(d, key, dictHashKey(d,key), existing)) == -1)
        return NULL;

    /* Allocate the memory and store the new entry.
     * Insert the element in top, with the assumption that in a database
     * system it is more likely that recently added entries are accessed
     * more frequently. */
	// ��ȡhash���������rehash������hash��ht[1]�������þ�hash��ht[0]
    ht = dictIsRehashing(d) ? &d->ht[1] : &d->ht[0];
    // ����dictEntry�ڴ�
	entry = zmalloc(sizeof(*entry));
    // ���ڵ���ӵ���ǰͰ������ΪͰ�нڵ������ͷ
	entry->next = ht->table[index];
    ht->table[index] = entry;
	// ���½ڵ�����
    ht->used++;

    /* Set the hash entry fields. */
	// ��hashֵ���õ��ڵ㣨entry����
    dictSetKey(d, entry, key);
    return entry;
}

/* Add or Overwrite:
 * Add an element, discarding the old value if the key already exists.
 * Return 1 if the key was added from scratch, 0 if there was already an
 * element with such key and dictReplace() just performed a value update
 * operation. */
/* ����k-v */
int dictReplace(dict *d, void *key, void *val)
{
    dictEntry *entry, *existing, auxentry;

    /* Try to add the element. If the key
     * does not exists dictAdd will succeed. */
	// ����ֱ�ӽ�k-v����ӵ��ֵ�
	// �����key�����ڵĻ�����ӳɹ�
    entry = dictAddRaw(d,key,&existing);
    if (entry) {
		// ��ֵ���õ��ڵ���
        dictSetVal(d, entry, val);
        return 1;
    }

    /* Set the new value and free the old one. Note that it is important
     * to do that in this order, as the value may just be exactly the same
     * as the previous one. In this context, think to reference counting,
     * you want to increment (set), and then decrement (free), and not the
     * reverse. */
	// ǳ������auxentry�����ԭ��dictEntry��void * key��val��dictEntry* next
    auxentry = *existing;
	// �����µ�ֵ
    dictSetVal(d, existing, val);
	// �ͷžɵ�ֵ
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
/* ��ӽڵ㣬����Ѵ��ڣ���ֱ�ӷ����Ѵ��ڵĽڵ� */
dictEntry *dictAddOrFind(dict *d, void *key) {
    dictEntry *entry, *existing;
	// ���key�Ѿ��������ֵ��У�entry = NULL��existing�����ҵ���Ӧ��key��dictEntry
	// ���key�����ڣ��򴴽��µ�hash�ڵ㣬���ڵ���key�������������ֵ䣬����ֵΪdictEntry����
    entry = dictAddRaw(d,key,&existing);
    return entry ? entry : existing;
}

/* Search and remove an element. This is an helper function for
 * dictDelete() and dictUnlink(), please check the top comment
 * of those functions. */
/* ���Ҳ�ɾ����������key�Ľڵ� 
 * 
 * ����nofree�����Ƿ���ü���ֵ���ͷź���
 * 0 -- ����
 * 1 -- ������
 * 
 * �ҵ����ɹ�ɾ�����ر�ɾ���Ľڵ㣬û�ҵ��򷵻�NULL
 */
static dictEntry *dictGenericDelete(dict *d, const void *key, int nofree) {
    uint64_t h, idx;
    dictEntry *he, *prevHe;
    int table;

    if (d->ht[0].used == 0 && d->ht[1].used == 0) return NULL;

	// �������rehash����æ��һ��rehash����
    if (dictIsRehashing(d)) _dictRehashStep(d);

	// ����hashֵ
    h = dictHashKey(d, key);

	// ����hash��
    for (table = 0; table <= 1; table++) {
		// ����Ͱ����
        idx = h & d->ht[table].sizemask;
		// ȡͰ�ϵ�����dictEntry��ͷ���
        he = d->ht[table].table[idx];
        prevHe = NULL;
        while(he) {
			// ����ҵ�Ŀ��ڵ�
            if (key==he->key || dictCompareKeys(d, key, he->key)) {
                /* Unlink the element from the list */
				// ���ýڵ������ɾ��
                if (prevHe)
                    prevHe->next = he->next;
                else
                    d->ht[table].table[idx] = he->next;
				// nofree == 0����ü���ֵ���ͷź��������ͷŽڵ㱾��
                if (!nofree) {
                    dictFreeKey(d, he);
                    dictFreeVal(d, he);
                    zfree(he);
                }
				// ������ʹ�ýڵ�����
                d->ht[table].used--;
				// ����ɾ���Ľڵ㣨�ڴ����ͷţ�heΪԭdictEntry�ĵ�ַ���ǿգ�
                return he;
            }
            prevHe = he;
            he = he->next;
        }
		// ����ֵ䲻������rehash���˳���������Ҫ�ٱ���ht[1]
        if (!dictIsRehashing(d)) break;
    }
	// û�ҵ�������NULL
    return NULL; /* not found */
}

/* Remove an element, returning DICT_OK on success or DICT_ERR if the
 * element was not found. */
/* ɾ��ָ��key��Ӧ�ڵ㣬�����ڴ�
 * �ɹ� -- ����DICT_OK
 * ʧ�� -- ����DICT_ERR
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
/* ���ֵ����Ƴ�ĳ���ڵ㣬����ɾ���������ڴ棩
 * ����ֵΪ�ýڵ�
 */
dictEntry *dictUnlink(dict *ht, const void *key) {
    return dictGenericDelete(ht,key,1);
}

/* You need to call this function to really free the entry after a call
 * to dictUnlink(). It's safe to call this function with 'he' = NULL. */
/* ���սڵ���ڴ� */
void dictFreeUnlinkedEntry(dict *d, dictEntry *he) {
	// �ڵ�Ϊ�գ�ֱ�ӷ���
    if (he == NULL) return;
	// �ڵ㲻Ϊ�գ����սڵ��key��value���ڴ�
    dictFreeKey(d, he);
    dictFreeVal(d, he);
    zfree(he);
}

/* Destroy an entire dictionary */
/* ����ֵ� */
int _dictClear(dict *d, dictht *ht, void(callback)(void *)) {
    unsigned long i;

    /* Free all the elements */
	// ���hash���hash�����Ѿ���ʼ������hash������Ԫ�أ���ʼ����
    for (i = 0; i < ht->size && ht->used > 0; i++) {
        dictEntry *he, *nextHe;

		// ����д�callback����, ��ÿ���65535����, ��ִ�лص�����һ��
		// ΪʲôҪ��ô��������ص��� ? ��ҪΪ�˱���hash��̫��, һֱɾ��������, ͨ���ص�����, ɾ�������������ܹ������µ�����
	    // https://www.modb.pro/db/72930
        if (callback && (i & 65535) == 0) callback(d->privdata);

		// Ͱ��û�нڵ㣬�򲻴���������һ��Ͱ
        if ((he = ht->table[i]) == NULL) continue;
		// Ͱ�ڴ��ڽڵ�
        while(he) {
            nextHe = he->next;
			// ��յ�ǰ�ڵ�
            dictFreeKey(d, he);
            dictFreeVal(d, he);
            zfree(he);
			// ������ʹ�ýڵ�
            ht->used--;
			// ��ȡ��һ���ڵ�
            he = nextHe;
        }
    }
    /* Free the table and the allocated cache structure */
	// �ͷ�hash��
    zfree(ht->table);
    /* Re-initialize the table */
	// ���³�ʼ���ֵ�
    _dictReset(ht);
    return DICT_OK; /* never fails */
}

/* Clear & Release the hash table */
/* ��ղ��ͷ��ֵ� */
void dictRelease(dict *d)
{
	// ���ht[0]��ht[1]
    _dictClear(d,&d->ht[0],NULL);
    _dictClear(d,&d->ht[1],NULL);
	// �����ֵ��ڴ�
    zfree(d);
}

/* ���ؽڵ��а���key�Ľڵ�
 * �ҵ� -- ���ؽڵ�
 * �Ҳ��� -- ����NULL
 */
dictEntry *dictFind(dict *d, const void *key)
{
    dictEntry *he;
    uint64_t h, idx, table;

	// �ֵ�Ϊ�գ�����NULL
    if (dictSize(d) == 0) return NULL; /* dict is empty */

	// �ֵ�����rehash����
    if (dictIsRehashing(d)) _dictRehashStep(d);
    
	// ����key��Ӧ��hashֵ
	h = dictHashKey(d, key);
    // ���ֵ��в���key
	for (table = 0; table <= 1; table++) {
		// ͨ��hashֵ����Ͱ����
        idx = h & d->ht[table].sizemask;
        // �ҵ�Ͱ���׽ڵ�
		he = d->ht[table].table[idx];
        // ����Ͱ�����нڵ�
		while(he) {
			// �ҵ�key���򷵻ظýڵ�
            if (key==he->key || dictCompareKeys(d, key, he->key))
                return he;
            he = he->next;
        }
		// ���������ht[0]��û�ҵ��ڵ㣬�����ֵ�����rehash��ȡht[1]������
        if (!dictIsRehashing(d)) return NULL;
    }
	// �������ˣ�˵����ht[0]��ht[1]�ж�û�ҵ�����key�Ľڵ㣬����NULL
    return NULL;
}

/* ��ȡ��������key�Ľڵ��ֵ(val)
 * �ڵ㲻Ϊ�� -- ���ؽڵ��ֵ
 * �ڵ�Ϊ��   -- NULL
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
/* �����ֵ�ָ�ƣ�����У�飩��64bit */
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

/* ���������ظ����ֵ�Ĳ���ȫ������ */
dictIterator *dictGetIterator(dict *d)
{
	// ����������ڴ�
    dictIterator *iter = zmalloc(sizeof(*iter));

	// ��������ʼ��
    iter->d = d;
    iter->table = 0;
    iter->index = -1;
    iter->safe = 0;
    iter->entry = NULL;
    iter->nextEntry = NULL;
    return iter;
}

/* ���������ظ����ֵ�İ�ȫ������*/
dictIterator *dictGetSafeIterator(dict *d) {
	// ��ȡ������
    dictIterator *i = dictGetIterator(d);

	// ���ð�ȫ��������ʶ
    i->safe = 1;
    return i;
}

/* ��ȡ����������һ���ڵ�
 * ���������ɣ�����NULL
 */
dictEntry *dictNext(dictIterator *iter)
{
    while (1) {
		// �����ǰ�ڵ�Ϊ��
        if (iter->entry == NULL) {
			// ��ȡҪ������hash��
            dictht *ht = &iter->d->ht[iter->table];
			// ���ʱ���ε���
            if (iter->index == -1 && iter->table == 0) {
				// ����ǰ�ȫ����������ͣrehash
                if (iter->safe)
                    dictPauseRehashing(iter->d);
				// ����ǲ���ȫ������������ֵ�һ��fingerprint
                else
                    iter->fingerprint = dictFingerprint(iter->d);
            }
			// ����������+1
            iter->index++;

			// �������������>��ǰ��������hash��Ĵ�С����˵���Ѿ��������
            if (iter->index >= (long) ht->size) {
				// �жϵ�ǰ�����Ƿ���rehash�����ҵ�������ǰ���ڱ���ht[0]
                if (dictIsRehashing(iter->d) && iter->table == 0) {
					// ��ʶ�������ڱ�����һ��hash��ht[1]
                    iter->table++;
					// ����hash������
                    iter->index = 0;
					// ��ȡ�ڶ���hash��ht[1]
                    ht = &iter->d->ht[1];
				// ����ֵ�û����rehash�����������Ѿ�������ht[1]�����˳�
                } else {
                    break;
                }
            }
			// �ߵ�����˵����û�е����굱ǰhash��ht
			// ���½ڵ�ָ�룬ָ����һ��Ͱ�ı�ͷ
            iter->entry = ht->table[iter->index];
        } else {
			// ִ�е����˵��iter��Ϊ�գ�����ǰ���ڵ���ĳ��Ͱ�е�����ڵ㣩
			// ��ȡ��һ���ڵ�
            iter->entry = iter->nextEntry;
        }
		// �����ǰ�ڵ㲻Ϊ�գ���ô��¼�ýڵ����һ���ڵ�
		// ������Ҫ�ڴ˱���next����Ϊʹ��iterator���˿��ܻ�ɾ��������صĽڵ�
        if (iter->entry) {
            /* We need to save the 'next' here, the iterator user
             * may delete the entry we are returning. */
            iter->nextEntry = iter->entry->next;
			// ���ص�ǰ�ڵ�
            return iter->entry;
        }
    }
	// ������ɣ�û����һ���ڵ㣩������NULL
    return NULL;
}

/* �ͷŸ������ֵ������ */
void dictReleaseIterator(dictIterator *iter)
{
    if (!(iter->index == -1 && iter->table == 0)) {
		// �ͷŰ�ȫ������
        if (iter->safe)
			// ���¿�ʼ���ֵ��rehash
            dictResumeRehashing(iter->d);
		// �ͷŲ���ȫ����������ָ֤���Ƿ�仯��
        else
            assert(iter->fingerprint == dictFingerprint(iter->d));
    }
    zfree(iter);
}

/* Return a random entry from the hash table. Useful to
 * implement randomized algorithms */
/* ��������ֵ�������һ���ڵ�
 * ������ʵ��������㷨
 * ����ֵ�Ϊ�գ�����NULL
 * ʵ��˼·�������ȡһ���ǿյ�Ͱ���ٴ�Ͱ�����ѡһ���ڵ�
 *
 * ȡÿ���ڵ�ĸ��ʲ�������Ϊÿ��Ͱ�������ȿ��ܲ�ͬ
 */
dictEntry *dictGetRandomKey(dict *d)
{
    dictEntry *he, *orighe;
    unsigned long h;
    int listlen, listele;

	// �ֵ�Ϊ��
    if (dictSize(d) == 0) return NULL;
    
	// ����rehash����æ��
	if (dictIsRehashing(d)) _dictRehashStep(d);
    
	// ����rehash����Ҫ����ht[0]��ht[1]
	if (dictIsRehashing(d)) {
		// ���ѭ����ȡ��֪��Ͱ���нڵ����
        do {
            /* We are sure there are no elements in indexes from 0
             * to rehashidx-1 */
			// 0~rehashidx-1��Ͱ��û��Ԫ�أ���Ǩ����ht[1]��
			// ���Ҫ��֤Ͱ��idx>=rehashidx
            h = d->rehashidx + (randomULong() % (dictSlots(d) - d->rehashidx));
			// ����h�Ĵ�Сȷ��ȡht[1]��ht[0]��Ͱ
            he = (h >= d->ht[0].size) ? d->ht[1].table[h - d->ht[0].size] :
                                      d->ht[0].table[h];
			// ֱ��ȡ��һ���ǿ�Ͱ���׽ڵ�
        } while(he == NULL);
	// ����rehash��ֻ�迼��ht[0]
    } else {
        do {
			// ����������������ӦͰidx
            h = randomULong() & d->ht[0].sizemask;
            // ȡ��Ͱ���׽ڵ�
			he = d->ht[0].table[h];
			// ֱ��ȡ��һ���ǿ�Ͱ���׽ڵ�
        } while(he == NULL);
    }

    /* Now we found a non empty bucket, but it is a linked
     * list and we need to get a random element from the list.
     * The only sane way to do so is counting the elements and
     * select a random index. */
	// �˴����ǻ����һ���ǿ�Ͱ���׽ڵ㣬���������һ������������Ҫ�������ѡһ��Ԫ��
    listlen = 0;
    orighe = he;
    // ����������
	while(he) {
        he = he->next;
        listlen++;
    }
	// ���ѡ��ڵ��������е�idx
    listele = random() % listlen;
    he = orighe;
	// �ҵ�Ŀ��ڵ�
    while(listele--) he = he->next;
    // ����Ŀ��ڵ�
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
/* ����ɼ�ָ�������Ľڵ㣬�����������ܴﲻ��count�ĸ���
 * �����Ҫ����һЩ���key�����������dictGetRandomKey��ܶ�
 *
 * ʵ��˼·�����ѡһ��Ͱ�������Ͱ��ʼȡ�ڵ㣬ֱ��ȡ��
 */
unsigned int dictGetSomeKeys(dict *d, dictEntry **des, unsigned int count) {
    unsigned long j; /* internal hash table id, 0 or 1. */
	// hash����������Ƿ�����rehash������
    unsigned long tables; /* 1 or 2 tables? */
    // stored��ʾ�Ѿ��ɼ��Ľڵ�����maxsizemaskΪhash�����size������
	unsigned long stored = 0, maxsizemask;
	// �ɼ���������
    unsigned long maxsteps;

	// ��෵���ֵ���ܽڵ���
    if (dictSize(d) < count) count = dictSize(d);
    // �ɼ���������ΪԪ�ظ�����10��
	maxsteps = count*10;

    /* Try to do a rehashing work proportional to 'count'. */
    // ���ݷ���key�ĸ�������æ��incremental rehashing
	for (j = 0; j < count; j++) {
        if (dictIsRehashing(d))
            _dictRehashStep(d);
        else
            break;
    }

	// ����ֵ�����rehash����Ҫ����2��hash���������һ��
    tables = dictIsRehashing(d) ? 2 : 1;
    maxsizemask = d->ht[0].sizemask;
	// ȡ2��hash��������ڵĻ��������size��Ӧ��sizemask
    if (tables > 1 && maxsizemask < d->ht[1].sizemask)
        maxsizemask = d->ht[1].sizemask;

    /* Pick a random point inside the larger table. */
	// ���ѡ��hashͰ
    unsigned long i = randomULong() & maxsizemask;
	// ͳ��ĿǰΪֹ�����˿�hashͰ������
    unsigned long emptylen = 0; /* Continuous empty entries so far. */
	// ���������key���㹻���ߴﵽ�������ޣ��˳�ѭ��
    while(stored < count && maxsteps--) {
		// ��������hash��
        for (j = 0; j < tables; j++) {
            /* Invariant of the dict.c rehashing: up to the indexes already
             * visited in ht[0] during the rehashing, there are no populated
             * buckets, so we can skip ht[0] for indexes between 0 and idx-1. */
			// �����2���������ڱ�����һ���������ѡ��Ͱ���Ѿ�Ǩ�Ƶķ�Χ
            if (tables == 2 && j == 0 && i < (unsigned long) d->rehashidx) {
                /* Moreover, if we are currently out of range in the second
                 * table, there will be no elements in both tables up to
                 * the current rehashing index, so we jump if possible.
                 * (this happens when going from big to small table). */
				// ���id > hash��1�ĳ��ȣ�˵��rehash��һ�����hash����һ��С��hash��
				// ��˲�����ht[1]��ȡ��Ҫ��ht[0]��ȡ
				// ��������Ϊht[0]�� < rehashidx��Ͱ�ѱ�Ǩ�ƣ����Դ˴�ѡ���һ��δ��Ǩ�Ƶ�Ͱ
                if (i >= d->ht[1].size)
                    i = d->rehashidx;
                else
					// ѡ���Ͱ�ѱ�Ǩ�ƣ�ȡht[1]����
                    continue;
            }
			// ������hashͰ�������ڵ�ǰ�����鳤�ȣ�������
            if (i >= d->ht[j].size) continue; /* Out of range for this table. */
			// �ҵ�Ͱ���׽ڵ�
            dictEntry *he = d->ht[j].table[i];

            /* Count contiguous empty buckets, and jump to other
             * locations if they reach 'count' (with a minimum of 5). */
			// ����׽ڵ�Ϊ��
            if (he == NULL) {
				// ͳ�ƿյ�hashͰ
                emptylen++;
				// �����Ͱ��������һ��������max(count,4)�������������������hashͰidx
                if (emptylen >= 5 && emptylen > count) {
                    i = randomULong() & maxsizemask;
                    emptylen = 0;
                }
			// ����׽ڵ㲻Ϊ��
            } else {
				// ���ÿյ�hashͰͳ��
                emptylen = 0;
				// ��������
                while (he) {
                    /* Collect all the elements of the buckets found non
                     * empty while iterating. */
					// ȡ�����еĽڵ㣬�ŵ�des�У�ֱ��ȡ��count���ڵ����ȡ�굱ǰ����
                    *des = he;
                    des++;
                    he = he->next;
                    stored++;
                    if (stored == count) return stored;
                }
            }
        }
		// ��ȡ��һ��hashͰ��λ��
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
/* ��ƽ��ȡһ�����key
 * ��dictGetRandomKey��ƽһ�㣬��Ҳ���Ǿ��Թ�ƽ
 */
dictEntry *dictGetFairRandomKey(dict *d) {
    dictEntry *entries[GETFAIR_NUM_ENTRIES];
	// �ȴ��ֵ���ѡȡ���������Ľڵ�
    unsigned int count = dictGetSomeKeys(d,entries,GETFAIR_NUM_ENTRIES);
    /* Note that dictGetSomeKeys() may return zero elements in an unlucky
     * run() even if there are actually elements inside the hash table. So
     * when we get zero, we call the true dictGetRandomKey() that will always
     * yield the element if the hash table has at least one. */
	// ���û�еõ��κ�һ���ڵ㣬�Ǿ���dictGetRandomKey����һ���ڵ�
    if (count == 0) return dictGetRandomKey(d);
	// ��������count���ڵ㣬���ѡһ������
    unsigned int idx = rand() % count;
    return entries[idx];
}

/* Function to reverse bits. Algorithm from:
 * http://graphics.stanford.edu/~seander/bithacks.html#ReverseParallel */
/* �����Ƶ��������
 * �ٸ����ӣ�����Ҫ��ת32λ���������㷨��˼·�ǣ�
 * �ȶԵ����ڵ�16λ, �ٶԵ����ڵ�8λ, �ٶԵ����ڵ�4λ, �ٶԵ����ڵ�2λ, ������ɷ�ת.
 */
static unsigned long rev(unsigned long v) {
	// CHAR_BITһ��Ϊ8��sizeof(v)Ϊv���ֽ���
    unsigned long s = CHAR_BIT * sizeof(v); // bit size; must be power of 2
    // �����32λ�Ļ�����32��1
	unsigned long mask = ~0UL; // unsigned long 0 ȡ��
    while ((s >>= 1) > 0) {
		// mask << s��mask��sλΪ1����sλΪ0
		// mask ^= (mask << s)��mask��sλΪ1����sλΪ0
        mask ^= (mask << s);
		// (v >> s) & mask����sλ�ƶ�����sλ����λΪ0
		// (v << s) & ~mask����sλ�ƶ�����sλ����λΪ0
		// |������ƴ���������µĽ���Ǹ�sλ�͵�sλ����
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
/* �ֵ�ɨ��
 * ���ã����������ֵ��Ԫ��
 *
 * ���������߼���
 * 1. ��ʼ�������0��Ϊcursor��v�����ú���
 * 2. ����ִ��һ�������������������´ε���ʹ�õ���cursor
 * 3. ����������cursorΪ0ʱ���������
 *
 * �ú�����֤�ڵ����ӿ�ʼ�������ڼ䣬һֱ�������ֵ��Ԫ�ػᱻ������
 * ���Ǵ���һ��Ԫ�ر����ض�εĿ���
 *
 * ÿ��һ��Ԫ�ر�����ʱ��dictScanFunc *fn�ᱻ����
 * fn�����ĵ�һ������ʱprivdata���ڶ�������ʱ�ֵ�ڵ�
 *
 * ����ԭ��
 * �ڶ����Ƹ�λ�϶�cursor���мӷ�����
 * ����cursor�Ķ�����λ��ת��Ȼ��Է�ת���ֵ���мӷ����㣬���Լӷ��Ľ����ת��
 *
 * ���������������Ϊ���õ�����ʱ��hash��������ڵ�����С��
 *
 * dict.c��hash���С��Ϊ2^n������ʹ������������һ����������һ��Ԫ�ص�λ��ʱͨ������key��hashֵ��size-1�İ�λ�������á�
 * �� key��hashֵ & size-1 = key��hashֵ % size
 *
 * ���hash���С�����˱仯����Σ�
 * 1. ���hash����
 * Ԫ�ػ��ɢ��ԭ��Ͱ�����ĵط����������ǵ���һ��4bit��cursor 1100��hash���С16����Ӧ����1111��
 * ���hash�����ݵ�64���µ�����Ϊ63��111111����Ҫ��ȡ�µ�Ͱ��ʲô����ԭ����1100����??1100
 * ���У�?��ʾ0��1���������Ѿ����ʹ���С��hash���е�Ͱλ��1100����
 *
 * ͨ�����޵�����λ�ı���λ����Ϊ��ת�ļ�������hash����ʱ��cursor����Ҫ���¿�ʼ�����Լ�������cursor��
 * ������Ҫ����1100��β�������Ѿ����ʹ������4λbit��ϵ�λ��
 * 
 * �ٸ����ӣ�����Ӹ�λ��ʼ������1100�����0011����չ��1λ����00011��
 * ��Ϊ��ʼ����ʱ���Ǵ�0��ʼ���Ѿ����ʹ�0000,1000,0100,1100,0010,1010,0110,1110,0001,1001,0101,1101,0011����ǰλ�ã�
 * ��������Ҫ����1011,0111,1111
 * �����չ�ˣ���ô���ǽ�Ҫ����01100����Ϊ�ǴӸ�λ��ʼ���������ڷ��ʹ���Ԫ�أ����λ���0��ȫ��С��00011��
 * ����֮ǰ���ʵ�λ�������ٴη��ʣ�������Ҫ���ʵ���00011,10011,01011,11011,00111,10111,01111,11111,000000
 *
 * 2. ���hash����С
 * �ٸ����ӣ�hash���С��16��8�����һ����3bits������Ѿ������ʹ���size = 8������111������ô���������ٱ�����
 * ������0111��1111ֻ�����λ��һ�����������ǲ���Ҫ�ٴη�����
 * 
 * ����˵����
 * ����Ӹ�λ��ʼ������1100��Ϊ0011����������ˣ�����011
 * ��ʼ����0��ʼ��0000,1000,0100,1100,0010,1010,0110,1110,0001,1001,0101,1101,0011����ǰλ�ã�����������Ҫ����1011,0111,1111
 * �������С�ˣ���ô�������ʵ���011,111,0����������ظ�Ԫ�أ�
 * �������Сǰ�����ʵ�1011������Ҫ����011����0011�ڵ��Ѿ����ʹ������Ի��ظ�����011
 *
 * Ǩ�ƹ�����2�ű���ô�죿
 * ��С��ʼ������ͨ����ǰcursor��չ����ı�
 * ��������ǰcursorΪ101��Ȼ����ڴ�СΪ16�ı��ڴ���з���(0)101��(1)101
 *
 * Limitations��
 * ���������ʱû��״̬�ģ����û�ж�����ڴ�����
 * ��Ƶ�ȱ�ݣ�
 * 1. ���ܷ����ظ�Ԫ�أ�����Ӧ�ò�����
 * 2. Ϊ�˲�����κ�Ԫ�أ���������Ҫ���ظ���Ͱ�ϵ����м����Լ���Ϊ��չhash����������±�
 *    ���Ե�����������һ�ε����з��ض��Ԫ��
 * 3. ���α���з�ת��ԭ�������������
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

	// �ֵ�Ϊ�գ�����������0
    if (dictSize(d) == 0) return 0;

    /* This is needed in case the scan callback tries to do dictFind or alike. */
	// �������rehash����ͣrehash״̬+1
    dictPauseRehashing(d);

	// ����ֵ�û����rehash
    if (!dictIsRehashing(d)) {
		// ��ȡht[0]�ĵ�ַ
        t0 = &(d->ht[0]);
		// ��ȡht[0]��size����
        m0 = t0->sizemask;

        /* Emit entries at cursor */
		// ���Ͱɨ�躯��bucketfn���ڣ�����bucketfnchuliҪ��ȡ��Ͱ
        if (bucketfn) bucketfn(privdata, &t0->table[v & m0]);
		// ��ȡͰ���׽ڵ�
        de = t0->table[v & m0];
		// ����ڵ���ڣ����������ϵĽڵ㣬����fn��������ÿһ���ڵ�
        while (de) {
            next = de->next;
            fn(privdata, de);
            de = next;
        }

        /* Set unmasked bits so incrementing the reversed cursor
         * operates on the masked bits */
		// ����hash��sizeΪ8������m0Ϊ29λ0��3λ1����...000111
		// ~m0 = ...111000
		// v |= ~m0����������λ�����ݣ�����29λΪ1����3λΪԭ��v��ʵ������xxx
		// ����v=3ul���򾭹���v |= ~m0��v=111...111xxx(111...111011)
        v |= ~m0;

        /* Increment the reverse cursor */
		// ��תcursor��v = xxx111...111(110111...111)
        v = rev(v);
		// xx(x+1)000...000(111000...000)
        v++;
		// �ٴη�ת��ԭ��������(000...000111)����011->111
        v = rev(v);
	// ����ֵ�����rehash
    } else {
		// ȡ��2��hash��ĵ�ַ
        t0 = &d->ht[0];
        t1 = &d->ht[1];

        /* Make sure t0 is the smaller and t1 is the bigger table */
        // ��֤t0��t1С
		if (t0->size > t1->size) {
            t0 = &d->ht[1];
            t1 = &d->ht[0];
        }

		// ��ȡsize����
        m0 = t0->sizemask;
        m1 = t1->sizemask;

        /* Emit entries at cursor */
		// �Ա�0Ԫ�ؽ��д������rehash�����һ��
        if (bucketfn) bucketfn(privdata, &t0->table[v & m0]);
        de = t0->table[v & m0];
        while (de) {
            next = de->next;
            fn(privdata, de);
            de = next;
        }

        /* Iterate over indices in larger table that are the expansion
         * of the index pointed to by the cursor in the smaller table */
		// ͨ��С���cursor��չ����cursor
		// ��Ϊm1>m0������С����һ��Ͱidx����չ������е�(m1+1)/(m0+1)��Ͱ
        do {
            /* Emit entries at cursor */
            if (bucketfn) bucketfn(privdata, &t1->table[v & m1]);
			// m1 > m0���˴���õ�Ͱ��ֵ��С��һ��
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
			// v�Ķ����Ʊ�ʾ�У���m1��m0�ߵ�����λ��Ϊ0�����˳�ѭ��
			// �ٸ����ӣ�С��4λ�����6λ��С�������0100
			// ����000100��ʼ������100100 -> 010100 -> 110100 -> 001100
			// ��ߵ�2λ�ٴα�Ϊ0����λ�ˣ���4λҲ�������һ��Ͱ��λ�ã�1100��
        } while (v & (m0 ^ m1));
    }

	// ���¿�ʼrehash
    dictResumeRehashing(d);

    return v;
}

/* ------------------------- private functions ------------------------------ */

/* Because we may need to allocate huge memory chunk at once when dict
 * expands, we will check this allocation is allowed or not if the dict
 * type has expandAllowed member function. */
/* ����չ�ֵ��ʱ�������Ҫ����ܶ��ڴ�
 * ���ֵ����Ͱ���expandAllowed������£�������Ҫ�����������Ƿ�����
 */
static int dictTypeExpandAllowed(dict *d) {
	// û��expandAllowed����������1����ʾ����
    if (d->type->expandAllowed == NULL) return 1;
	// ������Ҫ�������ݺ�������ڴ�͸��������ж��Ƿ���������
    return d->type->expandAllowed(
                    _dictNextPower(d->ht[0].used + 1) * sizeof(dictEntry*),
                    (double)d->ht[0].used / d->ht[0].size);
}

/* Expand the hash table if needed */
/* ����Ҫʱ��չhash�� */
static int _dictExpandIfNeeded(dict *d)
{
    /* Incremental rehashing already in progress. Return. */
	// ����rehash������DICT_OK
    if (dictIsRehashing(d)) return DICT_OK;

    /* If the hash table is empty expand it to the initial size. */
	// hash��Ϊ�գ���չ��Ĭ�ϴ�С
    if (d->ht[0].size == 0) return dictExpand(d, DICT_HT_INITIAL_SIZE);

    /* If we reached the 1:1 ratio, and we are allowed to resize the hash
     * table (global setting) or we should avoid it but the ratio between
     * elements/buckets is over the "safe" threshold, we resize doubling
     * the number of buckets. */
	// ���used/size>=1��expandAllowed�������ݣ��������Զ�resize flag = 1����used/size>=dict_force_resize_ratio������չ�ռ�
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
/* �����һ�����ڵ���size��2^n */
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
/* ���ؿ��Խ�key���뵽hash�������λ��
 * ���key�Ѵ��ڣ�����-1
 *
 * ����ֵ�����rehash����ô���Ƿ���1��hash�������
 * ��Ϊ���ֵ����rehashʱ���½ڵ����ǲ��뵽1��hash��
 */
static long _dictKeyIndex(dict *d, const void *key, uint64_t hash, dictEntry **existing)
{
    unsigned long idx, table;
    dictEntry *he;
    if (existing) *existing = NULL;

    /* Expand the hash table if needed */
	// �����Ҫ����չhash������չhash��
    if (_dictExpandIfNeeded(d) == DICT_ERR)
        return -1;

    for (table = 0; table <= 1; table++) {
		// ��������ֵ
        idx = hash & d->ht[table].sizemask;
        /* Search if this slot does not already contain the given key */
		// ���key�Ƿ����
        he = d->ht[table].table[idx];
        while(he) {
			// �ҵ�key
            if (key==he->key || dictCompareKeys(d, key, he->key)) {
				// ���ҵ��Ľڵ�ͨ��*existing����
                if (existing) *existing = he;
                return -1;
            }
			// ����Ͱ����һ���ڵ�
            he = he->next;
        }

		// ���е��˴���˵��ht[0]���нڵ㲻����key
		// ��˼���Ƿ���rehash���������rehash��������ht[1]���в���
        if (!dictIsRehashing(d)) break;
    }
    return idx;
}

/* ����ֵ��ϵ�����hash���Լ����нڵ�
 * �������ֵ�����
 */
void dictEmpty(dict *d, void(callback)(void*)) {
    _dictClear(d,&d->ht[0],callback);
    _dictClear(d,&d->ht[1],callback);
    d->rehashidx = -1;
    d->pauserehash = 0;
}

/* ����rehash */
void dictEnableResize(void) {
    dict_can_resize = 1;
}

/* �ر�rehash */
void dictDisableResize(void) {
    dict_can_resize = 0;
}

/* ��ȡ���ֵ�d�ж�Ӧ��key��hashֵ */
uint64_t dictGetHash(dict *d, const void *key) {
    return dictHashKey(d, key);
}

/* Finds the dictEntry reference by using pointer and pre-calculated hash.
 * oldkey is a dead pointer and should not be accessed.
 * the hash value should be provided using dictGetHash.
 * no string / key comparison is performed.
 * return value is the reference to the dictEntry if found, or NULL if not found. */
/* ����ָ�����ǰ����õ�hashֵѰ��entry
 * oldkey��һ�����ܱ��޸ĵĳ���ָ��
 * hashֵ��ͨ��dictGetHash����õ���
 * ����Ҫ�Ƚ�string/key
 * ���أ�
 * �ҵ�   -- ����dictEntry������
 * û�ҵ� -- NULL
 */
dictEntry **dictFindEntryRefByPtrAndHash(dict *d, const void *oldptr, uint64_t hash) {
    dictEntry *he, **heref;
    unsigned long idx, table;

	// �ֵ�Ϊ�ղ���Ҫ����
    if (dictSize(d) == 0) return NULL; /* dict is empty */
    for (table = 0; table <= 1; table++) {
		// ��Ͱ
        idx = hash & d->ht[table].sizemask;
		// ��Ͱ�׽ڵ�ĵ�ַ
        heref = &d->ht[table].table[idx];
		// �׽ڵ�
        he = *heref;
		// �������еĽڵ�key�ĵ�ַ�Ƿ���oldptr��ͬ
        while(he) {
            if (oldptr==he->key)
                return heref;
            heref = &he->next;
            he = *heref;
        }
		// ���û������hash������Ҫ����ht[1]
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
