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

+ 通过dictIterator遍历的问题

  + 当数据库中存有海量数据时，迭代dictIterator对字典进行一次数据库全遍历的代价很高

## 遍历——dictScan

+ 用途：迭代字典中数据时使用
  + 例：hscan命令迭代整个数据库中的key；zscan命令迭代有序集合所有成员与值，...
  
+ 特点：

  + dictScan过程中**可以进行rehash**
  + 保证所有数据能被遍历到

+ 缺陷：

  + 可能返回重复元素，该问题可在应用层解决
  + 为了不错过任何节点，dictScan的一次调用会遍历到给定游标对应桶（包含ht[0]和ht[1]）中的所有元素（即一次调用可能遍历多个节点）

+ 算法核心思想：

  + 扩容或缩容都是2倍增长或减少
  + 因此可以推导出同一个桶扩容或缩容后在新哈希表中的位置，可以避免重复遍历

+ dictScan函数

  + ```
    unsigned long dictScan(dict *d,
                           unsigned long v,
                           dictScanFunction *fn,
                           dictScanBucketFunction* bucketfn,
                           void *privdata)
    {
        dictht *t0, *t1;
        const dictEntry *de, *next;
        unsigned long m0, m1;
    
        if (dictSize(d) == 0) return 0;
    
        dictPauseRehashing(d);
    
        if (!dictIsRehashing(d)) {
            t0 = &(d->ht[0]);
            m0 = t0->sizemask;
    
            if (bucketfn) bucketfn(privdata, &t0->table[v & m0]);
            de = t0->table[v & m0];
            while (de) {
                next = de->next;
                fn(privdata, de);
                de = next;
            }
    
            v |= ~m0;
            v = rev(v);
            v++;
            v = rev(v);
        } else {
            t0 = &d->ht[0];
            t1 = &d->ht[1];
    
    		if (t0->size > t1->size) {
                t0 = &d->ht[1];
                t1 = &d->ht[0];
            }
    
            m0 = t0->sizemask;
            m1 = t1->sizemask;
    
            if (bucketfn) bucketfn(privdata, &t0->table[v & m0]);
            de = t0->table[v & m0];
            while (de) {
                next = de->next;
                fn(privdata, de);
                de = next;
            }
    
            do {
                if (bucketfn) bucketfn(privdata, &t1->table[v & m1]);
    
                de = t1->table[v & m1];
                while (de) {
                    next = de->next;
                    fn(privdata, de);
                    de = next;
                }
    
                v |= ~m1;
                v = rev(v);
                v++;
                v = rev(v);
    
            } while (v & (m0 ^ m1));
        }
    
    	// 重新开始rehash
        dictResumeRehashing(d);
    
        return v;
    }
    ```

  + 参数介绍

    + ```
      unsigned long dictScan(dict *d,
                             unsigned long v,
                             dictScanFunction *fn,
                             dictScanBucketFunction* bucketfn,
                             void *privdata)
      ```

    + d：迭代的字典

    + v：迭代开始的游标（哈希表中的索引），每次遍历后返回新的游标值

    + fn：函数指针，对节点进行的处理

    + bucketfn：函数指针，对桶进行的处理

    + privdata：函数fn所需参数

  + 函数流程

    + 如果正在rehash，pauserehash++

    + 判断字典是否在进行rehash

      + 不在进行rehash，直接根据传入的游标扫描对应的哈希桶，然后遍历节点（用函数fn处理每个节点）,最后反转游标二进制位，+1，再反转形成新的游标

        + 游标增加思路：（高位加1）

          ```
          // 假设hash表size为8，掩码m0为29位0，3位1，即...000111
          // ~m0 = ...111000
          // v |= ~m0，即保留低位的数据，即高29位为1，低3位为原有v的实际数据xxx
          // 假设v=3ul，则经过算v |= ~m0后，v=111...111xxx(111...111011)
          v |= ~m0;
          
          // 翻转cursor，v = xxx111...111(110111...111)
          v = rev(v);
          // xx(x+1)000...000(111000...000)
          v++;
          // 再次翻转回原来的数据(000...000111)，即011->111
          v = rev(v);
          ```

      + 在进行rehash（字典有2个哈希表），先对小表进行扫描，然后将小表的游标扩展为大表的游标（低位和小表相同的游标），并对大表对应桶内元素进行扫描

        + 游标增加思路和上面相同（即高位+1）
        + 退出条件（即小表桶对应的大表桶都扫描完成）：v & (m0 ^ m1) == 0。（v的二进制表示中，若m1比m0高的若干位都为0，则退出循环）
          + 举个例子：
            + 小表4位，大表6位，小表遍历到0100（即游标v == 0100）
            + 大表从000100开始遍历，100100 -> 010100 -> 110100 -> 001100
            + 此时，最高2位再次变为0，对于低4位而言进位了，获得了下一个桶的位置（1100）

    + 当前游标对应桶已处理完毕，pauserehash--，并返回获得的新游标

  + 例子——遍历过程中未进行rehash（同时存在1表）

    + case1：哈希表大小为4，迭代过程未进行rehash（掩码m0=0x11，~m0=0x100）

      + | 输入v | 初始值 | v\|=0x100 | v=rev(v) |  v++  | v=rev(v) | 返回v |
        | :---: | :----: | :-------: | :------: | :---: | :------: | :---: |
        |   0   | 0x000  |   0x100   |  0x001   | 0x010 |  0x010   |   2   |
        |   2   | 0x010  |   0x110   |  0x011   | 0x100 |  0x001   |   1   |
        |   1   | 0x001  |   0x101   |  0x101   | 0x110 |  0x011   |   3   |
        |   3   | 0x011  |   0x111   |  0x111   | 0x000 |  0x000   |   0   |

      + 按照0、2、1、3的顺序，遍历了所有桶

    + case2：哈希表大小为4，进行到第三次迭代时，哈希表扩容至8

      + 前2次迭代同case1的前2次迭代，遍历了0、2，当前返回的游标为1

      + 第三次迭代时，表扩容到8，掩码为m0=0x111，~m0=0x1000

      + | 输入v | 初始值 | v\|=0x1000 | v=rev(v) |  v++   | v=rev(v) | 返回v |
        | :---: | :----: | :--------: | :------: | :----: | :------: | :---: |
        |   1   | 0x0001 |   0x1001   |  0x1001  | 0x1010 |  0x0101  |   5   |
        |   5   | 0x0101 |   0x1101   |  0x1011  | 0x1100 |  0x0011  |   3   |
        |   3   | 0x0011 |   0x1011   |  0x1101  | 0x1110 |  0x0111  |   7   |
        |   1   | 0x0111 |   0x1111   |  0x1111  | 0x0000 |  0x0000  |   0   |

      + 迭代顺序小表0、2，大表1（0x001）、5（0x101）、3（0x011）、7（0x111）

      + 遍历次数为6，扩容后少遍历了4、6，因为游标为0、2的桶在扩容前已经迭代完，哈希表从4扩容到8，再经过rehash之后，游标为0、2的数据会分布在0|4（0x000，0x100）、2|6（0x010，0x110），即新哈希表的这4个桶已经遍历过，不需要重复遍历

    + case3：哈希表大小为8（掩码为m0=0x111，~m0=0x1000），进行到第五次迭代时，哈希表缩容至4

      + | 输入v | 初始值 | v\|=0x1000 | v=rev(v) |  v++   | v=rev(v) | 返回v |
        | :---: | :----: | :--------: | :------: | :----: | :------: | :---: |
        |   0   | 0x0000 |   0x1000   |  0x0001  | 0x0010 |  0x0100  |   4   |
        |   4   | 0x0100 |   0x1100   |  0x0011  | 0x0100 |  0x0010  |   2   |
        |   2   | 0x0010 |   0x1010   |  0x0101  | 0x0110 |  0x0110  |   6   |
        |   6   | 0x0110 |   0x1110   |  0x0111  | 0x1000 |  0x0001  |   1   |

      + 第5次迭代时，哈希表缩容到4（掩码m0=0x11，~m0=0x100）

      + | 输入v | 初始值 | v\|=0x1000 | v=rev(v) |  v++  | v=rev(v) | 返回v |
        | :---: | :----: | :--------: | :------: | :---: | :------: | :---: |
        |   1   | 0x001  |   0x101    |  0x101   | 0x110 |  0x011   |   3   |
        |   3   | 0x011  |   0x1101   |  0x111   | 0x000 |  0x000   |   0   |

      + 遍历次数为6，顺序为大表0、4、2、6，小表1、3，不需要重复遍历小表的0、2，是因为rehash之后，大表0|4、2|6桶中的节点会分布在小表0、2桶中，因此缩容后无需重复遍历

  + 例子——遍历过程中遇到rehash操作（同时存在2表）

    + case1：哈希表大小为8，扩容至16（正在进行rehash）

      + | 输入v | ht[0]遍历顺序 | ht[1]遍历顺序 | ht[1]遍历顺序 | 返回v       |
        | ----- | ------------- | ------------- | ------------- | ----------- |
        | 0     | 0（0x000）    | 0（0x0000）   | 8（0x1000）   | 4（0x0100） |
        | 4     | 4             | 4             | 12            | 2           |
        | 2     | 2             | 2             | 10            | 6           |
        | 6     | 6             | 6             | 14            | 1           |
        | 1     | 1             | 1             | 9             | 5           |
        | 5     | 5             | 5             | 13            | 3           |
        | 3     | 3             | 3（0x0011）   | 11（0x1011）  | 7（0x1111） |
        | 7     | 7             | 7（0x0111）   | 15（0x1111）  | 0（0x0000） |
  
      + ht[0]遍历了所有8个桶，ht[1]遍历了所有16个桶，没有重复或遗漏
  
  + 辅助函数：
  
    + 二进制逆序
  
      + ```
        static unsigned long rev(unsigned long v) {
            unsigned long s = CHAR_BIT * sizeof(v); // bit size; must be power of 2
        	unsigned long mask = ~0UL; // unsigned long 0 取反
            while ((s >>= 1) > 0) {
                mask ^= (mask << s);
        		// (v >> s) & mask，高s位移动到低s位，高位为0
        		// (v << s) & ~mask，低s位移动到高s位，低位为0
        		// |将二者拼起来，导致的结果是高s位和低s位互换
                v = ((v >> s) & mask) | ((v << s) & ~mask);
            }
            return v;
        }
        ```
  
      + 思路：（以32位整数位例）
  
        + 先对调相邻的16位，再对调相邻的8位，再对调相邻的4位，再对调相邻的2位，最终完成按比特位的反转
  



## API

