/* quicklist.h - A generic doubly linked quicklist implementation
 *
 * Copyright (c) 2014, Matt Stancliff <matt@genges.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this quicklist of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this quicklist of conditions and the following disclaimer in the
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

#include <stdint.h> // for UINTPTR_MAX

#ifndef __QUICKLIST_H__
#define __QUICKLIST_H__

/* Node, quicklist, and Iterator are the only data structures used currently. */

/* quicklistNode is a 32 byte struct describing a ziplist for a quicklist.
 * We use bit fields keep the quicklistNode at 32 bytes.
 * count: 16 bits, max 65536 (max zl bytes is 65k, so max count actually < 32k).
 * encoding: 2 bits, RAW=1, LZF=2.
 * container: 2 bits, NONE=1, ZIPLIST=2.
 * recompress: 1 bit, bool, true if node is temporary decompressed for usage.
 * attempted_compress: 1 bit, boolean, used for verifying during testing.
 * extra: 10 bits, free for future use; pads out the remainder of 32 bits */
/* quicklistNode结构
 * 占32bytes，其中3个指针各占8bytes，unsigned int占4bytes*2
 *
 * ps：unsigned int a:4; 表示a占用unsigned int的4个bit位
 */
typedef struct quicklistNode {
    // 指向当前节点的前后节点
    struct quicklistNode *prev;
    struct quicklistNode *next;
    // 指向节点对应的ziplist
    unsigned char *zl;
    // ziplist占用的字节数
    unsigned int sz;             /* ziplist size in bytes */
    // ziplist元素的数量
    unsigned int count : 16;     /* count of items in ziplist */
    // encoding表示编码类型：1表示原生的，2表示使用LZF进行压缩
    unsigned int encoding : 2;   /* RAW==1 or LZF==2 */
    // zl指向的容器类型，1表示None，2表示ziplist
    unsigned int container : 2;  /* NONE==1 or ZIPLIST==2 */
    // recompress表示这个节点之前是否为压缩节点
    // 如果是，则在使用之前要解压缩，时候之后要重新压缩
    unsigned int recompress : 1; /* was this node previous compressed? */

    unsigned int attempted_compress : 1; /* node can't compress; too small */
    unsigned int extra : 10; /* more bits to steal for future usage */
} quicklistNode;

/* quicklistLZF is a 4+N byte struct holding 'sz' followed by 'compressed'.
 * 'sz' is byte length of 'compressed' field.
 * 'compressed' is LZF data with total (compressed) length 'sz'
 * NOTE: uncompressed length is stored in quicklistNode->sz.
 * When quicklistNode->zl is compressed, node->zl points to a quicklistLZF */
/* 对ziplist利用LZF算法压缩时，quicklistNode节点指向quicklistLSF
 *
 * 4+N字节的结构
 *
 * 压缩前的长度存储在quicklistNode->sz中
 * 当quicklistNode->zl是压缩过的，node->zl指向一个quicklistLZF
 */
typedef struct quicklistLZF {
    // compressed占用的字节数
    unsigned int sz; /* LZF size in bytes*/
    // LZF data，长度为sz
    char compressed[];
} quicklistLZF;

/* Bookmarks are padded with realloc at the end of of the quicklist struct.
 * They should only be used for very big lists if thousands of nodes were the
 * excess memory usage is negligible, and there's a real need to iterate on them
 * in portions.
 * When not used, they don't add any memory overhead, but when used and then
 * deleted, some overhead remains (to avoid resonance).
 * The number of bookmarks used should be kept to minimum since it also adds
 * overhead on node deletion (searching for a bookmark to update). */
/* 书签 */
typedef struct quicklistBookmark {
    // 指向quicklistNode
    quicklistNode *node;
    // 书签名字
    char *name;
} quicklistBookmark;

#if UINTPTR_MAX == 0xffffffff
/* 32-bit */
#   define QL_FILL_BITS 14
#   define QL_COMP_BITS 14
#   define QL_BM_BITS 4
#elif UINTPTR_MAX == 0xffffffffffffffff
/* 64-bit */
#   define QL_FILL_BITS 16
#   define QL_COMP_BITS 16
#   define QL_BM_BITS 4 /* we can encode more, but we rather limit the user
                           since they cause performance degradation. */
#else
#   error unknown arch bits count
#endif

/* quicklist is a 40 byte struct (on 64-bit systems) describing a quicklist.
 * 'count' is the number of total entries.
 * 'len' is the number of quicklist nodes.
 * 'compress' is: 0 if compression disabled, otherwise it's the number
 *                of quicklistNodes to leave uncompressed at ends of quicklist.
 * 'fill' is the user-requested (or default) fill factor.
 * 'bookmakrs are an optional feature that is used by realloc this struct,
 *      so that they don't consume memory when not used. */
/* quicklist */
typedef struct quicklist {
    // 指向quicklist的头尾节点
    quicklistNode *head;
    quicklistNode *tail;
    // 所有ziplist中entry的数量和
    unsigned long count;        /* total count of all entries in all ziplists */
    // quicklist中quicklistNode的数量
    unsigned long len;          /* number of quicklistNodes */
    // 每个节点最大容量
    // 存放参数list-max-ziplist-size的值
    int fill : QL_FILL_BITS;              /* fill factor for individual nodes */
    // quicklist的压缩深度，与LZF相关；0=off
    // 存放参数list-compress-depth的值
    unsigned int compress : QL_COMP_BITS; /* depth of end nodes not to compress;0=off */
    // bookmarks数组的大小
    unsigned int bookmark_count: QL_BM_BITS;
    // 用于给quicklist重新分配内存时使用，不使用时不占空间
    quicklistBookmark bookmarks[];
} quicklist;

