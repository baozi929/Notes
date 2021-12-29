# ziplist压缩列表

## 介绍

+ 特点
  + 压缩列表时一个字节数组，是redis为了**节约内存**设计的线性数据结构
    + 可包含多个元素
    + 每个元素为一个字节数组或一个整数
  + 所有域都用小端存储

+ 作用：
  + hash，list，zset的底层实现
    + 使用场景：只包含少量元素，且每个元素为小整数值或长度较短的字符串
+ 代码
  + ziplist.h和ziplist.c

## 数据结构

+ ziplist

  + | zlbytes | zlta          il | zllen | entry | entry | ...  | entry | zlend |
    | ------- | ---------------- | ----- | ----- | ----- | ---- | ----- | ----- |

  + 说明：

    + zlbytes：4字节，因此压缩列表最多为2^32-1字节
  + zltail：4字节，压缩列表尾元素相对于压缩列表起始地址的偏移量
    + zllen：2字节，压缩列表的元素个数，最多2^16-1
  + entry：压缩列表存储的元素，可为字节数组或整数
    + zlend：1字节，压缩列表的结尾，为0xFF

  + 例子：

    + [0f 00 00 00] [0c 00 00 00] [02 00] [00 f3] [02 f6] [ff]

      ​          |                      |                |           |           |      |

      ​     zlbytes              zltail        entries    "2"        "5"   end

      + zlbytes：zlbytes = 15，表示整个ziplist占用15个字节
      + zltail：zltail = 12，表示最后一个元素的位置，即“5”的首地址
      + entries：entries=2
      + [00 f3]：00表示之前的长度为0，encoding 11110011表示2
      + [02 f6]：02表示之前的长度为2，encoding 11110110表示5
      + end：结束用0xff表示

+ ziplist节点

  + ```
    typedef struct zlentry {
        unsigned int prevrawlensize;
    	unsigned int prevrawlen;
        unsigned int lensize;
        unsigned int len;
    	unsigned int headersize;
        unsigned char encoding;
        unsigned char *p;
    } zlentry;
    ```

  + 说明：

    + previous_entry_length

      + 该字段表示前一个元素的字节长度
      + 包含：
        + previous_entry_length字段的长度prevrawlensize
        + previous_entry_length字段存储的内容prevrawlen
      + 长度为1或5字节
        + 前一个节点长度小于254字节，则previous_entry_length的长度为1字节
        + 前一个节点长度大于等于254字节，则previous_entry_length的长度为5字节
          + 第一个字节为0xFE（254）
          + 后四个字节为前一个节点的长度

    + headersize

      + 当前元素的header长度，即previous_entry_length和encoding的长度之和（即prevrawlensize + lensize）

    + encoding

      + 当前元素的编码，即content字段存储的数据类型（整数或字节数组），为了节约内存，编码**长度可变**

        + 字节数组编码

          + ```
            #define ZIP_STR_06B (0 << 6)
            #define ZIP_STR_14B (1 << 6)
            #define ZIP_STR_32B (2 << 6)
            ```

          + | 编码                                         | 编码长度 | content保存的值              |
            | -------------------------------------------- | -------- | ---------------------------- |
            | 00bbbbbb                                     | 1字节    | 最大长度为63的字节数组       |
            | 01bbbbbb xxxxxxxx                            | 2字节    | 最大长度为2^14 - 1的字节数组 |
            | 10------ aaaaaaaa bbbbbbbb cccccccc dddddddd | 5字节    | 最大长度为2^32 - 1的字节数组 |

        + 整数编码

          + ```
            #define ZIP_INT_16B (0xc0 | 0<<4)
            #define ZIP_INT_32B (0xc0 | 1<<4)
            #define ZIP_INT_64B (0xc0 | 2<<4)
            #define ZIP_INT_24B (0xc0 | 3<<4)
            #define ZIP_INT_8B 0xfe
            ```

          + | 编码     | 编码长度 | content保存的值                                              |
            | -------- | -------- | ------------------------------------------------------------ |
            | 11000000 | 1字节    | int16_t类型的整数                                            |
            | 11010000 | 1字节    | int32_t类型的整数                                            |
            | 11100000 | 1字节    | int64_t类型的整数                                            |
            | 11110000 | 1字节    | 24位有符号整数                                               |
            | 11111110 | 1字节    | 8位有符号整数                                                |
            | 1111xxxx | 1字节    | 没有content属性<br />xxxx四位保存0~12之间的整数<br />（0001~1101，因为不能用0000和1111） |

        + 字段解析：

          + 前两位可用于判断是整数类型或字节数组
            + 11为整数类型，可通过后续位判断其具体类型
            + 00、01、10位字节数组，且可以直接判断其最大长度

    + content

