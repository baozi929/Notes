# Stream 消息队列

## 简介

+ Stream由消息、生产者、消费者、消费组四部分组成
+ 结构图：
  + <img src="https://github.com/baozi929/Notes/blob/main/redis/learning-redis/figures/stream_stream.png" width="400"/>
+ 说明
  + 消息
    + 每个消息有唯一的消息ID，消息ID严格递增
    + 消息内容由多可field-value对组成
  + 生产者
    + 负责向消息队列中生产消息
  + 消费者
    + 消费某个消息流
    + 可以归属于某个消费组，也可以不归属任何消费组。当消费者不归属于任何消费组时，该消费者可以消费消息队列中的任何消息
  + 消费组
    + 每个消费组通过组名唯一标识，每个消费组都可以消费该消息队列的全部消息，多个消费组之间相互独立
    + 每个消费组可以有多个消费者，消费者通过名称唯一标识，消费者之间的关系是竞争关系，即一个消息只能由该组的一个成员消费
    + 组内成员消费消息后需要确认，每个消息组都有一个待确认消息队列（pending entry list，简称pel），用以维护该消费组已消费但没确认的消息
    + 消费组中每个成员也有一个待确认消息队列，维护该消费者已消费但尚未确认的消息
+ Stream的底层实现
  + listpack
  + Rax树
+ 代码
  + stream.h
  + listpack.c、listpack.h、listpack_malloc.h
  + rax.h、rax.c、rax_malloc.h
  

## listpack

+ 什么是listpack

  + A lists of strings serialization format，即将一个字符串列表进行序列化存储
  + 作用：存储字符串或整型

+ 结构图

  + <img src="https://github.com/baozi929/Notes/blob/main/redis/learning-redis/figures/stream_listpack.png" width="400"/>

  + 说明：

    + Total Bytes：4字节

      + 整个listpack的空间大小，每个listpack最多占2^32-1个字节（4,294,967,295字节）

    + Num Elem：2字节

      + listpack中元素个数，即entry的个数
      + 注意：可以存放大于65535个entry，此时Num Elem被设置为65535。此时若需要获取元素个数，需要遍历整个listpack

    + Entry

      + listpack的元素，内容为字符串或整型

      + 每个entry分为3个部分

        + encode：1字节，为该元素的编码方式

          + | Encode的内容 | 含义                                                         |
            | ------------ | ------------------------------------------------------------ |
            | 0xxx xxx     | 7位无符号整型，后七位为数据(content)                         |
            | 10LL LLLL    | 6为长度的字符串，后6bit为字符串长度，之后为字符串内容        |
            | 110x xxx     | 13位整型，后5bit以及下个字节为数据内容                       |
            | 1110 LLLL    | 12位长度的字符串，后4bit以及下个字节为字符串长度，之后为字符串内容 |
            | 1111 0000    | 32位长度的字符串，后4字节为字符串长度，之后为字符串内容      |
            | 1111 0001    | 16位整型，后2个字节为数据                                    |
            | 1111 0010    | 24位整型，后3个字节为数据                                    |
            | 1111 0011    | 32位整型，后4个字节为数据                                    |
            | 1111 0100    | 64位整型，后8个字节为数据                                    |

        + content：

          + 整型
            + 不实际存储负数，而是将负数转换为正数进行存储
            + 例：13位整型存储中，存储范围[0,8191]，其中[0,4095]对应非负的[0,4095]，[4096,8191]对应[-4096,-1]

        + backlen：占用的字节数小于等于5，记录Entry的长度（Encode+content），但不包含backlen自身长度

          + 第一个bit用于标识：0代表结束，1代表尚未结束。因此每个byte只有7个字节有效
          + 常用场景：从后向前遍历时，当我们需要找到当前元素的上一个元素时，可以从后向前依次查找每个字节，找到上个Entry的backlen字段的结束标识，从而可以计算上一个元素的长度
          + 例：backlen为0000 0001 1000 1000，即该元素的长度为00 0000 1000 1000，即136字节。由此可以找到Entry的首地址

    + End：1字节

      + listpack的结束标志，内容为0xFF



## Rax树

+ 介绍
  + 前缀树是字符串查找时常用的数据结构，能在字符串集合中快速查找到某个字符串。但由于每个节点只存储字符串中的一个字符，有时会造成空间的浪费
  + Rax的出现就是为了解决这个问题。它不仅可以存储字符串，还可以为字符串设置一个值，即key-value对
  + Rax树通过节点压缩节省时间
    + 图示：
      + <img src="https://github.com/baozi929/Notes/blob/main/redis/learning-redis/figures/stream_rax.png" width="400"/>

