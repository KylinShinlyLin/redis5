/* quicklist.h - A generic doubly linked quicklist implementation
 *
 * Copyright (c) 2014, Matt Stancliff <matt@genges.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this quicklist of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this quicklist of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __QUICKLIST_H__
#define __QUICKLIST_H__

/* Node, quicklist, and Iterator are the only data structures used currently. */

/* quicklistNode is a 32 byte struct describing a ziplist for a quicklist.
 * We use bit fields keep the quicklistNode at 32 bytes.
 * count: 16 bits, max 65536 (max zl bytes is 65k, so max count actually < 32k).
 * encoding: 2 bits, RAW=1, LZF=2.
 * container: 2 bits, NONE=1, ZIPLIST=2.
 * recompress: 1 bit, bool, true if node is temporarry decompressed for usage.
 * attempted_compress: 1 bit, boolean, used for verifying during testing.
 * extra: 10 bits, free for future use; pads out the remainder of 32 bits */
//quicklistNode结构
typedef struct quicklistNode {
    struct quicklistNode *prev; //前驱
    struct quicklistNode *next; //后继
    unsigned char *zl;          //指向元素的指针
    unsigned int sz;            //整个ziplist的字节大小 /* ziplist size in bytes */
    unsigned int count : 16;    //ziplist的元素数量 /* count of items in ziplist */
    unsigned int encoding : 2;  //编码方式：1原生编码 2使用LZF压缩  /* RAW==1 or LZF==2 */
    unsigned int container : 2; //zl指向的容器类型 1表示none 2表示使用ziplist存储 /* NONE==1 or ZIPLIST==2 */
    unsigned int recompress : 1;//代表这个节点之前是否是压缩节点，若是，则在使用压缩节点前先进行解压，使用后需要重新压缩，此外为1，代表是压缩节点；  /* was this node previous compressed? */
    unsigned int attempted_compress : 1; //attempted_compress测试时使用； /* node can't compress; too small */
    unsigned int extra : 10; //预留字段 /* more bits to steal for future usage */
} quicklistNode;

/* quicklistLZF is a 4+N byte struct holding 'sz' followed by 'compressed'.
 * 'sz' is byte length of 'compressed' field.
 * 'compressed' is LZF data with total (compressed) length 'sz'
 * NOTE: uncompressed length is stored in quicklistNode->sz.
 * When quicklistNode->zl is compressed, node->zl points to a quicklistLZF */
//压缩后的quicklistNode结构
typedef struct quicklistLZF {
    unsigned int sz; //压缩后节点所占字节大小 /* LZF size in bytes*/
    char compressed[]; //压缩数据
} quicklistLZF;

/* quicklist is a 40 byte struct (on 64-bit systems) describing a quicklist.
 * 'count' is the number of total entries.
 * 'len' is the number of quicklist nodes.
 * 'compress' is: -1 if compression disabled, otherwise it's the number
 *                of quicklistNodes to leave uncompressed at ends of quicklist.
 * 'fill' is the user-requested (or default) fill factor. */
//quicklist定义
typedef struct quicklist {
    quicklistNode *head; //头结点
    quicklistNode *tail; //尾节点
    unsigned long count; //quicklist中的元素总数       /* total count of all entries in all ziplists */
    unsigned long len;   //quicklistNode节点个数       /* number of quicklistNodes */
    int fill : 16;       //每个quicklistNode中的ziplist长度      /* fill factor for individual nodes */
    unsigned int compress : 16; //不压缩的末端节点深度 /* depth of end nodes not to compress;0=off */
} quicklist;

//quicklist 迭代器
typedef struct quicklistIter {
    const quicklist *quicklist; //quicklist
    quicklistNode *current;  //指向当前遍历到的元素
    unsigned char *zi;   //指向元素所在的ziplist
    long offset;  //offset表明节点在所在的ziplist中的偏移量 /* offset in current ziplist */
    int direction; //direction表明迭代器的方向
} quicklistIter;

