/* adlist.c - A generic doubly linked list implementation
 *
 * Copyright (c) 2006-2010, Salvatore Sanfilippo <antirez at gmail dot com>
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


#include <stdlib.h>
#include "adlist.h"
#include "zmalloc.h"

/* Create a new list. The created list can be freed with
 * listRelease(), but private value of every node need to be freed
 * by the user before to call listRelease(), or by setting a free method using
 * listSetFreeMethod.
 *
 * On error, NULL is returned. Otherwise the pointer to the new list. */
/* 创建一个新的链表
 * 创建成功 -- 返回指向新链表的指针
 * 创建失败 -- 返回NULL
 */
list *listCreate(void)
{
    struct list *list;

    // 内存分配
    if ((list = zmalloc(sizeof(*list))) == NULL)
        return NULL;
    // 初始化
    list->head = list->tail = NULL;
    list->len = 0;
    list->dup = NULL;
    list->free = NULL;
    list->match = NULL;
    return list;
}

/* Remove all the elements from the list without destroying the list itself. */
/* 删除list中的所有元素 */
void listEmpty(list *list)
{
    unsigned long len;
    listNode *current, *next;

    current = list->head;
    len = list->len;
    while(len--) {
        next = current->next;
        // 如果list有值释放函数，则调用它以释放当前元素的值
        if (list->free) list->free(current->value);
        // 释放节点
        zfree(current);
        current = next;
    }
    // 清空list中的成员
    list->head = list->tail = NULL;
    list->len = 0;
}

/* Free the whole list.
 *
 * This function can't fail. */
void listRelease(list *list)
{
    listEmpty(list);
    // 释放链表
    zfree(list);
}

/* Add a new node to the list, to head, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */
/* 从list表头增加一个包含特定值value的节点
 * 增加成功 -- 返回传入的list指针
 * 增加失败 -- （为新节点内存分配出错）返回NULL，不执行任何操作
 */
list *listAddNodeHead(list *list, void *value)
{
    listNode *node;

    // 分配内存
    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;
    
    // 设置节点的值
    node->value = value;

    // 增加节点至空链表
    if (list->len == 0) {
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    // 增加节点至非空链表
    } else {
        node->prev = NULL;
        node->next = list->head;
        list->head->prev = node;
        list->head = node;
    }

    // 更新list节点数
    list->len++;
    return list;
}

/* Add a new node to the list, to tail, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */
/* 从list表尾增加一个包含指定值value的节点
 * 增加成功 -- 返回传入的list指针
 * 增加失败 -- （为新节点内存分配出错）返回NULL，不执行任何操作
 */
list *listAddNodeTail(list *list, void *value)
{
    listNode *node;

    // 分配内存
    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;

    // 设置节点的值
    node->value = value;

    // 增加节点至空链表
    if (list->len == 0) {
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    // 增加节点至非空链表
    } else {
        node->prev = list->tail;
        node->next = NULL;
        list->tail->next = node;
        list->tail = node;
    }

    // 更新list节点数
    list->len++;
    return list;
}

/* 创建一个包含值value的节点，将其插入old_node之前或之后
 * after = 0 -- 新节点插入old_node之前
 * after = 1 -- 新节点插入old_node之后
 */
list *listInsertNode(list *list, listNode *old_node, void *value, int after) {
    listNode *node;

    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;
    node->value = value;

    // 新节点插入old_node之后，更新node的prev/next
    if (after) {
        node->prev = old_node;
        node->next = old_node->next;
        // 若old_node为list尾节点，则将当前节点设为尾节点
        if (list->tail == old_node) {
            list->tail = node;
        }
    // 新节点插入old_node之前，更新node的prev/next
    } else {
        node->next = old_node;
        node->prev = old_node->prev;
        // 若old_node为list头节点，则将当前节点设为头节点
        if (list->head == old_node) {
            list->head = node;
        }
    }

    // 更新node前置节点的next
    if (node->prev != NULL) {
        node->prev->next = node;
    }
    // 更新node后置节点的prev
    if (node->next != NULL) {
        node->next->prev = node;
    }

    // 更新list节点数
    list->len++;
    return list;
}

/* Remove the specified node from the specified list.
 * It's up to the caller to free the private value of the node.
 *
 * This function can't fail. */