+ 结构

  + Rax树

    + ```
      typedef struct rax {
          raxNode *head;
          uint64_t numele;
          uint64_t numnodes;
      } rax;
      ```

    + 说明：

      + head：指向头节点
      + numele：元素个数（key的个数）
      + numnodes：节点个数

  + Rax树的节点

    + ```
      typedef struct raxNode {
          uint32_t iskey:1;     /* Does this node contain a key? */
          uint32_t isnull:1;    /* Associated value is NULL (don't store it). */
          uint32_t iscompr:1;   /* Node is compressed. */
          uint32_t size:29;     /* Number of children, or compressed string len. */
          unsigned char data[];
      } raxNode;
      ```

    + 说明：

      + iskey：1bit，当前节点是否包含1个key
      + isnull：1bit，当前key对应的value是否为空
      + iscmpr：1bit，当前节点是否为压缩节点
      + size：29bit，压缩节点压缩的字符串长度，或非压缩节点的子节点个数
      + data：包含填充字段，同时存储当前节点包含字符串以及子节点的指针、key对应的value指针

    + raxNode的类型：

      + 压缩节点

        + 结构图

          + | iskey | isnull | iscompr=1 | size=3 | A    | B    | C    | pad  | C-ptr | value-ptr? |
            | ----- | ------ | --------- | ------ | ---- | ---- | ---- | ---- | ----- | ---------- |

          + 说明：

            + iskey为1且isnull为0时，value-ptr存在，否则value-ptr不存在
            + iscompr=1，即该节点为压缩节点
            + size=3，即存储了3个字符；紧接着的三个字符即为该节点存储的字符串，根据字符串的长度确定是否需要填充字段（填充必要的字节，使得后面的指针地址放到何时的位置）
            + 因为是压缩节点，所以只有最后一个字符有节点

      + 非压缩节点

        + 结构图

          + | iskey | isnull | iscompr=0 | size=2 | X    | Y    | pad  | X-ptr | Y-ptr | value-ptr? |
            | ----- | ------ | --------- | ------ | ---- | ---- | ---- | ----- | ----- | ---------- |

            

          + 说明：

            + 与压缩节点的区别：每个字符都有一个子节点。
            + 字符个数小于2时，都是非压缩节点。

  + raxStack结构

    + ```
      typedef struct raxStack {
          void **stack;
          size_t items, maxitems;
          void *static_items[RAX_STACK_STATIC_ITEMS];
          int oom;
      } raxStack;
      ```

    + 说明：

      + stack：记录路径，指针可能指向static_items（路径较短时）或堆空间内存
      + items，maxitems：stack指向的空间的已用空间以及最大空间
      + static_items：数组，数组中的每个元素都是指针，用于存储路径
      + oom：当前栈是否出现过内存溢出

    + 作用：

      + 存储从根节点到当前节点的路径

  + raxIterator

    + ```
      typedef struct raxIterator {
          int flags;
          rax *rt;                /* Radix tree we are iterating. */
          unsigned char *key;     /* The current string. */
          void *data;             /* Data associated to this key. */
          size_t key_len;         /* Current key length. */
          size_t key_max;         /* Max key len the current key buffer can hold. */
          unsigned char key_static_string[RAX_ITER_STATIC_LEN];
          raxNode *node;          /* Current node. Only for unsafe iteration. */
          raxStack stack;         /* Stack used for unsafe iteration. */
          raxNodeCallback node_cb; /* Optional node callback. Normally set to NULL. */
      } raxIterator;
      ```

    + flag：迭代器标志位，目前有以下三种：

      + RAX_ITER_JUST_SEEKED：表示迭代器指向的元素时刚刚搜索过的
      + RAX_ITER_EOF：表示当前迭代器已经遍历到Rax树的最后一个节点
      + RAX_ITER_SAFE：表示当前迭代器为安全迭代器

    + rt：迭代器当前遍历的Rax树

    + key：迭代器当前遍历到的key，该指针指向key_static_string或者从堆中申请的内存

    + data：与key关联的value

    + key_len：当前key的长度

    + key_max：当前key buffer最长允许的key长度

    + key_static_string：key的默认存储空间，当key比较大时，会使用堆空间内存

    + node：当前key所在的raxNode

    + stack：记录从根节点到当前节点的路径，用于raxNode的向上遍历

    + node_cb：节点的callback函数，通常为NULL

