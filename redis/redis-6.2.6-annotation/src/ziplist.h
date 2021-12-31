/*
 * Copyright (c) 2009-2012, Pieter Noordhuis <pcnoordhuis at gmail dot com>
 * Copyright (c) 2009-2012, Salvatore Sanfilippo <antirez at gmail dot com>
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

#ifndef _ZIPLIST_H
#define _ZIPLIST_H

#define ZIPLIST_HEAD 0
#define ZIPLIST_TAIL 1

/* Each entry in the ziplist is either a string or an integer. */
typedef struct {
    /* When string is used, it is provided with the length (slen). */
    unsigned char *sval;
    unsigned int slen;
    /* When integer is used, 'sval' is NULL, and lval holds the value. */
    long long lval;
} ziplistEntry;

/* �����µ�ѹ���б� */
unsigned char *ziplistNew(void);
/* �ϲ�2��ѹ���б��ڶ���ѹ���б�ƴ�ڵ�һ��ѹ���б�֮�󡣷���ֵΪ�ϲ����ѹ���б��ϲ�ʧ�ܷ���NULL */
unsigned char *ziplistMerge(unsigned char **first, unsigned char **second);
/* ����Ϊslen���ַ���s����ͷ��β����ѹ���б�zl��where��ʾ��ͷ����β���� */
unsigned char *ziplistPush(unsigned char *zl, unsigned char *s, unsigned int slen, int where);
/* ���ݸ��������������б���������ָ���ڵ��ָ�� */
unsigned char *ziplistIndex(unsigned char *zl, int index);
/* ����ѹ���б�zl��pָ��ڵ�ĺ��ýڵ㡣��pΪ��ĩ�˻��β�ڵ㣬����NULL */
unsigned char *ziplistNext(unsigned char *zl, unsigned char *p);
/* ����ѹ���б�zl��pָ��ڵ��ǰ�ýڵ㡣��pΪ��ͷ��ziplistΪ�գ�����NULL */
unsigned char *ziplistPrev(unsigned char *zl, unsigned char *p);
/* ȡ��p��ָ��ڵ��ֵ��
 * ����ڵ㱣�����STR�����ַ���ָ�뱣����*sstr�У��ַ������ȱ��浽*slen��
 * ����ڵ㱣�����INT�����������浽*sval
 */
unsigned int ziplistGet(unsigned char *p, unsigned char **sval, unsigned int *slen, long long *lval);
/* ��p����һ���µĽڵ㣬����Ϊs������Ϊslen */
unsigned char *ziplistInsert(unsigned char *zl, unsigned char *p, unsigned char *s, unsigned int slen);
/* ��ѹ���б�zl��ɾ��ָ���ڵ�*p��ԭ�ظ���pָ���λ�� */
unsigned char *ziplistDelete(unsigned char *zl, unsigned char **p);
/* ��ѹ���б�zl�Ķ�Ӧ����index�Ľڵ㿪ʼ��ɾ��num��Ԫ�� */
unsigned char *ziplistDeleteRange(unsigned char *zl, int index, unsigned int num);
unsigned char *ziplistReplace(unsigned char *zl, unsigned char *p, unsigned char *s, unsigned int slen);
unsigned int ziplistCompare(unsigned char *p, unsigned char *s, unsigned int slen);
unsigned char *ziplistFind(unsigned char *zl, unsigned char *p, unsigned char *vstr, unsigned int vlen, unsigned int skip);
/* ����ziplist�ĳ���(entry����) */
unsigned int ziplistLen(unsigned char *zl);
size_t ziplistBlobLen(unsigned char *zl);
void ziplistRepr(unsigned char *zl);
typedef int (*ziplistValidateEntryCB)(unsigned char* p, void* userdata);
int ziplistValidateIntegrity(unsigned char *zl, size_t size, int deep,
                             ziplistValidateEntryCB entry_cb, void *cb_userdata);
void ziplistRandomPair(unsigned char *zl, unsigned long total_count, ziplistEntry *key, ziplistEntry *val);
void ziplistRandomPairs(unsigned char *zl, unsigned int count, ziplistEntry *keys, ziplistEntry *vals);
unsigned int ziplistRandomPairsUnique(unsigned char *zl, unsigned int count, ziplistEntry *keys, ziplistEntry *vals);
int ziplistSafeToAdd(unsigned char* zl, size_t add);

#ifdef REDIS_TEST
int ziplistTest(int argc, char *argv[], int accurate);
#endif

#endif /* _ZIPLIST_H */
