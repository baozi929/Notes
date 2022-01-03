#ifndef STREAM_H
#define STREAM_H

#include "rax.h"
#include "listpack.h"

/* Stream item ID: a 128 bit number composed of a milliseconds time and
 * a sequence counter. IDs generated in the same millisecond (or in a past
 * millisecond if the clock jumped backward) will use the millisecond time
 * of the latest generated ID and an incremented sequence. */

/* 消息ID，128位 */
typedef struct streamID {
    // 19700101至今的毫秒数
    uint64_t ms;        /* Unix time in milliseconds. */
    // 序号
    uint64_t seq;       /* Sequence number. */
} streamID;

typedef struct stream {
    // 持有stream的rax树
    // rax树存储消息生产者生产的具体消息，每个消息有唯一的ID
    // 以消息ID为键，消息内容为值存储在rax树中
    // 注意，rax树种的一个节点可能存储多个消息
    rax *rax;               /* The radix tree holding the stream. */
    // 当前stream中的消息个数（不包括已经删除的消息）
    uint64_t length;        /* Number of elements inside this stream. */
    // 当前stream中最后插入的消息的ID，stream为空时，设置为0
    streamID last_id;       /* Zero if there are yet no items. */
    // 当前stream相关的消费组，以消费组的组名为键，streamCG为值存储在rax中
    rax *cgroups;           /* Consumer groups dictionary: name -> streamCG */
} stream;

/* We define an iterator to iterate stream items in an abstract way, without
 * caring about the radix tree + listpack representation. Technically speaking
 * the iterator is only used inside streamReplyWithRange(), so could just
 * be implemented inside the function, but practically there is the AOF
 * rewriting code that also needs to iterate the stream to emit the XADD
 * commands. */
/* 迭代器，用于遍历stream中的消息 */
typedef struct streamIterator {
    // 当前迭代器正在遍历的消息流
    stream *stream;         /* The stream we are iterating. */
    // 实际消息存储在listpack中，每个listpack都有master entry（根据第一个插入的消息构建）
    // master_id为该消息id
    streamID master_id;     /* ID of the master entry at listpack head. */
    // master entry中field的个数
    uint64_t master_fields_count;       /* Master entries # of fields. */
    // master entry field在listpack中的首地址
    unsigned char *master_fields_start; /* Master entries start in listpack. */
    // 当listpack中消息的field与master entry的field完全相同时，需要记录当前所在的field的具体位置
    // master_fields_ptr就是实现这个功能的
    unsigned char *master_fields_ptr;   /* Master field to emit next. */
    // 当前遍历的消息的标志位
    int entry_flags;                    /* Flags of entry we are emitting. */
    // 迭代器方向，True表示从尾->头
    int rev;                /* True if iterating end to start (reverse). */
    // 该迭代器处理的消息ID的范围，start
    uint64_t start_key[2];  /* Start key as 128 bit big endian. */
    // 该迭代器处理的消息ID的范围，end
    uint64_t end_key[2];    /* End key as 128 bit big endian. */
    // rax迭代器，用于遍历rax中的所有key
    raxIterator ri;         /* Rax iterator. */
    // 指向当前listpack的指针
    unsigned char *lp;      /* Current listpack. */
    // 指向当前listpack中正在遍历的元素
    unsigned char *lp_ele;  /* Current listpack cursor. */
    // 指向当前消息的flags
    unsigned char *lp_flags; /* Current entry flags pointer. */
    /* Buffers used to hold the string of lpGet() when the element is
     * integer encoded, so that there is no string representation of the
     * element inside the listpack itself. */
    // 从listpack中读取数据时用的缓存
    unsigned char field_buf[LP_INTBUF_SIZE];
    // 从listpack中读取数据时用的缓存
    unsigned char value_buf[LP_INTBUF_SIZE];
} streamIterator;