+ 


## 操作

+ 解码压缩列表的元素

  + ```
    static inline void zipEntry(unsigned char *p, zlentry *e) {
        ZIP_DECODE_PREVLEN(p, e->prevrawlensize, e->prevrawlen);
        ZIP_ENTRY_ENCODING(p + e->prevrawlensize, e->encoding);
        ZIP_DECODE_LENGTH(p + e->prevrawlensize, e->encoding, e->lensize, e->len);
        assert(e->lensize != 0); /* check that encoding was valid. */
        e->headersize = e->prevrawlensize + e->lensize;
        e->p = p;
    }
    ```

  + 说明：

    + ZIP_DECODE_PREVLEN：解码previous_entry_length字段 。

      + ```
        #define ZIP_BIG_PREVLEN 254
        
        #define ZIP_DECODE_PREVLENSIZE(ptr, prevlensize) do {                    \
            if ((ptr)[0] < ZIP_BIG_PREVLEN) {                                    \
                (prevlensize) = 1;                                               \
            } else {                                                             \
                (prevlensize) = 5;                                               \
            }                                                                    \
        } while(0)
        
        #define ZIP_DECODE_PREVLEN(ptr, prevlensize, prevlen) do {               \
            ZIP_DECODE_PREVLENSIZE(ptr, prevlensize);                            \
        	if ((prevlensize) == 1) {                                            \
                (prevlen) = (ptr)[0];                                            \
            } else { /* prevlensize == 5 */                                      \
                (prevlen) = ((ptr)[4] << 24) |                                   \
                            ((ptr)[3] << 16) |                                   \
                            ((ptr)[2] <<  8) |                                   \
                            ((ptr)[1]);                                          \
            }                                                                    \
        } while(0)
        ```

      + 输入值：ptr指向元素的首地址

      + 返回值：previous_entry_length字段的参数，包含e->prevrawlensize和e->prevrawlen

    + ZIP_ENTRY_ENCODING：解码encoding字段。

      + ```
        #define ZIP_ENTRY_ENCODING(ptr, encoding) do {  \
            (encoding) = ((ptr)[0]); \
            if ((encoding) < ZIP_STR_MASK) (encoding) &= ZIP_STR_MASK; \
        } while(0)
        ```

      + 输入值：ptr指向元素的首地址+previous_entry_length长度

      + 返回值：encoding

    + ZIP_DECODE_LENGTH：解码encoding长度。

      + ```
        #define ZIP_DECODE_LENGTH(ptr, encoding, lensize, len) do {                    \
            if ((encoding) < ZIP_STR_MASK) {                                           \
                if ((encoding) == ZIP_STR_06B) {                                       \
                    (lensize) = 1;                                                     \
                    (len) = (ptr)[0] & 0x3f;                                           \
                } else if ((encoding) == ZIP_STR_14B) {                                \
                    (lensize) = 2;                                                     \
                    (len) = (((ptr)[0] & 0x3f) << 8) | (ptr)[1];                       \
                } else if ((encoding) == ZIP_STR_32B) {                                \
                    (lensize) = 5;                                                     \
                    (len) = ((ptr)[1] << 24) |                                         \
                            ((ptr)[2] << 16) |                                         \
                            ((ptr)[3] <<  8) |                                         \
                            ((ptr)[4]);                                                \
                } else {                                                               \
                    (lensize) = 0; /* bad encoding, should be covered by a previous */ \
                    (len) = 0;     /* ZIP_ASSERT_ENCODING / zipEncodingLenSize, or  */ \
                                   /* match the lensize after this macro with 0.    */ \
                }                                                                      \
            } else {                                                                   \
                (lensize) = 1;                                                         \
                if ((encoding) == ZIP_INT_8B)  (len) = 1;                              \
                else if ((encoding) == ZIP_INT_16B) (len) = 2;                         \
                else if ((encoding) == ZIP_INT_24B) (len) = 3;                         \
                else if ((encoding) == ZIP_INT_32B) (len) = 4;                         \
                else if ((encoding) == ZIP_INT_64B) (len) = 8;                         \
                else if (encoding >= ZIP_INT_IMM_MIN && encoding <= ZIP_INT_IMM_MAX)   \
                    (len) = 0; /* 4 bit immediate */                                   \
                else                                                                   \
                    (lensize) = (len) = 0; /* bad encoding */                          \
            }                                                                          \
        } while(0)
        ```

        

      + 输入值：ptr，encoding

      + 返回值：编码节点长度所需字节数lensize，节点的长度len

    + 解码后的信息通过指针e返回