/* 用于遍历quicklist的迭代器 */
typedef struct quicklistIter {
    // 指向当前元素所处quicklist
    const quicklist *quicklist;
    // 指向当前元素所在quicklistNode
    quicklistNode *current;
    // 指向元素所在ziplist
    unsigned char *zi;
    // 指向ziplist的第offset个entry
    long offset; /* offset in current ziplist */
    // 迭代器方向
    int direction;
} quicklistIter;

/* quicklistNode中有ziplist，这是ziplist中的Entry */
typedef struct quicklistEntry {
    // 指向当前元素所在的quicklist
    const quicklist *quicklist;
    // 指向当前元素所在的quicklistNode
    quicklistNode *node;
    // 指向当前元素所在的ziplist
    unsigned char *zi;
    // 通过检查*value是否为NULL可以确定entry的数据类型是STR还是INT
    // 当前节点的字符串内容
    unsigned char *value;
    // 当前节点的整形值
    long long longval;
    // 该节点的数据占用的字节数，配合value使用
    unsigned int sz;
    // 该节点相对于整个ziplist的偏移量（即该节点为ziplist的第offset个节点）
    int offset;
} quicklistEntry;

#define QUICKLIST_HEAD 0
#define QUICKLIST_TAIL -1

/* quicklist node encodings */
#define QUICKLIST_NODE_ENCODING_RAW 1
#define QUICKLIST_NODE_ENCODING_LZF 2

/* quicklist compression disable */
#define QUICKLIST_NOCOMPRESS 0

/* quicklist container formats */
#define QUICKLIST_NODE_CONTAINER_NONE 1
#define QUICKLIST_NODE_CONTAINER_ZIPLIST 2

#define quicklistNodeIsCompressed(node)                                        \
    ((node)->encoding == QUICKLIST_NODE_ENCODING_LZF)

/* Prototypes */
quicklist *quicklistCreate(void);
quicklist *quicklistNew(int fill, int compress);
void quicklistSetCompressDepth(quicklist *quicklist, int depth);
void quicklistSetFill(quicklist *quicklist, int fill);
void quicklistSetOptions(quicklist *quicklist, int fill, int depth);
void quicklistRelease(quicklist *quicklist);
int quicklistPushHead(quicklist *quicklist, void *value, const size_t sz);
int quicklistPushTail(quicklist *quicklist, void *value, const size_t sz);
void quicklistPush(quicklist *quicklist, void *value, const size_t sz,
                   int where);
void quicklistAppendZiplist(quicklist *quicklist, unsigned char *zl);
quicklist *quicklistAppendValuesFromZiplist(quicklist *quicklist,
                                            unsigned char *zl);
quicklist *quicklistCreateFromZiplist(int fill, int compress,
                                      unsigned char *zl);
void quicklistInsertAfter(quicklist *quicklist, quicklistEntry *node,
                          void *value, const size_t sz);
void quicklistInsertBefore(quicklist *quicklist, quicklistEntry *node,
                           void *value, const size_t sz);
void quicklistDelEntry(quicklistIter *iter, quicklistEntry *entry);
int quicklistReplaceAtIndex(quicklist *quicklist, long index, void *data,
                            int sz);
int quicklistDelRange(quicklist *quicklist, const long start, const long stop);
quicklistIter *quicklistGetIterator(const quicklist *quicklist, int direction);
quicklistIter *quicklistGetIteratorAtIdx(const quicklist *quicklist,
                                         int direction, const long long idx);
int quicklistNext(quicklistIter *iter, quicklistEntry *node);
void quicklistReleaseIterator(quicklistIter *iter);
quicklist *quicklistDup(quicklist *orig);
int quicklistIndex(const quicklist *quicklist, const long long index,
                   quicklistEntry *entry);
void quicklistRewind(quicklist *quicklist, quicklistIter *li);
void quicklistRewindTail(quicklist *quicklist, quicklistIter *li);
void quicklistRotate(quicklist *quicklist);
int quicklistPopCustom(quicklist *quicklist, int where, unsigned char **data,
                       unsigned int *sz, long long *sval,
                       void *(*saver)(unsigned char *data, unsigned int sz));
int quicklistPop(quicklist *quicklist, int where, unsigned char **data,
                 unsigned int *sz, long long *slong);
unsigned long quicklistCount(const quicklist *ql);
int quicklistCompare(unsigned char *p1, unsigned char *p2, int p2_len);
size_t quicklistGetLzf(const quicklistNode *node, void **data);

/* bookmarks */
int quicklistBookmarkCreate(quicklist **ql_ref, const char *name, quicklistNode *node);
int quicklistBookmarkDelete(quicklist *ql, const char *name);
quicklistNode *quicklistBookmarkFind(quicklist *ql, const char *name);
void quicklistBookmarksClear(quicklist *ql);

#ifdef REDIS_TEST
int quicklistTest(int argc, char *argv[], int accurate);
#endif

/* Directions for iterators */
#define AL_START_HEAD 0
#define AL_START_TAIL 1

#endif /* __QUICKLIST_H__ */
