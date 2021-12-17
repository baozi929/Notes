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

    + <img src="https://github.com/baozi929/Notes/blob/main/redis/learning-redis/figures/dict_emptyHashTable.png" width="200"/><br/>

+ 哈希表节点

  + ```
    typedef struct dictEntry {
    	// 键
        void *key;
    	// 值
    	// u的所有成员占用同一段内存，同一时刻只能保存一个成员的值
    	// 节点的值v，可为指针，uint64整数，int64整数，浮点数
        union {
            void *val;
            uint64_t u64;
            int64_t s64;
            double d;
        } v;
    	// 指向下一个hash表节点
        struct dictEntry *next;
    } dictEntry;
    ```

  + 

+ 



## API

