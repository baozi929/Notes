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

  + 变量介绍

    + ```
      zskiplistNode *zslInsert(zskiplist *zsl, double score, sds ele) {
          zskiplistNode *update[ZSKIPLIST_MAXLEVEL], *x;
          unsigned int rank[ZSKIPLIST_MAXLEVEL];
          int i, level;
      
          serverAssert(!isnan(score));
      	......
      ```

    + zsl：跳跃表地址

    + score：插入元素的分数

    + ele：插入元素的数据

    + update：32个元素的数组，用于记录每层待插入元素的前一个元素

    + rank：32个元素的数组，用于记录前置节点与第一个节点之间的跨度，即元素在列表中的排名-1

  + 查找要插入节点的位置

    + ```
      x = zsl->header;
      for (i = zsl->level-1; i >= 0; i--) {
          rank[i] = i == (zsl->level-1) ? 0 : rank[i+1];
          while (x->level[i].forward &&
                  (x->level[i].forward->score < score ||
                      (x->level[i].forward->score == score &&
                      sdscmp(x->level[i].forward->ele,ele) < 0)))
          {
              rank[i] += x->level[i].span;
              x = x->level[i].forward;
          }
          update[i] = x;
      }
      ```

    + 图示：

      + <img src="https://github.com/baozi929/Notes/blob/main/redis/learning-redis/figures/zskiplist_zskiplist.png" width="600"/>
      + 跳跃表长度为3，高度为2。假设要插入一个节点，分值为31，层高为3
      + 节点插入逻辑：
        + x为跳跃表头节点
        + 进入for循环
          + 第一次进入循环，i = 2 - 1 = 1
            + `i == zsl->level-1`，因此`rank[1]=0`
            + while循环
              + 第一次循环
                + while循环条件
                  + `x->level[1].forward`存在
                  + `x->level[1].forward->score == 1`，该值小于待插入节点的score 31，不需要进入后续判断，进入while循环
                + 进入循环，更新rank[1]和x
                  + `rank[1] = rank[1] + x->level[1].span = 0 + 1 = 1`
                  + `x = x->level[1].forward`，即为第一个节点（score=1）
              + 第二次
                + while循环条件
                  + x为第一个节点，`x->level[1].forward=NULL`，因此退出循环
            + update[1]更新为x，即第一个节点（score=1）
          + 第二次进入循环，i = 0，x为跳跃表第一个节点（score=1）
            + `i != zsl->level-1`，因此`rank[0] = rank[1] = 1`
            + while循环
              + 第一次循环
                + while循环条件
                  + `x->level[0].forward`存在
                  + `x->level[0].forward->score == 21`，该值小于待插入节点的score 31，进入while循环
                + 进入循环，更新rank[0]和x
                  + `rank[0] = rank[0] + x->level[0].span = 1 + 1 = 2 `
                  + `x = x->level[0].forward`，即为第二个节点（score=21）
              + 第二次循环
                + while循环条件
                  + x为第二个节点，`x = x->level[0].forward`存在
                  + `x->level[0].forward->score == 41`大于要插入的score 31，因此退出循环
      + 经过上述过程，update数组和rank被赋值为：
        + <img src="https://github.com/baozi929/Notes/blob/main/redis/learning-redis/figures/zskiplist_zslInsert_1.png" width="600"/>

  + 调整跳跃表高度

    + ```
      level = zslRandomLevel();
      if (level > zsl->level) {
          for (i = zsl->level; i < level; i++) {
              rank[i] = 0;
              update[i] = zsl->header;
              update[i]->level[i].span = zsl->length;
          }
          zsl->level = level;
      }
      ```

    + 说明：

      + 插入节点的高度是随机的，通过函数zslRandomLevel()确定新插入节点的层高
      + 假设插入节点的高度为3，大于当前跳跃表的高度2，则需要调整跳跃表的高度
        + for循环：i = 2; i < 3; i++
          + i = 2
            + `rank[2] = 0`
            + `update[2] = zsl->header`
            + `update[2]->level[2].span = zsl->length = 3`
      + 经过上述过程，调整高度，更新rank[2]，更新update[2]
        + <img src="https://github.com/baozi929/Notes/blob/main/redis/learning-redis/figures/zskiplist_zslInsert_2.png" width="600"/>

  + 插入节点

    + ```
      x = zslCreateNode(level,score,ele);
      for (i = 0; i < level; i++) {
          x->level[i].forward = update[i]->level[i].forward;
          update[i]->level[i].forward = x;
      
          x->level[i].span = update[i]->level[i].span - (rank[0] - rank[i]);
          update[i]->level[i].span = (rank[0] - rank[i]) + 1;
      }
      ```

    + 说明：

      + 首先根据level、score、ele创建一个新节点x
      + for循环：i = 0; i < 3; i++
        + 第一次循环：i = 0
          + x节点第0层的后退节点指向update[0]，前进节点指向update[0]->level[0].forward
          + 更新span
            + x与x第0层后退节点：rank[0] - rank[0] + 1 = 1
            + x与x第0层前进节点：update[0]->level[0].span - (rank[0] - rank[0]) = 1 - 0 = 1
        + 第二次循环：i = 1
          + x节点第1层的后退节点指向update[1]，前进节点指向update[1]->level[1].forward（NULL）
          + 更新span
            + x与x第1层后退节点：rank[0] - rank[1] + 1 = 1 + 1 = 2
            + x与x第1层前进节点：update[1]->level[1].span - (rank[0] - rank[1]) = 2 - 1 = 1
        + 第三次循环：i = 2
          + x节点第2层的后退节点指向update[2]，前进节点指向update[2]->level[2].forward（NULL）
          + 更新span
            + x与x第1层后退节点：rank[0] - rank[2] + 1 = 2 + 1 = 3
            + x与x第1层前进节点：update[2]->level[2].span - (rank[0] - rank[2]) = 3 - 2 = 1

  + 更新没有用到（层数比插入节点高）的update数组的span（+1）

    + ```
      for (i = level; i < zsl->level; i++) {
          update[i]->level[i].span++;
      }
      ```

    + 说明：

      + 在上述例子中，不需要更新。但如果插入节点高度<跳跃表的高度，则需要更新层数高于插入节点的update节点对应的span（span+1因为中间多了一个新插入的节点）

  + 更新backward

    + ```
      x->backward = (update[0] == zsl->header) ? NULL : update[0];
      if (x->level[0].forward)
          x->level[0].forward->backward = x;
      else
          zsl->tail = x;
      zsl->length++;
      return x;
      ```

    + 说明：

      + 如果update[0]为跳跃表的头结点，新插入节点的backward为NULL；否则其backward节点为update[0]
      + 前进节点为0，则该节点为尾节点；否则，需要更新前进节点的backward为新插入节点
      + 跳跃表的长度更新（+1）

  + 最后插入节点后的跳跃表为

    + <img src="https://github.com/baozi929/Notes/blob/main/redis/learning-redis/figures/zskiplist_zslInsert_final.png" width="600"/>

