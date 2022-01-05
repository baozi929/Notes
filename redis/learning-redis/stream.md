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
  + stream.h、t_stream.c
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

        + encoding：1字节，为该元素的编码方式

          + | encoding的内容 | 含义                                                         |
            | -------------- | ------------------------------------------------------------ |
            | 0xxx xxx       | 7位无符号整型，后七位为数据(content)                         |
            | 10LL LLLL      | 6为长度的字符串，后6bit为字符串长度，之后为字符串内容        |
            | 110x xxx       | 13位整型，后5bit以及下个字节为数据内容                       |
            | 1110 LLLL      | 12位长度的字符串，后4bit以及下个字节为字符串长度，之后为字符串内容 |
            | 1111 0000      | 32位长度的字符串，后4字节为字符串长度，之后为字符串内容      |
            | 1111 0001      | 16位整型，后2个字节为数据                                    |
            | 1111 0010      | 24位整型，后3个字节为数据                                    |
            | 1111 0011      | 32位整型，后4个字节为数据                                    |
            | 1111 0100      | 64位整型，后8个字节为数据                                    |

        + content：

          + 整型
            + 不实际存储负数，而是将负数转换为正数进行存储
            + 例：13位整型存储中，存储范围[0,8191]，其中[0,4095]对应非负的[0,4095]，[4096,8191]对应[-4096,-1]

        + backlen：占用的字节数小于等于5，记录Entry的长度（encoding+content），但不包含backlen自身长度

          + 第一个bit用于标识：0代表结束，1代表尚未结束。因此每个byte只有7个字节有效
          + 常用场景：从后向前遍历时，当我们需要找到当前元素的上一个元素时，可以从后向前依次查找每个字节，找到上个Entry的backlen字段的结束标识，从而可以计算上一个元素的长度
          + 例：backlen为**0**000 0001 **1**000 1000，即该元素的长度为00 0000 1000 1000，即136字节。由此可以找到Entry的首地址

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

## Stream结构

+ 介绍：

  + 由下图可见，每个消息流包含一个Rax结构。

+ 图示

  + <img src="https://github.com/baozi929/Notes/blob/main/redis/learning-redis/figures/stream_stream_structure.png" width="600"/>
  + 说明：
    + Redis Stream的实现依赖于Rax结构以及listpack结构。以消息ID为key，listpack结构为value存储在Rax结构中
    + 每个消息的具体信息存储在listpack中
      + 每个listpack都有一个master entry，该结构中存储了创建listpack时待插入消息的所有field。原因：同一个消息流，消息内容通常有相似性，如果后续消息的field与master entry内容相同，则不需要存储其field
      + 每个listpack中可能存储多条信息

+ 消息存储

  + 消息ID，128位

    + ```
      typedef struct streamID {
          uint64_t ms;        /* Unix time in milliseconds. */
          uint64_t seq;       /* Sequence number. */
      } streamID;
      ```

  + 消息存储的格式

    + 介绍：

      + stream的消息内容存储在listpack中，listpack可用于存储字符串或整型数据
      + listpack中的单个元素称为entry，**消息存储格式的每一个字段都是一个entry，并不是将整个消息作为字符串存储的**
      + 每个listpack会存储多个消息，存储消息个数由stream-node-max-bytes（listpack节点最大占用的内存数）和stream-node-max-entries（每个listpack最大存储的元素个数）决定
      + 每个消息会占用多个listpack entry
      + 每个listpack会存储多个消息
      + 每个listpack在创建时，会构造该节点的master entry（根据第一个插入的消息构建）

    + listpack master entry结构

      + | count | deleted | num-fields | field-1 | field-2 | ...  | field-N | 0    |
        | ----- | ------- | ---------- | ------- | ------- | ---- | ------- | ---- |

      + 说明：

        + count：当前listpack中所有未删除的消息个数
        + deleted：当前listpack中所有已删除的消息个数
        + num-fields：field的个数
        + field-1,...,field-N：当前listpack中第一个插入的消息的所有field
        + 0：标志位，从后向前遍历该listpack所有消息时使用

      + 注意：

        + **此处的字段（如count、deleted等）都是listpack的一个元素**，此处省略了listpack每个元素存储式的encoding以及backlen字段

        + 存储一个消息时，如果**该消息的field与master entry的field完全相同**，则不需要再次存储field，此时存储的消息：

          + | flags | streamID.ms | streamID.seq | value-1 | ...  | value-N | Ip-count |
            | ----- | ----------- | ------------ | ------- | ---- | ------- | -------- |

          + 说明：

            + flags：消息标志位
              + STREAM_ITEM_FLAG_NONE：无特殊标识
              + STREAM_ITEM_FLAG_DELETED：该消息已被删除
              + STREAM_ITEM_FLAG_SAMEFIELDS：该消息的field与master entry完全相同
            + streamID.ms和streamID.seq：消息ID减去master entry id之后的值
            + value：存储了该消息每个field对应的内容
            + Ip-count：该消息占用listpack的元素个数，即3+N

        + 消息的field与master entry的field不完全相同时的消息存储：

          + | flags | streamID.ms | streamID.seq | num-fields | field-1 | value-1 | ...  | field-N | value-N | Ip-count |
            | ----- | ----------- | ------------ | ---------- | ------- | ------- | ---- | ------- | ------- | -------- |

          + 说明：

            + flags、streamID.ms、streamID.seq和之前一样
            + num-fields：该消息field的个数
            + field-value：存储消息的域-值对，即消息的具体内容
            + Ipcount：该消息占用的listpack的元素个数，即4+2N