/* 删除list中给定节点
 * 对节点值的释放工作由调用者完成
 */
void listDelNode(list *list, listNode *node)
{
    // 调整前置节点的指针
    if (node->prev)
        node->prev->next = node->next;
    else
        // 删除的是头结点时
        list->head = node->next;

    // 调整后置节点的指针
    if (node->next)
        node->next->prev = node->prev;
    else
        // 删除的是尾节点时
        list->tail = node->prev;

    // 释放值
    if (list->free) list->free(node->value);
    
    // 释放节点
    zfree(node);

    // 链表数减一
    list->len--;
}

/* Returns a list iterator 'iter'. After the initialization every
 * call to listNext() will return the next element of the list.
 *
 * This function can't fail. */
/* 返回一个迭代器iter
 * 初始化之后每次调用listNext()会返回list的下一个元素（迭代到的节点）
 *
 * 参数direction决定迭代器方向
 * AL_START_HEAD -- 头->尾
 * AL_START_TAIL -- 尾->头
 */
listIter *listGetIterator(list *list, int direction)
{
    listIter *iter;
    
    // 分配内存
    if ((iter = zmalloc(sizeof(*iter))) == NULL) return NULL;
    
    // 根据迭代方向，选择迭代器初始节点
    if (direction == AL_START_HEAD)
        iter->next = list->head;
    else
        iter->next = list->tail;

    // 记录迭代方向
    iter->direction = direction;

    return iter;
}

/* Release the iterator memory */
/* 释放迭代器内存 */
void listReleaseIterator(listIter *iter) {
    zfree(iter);
}

/* Create an iterator in the list private iterator structure */
/* 设置迭代器方向为 AL_START_HEAD
 * 将迭代指针重新指向表头节点
 */
void listRewind(list *list, listIter *li) {
    li->next = list->head;
    li->direction = AL_START_HEAD;
}

/* 设置迭代器方向为 AL_START_TAIL
 * 将迭代指针重新指向表尾节点
 */
void listRewindTail(list *list, listIter *li) {
    li->next = list->tail;
    li->direction = AL_START_TAIL;
}

/* Return the next element of an iterator.
 * It's valid to remove the currently returned element using
 * listDelNode(), but not to remove other elements.
 *
 * The function returns a pointer to the next element of the list,
 * or NULL if there are no more elements, so the classical usage
 * pattern is:
 *
 * iter = listGetIterator(list,<direction>);
 * while ((node = listNext(iter)) != NULL) {
 *     doSomethingWith(listNodeValue(node));
 * }
 *
 * */
/* 返回迭代器指向的节点
 * 允许删除当前节点（用listDelNode()），但不可删除链表其他元素
 * 返回值为一个节点或NULL（遍历完）
 *
 * 常用情况：
 * iter = listGetIterator(list,<direction>);
 * while ((node = listNext(iter)) != NULL) {
 *     doSomethingWith(listNodeValue(node));
 * }
 */
listNode *listNext(listIter *iter)
{
    listNode *current = iter->next;

    if (current != NULL) {
        if (iter->direction == AL_START_HEAD)
            iter->next = current->next;
        else
            iter->next = current->prev;
    }
    return current;
}

/* Duplicate the whole list. On out of memory NULL is returned.
 * On success a copy of the original list is returned.
 *
 * The 'Dup' method set with listSetDupMethod() function is used
 * to copy the node value. Otherwise the same pointer value of
 * the original node is used as value of the copied node.
 *
 * The original list both on success or error is never modified. */
/* 复制整个list
 * 如果内存分配失败（listCreate()返回NULL），则不复制，返回NULL
 * 如果复制成功，则返回输入链表的副本
 *
 * 如果链表有设置复制函数dup，则对值的复制使用复制函数进行
 * 否则复制节点的指针值将会使用旧节点的值
 *
 * 无论复制成功或者失败，输入list都不会改变
 *
 * T = O(N)
 */
