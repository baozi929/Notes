# Dict / Hash tables

## 介绍

+ 代码
  + dict.h和dict.c

## 数据结构

+ 哈希表

  + ```
    typedef struct dictht {
        dictEntry **table;
        unsigned long size;
        unsigned long sizemask;
        unsigned long used;
    } dictht;
    ```

  + 包含：

    + table：哈希表数组
      + dictEntry*表示hash节点的指针，dictEntry**表示数组首地址
    + size：哈希表大小，一般为2^n
      + 获取哈希表桶idx可以通过%或&实现，但取模操作比&操作性能差一些
    + sizemask：hash数组长度掩码，一般sizemask = 2^n - 1
    + used：哈希表k-v对的个数

  + 图示（size为4的空哈希表）

    + <img src="https://github.com/baozi929/Notes/blob/main/redis/learning-redis/figures/dict_emptyHashTable.png" width="400"/><br/>

+ 哈希表节点

  + ```
    typedef struct dictEntry {
        void *key;
        union {
            void *val;
            uint64_t u64;
            int64_t s64;
            double d;
        } v;
        struct dictEntry *next;
    } dictEntry;
    ```
    
  + 包含

    + key：键
    + v：保存键对应的值，值可以为指针，uint64_t的整数，int64_t的整数，或double
      + union：所有成员占用同一段内存，同一时刻只能保存一个成员的值
    + next：指向下一个哈希表节点。当多个节点的key对应的哈希值落在同一个哈希桶中的时候，将多个哈希值相同的节点连接在一起，以解决键冲突的问题

  + 图示（包含2个索引相同的键k0和k1的哈希表，二者通过k1节点的next指针连结）

    + <img src="https://github.com/baozi929/Notes/blob/main/redis/learning-redis/figures/dict_hashTableWith2Keys.png" width="600"/>

+ 字典

  + ```
    typedef struct dict {
        dictType *type;
        void *privdata;
    	dictht ht[2];
        long rehashidx;
        int16_t pauserehash;
    } dict;
    ```
    
  + 包含：

    + type：类型特定函数。指向dictType结构的指针，每个dictType结构保存了用于操作特定类型键值对的函数，Redis会为用户不同的字典设置不同的类型特定函数。

      + ```
        typedef struct dictType {
            uint64_t (*hashFunction)(const void *key);
            void *(*keyDup)(void *privdata, const void *key);
            void *(*valDup)(void *privdata, const void *obj);
            int (*keyCompare)(void *privdata, const void *key1, const void *key2);
            void (*keyDestructor)(void *privdata, void *key);
            void (*valDestructor)(void *privdata, void *obj);
            int (*expandAllowed)(size_t moreMem, double usedRatio);
        } dictType;
        ```
  
    + privatedata：私有数据

    + ht：一个字典包含2个哈希表。通常只使用ht[0]，只会在rehash的时候使用ht[1]

    + rehashidx：记录当前rehash进度。如果没有在rehash，rehashidx == -1

    + pauserehash：pauserehash > 0 表示rehash暂停；= 0 表示可以rehash（< 0 说明有bug）。

      + 为什么暂停时需要计数？因为可能同时存在多个安全迭代器和扫描，导致字典的pauserehash的值可能 > 1

  + 图示（未进行rehash的字典）

    + <img src="https://github.com/baozi929/Notes/blob/main/redis/learning-redis/figures/dict_dict.png" width="600"/>

## 哈希算法

+ 插入键值对逻辑：

  + 根据键值对中的键计算哈希值和索引值

    + 哈希值计算：

      + ```
        #define dictHashKey(d, key) (d)->type->hashFunction(key)
        ```

    + 根据哈希值和哈希表的sizemask计算索引（index为桶索引，d为字典，key为键值对的键，ht[x]可为ht[0]或ht[1]）

      + ```
        index = dictHashKey(d, key) & d->ht[x].sizemask;
        ```

  + 根据索引值，将包含键值对的哈希表节点放到哈希表的桶中

    + **哈希冲突**
      + 情况描述：两个及以上相同的键被分配到哈希表的同一个索引上
      + 处理方式：通过链表将这些节点连起来，存在同一个哈希表的桶中。出于效率考虑，每次插入新节点都添加到该链表的表头

+ 哈希算法

  + Redis3.0使用MurmurHash2
  + Redis6.0使用SipHash

## Incremental Rehash

+ 为什么要rehash？

  + 在哈希表的使用过程中，哈希表保存的键值对的数量会发生变化。为了使哈希表的负载因子维持在合理范围，当哈希表保存的键值对过多或过少时，需要对哈希表的大小进行扩展或收缩
    + 负载因子：`d->ht[0].used/d->ht[0].size`