+ 删除节点

  + ```
    int zslDelete(zskiplist *zsl, double score, sds ele, zskiplistNode **node) {
        zskiplistNode *update[ZSKIPLIST_MAXLEVEL], *x;
        int i;
    
        x = zsl->header;
        for (i = zsl->level-1; i >= 0; i--) {
            while (x->level[i].forward &&
                    (x->level[i].forward->score < score ||
                        (x->level[i].forward->score == score &&
                         sdscmp(x->level[i].forward->ele,ele) < 0)))
            {
                x = x->level[i].forward;
            }
            update[i] = x;
        }
    
        x = x->level[0].forward;
        if (x && score == x->score && sdscmp(x->ele,ele) == 0) {
            zslDeleteNode(zsl, x, update);
            if (!node)
                zslFreeNode(x);
            else
                *node = x;
            return 1;
        }
        return 0; /* not found */
    }
    ```

  + 说明：

    + 获取update数组，记录每层待删除元素的前一个元素

    + 从将节点x从跳跃表上断开

      + ```
        void zslDeleteNode(zskiplist *zsl, zskiplistNode *x, zskiplistNode **update) {
            int i;
            for (i = 0; i < zsl->level; i++) {
                if (update[i]->level[i].forward == x) {
                    update[i]->level[i].span += x->level[i].span - 1;
                    update[i]->level[i].forward = x->level[i].forward;
                } else {
                    update[i]->level[i].span -= 1;
                }
            }
        
            if (x->level[0].forward) {
                x->level[0].forward->backward = x->backward;
            } else {
                zsl->tail = x->backward;
            }
        
            while(zsl->level > 1 && zsl->header->level[zsl->level-1].forward == NULL)
                zsl->level--;
            zsl->length--;
        }
        ```

      + 说明：

        + 更新当前节点x每一层后退节点update[i]与x前进节点的关系，并更新update[i]对应span
        + 更新x的前进节点的后退节点（因为要删除x节点，所以指向x的后置节点）；如果x是最后一个节点，zsl的尾节点为x的后退节点
        + 更新跳跃表最大层数（如果删除的是最高节点，且该层没有其他节点）和节点计数（-1）

    + 释放节点x

+ 删除跳跃表

  + ```
    void zslFree(zskiplist *zsl) {
        zskiplistNode *node = zsl->header->level[0].forward, *next;
    
        zfree(zsl->header);
        while(node) {
            next = node->level[0].forward;
            zslFreeNode(node);
            node = next;
        }
    
        zfree(zsl);
    }
    ```

    + 说明：
      + 从头结点的第0层开始，通过forward指针向后遍历，每遇到一个节点释放其内存。当所有节点的内存都被释放之后，释放跳跃表结构的内存。




## API

