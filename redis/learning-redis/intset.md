# intset 整数集合

## 介绍

+ 特点
  + 用于保存整数值的集合抽象数据结构
  + 支持的类型：int16_t、int32_t、int64_t
  + 集合中不出现重复元素
+ 作用：
  + 集合键的底层实现之一，当一个集合**只包含整数值**元素，且这个集合的元素**数量不多**时，redis会使用整数集合作为集合键的底层实现
+ 代码
  + intset.h和intset.c

## 数据结构

+ intset

  + ```
    typedef struct intset {
        uint32_t encoding;
        uint32_t length;
        int8_t contents[];
    } intset;
    ```

  + 包含：

    + encoding：编码方式

    + length：集合包含的元素数量

    + contents[]：保存元素的数组，按值的大小从小到大排序，不包含重复项。虽然类型为int8_t，但不保存int8_t类型的值，其类型取决于encoding的值（INTSET_ENC_INT16、INTSET_ENC_INT32、INTSET_ENC_INT64）

      + ```
        #define INTSET_ENC_INT16 (sizeof(int16_t))
        #define INTSET_ENC_INT32 (sizeof(int32_t))
        #define INTSET_ENC_INT64 (sizeof(int64_t))
        ```

      + 判断一个值的类型：

        + ```
          static uint8_t _intsetValueEncoding(int64_t v) {
              if (v < INT32_MIN || v > INT32_MAX)
                  return INTSET_ENC_INT64;
              else if (v < INT16_MIN || v > INT16_MAX)
                  return INTSET_ENC_INT32;
              else
                  return INTSET_ENC_INT16;
          }
          ```

+ 



## API

