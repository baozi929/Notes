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

        + backlen：Entry的长度（Encode+content）

    + End：1字节

      + listpack的结束标志，内容为0xFF

+ 



