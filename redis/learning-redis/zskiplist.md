# Skiplist 跳跃表

## 介绍

+ 特点
  + 效率与红黑树相近，但实现简单
+ 介绍：
  + 跳跃表由很多层构成
  + 跳跃表有一个头结点header，头结点有一个32层的结构，每层结构包含指向本层的下一个节点的指针，指向本层下一个节点中间跨越的节点为本层的跨度（span）
  + 除头结点header之外层数最多的节点的层高为跳跃表的高度（level）
  + **每层**都是一个有序链表，**数据递增**
  + 除头结点header之外，一个元素在上层有序链表中出现，则它一定会在下层有序链表中出现
  + 跳跃表每层的最后一个节点指向NULL，表示本层有序链表的结束
  + 跳跃表拥有一个尾指针tail，指向跳跃表的最后一个节点
  + **最底层的有序链表包含所有节点**，最底层的节点个数为跳跃表的长度length（不包含头结点header）
  + 每个节点包含一个后退指针backward，头结点和第一个节点指向NULL，其他节点指向最底层的前一个节点
+ 代码
  + server.h和t_zset.c

## 数据结构

+ zskiplist结构

  + ```
    typedef struct zskiplist {
        struct zskiplistNode *header, *tail;
        unsigned long length;
        int level;
    } zskiplist;
    ```

  + 包含

    + 跳跃表头结点header，尾结点tail
    + length：跳跃表总节点数
    + level：跳跃表总层数，即所有节点层数的最大值

+ 节点

  + ```
    typedef struct zskiplistNode {
    	// 节点数据
        sds ele;
    	// 数据对应的分数
        double score;
    	// 指向链表前一个节点的指针
        struct zskiplistNode *backward;
    	// 指向各层链表后一个节点的指针
    	// level[]是一个柔性数组，占用的内存不在zskiplistNode中，而需要插入节点时单独为其分配
        struct zskiplistLevel {
    		// 每层对应一个后向指针
            struct zskiplistNode *forward;
    		// 表示当前指针跨越了多少节点，用于计算元素排名
            unsigned long span;
        } level[];
    } zskiplistNode;
    ```

  + 

+ 参数

  + ```
    /* zskiplist节点最大层数 */
    #define ZSKIPLIST_MAXLEVEL 32 /* Should be enough for 2^64 elements */
    /* zskiplist如果一个节点有第i层(i>=1)指针（即节点已经在第1层到第i层链表中），那么它有第(i+1)层指针的概率为p */
    #define ZSKIPLIST_P 0.25      /* Skiplist P = 1/4 */
    ```

  + 




## API