+ 关键结构体介绍

  + stream

    + ```
      typedef struct stream {
          rax *rax;               /* The radix tree holding the stream. */
          uint64_t length;        /* Number of elements inside this stream. */
          streamID last_id;       /* Zero if there are yet no items. */
          rax *cgroups;           /* Consumer groups dictionary: name -> streamCG */
      } stream;
      ```

    + 说明：

      + rax：持有stream的rax树。rax树**存储消息生产者生产的具体消息**，每个消息有唯一的ID
        + key：消息ID
        + value：消息内容
        + 注意：rax树中的一个节点可能存储多个消息
      + length：当前stream中的**消息个数**（不包括已经删除的消息）
      + last_id：当前stream中**最后插入的消息的ID**，stream为空时，设置为0
      + cgroups：当前stream相关的消费组，存储在rax树中
        + key：消费组的组名
        + value：streamCG为值

  + 消费组

    + 每个stream有多个消费组，每个消费组通过组名唯一标识，同时关联一个streamCG结构

    + ```
      typedef struct streamCG {
          streamID last_id;
          rax *pel;
          rax *consumers;
      } streamCG;
      ```

    + 说明：

      + last_id：该消费组已确认的最后一个消息的ID
      + pel：该消费组尚未确认的消息
        + key：消息ID
        + value：streamNACK（代表一个尚未确认的消息）
      + consumers：该消费组中所有的消费者
        + key：消费者的名称为
        + value：streamConsumer（代表一个消费者）

  + 消费者

    + 每个消费者通过streamConsumer唯一标识

    + ```
      typedef struct streamConsumer {
          mstime_t seen_time;
          sds name;
          rax *pel;
      } streamConsumer;
      ```

    + 说明：

      + seen_time：该消费者最后一次活跃的时间
      + name：消费者的名称
      + pel：该消费者尚未确认的消息
        + key：消息ID
        + value：streamNACK

  + 未确认消息

    + 维护消费组或消费者尚未确认的消息

      + 注意：消费组的pel中的元素与每个消费者的pel中的元素是**共享**的，即该消费组消费了某个消息，这个消息会同时放到消费组以及该消费者的pel队列中，并且二者是同一个streamNACK结构

    + ```
      typedef struct streamNACK {
          mstime_t delivery_time;
          uint64_t delivery_count;
          streamConsumer *consumer;
      } streamNACK;
      ```

    + 说明：

      + delivery_time：该消息最后发送给消费方的时间
      + delivery_count：该消息已经发送的次数（组内的成员可以通过xclaim命令获取某个消息的处理权，该消息已经分给组内另一个消费者但其没有确认该消息）
      + consumer：该消息当前归属的消费者

  + 迭代器

    + ```
      typedef struct streamIterator {
          stream *stream;
          streamID master_id;
          uint64_t master_fields_count;
          unsigned char *master_fields_start;
          unsigned char *master_fields_ptr;
          int entry_flags;
          int rev;
          uint64_t start_key[2];
          uint64_t end_key[2];
          raxIterator ri;
          unsigned char *lp;
          unsigned char *lp_ele;
          unsigned char *lp_flags;
          unsigned char field_buf[LP_INTBUF_SIZE];
          unsigned char value_buf[LP_INTBUF_SIZE];
      } streamIterator;
      ```

    + 说明：

      + stream：当前迭代器正在遍历的消息流
      + master entry相关（即根据listpack第一个插入的消息构建的entry）：
        + master_id：消息id
        + master_fields_count：master entry中field的个数
        + master_fields_start：master entry field在listpack中的首地址
        + master_fields_ptr： 当listpack中消息的field与master entry的field完全相同时，需要记录当前所在的field的具体位置。master_fields_ptr就是实现这个功能的
      + entry_flags：当前遍历的消息的标志位
      + rev：迭代器方向，True表示从尾->头
      + start_key、end_key：该迭代器处理的消息ID的范围（128bits，大端）
      + ri： rax迭代器，用于遍历rax中的所有key
      + listpack相关指针：
        + lp：指向当前listpack的指针
        + lp_ele：指向当前listpack中正在遍历的元素
        + lp_flags：指向当前消息的flags
      + 从listpack中读取数据时用的缓存：
        + field_buf
        + value_buf