+ Incremental Rehash

  + 什么是Incremental Rehash？

    + 将上述的rehash过程分多次完成

    + 在执行插入、删除、查找、修改等操作前，都判断当前字典是否正在rehash，如果正在rehash，且rehash未处于暂停状态，则帮忙进行一步rehash操作

      + ```
        if (dictIsRehashing(d)) _dictRehashStep(d);
        ```

      + ```
        static void _dictRehashStep(dict *d) {
        	if (d->pauserehash == 0) dictRehash(d,1);
        }
        ```

  + 为什么Incremental Rehash？

    + dict中存在的键值对可能很多，如百万、千万、亿的数量级，如果rehash需要一次性完成，可能导致redis服务器在一段时间内无法服务

+ Rehash的步骤

  + 为字典的哈希表ht[1]分配空间，ht[1]的大小取决于目标扩容大小size（与ht[0]中包含的键值对的数量有关，即ht[0].used）。取第一个大于size的2的整数次幂2^n

    + ```
      unsigned long realsize = _dictNextPower(size);
      ```

    + 将字典的rehashidx标为0，表示将从第0个桶进行迁移

  + 将保存在ht[0]中的所有键值对rehash到ht[1]上

    + 这不是一次完成，而是在字典执行插入、删除、查找、修改等操作时，程序除了执行指定操作之外，还要帮忙做一步rehash操作
      + 计算ht[0]中rehashidx对应的桶中所有键值对对应的哈希值和索引值
      + 将ht[0]中的键值对放到哈希表ht[1]的指定桶中
      + 将rehashidx改为下一个要被迁移的桶的索引

  + 当所有ht[0]中的键值对都迁移到ht[1]上时，迁移完成，需要对字典的状态进行更新，包括

    + ```
      if (d->ht[0].used == 0) {
          zfree(d->ht[0].table);
          d->ht[0] = d->ht[1];
          _dictReset(&d->ht[1]);
          d->rehashidx = -1;
          return 0;
      }
      ```

    + 回收旧哈希表ht[0]的空间

    + 用新哈希表ht[1]替换旧哈希表ht[0]

    + 重置哈希表ht[1]

      + ```
        static void _dictReset(dictht *ht)
        {
            ht->table = NULL;
            ht->size = 0;
            ht->sizemask = 0;
            ht->used = 0;
        }
        ```

    + 并将字典中的rehashidx字段标为-1，表示当前字典未进行rehash

+ 自动rehash的条件

  + 负载因子 >= 1，并且允许自动resize的flag（即dict_can_resize）== 1
  + 负载因子 >= dict_force_resize_ratio （dict_force_resize_ratio = 5）

+ Rehash过程中哈希表的操作

  + rehash过程中，字典会同时使用2个哈希表ht[0]和ht[1]，因此对哈希表的操作需要考虑这一点
    + 对1个表进行操作
      + 插入：新插入的键值对直接放入ht[1]
    + 对2个表进行操作（需要在2个表中找到对应键值对）
      + 删除
      + 查找
      + 修改

## 遍历——dictIterator

+ 字典迭代器

  + ```
    typedef struct dictIterator {
        dict *d;
        long index;
        int table, safe;
        dictEntry *entry, *nextEntry;
        long long fingerprint;
    } dictIterator;
    ```
    
    + d：被迭代的字典指针
    + index：迭代器当前迭代到的桶的索引值
    + table：正在迭代的hash表（ht[0]或ht[1]）
    + save：当前迭代器是否为安全迭代器，1表示安全迭代器
    + entry，nextEntry：当前节点，下一个节点
    + fingerprint：字典的fingerprint，64位整数，ht[0]和ht[1]的table、size、used字段组合形成的hash值（见函数`long long dictFingerprint(dict *d);`）。字典不发生改变时，该值不变；字典发生改变时，改值也发生变化。防止迭代的过程中字典结构被修改。
    
  + 迭代器的大小（64位系统）

    + 48 bytes：dict指针（8 bytes） + long（8 bytes） + 2 int(4 bytes * 2) + 2 dictEntry指针（8 bytes * 2） +  long long（8 bytes）

  + 迭代器相关API：

    + ```
      dictIterator *dictGetIterator(dict *d);
      dictIterator *dictGetSafeIterator(dict *d);
      dictEntry *dictNext(dictIterator *iter);
      void dictReleaseIterator(dictIterator *iter);
      ```

