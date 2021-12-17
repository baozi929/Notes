# A generic doubly linked list

## 介绍

+ 代码
  + adlist.h和adlist.c

## 数据结构

+ 链表

  + ```
    typedef struct list {
        listNode *head;
        listNode *tail;
        void *(*dup)(void *ptr);
        void (*free)(void *ptr);
        int (*match)(void *ptr, void *key);
        unsigned long len;
    } list;
    ```

  + 包含：

    + 表头指针head，表尾指针tail
    + 用于实现多态链表的成员函数dup、free和match
      + dup函数：复制链表节点保存的值
      + free函数：释放链表节点保存的值
      + match函数：比较链表节点保存的值与另一输入值是否相等
    + 链表长度计数器len

  + 图示

    + ![image-20211217180126303](C:\Users\qq\AppData\Roaming\Typora\typora-user-images\image-20211217180126303.png)

+ 节点

  + ```
    typedef struct listNode {
        struct listNode *prev;
        struct listNode *next;
        void *value;
    } listNode;
    ```

  + 包含：

    + 指向前置节点的指针prev
    + 指向后置节点的指针next
    + 节点的值

  + 图示

    + ![](C:\Users\qq\AppData\Roaming\Typora\typora-user-images\image-20211217180049948.png)

+ 迭代器

  + ```
    typedef struct listIter {
        listNode *next;
        int direction;
    } listIter;
    ```

  + 包含：

    + 下一个迭代的节点next

    + 迭代的方向direction

      + ```
        #define AL_START_HEAD 0 // 头->尾
        #define AL_START_TAIL 1 // 尾->头
        ```

## API



