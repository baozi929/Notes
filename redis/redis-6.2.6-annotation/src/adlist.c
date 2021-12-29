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
/* ����һ���µ�����
 * �����ɹ� -- ����ָ���������ָ��
 * ����ʧ�� -- ����NULL
 */
list *listCreate(void)
{
    struct list *list;

    // �ڴ����
    if ((list = zmalloc(sizeof(*list))) == NULL)
        return NULL;
    // ��ʼ��
    list->head = list->tail = NULL;
    list->len = 0;
    list->dup = NULL;
    list->free = NULL;
    list->match = NULL;
    return list;
}

/* Remove all the elements from the list without destroying the list itself. */
/* ɾ��list�е�����Ԫ�� */
void listEmpty(list *list)
{
    unsigned long len;
    listNode *current, *next;

    current = list->head;
    len = list->len;
    while(len--) {
        next = current->next;
        // ���list��ֵ�ͷź���������������ͷŵ�ǰԪ�ص�ֵ
        if (list->free) list->free(current->value);
        // �ͷŽڵ�
        zfree(current);
        current = next;
    }
    // ���list�еĳ�Ա
    list->head = list->tail = NULL;
    list->len = 0;
}

/* Free the whole list.
 *
 * This function can't fail. */
void listRelease(list *list)
{
    listEmpty(list);
    // �ͷ�����
    zfree(list);
}

/* Add a new node to the list, to head, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */
/* ��list��ͷ����һ�������ض�ֵvalue�Ľڵ�
 * ���ӳɹ� -- ���ش����listָ��
 * ����ʧ�� -- ��Ϊ�½ڵ��ڴ�����������NULL����ִ���κβ���
 */
list *listAddNodeHead(list *list, void *value)
{
    listNode *node;

    // �����ڴ�
    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;
    
    // ���ýڵ��ֵ
    node->value = value;

    // ���ӽڵ���������
    if (list->len == 0) {
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    // ���ӽڵ����ǿ�����
    } else {
        node->prev = NULL;
        node->next = list->head;
        list->head->prev = node;
        list->head = node;
    }

    // ����list�ڵ���
    list->len++;
    return list;
}

/* Add a new node to the list, to tail, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */
/* ��list��β����һ������ָ��ֵvalue�Ľڵ�
 * ���ӳɹ� -- ���ش����listָ��
 * ����ʧ�� -- ��Ϊ�½ڵ��ڴ�����������NULL����ִ���κβ���
 */
list *listAddNodeTail(list *list, void *value)
{
    listNode *node;

    // �����ڴ�
    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;

    // ���ýڵ��ֵ
    node->value = value;

    // ���ӽڵ���������
    if (list->len == 0) {
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    // ���ӽڵ����ǿ�����
    } else {
        node->prev = list->tail;
        node->next = NULL;
        list->tail->next = node;
        list->tail = node;
    }

    // ����list�ڵ���
    list->len++;
    return list;
}

/* ����һ������ֵvalue�Ľڵ㣬�������old_node֮ǰ��֮��
 * after = 0 -- �½ڵ����old_node֮ǰ
 * after = 1 -- �½ڵ����old_node֮��
 */
list *listInsertNode(list *list, listNode *old_node, void *value, int after) {
    listNode *node;

    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;
    node->value = value;

    // �½ڵ����old_node֮�󣬸���node��prev/next
    if (after) {
        node->prev = old_node;
        node->next = old_node->next;
        // ��old_nodeΪlistβ�ڵ㣬�򽫵�ǰ�ڵ���Ϊβ�ڵ�
        if (list->tail == old_node) {
            list->tail = node;
        }
    // �½ڵ����old_node֮ǰ������node��prev/next
    } else {
        node->next = old_node;
        node->prev = old_node->prev;
        // ��old_nodeΪlistͷ�ڵ㣬�򽫵�ǰ�ڵ���Ϊͷ�ڵ�
        if (list->head == old_node) {
            list->head = node;
        }
    }

    // ����nodeǰ�ýڵ��next
    if (node->prev != NULL) {
        node->prev->next = node;
    }
    // ����node���ýڵ��prev
    if (node->next != NULL) {
        node->next->prev = node;
    }

    // ����list�ڵ���
    list->len++;
    return list;
}

