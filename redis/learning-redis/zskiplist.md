# Skiplist 跳跃表

## 介绍

+ 特点
  + 效率与红黑树相近，但实现简单
    + 复杂度：查询、插入、删除的平均复杂度均为O(logN)，最差情况为O(N)
  + 应用：有序集合的底层实现
+ 介绍：
  + 跳跃表由很多层构成
  + 跳跃表有一个头节点header，头节点有一个32层的结构，每层结构包含指向本层的下一个节点的指针，指向本层下一个节点中间跨越的节点为本层的跨度（span）
  + 除头节点header之外层数最多的节点的层高为跳跃表的高度（level）
  + **每层**都是一个有序链表，**数据递增**
  + 除头节点header之外，一个元素在上层有序链表中出现，则它一定会在下层有序链表中出现
  + 跳跃表每层的最后一个节点指向NULL，表示本层有序链表的结束
  + 跳跃表拥有一个尾指针tail，指向跳跃表的最后一个节点
  + **最底层的有序链表包含所有节点**，最底层的节点个数为跳跃表的长度length（不包含头节点header）
  + 每个节点包含一个后退指针backward，头节点和第一个节点指向NULL，其他节点指向最底层的前一个节点
+ 代码
  + server.h和t_zset.c

## 数据结构

+ zskiplist节点

  + ```c
    typedef struct zskiplistNode {
        sds ele;
        double score;
        struct zskiplistNode *backward;
        struct zskiplistLevel {
            struct zskiplistNode *forward;
            unsigned long span;
        } level[];
    } zskiplistNode;
    ```
    
  + 包含：
  
    + ele：用于存储节点的数据
    + score：用于存储排序的分值（按从小到大排序）
    + backward：后退指针，用于从后向前遍历跳跃表。指向当前节点最底层的前一个节点。头节点和第一个节点的backward指针指向NULL
    + level：柔性数组，每个节点的数组长度不一样，在生成跳跃表节点时，随机生成一个1~32的值，值越大出现的概率越低。level数组包含：
    + forward：指向本层下一个节点，尾节点的forward指向NULL
      + span：forward指向的节点与本节点之间的元素个数。span值越大，跳过的节点个数越多

+ zskiplist结构

  + ```c
    typedef struct zskiplist {
        struct zskiplistNode *header, *tail;
        unsigned long length;
        int level;
    } zskiplist;
    ```

  + 包含

    + header：头节点，是跳跃表的一个特殊节点，它的level数组包含32个元素。头节点中，ele为NULL，score为0；且头节点不计入跳跃表的总长度。头节点初始化时，level数组的32个元素的forward都指向NULL，span值为0
    + tail：尾节点
    + length：跳跃表总节点数（不包含头节点）
    + level：跳跃表总层数，即所有节点层数的最大值
    
  + 图示：

    + <img src="https://github.com/baozi929/Notes/blob/main/redis/learning-redis/figures/zskiplist_zskiplist.png" width="600"/>

+ 节点层高

  + 参数

    + ```c
      #define ZSKIPLIST_MAXLEVEL 32
      #define ZSKIPLIST_P 0.25
      ```

    + 参数说明：

      + ZSKIPLIST_MAXLEVEL：zskiplist节点最大层数（从64改为32的原因见https://github.com/redis/redis/pull/6818）
      + ZSKIPLIST_P：zskiplist如果一个节点有第i层(i>=1)指针（即节点已经在第1层到第i层链表中），那么它有第(i+1)层指针的概率为p

  + 节点层高

    + 通过以下函数随机生成

      + ```c
        int zslRandomLevel(void) {
            int level = 1;
            while ((random()&0xFFFF) < (ZSKIPLIST_P * 0xFFFF))
                level += 1;
            return (level<ZSKIPLIST_MAXLEVEL) ? level : ZSKIPLIST_MAXLEVEL;
        }
        ```

      + 说明：

        + level的初始值为1，通过while循环，每次生成一个随机数，如果其低16位小于0xFFFF*0.25，则level值+1，否则退出。返回值为level与ZSKIPLIST_MAXLEVEL的较小值

    + 层高与概率

      + 假设p=ZSKIPLIST_P

        + 节点层高为1的概率为(1-p)

        + 节点层高为2的概率为p(1-p)

        + 节点层高为3的概率为p^2(1-p)

        + ....

        + 节点层高为n的概率为p^(n-1)(1-p)

        + 节点期望层高
          $$
          \begin{align}
          E= & 1\times(1-p)+2\times p(1-p)+3\times p^2(1-p)+...... \\
           = & (1-p)\sum_1^\infty ip^{i-1} \\
           = & 1/(1-p)
          \end{align}
          $$

## 操作

+ 创建跳跃表

  + 创建跳跃表节点

    + ```
      zskiplistNode *zslCreateNode(int level, double score, sds ele) {
      	zskiplistNode *zn =
              zmalloc(sizeof(*zn)+level*sizeof(struct zskiplistLevel));
      
          zn->score = score;
          zn->ele = ele;
          return zn;
      }
      ```

    + 说明：

      + 申请内存：为节点本身以及level数组申请内存（层数为level，该值在创建节点时已确定），即一个zskiplistNode和level个zskiplistLevel的内存大小之和
      + 初始化：创建zskiplistNode时，分值score和数据ele已确定，由此对zskiplistNode的成员变量进行初始化

  + 创建跳跃表

    + ```
      zskiplist *zslCreate(void) {
          int j;
          zskiplist *zsl;
      
          zsl = zmalloc(sizeof(*zsl));
          zsl->level = 1;
          zsl->length = 0;
          zsl->header = zslCreateNode(ZSKIPLIST_MAXLEVEL,0,NULL);
          for (j = 0; j < ZSKIPLIST_MAXLEVEL; j++) {
              zsl->header->level[j].forward = NULL;
              zsl->header->level[j].span = 0;
          }
          zsl->header->backward = NULL;
          zsl->tail = NULL;
          return zsl;
      }
      ```

    + 说明：

      + 创建跳跃表结构体对象zsl，为其分配内存并初始化层数level = 1，长度length = 0
      + 创建头结点，并将跳跃表的header指向头结点
        + 头结点层数level为ZSKIPLIST_MAXLEVEL，分数score为0，数据ele为NULL
        + level数组中的每一个元素的forward为NULL，span为0
        + 头结点的后退指针为NULL
      + 跳跃表的尾节点为NULL

+ 插入节点

  + ```
    ```

  + 

+ 删除节点

+ 删除跳跃表




## API