+ 迭代器分类：

  + 普通迭代器

    + ```
      dictIterator *dictGetIterator(dict *d)
      {
          dictIterator *iter = zmalloc(sizeof(*iter));
      
          iter->d = d;
          iter->table = 0;
          iter->index = -1;
          iter->safe = 0;
          iter->entry = NULL;
          iter->nextEntry = NULL;
          return iter;
      }
      ```

    + 说明：

      + 保证数据准确性的方法：fingerprint不应在迭代过程中发生变化
      + 因为对字典进行插入、删除、修改、查找等操作都有可能调用_dictRehashStep()函数（进行incremantal rehash），从而导致fingerprint发生变化
      + 所以普通迭代器**只能用于调用dictNext()**函数来迭代整个字典

  + 安全迭代器

    + ```
      dictIterator *dictGetSafeIterator(dict *d) {
          dictIterator *i = dictGetIterator(d);
      
          i->safe = 1;
          return i;
      }
      ```

    + 说明：

      + 保证读取数据准确性的方法：
        + 使用安全迭代器时，暂停incremental rehash，从而保证字典在迭代器迭代过程中数据不会被重复遍历
      + 安全迭代器即使在迭代时，也可以对字典进行插入、删除、修改、查找等操作
      + 安全迭代器也可以调用dictNext()来遍历字典中的节点

+ 迭代器操作

  + 获取下一个节点dictEntry

    + ```
      dictEntry *dictNext(dictIterator *iter)
      {
          while (1) {
              if (iter->entry == NULL) {
                  dictht *ht = &iter->d->ht[iter->table];
                  if (iter->index == -1 && iter->table == 0) {
                      if (iter->safe)
                          dictPauseRehashing(iter->d);
                      else
                          iter->fingerprint = dictFingerprint(iter->d);
                  }
                  iter->index++;
      
                  if (iter->index >= (long) ht->size) {
                      if (dictIsRehashing(iter->d) && iter->table == 0) {
                          iter->table++;
                          iter->index = 0;
                          ht = &iter->d->ht[1];
                      } else {
                          break;
                      }
                  }
                  iter->entry = ht->table[iter->index];
              } else {
                  iter->entry = iter->nextEntry;
              }
              if (iter->entry) {
                  iter->nextEntry = iter->entry->next;
                  return iter->entry;
              }
          }
          return NULL;
      }
      ```

    + 说明：

      + 循环退出条件：1. 找到节点，返回该节点；2. 遍历所有哈希桶没找到，退出并返回NULL
      + 判断当前节点值是否为空
        + 如果为空，说明第一次进入迭代器，或者需要进入下一个哈希桶寻找节点
          + 两种情况：
            + 如果是第一次进入迭代器，需要判断是否是安全迭代器。如果为**安全迭代器**，pauserehash++；如果是**非安全迭代器**，则计算fingerprint的值，并赋给迭代器
            + 如果不是第一次进入迭代器，说明当前桶已经遍历完成，需要进入下一个桶
            + 因此两种情况都需要将迭代器的桶索引index++
          + 如果当前的索引大于当前哈希表的长度
            + 如果正在rehash且当前遍历的是ht[0]，那么需要从ht[1]的0号桶开始遍历（table++，index=0）
            + 其他情况说明已经遍历完成，**退出循环**
          + 获取**当前哈希桶的首节点**
        + 如果不为空，说明正在某一个桶的某一个节点，继续遍历**取下一个节点**即可
      + 最终获得的节点
        + 如果不为空，说明找到节点，需要更新iterator的nextEntry为该节点的下一个节点（保存下一个节点可以保证当前节点被删除时后续迭代数据不丢失）
        + 如果为空，说明没有找到节点，返回NULL

  + 回收迭代器内存

    + ```
      void dictReleaseIterator(dictIterator *iter)
      {
          if (!(iter->index == -1 && iter->table == 0)) {
              if (iter->safe)
                  dictResumeRehashing(iter->d);
              else
                  assert(iter->fingerprint == dictFingerprint(iter->d));
          }
          zfree(iter);
      }
      ```

    + 说明：

      + 安全迭代器可以直接释放，且释放之后可以将pauserehash--
      + 非安全迭代器必须保证fingerprint没有变化才能释放，否则失败

+ 全遍历的问题

  + 当数据库中存有海量数据时，执行KEYS命令进行一次数据库全遍历的代价很高

## 遍历——dictScan

+ 用途：迭代字典中数据时使用
  + 例：hscan命令迭代整个数据库中的key；zscan命令迭代有序集合所有成员与值，...
+ dictScan过程中可以进行rehash



## API