//quicklistEntry 元素entry
typedef struct quicklistEntry {
    const quicklist *quicklist; //quicklist
    quicklistNode *node;        //指向当前元素所在的quicklistNode
    unsigned char *zi;          //指向当前元素所在的ziplist
    unsigned char *value;       //指向该节点的字符串内容
    long long longval;          //为该节点的整数值
    unsigned int sz;            //该节点的大小
    int offset;                 //表明该节点相对于整个ziplist的偏移量，即该节点是ziplist的第多少个entry
} quicklistEntry;

#define QUICKLIST_HEAD 0
#define QUICKLIST_TAIL -1

/* quicklist node encodings */
#define QUICKLIST_NODE_ENCODING_RAW 1
#define QUICKLIST_NODE_ENCODING_LZF 2

/* quicklist compression disable */
#define QUICKLIST_NOCOMPRESS 0

/* quicklist container formats */
#define QUICKLIST_NODE_CONTAINER_NONE 1
#define QUICKLIST_NODE_CONTAINER_ZIPLIST 2

#define quicklistNodeIsCompressed(node)                                        \
    ((node)->encoding == QUICKLIST_NODE_ENCODING_LZF)

/* Prototypes */
quicklist *quicklistCreate(void);
quicklist *quicklistNew(int fill, int compress);
void quicklistSetCompressDepth(quicklist *quicklist, int depth);
void quicklistSetFill(quicklist *quicklist, int fill);
void quicklistSetOptions(quicklist *quicklist, int fill, int depth);
void quicklistRelease(quicklist *quicklist);
int quicklistPushHead(quicklist *quicklist, void *value, const size_t sz);
int quicklistPushTail(quicklist *quicklist, void *value, const size_t sz);
void quicklistPush(quicklist *quicklist, void *value, const size_t sz,
                   int where);
void quicklistAppendZiplist(quicklist *quicklist, unsigned char *zl);
quicklist *quicklistAppendValuesFromZiplist(quicklist *quicklist,
                                            unsigned char *zl);
quicklist *quicklistCreateFromZiplist(int fill, int compress,
                                      unsigned char *zl);
void quicklistInsertAfter(quicklist *quicklist, quicklistEntry *node,
                          void *value, const size_t sz);
void quicklistInsertBefore(quicklist *quicklist, quicklistEntry *node,
                           void *value, const size_t sz);
void quicklistDelEntry(quicklistIter *iter, quicklistEntry *entry);
int quicklistReplaceAtIndex(quicklist *quicklist, long index, void *data,
                            int sz);
int quicklistDelRange(quicklist *quicklist, const long start, const long stop);
quicklistIter *quicklistGetIterator(const quicklist *quicklist, int direction);
quicklistIter *quicklistGetIteratorAtIdx(const quicklist *quicklist,
                                         int direction, const long long idx);
int quicklistNext(quicklistIter *iter, quicklistEntry *node);
void quicklistReleaseIterator(quicklistIter *iter);
quicklist *quicklistDup(quicklist *orig);
int quicklistIndex(const quicklist *quicklist, const long long index,
                   quicklistEntry *entry);
void quicklistRewind(quicklist *quicklist, quicklistIter *li);
void quicklistRewindTail(quicklist *quicklist, quicklistIter *li);
void quicklistRotate(quicklist *quicklist);
int quicklistPopCustom(quicklist *quicklist, int where, unsigned char **data,
                       unsigned int *sz, long long *sval,
                       void *(*saver)(unsigned char *data, unsigned int sz));
int quicklistPop(quicklist *quicklist, int where, unsigned char **data,
                 unsigned int *sz, long long *slong);
unsigned long quicklistCount(const quicklist *ql);
int quicklistCompare(unsigned char *p1, unsigned char *p2, int p2_len);
size_t quicklistGetLzf(const quicklistNode *node, void **data);

#ifdef REDIS_TEST
int quicklistTest(int argc, char *argv[]);
#endif

/* Directions for iterators */
#define AL_START_HEAD 0
#define AL_START_TAIL 1

#endif /* __QUICKLIST_H__ */