/* Remove the specified node from the specified list.
 * It's up to the caller to free the private value of the node.
 *
 * This function can't fail. */
/* ɾ��list�и����ڵ�
 * �Խڵ�ֵ���ͷŹ����ɵ��������
 */
void listDelNode(list *list, listNode *node)
{
    // ����ǰ�ýڵ��ָ��
    if (node->prev)
        node->prev->next = node->next;
    else
        // ɾ������ͷ���ʱ
        list->head = node->next;

    // �������ýڵ��ָ��
    if (node->next)
        node->next->prev = node->prev;
    else
        // ɾ������β�ڵ�ʱ
        list->tail = node->prev;

    // �ͷ�ֵ
    if (list->free) list->free(node->value);
    
    // �ͷŽڵ�
    zfree(node);

    // ��������һ
    list->len--;
}

/* Returns a list iterator 'iter'. After the initialization every
 * call to listNext() will return the next element of the list.
 *
 * This function can't fail. */
/* ����һ��������iter
 * ��ʼ��֮��ÿ�ε���listNext()�᷵��list����һ��Ԫ�أ��������Ľڵ㣩
 *
 * ����direction��������������
 * AL_START_HEAD -- ͷ->β
 * AL_START_TAIL -- β->ͷ
 */
listIter *listGetIterator(list *list, int direction)
{
    listIter *iter;
    
    // �����ڴ�
    if ((iter = zmalloc(sizeof(*iter))) == NULL) return NULL;
    
    // ���ݵ�������ѡ���������ʼ�ڵ�
    if (direction == AL_START_HEAD)
        iter->next = list->head;
    else
        iter->next = list->tail;

    // ��¼��������
    iter->direction = direction;

    return iter;
}

/* Release the iterator memory */
/* �ͷŵ������ڴ� */
void listReleaseIterator(listIter *iter) {
    zfree(iter);
}

/* Create an iterator in the list private iterator structure */
/* ���õ���������Ϊ AL_START_HEAD
 * ������ָ������ָ���ͷ�ڵ�
 */
void listRewind(list *list, listIter *li) {
    li->next = list->head;
    li->direction = AL_START_HEAD;
}

/* ���õ���������Ϊ AL_START_TAIL
 * ������ָ������ָ���β�ڵ�
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
/* ���ص�����ָ��Ľڵ�
 * ����ɾ����ǰ�ڵ㣨��listDelNode()����������ɾ����������Ԫ��
 * ����ֵΪһ���ڵ��NULL�������꣩
 *
 * ���������
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
/* ��������list
 * ����ڴ����ʧ�ܣ�listCreate()����NULL�����򲻸��ƣ�����NULL
 * ������Ƴɹ����򷵻���������ĸ���
 *
 * ������������ø��ƺ���dup�����ֵ�ĸ���ʹ�ø��ƺ�������
 * �����ƽڵ��ָ��ֵ����ʹ�þɽڵ��ֵ
 *
 * ���۸��Ƴɹ�����ʧ�ܣ�����list������ı�
 *
 * T = O(N)
 */