+ 创建压缩列表

  + ```
    #define ZIPLIST_BYTES(zl)       (*((uint32_t*)(zl)))
    #define ZIPLIST_TAIL_OFFSET(zl) (*((uint32_t*)((zl)+sizeof(uint32_t))))
    #define ZIPLIST_LENGTH(zl)      (*((uint16_t*)((zl)+sizeof(uint32_t)*2)))
    
    unsigned char *ziplistNew(void) {
        unsigned int bytes = ZIPLIST_HEADER_SIZE+ZIPLIST_END_SIZE;
        unsigned char *zl = zmalloc(bytes);
        ZIPLIST_BYTES(zl) = intrev32ifbe(bytes);
        ZIPLIST_TAIL_OFFSET(zl) = intrev32ifbe(ZIPLIST_HEADER_SIZE);
        ZIPLIST_LENGTH(zl) = 0;
        zl[bytes-1] = ZIP_END;
        return zl;
    }
    ```

  + 说明：

    + 计算空表占用内存（表头+ZIP_END的大小），并分配内存
      + zlbytes(4)+zltail(4)+zllen(2)+zlend(1) = 11 bytes
    + 初始化上述列表属性

+ 插入元素

  + ```
    unsigned char *ziplistInsert(unsigned char *zl, unsigned char *p, unsigned char *s, unsigned int slen) {
        return __ziplistInsert(zl,p,s,slen);
    }
    ```

  + ```
    unsigned char *__ziplistInsert(unsigned char *zl, unsigned char *p, unsigned char *s, unsigned int slen) {
    	size_t curlen = intrev32ifbe(ZIPLIST_BYTES(zl)), reqlen, newlen;
        unsigned int prevlensize, prevlen = 0;
        size_t offset;
        int nextdiff = 0;
        unsigned char encoding = 0;
        long long value = 123456789; 
        zlentry tail;
    
        if (p[0] != ZIP_END) {
            ZIP_DECODE_PREVLEN(p, prevlensize, prevlen);
        } else {
            unsigned char *ptail = ZIPLIST_ENTRY_TAIL(zl);
            if (ptail[0] != ZIP_END) {
                prevlen = zipRawEntryLengthSafe(zl, curlen, ptail);
            }
        }
    
        if (zipTryEncoding(s,slen,&value,&encoding)) {
            reqlen = zipIntSize(encoding);
        } else {
            reqlen = slen;
        }
        reqlen += zipStorePrevEntryLength(NULL,prevlen);
        reqlen += zipStoreEntryEncoding(NULL,encoding,slen);
    
        int forcelarge = 0;
        nextdiff = (p[0] != ZIP_END) ? zipPrevLenByteDiff(p,reqlen) : 0;
        if (nextdiff == -4 && reqlen < 4) {
            nextdiff = 0;
            forcelarge = 1;
        }
    
        offset = p-zl;
        newlen = curlen+reqlen+nextdiff;
        zl = ziplistResize(zl,newlen);
        p = zl+offset;
    
        if (p[0] != ZIP_END) {
            memmove(p+reqlen,p-nextdiff,curlen-offset-1+nextdiff);
    
            if (forcelarge)
                zipStorePrevEntryLengthLarge(p+reqlen,reqlen);
            else
                zipStorePrevEntryLength(p+reqlen,reqlen);
    
            ZIPLIST_TAIL_OFFSET(zl) =
                intrev32ifbe(intrev32ifbe(ZIPLIST_TAIL_OFFSET(zl))+reqlen);
    
            assert(zipEntrySafe(zl, newlen, p+reqlen, &tail, 1));
            if (p[reqlen+tail.headersize+tail.len] != ZIP_END) {
                ZIPLIST_TAIL_OFFSET(zl) =
                    intrev32ifbe(intrev32ifbe(ZIPLIST_TAIL_OFFSET(zl))+nextdiff);
            }
        } else {
            /* This element will be the new tail. */
            ZIPLIST_TAIL_OFFSET(zl) = intrev32ifbe(p-zl);
        }
    
        if (nextdiff != 0) {
            offset = p-zl;
            zl = __ziplistCascadeUpdate(zl,p+reqlen);
            p = zl+offset;
        }
    
        p += zipStorePrevEntryLength(p,prevlen);
        p += zipStoreEntryEncoding(p,encoding,slen);
        if (ZIP_IS_STR(encoding)) {
            memcpy(p,s,slen);
        } else {
            zipSaveInteger(p,value,encoding);
        }
        ZIPLIST_INCR_LENGTH(zl,1);
        return zl;
    }
    ```
    
  + 说明：

    + 首先，判断是插入列表末端还是中间

      + 插入中间
      + 插入尾部，需要判断列表是否为空
        + 如果列表不为空，取出尾节点的prevlen
        + 如果列表为空，prevlen=0

    + 尝试通过输入的节点数据s和长度slen将其编码，并获得存储新节点所需的字节数

      + 编码为int，存储数据需要的字节数与编码有关
      + 编码为string，slen即为存储数据所需字节数

    + 需要判断插入当前节点后，后一个节点是否需要增加长度（previous_entry_length可能为1、5，所以长度变化可能为-4、0、4）

      + 需要额外判断的情况，如果长度变化为-4且新插入节点长度<4，会导致压缩列表总长度减少
      + 后续分配内存用的realloc，可能回收多余内存，导致数据丢失
      + 因此需要额外标记，做特殊处理

    + 移动后续节点

      + 后续有节点
        + 将后续节点往后移动（移动距离为之前计算的新插入节点的长度+后一个节点的新旧header的差值）
        + 更新previous_entry_length
        + 更新zltail字段
          + 新节点后面只有1个节点，zltail指向新插入节点结束后的字节
          + 新节点后面有多个节点，zltail需要考虑nextdiff
      + 后续无节点，不需要移动节点
        + 更新zltail字段

    + 级联更新

      + ```
        unsigned char *__ziplistCascadeUpdate(unsigned char *zl, unsigned char *p) {
        ....
        }
        ```

      + 简要介绍：

        + 当一个节点添加到某个节点之前的时候，如果原节点的prevlen不足以保存新节点的长度，需要对原节点的空间进行扩展（1byte -> 5bytes）。但是，当对原节点进行扩展之后，原节点的下一个节点的prevlen仍然可能不足（在多个节点长度接近ZIP_BIGLEN时可能出现），因此可能出现需要连续扩展的情况。
        + 虽然节点长度的变小导致连续缩小也有可能出现，但为了避免扩展—缩小—扩展—缩小的情况反复出现。本函数不处理这种情况，而是任由prevlen比所需的长度更长。

    + 找到entry数据开始的地址，并将entry的内容填入

    + 更新zllen字段