## Listpack的实现

+ 介绍：

  + 在Stream中用于存储消息内容
  + 特点：查询效率低，只适合末尾增删（但消息队列通常只需要在末尾添加消息，所以可以使用该结构）
  
+ 初始化

  + ```
    unsigned char *lpNew(size_t capacity) {
        unsigned char *lp = lp_malloc(capacity > LP_HDR_SIZE+1 ? capacity : LP_HDR_SIZE+1);
        if (lp == NULL) return NULL;
        lpSetTotalBytes(lp,LP_HDR_SIZE+1);
        lpSetNumElements(lp,0);
        lp[LP_HDR_SIZE] = LP_EOF;
        return lp;
    }
    ```

  + 说明：

    + 默认分配7个字节，包含：Total Bytes（4字节） + Num Elem（2字节） + End（1字节）
    + 初始化Total Bytes、Num Elem、End
    
  + 图示

    + | Total Bytes = 7 | Num = 0 | End  |
      | --------------- | ------- | ---- |

+ 增删改

  + 增删改查都是基于lpInsert函数实现
  
  + ```
    unsigned char *lpInsert(unsigned char *lp, unsigned char *ele, uint32_t size, unsigned char *p, int where, unsigned char **newp)
    ```
  
  + 说明：
  
    + lp：当前listpack
    + ele：待插入的新元素或待替换的新元素；如果ele为空，说明需要删除
    + size：ele的长度
    + p：待插入的位置或待替换的元素位置
    + where：LP_BEFORE、LP_AFTER、LP_REPLACE；可以通过转换为一种插入操作以及一种替换操作（2种情况）
      + LP_AFTER可以转换为LP_BEFORE（p指向下一个元素即可）
      + ele为空可以转换为LP_REPLACE
    + newp：用于返回插入/替换的新元素或删除元素的下一个元素；如果指向LP_EOF（删除最后一个元素时，会出现这种情况），则返回NULL
  
  + 函数流程：
  
    + 计算新元素需要的空间
    + 计算插入/替换后整个listpack所需空间，如果需要更过空间，通过realloc申请空间
    + 调整listpack中老元素的位置，为新元素预留空间
    + realloc以释放多余空间
    + 在新的listpack中进行插入或替换操作
    + 更新新的listpack的header（占用字节数Total Bytes和元素数量Num）
  
+ 遍历操作

  + ````
    unsigned char *lpFirst(unsigned char *lp);
    unsigned char *lpLast(unsigned char *lp);
    unsigned char *lpNext(unsigned char *lp, unsigned char *p);
    unsigned char *lpPrev(unsigned char *lp, unsigned char *p);
    ````

  + 说明：

    + 实现一般通过encoding或backlen获取当前entry长度，从而找到目标节点

+ 读取元素

  + 

