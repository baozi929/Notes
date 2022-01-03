#ifndef STREAM_H
#define STREAM_H

#include "rax.h"
#include "listpack.h"

/* Stream item ID: a 128 bit number composed of a milliseconds time and
 * a sequence counter. IDs generated in the same millisecond (or in a past
 * millisecond if the clock jumped backward) will use the millisecond time
 * of the latest generated ID and an incremented sequence. */

/* ��ϢID��128λ */
typedef struct streamID {
    // 19700101����ĺ�����
    uint64_t ms;        /* Unix time in milliseconds. */
    // ���
    uint64_t seq;       /* Sequence number. */
} streamID;

typedef struct stream {
    // ����stream��rax��
    // rax���洢��Ϣ�����������ľ�����Ϣ��ÿ����Ϣ��Ψһ��ID
    // ����ϢIDΪ������Ϣ����Ϊֵ�洢��rax����
    // ע�⣬rax���ֵ�һ���ڵ���ܴ洢�����Ϣ
    rax *rax;               /* The radix tree holding the stream. */
    // ��ǰstream�е���Ϣ�������������Ѿ�ɾ������Ϣ��
    uint64_t length;        /* Number of elements inside this stream. */
    // ��ǰstream�����������Ϣ��ID��streamΪ��ʱ������Ϊ0
    streamID last_id;       /* Zero if there are yet no items. */
    // ��ǰstream��ص������飬�������������Ϊ����streamCGΪֵ�洢��rax��
    rax *cgroups;           /* Consumer groups dictionary: name -> streamCG */
} stream;

/* We define an iterator to iterate stream items in an abstract way, without
 * caring about the radix tree + listpack representation. Technically speaking
 * the iterator is only used inside streamReplyWithRange(), so could just
 * be implemented inside the function, but practically there is the AOF
 * rewriting code that also needs to iterate the stream to emit the XADD
 * commands. */
/* �����������ڱ���stream�е���Ϣ */
typedef struct streamIterator {
    // ��ǰ���������ڱ�������Ϣ��
    stream *stream;         /* The stream we are iterating. */
    // ʵ����Ϣ�洢��listpack�У�ÿ��listpack����master entry�����ݵ�һ���������Ϣ������
    // master_idΪ����Ϣid
    streamID master_id;     /* ID of the master entry at listpack head. */
    // master entry��field�ĸ���
    uint64_t master_fields_count;       /* Master entries # of fields. */
    // master entry field��listpack�е��׵�ַ
    unsigned char *master_fields_start; /* Master entries start in listpack. */
    // ��listpack����Ϣ��field��master entry��field��ȫ��ͬʱ����Ҫ��¼��ǰ���ڵ�field�ľ���λ��
    // master_fields_ptr����ʵ��������ܵ�
    unsigned char *master_fields_ptr;   /* Master field to emit next. */
    // ��ǰ��������Ϣ�ı�־λ
    int entry_flags;                    /* Flags of entry we are emitting. */
    // ����������True��ʾ��β->ͷ
    int rev;                /* True if iterating end to start (reverse). */
    // �õ������������ϢID�ķ�Χ��start
    uint64_t start_key[2];  /* Start key as 128 bit big endian. */
    // �õ������������ϢID�ķ�Χ��end
    uint64_t end_key[2];    /* End key as 128 bit big endian. */
    // rax�����������ڱ���rax�е�����key
    raxIterator ri;         /* Rax iterator. */
    // ָ��ǰlistpack��ָ��
    unsigned char *lp;      /* Current listpack. */
    // ָ��ǰlistpack�����ڱ�����Ԫ��
    unsigned char *lp_ele;  /* Current listpack cursor. */
    // ָ��ǰ��Ϣ��flags
    unsigned char *lp_flags; /* Current entry flags pointer. */
    /* Buffers used to hold the string of lpGet() when the element is
     * integer encoded, so that there is no string representation of the
     * element inside the listpack itself. */
    // ��listpack�ж�ȡ����ʱ�õĻ���
    unsigned char field_buf[LP_INTBUF_SIZE];
    // ��listpack�ж�ȡ����ʱ�õĻ���
    unsigned char value_buf[LP_INTBUF_SIZE];
} streamIterator;

/* Consumer group. */
/* ������ */
typedef struct streamCG {
    // ����������ȷ�ϵ����һ����Ϣ��ID
    streamID last_id;       /* Last delivered (not acknowledged) ID for this
                               group. Consumers that will just ask for more
                               messages will served with IDs > than this. */
    // ����������δȷ�ϵ���Ϣ
    // key����ϢID
    // value��streamNACK������һ����δȷ�ϵ���Ϣ��
    rax *pel;               /* Pending entries list. This is a radix tree that
                               has every message delivered to consumers (without
                               the NOACK option) that was yet not acknowledged
                               as processed. The key of the radix tree is the
                               ID as a 64 bit big endian number, while the
                               associated value is a streamNACK structure.*/
    // �������������е�������
    // key�������ߵ�����
    // value��streamConsumer������һ�������ߣ�
    rax *consumers;         /* A radix tree representing the consumers by name
                               and their associated representation in the form
                               of streamConsumer structures. */
} streamCG;

/* A specific consumer in a consumer group.  */
/* �������е�һ�������� */
typedef struct streamConsumer {
    // �����������һ�λ�Ծ��ʱ��
    mstime_t seen_time;         /* Last time this consumer was active. */
    // �����ߵ�����
    sds name;                   /* Consumer name. This is how the consumer
                                   will be identified in the consumer group
                                   protocol. Case sensitive. */
    // ����������δȷ�ϵ���Ϣ
    // key����ϢID
    // value��streamNACK
    rax *pel;                   /* Consumer specific pending entries list: all
                                   the pending messages delivered to this
                                   consumer not yet acknowledged. Keys are
                                   big endian message IDs, while values are
                                   the same streamNACK structure referenced
                                   in the "pel" of the consumer group structure
                                   itself, so the value is shared. */
} streamConsumer;

/* Pending (yet not acknowledged) message in a consumer group. */
/* ��������δȷ�ϵ���Ϣ */
typedef struct streamNACK {
    // ����Ϣ����͸����ѷ���ʱ��
    mstime_t delivery_time;     /* Last time this message was delivered. */
    // ����Ϣ�Ѿ����͵Ĵ���
    uint64_t delivery_count;    /* Number of times this message was delivered.*/
    // ����Ϣ��ǰ������������
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
