# quicklist快速列表

## 介绍

+ 介绍
  + List的底层实现，在Redis 3.2版本引入
    + 引入之前，采用压缩列表ziplist和双向链表adlist作为List的底层实现
      + 元素较少时且保存的对象为小整数值或长度较短的字符串，使用压缩列表ziplist
        + 优点：节省空间
        + 缺点：存储空间连续，元素个数较多时，修改需要重新分配内存，影响效率
      + 其他情况，使用双向链表adlist
    + 引入quicklist的原因
      + 在时间效率和空间效率进行折中
  + quicklist的结构
    + 双向链表，每个节点为一个ziplist结构
      + 若每个节点为只有1个entry的ziplist，退化为双向链表
      + 若quicklist只有1个元素，退化为ziplist

+ 代码
  + quicklist.h和quicklist.c
  + lzf压缩：lzf.h、lzf_c.c和lzf_d.c

## 数据结构

+ quicklistNode

  + ```
    typedef struct quicklistNode {
        struct quicklistNode *prev;
        struct quicklistNode *next;
        unsigned char *zl;
        unsigned int sz;
        unsigned int count : 16;
        unsigned int encoding : 2;
        unsigned int container : 2;
        unsigned int recompress : 1;
        unsigned int attempted_compress : 1;
        unsigned int extra : 10;
    } quicklistNode;
    ```

  + 说明

    + 占32字节
      + 3个指针各占64位，即8bytes
      + unsigned int占4bytes
      + 最后有一些占16、2、2、1、1、10位的unsigned int，加起来4bytes

+ quicklistNode中的ziplist中的一个Entry

  + ```
    typedef struct quicklistEntry {
        const quicklist *quicklist;
        quicklistNode *node;
        unsigned char *zi;
        unsigned char *value;
        long long longval;
        unsigned int sz;
        int offset;
    } quicklistEntry;
    ```
  
+ quicklist

  + ```
    typedef struct quicklist {
        quicklistNode *head;
        quicklistNode *tail;
        unsigned long count;
        unsigned long len;
        int fill : QL_FILL_BITS;
        unsigned int compress : QL_COMP_BITS;
        unsigned int bookmark_count: QL_BM_BITS;
        quicklistBookmark bookmarks[];
    } quicklist;
    ```

  + 说明：

    + head、tail：指向quicklist的头尾节点

    + count：所有ziplist中entry的数量和

    + len：quicklist中quicklistNode的数量

    + fill：每个节点的最大容量，可通过Redis修改参数list-max-ziplist-size来配置节点所占内存大小

      + fill为正数时，表示每个ziplist最多包含的数据项数

      + fill为负数时：

        + | 数值 | 含义                  |
          | ---- | --------------------- |
          | -1   | ziplist节点最大为4KB  |
          | -2   | ziplist节点最大为8KB  |
          | -3   | ziplist节点最大为16KB |
          | -4   | ziplist节点最大为32KB |
          | -5   | ziplist节点最大为64KB |

    + compress：quicklist的压缩深度，与LZF相关；0=off。quicklistNode节点个数较多时，我们经常访问两端的数据，为了进一步节省空间，Redis允许对中间的quicklistNode节点进行压缩。可通过Redis修改参数list-compress-depth来设置compress参数。该值的具体含义：两端各有compress个节点不压缩

      + 图示：3个节点，compress=1
        + <img src="https://github.com/baozi929/Notes/blob/main/redis/learning-redis/figures/quicklist_compress.png" width="600"/>

    + quicklistBookmark：在给quicklist分配内存时使用，不使用时不占用空间

+ quicklistLZF

  + ```
    typedef struct quicklistLZF {
        unsigned int sz; /* LZF size in bytes*/
        char compressed[];
    } quicklistLZF;
    ```

  + 说明：

    + quicklistNode如果是LZF压缩的类型，则`quicklistNode->zl`会指向一个quicklistLZF

+ 迭代器quicklistIter

  + ```
    typedef struct quicklistIter {
        const quicklist *quicklist;
        quicklistNode *current;
        unsigned char *zi;
        long offset; /* offset in current ziplist */
        int direction;
    } quicklistIter;
    ```




## 数据压缩

+ ~~有需要再研究具体实现，这里就大概了解一下~~

+ 说明

  + quicklistNode使用ziplist，其主要优势在与节省存储空间
  + 使用LZF压缩可以进一步降低占用的空间

+ LZF压缩后的数据结构图

  + | 解释字段 | 数据 | ...  | 解释字段 | 数据 |
    | -------- | ---- | ---- | -------- | ---- |

  + 说明：

    + 解释字段（LZF压缩格式有3种，因此解释字段有3种）
      + 字面型
        + 解释字段占用1字节
          + 格式：000L LLLL
            + 数据长度：由解释字段后5位决定（LLLLL+1为数据的长度）
          + 例：0000 0001表示数据长度为2
      + 简短重复型
        + 解释字段占用2字节，没有数据字段
          + 格式：LLLo oooo oooo oooo
            + 数据长度：由解释字段前三位决定（LLL+2为数据的长度）
            + 偏移量：所有o组成的字面值+1（o oooo oooo oooo+1）
            + 数据内容为前面数据内容的重复，重复长度小于8
          + 例：0010 0000 0000 0100表示前面5字节处内容重复，重复3个字节
      + 批量重复型
        + 解释字段占3个字节，没有数据字段，数据内容与全面内容重复
          + 格式：111o oooo LLLL LLLL oooo oooo
            + 数据长度：所有L组成的字面值+9
            + 偏移量：所有o组成的字面值+1
          + 例：1110 0000 0000 0010 0001 0000
            + 长度：0000 0010 + 9 = 11
            + 偏移量：0 0000 0001 0000 + 1 = 17
            + 说明：与前面17字节处内容重复，重复17字节

+ 压缩

  + LZF数据压缩的基本思想：数据与前面重复的，记录重复位置以及重复长度；否则，记录原始数据内容
  + 算法流程：
    + 遍历输入字符串，对当前字符串及其后面2个字符进行哈希运算。
    + 如果哈希表中找到曾经出现的记录，则计算重复字节的长度及位置，反之直接输出数据

+ 解压缩

  + 根据压缩后的数据格式可进行解压

## 操作

+ 新建与初始化

  + ```
    quicklist *quicklistCreate(void) {
        struct quicklist *quicklist;
    
        quicklist = zmalloc(sizeof(*quicklist));
        quicklist->head = quicklist->tail = NULL;
        quicklist->len = 0;
        quicklist->count = 0;
        quicklist->compress = 0;
        quicklist->fill = -2;
        quicklist->bookmark_count = 0;
        return quicklist;
    }
    ```

  + 

+ 添加元素

+ 删除元素

+ 更改元素

+ 查找元素



## API