list *listDup(list *orig)
{
    list *copy;
    listIter iter;
    listNode *node;

    // ���������������ڴ沢��ʼ����
    if ((copy = listCreate()) == NULL)
        return NULL;

    // ���ýڵ�ֵ��������ԭlist�ĺ�����
    copy->dup = orig->dup;
    copy->free = orig->free;
    copy->match = orig->match;

    // ���õ��������򣬲�������������ָ��orig��ͷ�ڵ�
    listRewind(orig, &iter);
    // ����������������
    while((node = listNext(&iter)) != NULL) {
        void *value;

        // ���ƽڵ�ֵ
        if (copy->dup) {
            value = copy->dup(node->value);
            if (value == NULL) {
                listRelease(copy);
                return NULL;
            }
        } else
            value = node->value;

        // ���ڵ���ӵ�����
        if (listAddNodeTail(copy, value) == NULL) {
            listRelease(copy);
            return NULL;
        }
    }

    // ���ظ���
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
/* ���Ҹ���key��Ӧ��node
 * ʹ��match�����Ƚϣ����û��match������ֱ�ӱȽ�node��ֵ
 *
 * ƥ��ɹ� -- ���ص�һ��ƥ��Ľڵ�
 * ƥ��ʧ�� -- ����NULL
 */
listNode *listSearchKey(list *list, void *key)
{
    listIter iter;
    listNode *node;

    // ���õ�����
    listRewind(list, &iter);
    // ������������
    while((node = listNext(&iter)) != NULL) {
        // �Ƚ�
        if (list->match) {
            // �ҵ��ڵ�
            if (list->match(node->value, key)) {
                return node;
            }
        } else {
            // �ҵ��ڵ�
            if (key == node->value) {
                return node;
            }
        }
    }
    // δ�ҵ�
    return NULL;
}

/* Return the element at the specified zero-based index
 * where 0 is the head, 1 is the element next to head
 * and so on. Negative integers are used in order to count
 * from the tail, -1 is the last element, -2 the penultimate
 * and so on. If the index is out of range NULL is returned. */
/* ��������Ը���index��Ӧ�Ľڵ�
 * ������0��ʼ��Ҳ��Ϊ������-1��ʾ���һ���ڵ㣬�Դ����ƣ�
 * �������������Χ������NULL
 */
listNode *listIndex(list *list, long index) {
    listNode *n;

    // ����Ϊ��������β�ڵ㿪ʼ����
    if (index < 0) {
        index = (-index)-1;
        n = list->tail;
        while(index-- && n) n = n->prev;
    // ����Ϊ��������ͷ��㿪ʼ����
    } else {
        n = list->head;
        while(index-- && n) n = n->next;
    }
    return n;
}

/* Rotate the list removing the tail node and inserting it to the head. */
/* β�ڵ��Ƶ�ͷ�ڵ� */
void listRotateTailToHead(list *list) {
    if (listLength(list) <= 1) return;

    /* Detach current tail */
    // ȡ��β�ڵ�
    listNode *tail = list->tail;
    list->tail = tail->prev;
    list->tail->next = NULL;

    /* Move it as head */
    // ���뵽��ͷ
    list->head->prev = tail;
    tail->prev = NULL;
    tail->next = list->head;
    list->head = tail;
}

/* Rotate the list removing the head node and inserting it to the tail. */
/* ͷ����Ƶ�β��� */
void listRotateHeadToTail(list *list) {
    if (listLength(list) <= 1) return;

    /* Detach current head */
    // ȡ��ͷ���
    listNode *head = list->head;
    list->head = head->next;
    list->head->prev = NULL;

    /* Move it as tail */
    // ���뵽��β
    list->tail->next = head;
    head->next = NULL;
    head->prev = list->tail;
    list->tail = head;
}

/* Add all the elements of the list 'o' at the end of the
 * list 'l'. The list 'other' remains empty but otherwise valid. */
/* ����list l��list o
 * ���Ӻ�list o��Ϊ��
 */
void listJoin(list *l, list *o) {
    // 1. list oΪ��
    if (o->len == 0) return;

    // 2. list o��Ϊ��
    o->head->prev = l->tail;

    // 2.1 list l��Ϊ��
    if (l->tail)
        l->tail->next = o->head;
    // 2.2 list lΪ��
    else
        l->head = o->head;

    l->tail = o->tail;
    l->len += o->len;

    /* Setup other as an empty list. */
    // list o��Ϊ��list
    o->head = o->tail = NULL;
    o->len = 0;
}
