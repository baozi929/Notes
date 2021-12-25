# Simple Dynamic Strings (SDS)

## 介绍

+ 特点
  + 获取字符串长度的复杂度O(1)
    + 因为在结构中记录字符串长度len
  + 二进制安全，可以保存文本和二进制数据
    + 可通过len判断是否结束，不依赖于\0读字符串
  + 通过空间预分配和惰性空间释放减少分配内存的次数
  + 兼容部分<string.h>中的函数
  + 杜绝缓冲区溢出
    + <string.h>/strcat在调用时假定用户分配足够空间，如果空间不足，会错误地修改第一个字符串后面的内存
    + SDS会检查大小是否充足（不够则扩容），再进行修改
+ 作用：
  + 保存数据库中的字符串值
  + 作为缓冲区（buffer）：AOF模块中的AOF缓冲区，以及客户端状态中的输入缓冲区
+ 代码
  + sds.h和sds.c

## 数据结构

+ sds结构体

  + 从redis 4.0开始, 开始使用以下5种sdshdr${n}的定义

  + sdshdr5

    + 实际未使用

    + ```
      struct __attribute__ ((__packed__)) sdshdr5 {
          unsigned char flags; /* 3 lsb of type, and 5 msb of string length */
          char buf[];
      };
      ```

  + sdshdr8、sdshdr16、sdshdr32和sdshdr64

    + ```
      struct __attribute__ ((__packed__)) sdshdr${n} {
          uint${n}_t len; /* used */
      	uint${n}_t alloc; /* excluding the header and '\0' */
          unsigned char flags; /* 3 lsb of type, 5 unused bits */
      	char buf[];
      };
      ```

      + `__attribute__ ((__packed__))`用于在结构体的声明中取消字节对齐
        + 让编译器更紧凑地使用内存
        + 可以通过buf[-1]找到flags
      + ${n}为8、16、32或64，存储最长2^n-1长度的数据

    + 字段

      + len：buf中已占用的字节数

      + alloc：buf分配的长度，不包含header和空终止符'\0'

      + flags：低3位表示sdsHdr的类型，高5位未使用

        + sdsHdr类型

          + ```
            #define SDS_TYPE_5  0
            #define SDS_TYPE_8  1
            #define SDS_TYPE_16 2
            #define SDS_TYPE_32 3
            #define SDS_TYPE_64 4
            ```

        + 取sdsHdr的类型可通过掩码获取，`flags&SDS_TYPE_MASK`即为sdsHdr类型

          + ````
            #define SDS_TYPE_MASK 7
            #define SDS_TYPE_BITS 3
            ````

      + buf：数据空间

    + 结构表示

      + <table class="tg">
        <thead>
          <tr>
            <th class="tg-c3ow">len</th>
            <th class="tg-c3ow">alloc</th>
            <th class="tg-c3ow">flags</th>
            <th class="tg-c3ow" colspan="4">buf</th>
          </tr>
        </thead>
        <tbody>
          <tr>
            <td class="tg-c3ow">${n} bytes</td>
            <td class="tg-c3ow">${n} bytes</td>
            <td class="tg-c3ow">1 byte</td>
            <td class="tg-c3ow">1 byte</td>
            <td class="tg-c3ow">1 byte</td>
            <td class="tg-baqh">...</td>
            <td class="tg-baqh">1 byte</td>
          </tr>
        </tbody>
        </table>


## API

+ 新建字符串
+ 释放字符串
+ 拼接字符串
+ ...