/* Consumer group. */
/* 消费组 */
typedef struct streamCG {
    // 该消费组已确认的最后一个消息的ID
    streamID last_id;       /* Last delivered (not acknowledged) ID for this
                               group. Consumers that will just ask for more
                               messages will served with IDs > than this. */
    // 该消费组尚未确认的消息
    // key：消息ID
    // value：streamNACK（代表一个尚未确认的消息）
    rax *pel;               /* Pending entries list. This is a radix tree that
                               has every message delivered to consumers (without
                               the NOACK option) that was yet not acknowledged
                               as processed. The key of the radix tree is the
                               ID as a 64 bit big endian number, while the
                               associated value is a streamNACK structure.*/
    // 该消费组中所有的消费者
    // key：消费者的名称
    // value：streamConsumer（代表一个消费者）
    rax *consumers;         /* A radix tree representing the consumers by name
                               and their associated representation in the form
                               of streamConsumer structures. */
} streamCG;

/* A specific consumer in a consumer group.  */
/* 消费组中的一个消费者 */
typedef struct streamConsumer {
    // 该消费者最后一次活跃的时间
    mstime_t seen_time;         /* Last time this consumer was active. */
    // 消费者的名称
    sds name;                   /* Consumer name. This is how the consumer
                                   will be identified in the consumer group
                                   protocol. Case sensitive. */
    // 该消费者尚未确认的消息
    // key：消息ID
    // value：streamNACK
    rax *pel;                   /* Consumer specific pending entries list: all
                                   the pending messages delivered to this
                                   consumer not yet acknowledged. Keys are
                                   big endian message IDs, while values are
                                   the same streamNACK structure referenced
                                   in the "pel" of the consumer group structure
                                   itself, so the value is shared. */
} streamConsumer;

/* Pending (yet not acknowledged) message in a consumer group. */
/* 消费组中未确认的消息 */
typedef struct streamNACK {
    // 该消息最后发送给消费方的时间
    mstime_t delivery_time;     /* Last time this message was delivered. */
    // 该消息已经发送的次数
    uint64_t delivery_count;    /* Number of times this message was delivered.*/
    // 该消息当前归属的消费者
    streamConsumer *consumer;   /* The consumer this message was delivered to
                                   in the last delivery. */
} streamNACK;

/* Stream propagation informations, passed to functions in order to propagate
 * XCLAIM commands to AOF and slaves. */
typedef struct streamPropInfo {
    robj *keyname;
    robj *groupname;
} streamPropInfo;

/* Prototypes of exported APIs. */
struct client;

/* Flags for streamLookupConsumer */
#define SLC_NONE      0
#define SLC_NOCREAT   (1<<0) /* Do not create the consumer if it doesn't exist */
#define SLC_NOREFRESH (1<<1) /* Do not update consumer's seen-time */

stream *streamNew(void);
void freeStream(stream *s);
unsigned long streamLength(const robj *subject);
size_t streamReplyWithRange(client *c, stream *s, streamID *start, streamID *end, size_t count, int rev, streamCG *group, streamConsumer *consumer, int flags, streamPropInfo *spi);
void streamIteratorStart(streamIterator *si, stream *s, streamID *start, streamID *end, int rev);
int streamIteratorGetID(streamIterator *si, streamID *id, int64_t *numfields);
void streamIteratorGetField(streamIterator *si, unsigned char **fieldptr, unsigned char **valueptr, int64_t *fieldlen, int64_t *valuelen);
void streamIteratorRemoveEntry(streamIterator *si, streamID *current);
void streamIteratorStop(streamIterator *si);
streamCG *streamLookupCG(stream *s, sds groupname);
streamConsumer *streamLookupConsumer(streamCG *cg, sds name, int flags, int *created);
streamCG *streamCreateCG(stream *s, char *name, size_t namelen, streamID *id);
streamNACK *streamCreateNACK(streamConsumer *consumer);
void streamDecodeID(void *buf, streamID *id);
int streamCompareID(streamID *a, streamID *b);
void streamFreeNACK(streamNACK *na);
int streamIncrID(streamID *id);
int streamDecrID(streamID *id);
void streamPropagateConsumerCreation(client *c, robj *key, robj *groupname, sds consumername);
robj *streamDup(robj *o);
int streamValidateListpackIntegrity(unsigned char *lp, size_t size, int deep);
int streamParseID(const robj *o, streamID *id);
robj *createObjectFromStreamID(streamID *id);
int streamAppendItem(stream *s, robj **argv, int64_t numfields, streamID *added_id, streamID *use_id);
int streamDeleteItem(stream *s, streamID *id);
int64_t streamTrimByLength(stream *s, long long maxlen, int approx);
int64_t streamTrimByID(stream *s, streamID minid, int approx);

#endif