+ 删除节点

  + ```
    unsigned char *ziplistDelete(unsigned char *zl, unsigned char **p) {
        size_t offset = *p-zl;
        zl = __ziplistDelete(zl,*p,1);
    
        *p = zl+offset;
        return zl;
    }
    ```

  + 说明：

    + 输入：
      + zl：指向压缩列表首地址
      + *p：指向待删除元素首地址（p可以返回）
    + 返回值：
      + zl：压缩列表首地址
    + 调用底层`__ziplistDelete`函数实现删除。`__ziplistDelete`函数可以同时删除多个元素，输入参数p指向的是首个待删除的元素地址，num表示删除元素数目。
      + 计算待删除元素的总长度
      + 数据复制
      + 重新分配内存
      + 更新压缩列表相关参数

+ 遍历压缩列表

  + 访问后置节点：

    + ```
      unsigned char *ziplistNext(unsigned char *zl, unsigned char *p) {
          ((void) zl);
          if (p[0] == ZIP_END) {
              return NULL;
          }
      
          p += zipRawEntryLength(p);
          if (p[0] == ZIP_END) {
              return NULL;
          }
      
          return p;
      }
      ```

  + 访问前置节点：

    + ```
      unsigned char *ziplistPrev(unsigned char *zl, unsigned char *p) {
          unsigned int prevlensize, prevlen = 0;
          if (p[0] == ZIP_END) {
              p = ZIPLIST_ENTRY_TAIL(zl);
              return (p[0] == ZIP_END) ? NULL : p;
          } else if (p == ZIPLIST_ENTRY_HEAD(zl)) {
              return NULL;
          } else {
              ZIP_DECODE_PREVLEN(p, prevlensize, prevlen);
              assert(prevlen > 0);
              p-=prevlen;
              size_t zlbytes = intrev32ifbe(ZIPLIST_BYTES(zl));
              zipAssertValidEntry(zl, zlbytes, p);
              return p;
          }
      }
      ```



## API

