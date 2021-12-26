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

  + | zlbytes | zltail | zllen | entry | entry | ...  | entry | zlend |
    | ------- | ------ | ----- | ----- | ----- | ---- | ----- | ----- |

  + 例子

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

      + 长度为1或5字节
        + 前一个节点长度小于254字节，则previous_entry_length的长度为1字节
        + 前一个节点长度大于等于254字节，则previous_entry_length的长度为5字节
          + 第一个字节为0xFE（254）
          + 后四个字节为前一个节点的长度

    + encoding

      + 当前元素的编码

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

+ 



## API