list *listDup(list *orig)
{
    list *copy;
    listIter iter;
    listNode *node;

    // 创建新链表（分配内存并初始化）
    if ((copy = listCreate()) == NULL)
        return NULL;

    // 设置节点值处理函数（原list的函数）
    copy->dup = orig->dup;
    copy->free = orig->free;
    copy->match = orig->match;

    // 设置迭代器方向，并将迭代器设置指向orig的头节点
    listRewind(orig, &iter);
    // 迭代整个输入链表
    while((node = listNext(&iter)) != NULL) {
        void *value;

        // 复制节点值
        if (copy->dup) {
            value = copy->dup(node->value);
            if (value == NULL) {
                listRelease(copy);
                return NULL;
            }
        } else
            value = node->value;

        // 将节点添加到链表
        if (listAddNodeTail(copy, value) == NULL) {
            listRelease(copy);
            return NULL;
        }
    }

    // 返回副本
    return copy;
}

/* Search the list for a node matching a given key.
 * The match is performed using the 'match' method
 * set with listSetMatchMethod(). If no 'match' method
 * is set, the 'value' pointer of every node is directly
 * compared with the 'key' pointer.
 *
 * On success the first matching node pointer is returned
 * (search starts from head). If no matching node exists
 * NULL is returned. */
/* 查找给定key对应的node
 * 使用match方法比较，如果没有match方法就直接比较node的值
 *
 * 匹配成功 -- 返回第一个匹配的节点
 * 匹配失败 -- 返回NULL
 */
listNode *listSearchKey(list *list, void *key)
{
    listIter iter;
    listNode *node;

    // 设置迭代器
    listRewind(list, &iter);
    // 迭代整个链表
    while((node = listNext(&iter)) != NULL) {
        // 比较
        if (list->match) {
            // 找到节点
            if (list->match(node->value, key)) {
                return node;
            }
        } else {
            // 找到节点
            if (key == node->value) {
                return node;
            }
        }
    }
    // 未找到
    return NULL;
}

/* Return the element at the specified zero-based index
 * where 0 is the head, 1 is the element next to head
 * and so on. Negative integers are used in order to count
 * from the tail, -1 is the last element, -2 the penultimate
 * and so on. If the index is out of range NULL is returned. */
/* 返回链表对给定index对应的节点
 * 索引从0开始，也可为负数（-1表示最后一个节点，以此类推）
 * 如果索引超出范围，返回NULL
 */
listNode *listIndex(list *list, long index) {
    listNode *n;

    // 索引为负数，从尾节点开始查找
    if (index < 0) {
        index = (-index)-1;
        n = list->tail;
        while(index-- && n) n = n->prev;
    // 索引为正数，从头结点开始查找
    } else {
        n = list->head;
        while(index-- && n) n = n->next;
    }
    return n;
}

/* Rotate the list removing the tail node and inserting it to the head. */
/* 尾节点移到头节点 */
void listRotateTailToHead(list *list) {
    if (listLength(list) <= 1) return;

    /* Detach current tail */
    // 取出尾节点
    listNode *tail = list->tail;
    list->tail = tail->prev;
    list->tail->next = NULL;

    /* Move it as head */
    // 插入到表头
    list->head->prev = tail;
    tail->prev = NULL;
    tail->next = list->head;
    list->head = tail;
}

/* Rotate the list removing the head node and inserting it to the tail. */
/* 头结点移到尾结点 */
void listRotateHeadToTail(list *list) {
    if (listLength(list) <= 1) return;

    /* Detach current head */
    // 取出头结点
    listNode *head = list->head;
    list->head = head->next;
    list->head->prev = NULL;

    /* Move it as tail */
    // 插入到表尾
    list->tail->next = head;
    head->next = NULL;
    head->prev = list->tail;
    list->tail = head;
}

/* Add all the elements of the list 'o' at the end of the
 * list 'l'. The list 'other' remains empty but otherwise valid. */
/* 连结list l和list o
 * 连接后list o置为空
 */
void listJoin(list *l, list *o) {
    // 1. list o为空
    if (o->len == 0) return;

    // 2. list o不为空
    o->head->prev = l->tail;

    // 2.1 list l不为空
    if (l->tail)
        l->tail->next = o->head;
    // 2.2 list l为空
    else
        l->head = o->head;

    l->tail = o->tail;
    l->len += o->len;

    /* Setup other as an empty list. */
    // list o变为空list
    o->head = o->tail = NULL;
    o->len = 0;
}
