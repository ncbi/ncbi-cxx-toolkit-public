/* $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Memory-resident FIFO storage area (to be used e.g. in I/O buffering)
 *
 */

#include "ncbi_assert.h"
#include <connect/ncbi_buffer.h>
#include <stdlib.h>
#include <string.h>


/* Buffer chunk
 */
typedef struct SBufChunkTag {
    struct SBufChunkTag* next;
    size_t extent;      /* total allocated size of "data" (0 if none)        */
    size_t skip;        /* # of bytes already discarded(read) from the chunk */
    size_t size;        /* of data (including the discarded "skip" bytes)    */
    void*  base;        /* base ptr of the data chunk if to be free()'d      */
    char*  data;        /* data stored in this chunk                         */
} SBufChunk;


/* Buffer
 */
struct SNcbiBuf {
    SBufChunk* list;    /* the linked list of chunks                         */
    SBufChunk* last;    /* shortcut to the last chunk in the list            */
    size_t     unit;    /* chunk size unit                                   */
    size_t     size;    /* total buffer size; m.b.consistent at all times    */
};


extern size_t BUF_SetChunkSize(BUF* buf, size_t chunk_size)
{
    /* create buffer internals, if not created yet */
    if (!*buf) {
        if (!(*buf = (struct SNcbiBuf*) malloc(sizeof(**buf))))
            return 0;
        (*buf)->list = (*buf)->last = 0;
        (*buf)->size = 0;
    }

    /* and set the min. mem. chunk unit size */
    (*buf)->unit = chunk_size ? chunk_size : BUF_DEF_CHUNK_SIZE;
    return (*buf)->unit;
}


extern size_t BUF_Size(BUF buf)
{
#if defined(_DEBUG)  &&  !defined(NDEBUG)  
    size_t     size;
    SBufChunk* chunk;

    if (!buf)
        return 0;

    for (size = 0, chunk = buf->list;  chunk;  chunk = chunk->next) {
        /* NB: no empty blocks allowed within the list */
        assert(chunk->size > chunk->skip);
        size += chunk->size - chunk->skip;
    }
    assert(size == buf->size);
    return size;
#else
    return buf ? buf->size : 0;
#endif /*_DEBUG && !NDEBUG*/
}


/* Create a new chunk.
 * Allocate at least "chunk_size" bytes, but no less than "data_size" bytes.
 * Special case: "data_size" == 0 results in no data storage allocation.
 */
static SBufChunk* s_AllocChunk(size_t data_size, size_t chunk_size)
{
    size_t alloc_size = ((data_size + chunk_size - 1)
                         / chunk_size) * chunk_size;
    SBufChunk* chunk = (SBufChunk*) malloc(sizeof(*chunk) + alloc_size);
    if (!chunk)
        return 0;

    /* NB: leave chunk->next uninited! */
    chunk->extent = alloc_size;
    chunk->skip   = 0;
    chunk->size   = 0;
    chunk->base   = 0/*not yet used*/;
    chunk->data   = alloc_size ? (char*) chunk + sizeof(*chunk) : 0;
    return chunk;
}


/*not yet public*/
int/*bool*/ BUF_AppendEx(BUF* buf, void* base, size_t alloc_size,
                         void* data, size_t size)
{
    SBufChunk* chunk;

    if (!size)
        return 1/*true*/;
    if (!data)
        return 0/*false*/;

    /* init the buffer internals, if not init'd yet */
    if (!*buf  &&  !BUF_SetChunkSize(buf, 0))
        return 0/*false*/;

    if (!(chunk = s_AllocChunk(0, (*buf)->unit)))
        return 0/*false*/;

    assert(!chunk->data);
    chunk->base   = base;
    chunk->extent = alloc_size;
    chunk->data   = (char*) data;
    chunk->size   = size;
    chunk->next   = 0;

    if ((*buf)->last)
        (*buf)->last->next = chunk;
    else
        (*buf)->list       = chunk;
    (*buf)->last  = chunk;
    (*buf)->size += size;
    return 1/*true*/;
}


extern int/*bool*/ BUF_Append(BUF* buf, const void* data, size_t size)
{
    return BUF_AppendEx(buf, 0, 0, (void*) data, size);
}


/*not yet public*/
int/*bool*/ BUF_PrependEx(BUF* buf, void* base, size_t alloc_size,
                          void* data, size_t size)
{
    SBufChunk* chunk;

    if (!size)
        return 1/*true*/;
    if (!data)
        return 0/*false*/;
    
    /* init the buffer internals, if not init'd yet */
    if (!*buf  &&  !BUF_SetChunkSize(buf, 0))
        return 0/*false*/;

    if (!(chunk = s_AllocChunk(0, (*buf)->unit)))
        return 0/*false*/;

    assert(!chunk->data);
    chunk->base   = base;
    chunk->extent = alloc_size;
    chunk->data   = (char*) data;
    chunk->size   = size;
    chunk->next   = (*buf)->list;

    if (!(*buf)->last) {
        assert(!chunk->next);
        (*buf)->last = chunk;
    }
    (*buf)->list  = chunk;
    (*buf)->size += size;
    return 1/*true*/;
}


extern int/*bool*/ BUF_Prepend(BUF* buf, const void* data, size_t size)
{
    return BUF_PrependEx(buf, 0, 0, (void*) data, size);
}


extern int/*bool*/ BUF_Write(BUF* buf, const void* src, size_t size)
{
    SBufChunk* tail;
    size_t pending;

    if (!size)
        return 1/*true*/;
    if (!src)
        return 0/*false*/;

    /* init the buffer internals, if not init'd yet */
    if (!*buf  &&  !BUF_SetChunkSize(buf, 0))
        return 0/*false*/;

    /* find the last allocated chunk */
    tail = (*buf)->last;

    /* schedule to write to an unfilled space of the last allocated chunk */
    if (tail  &&  tail->extent > tail->size) {
        pending = tail->extent - tail->size;
        if (pending > size)
            pending = size;
        size -= pending;
    } else
        pending = 0;

    /* if necessary, allocate a new chunk and write to it */
    if (size) {
        SBufChunk* next;
        if (!(next = s_AllocChunk(size, (*buf)->unit)))
            return 0/*false*/;
        memcpy(next->data, (const char*) src + pending, size);
        next->size = size;
        next->next = 0;

        /* append the new chunk to the list */
        if (tail) {
            tail->next   = next;
            assert( (*buf)->list);
        } else {
            assert(!(*buf)->list);
            (*buf)->list = next;
        }
        (*buf)->last = next;
    }

    if (pending) {
        memcpy(tail->data + tail->size, src, pending);
        tail->size += pending;
    }
    (*buf)->size += pending + size;
    return 1/*true*/;
}


extern int/*bool*/ BUF_PushBack(BUF* buf, const void* src, size_t size)
{
    SBufChunk* head;

    if (!size)
        return 1/*true*/;
    if (!src)
        return 0/*false*/;

    /* init the buffer internals, if not init'd yet */
    if (!*buf  &&  !BUF_SetChunkSize(buf, 0))
        return 0/*false*/;

    head = (*buf)->list;

    /* allocate and link a new chunk at the beginning of the chunk list */
    if (!head  ||  !head->extent  ||  head->skip < size) {
        size_t     skip = head  &&  head->extent ? head->skip : 0;
        SBufChunk* next = head;
        if (!(head = s_AllocChunk(size -= skip, (*buf)->unit)))
            return 0/*false*/;
        if (skip) {
            memcpy(next->data, (const char*) src + size, skip);
            (*buf)->size += skip;
            next->skip = 0;
        }
        head->skip = head->size = head->extent;
        if (!(head->next = next)) {
            assert(!(*buf)->last);
            (*buf)->last = head;
        } else
            assert( (*buf)->last);
        (*buf)->list = head;
    }

    /* write data */
    assert(head->skip >= size);
    head->skip -= size;
    memcpy(head->data + head->skip, src, size);
    (*buf)->size += size;
    return 1/*true*/;
}


extern size_t BUF_PeekAtCB(BUF      buf,
                           size_t   pos,
                           size_t (*callback)(void*, const void*, size_t),
                           void*    cbdata,
                           size_t   size)
{
    size_t     todo;
    SBufChunk* chunk;

    if (!size  ||  !buf  ||  !buf->size  ||  !buf->list)
        return 0;

    /* special treatment for NULL callback */
    if (!callback) {
        if (buf->size <= pos)
            return 0;
        todo = buf->size - pos;
        return todo < size ? todo : size;
    }

    /* skip "pos" bytes */
    for (chunk = buf->list;  chunk;  chunk = chunk->next) {
        size_t avail = chunk->size - chunk->skip;
        assert(chunk->size > chunk->skip);
        if (avail > pos)
            break;
        pos -= avail;
    }

    /* process the peeked data */
    for (todo = size;  todo  &&  chunk;  chunk = chunk->next, pos = 0) {
        size_t skip = chunk->skip + pos;
        size_t copy = chunk->size - skip;
        assert(chunk->size > skip);
        if (copy > todo)
            copy = todo;

        skip  = callback(cbdata, (const char*) chunk->data + skip, copy);
        if (skip > copy)
            skip = copy;
        todo -= skip;
        if (skip < copy)
            break;
    }

    assert(size >= todo);
    return size - todo;
}


static size_t s_MemcpyCB(void* cbdata, const void* data, size_t size)
{
    char** dst = (char**) cbdata;
    memcpy(*dst, data, size);
    *dst += size;
    return size;
}


extern size_t BUF_PeekAt(BUF buf, size_t pos, void* dst, size_t size)
{
    void* cbdata = dst;
    return BUF_PeekAtCB(buf, pos, dst ? s_MemcpyCB : 0, &cbdata, size);
}


extern size_t BUF_Peek(BUF buf, void* dst, size_t size)
{
    return BUF_PeekAt(buf, 0, dst, size);
}


extern size_t BUF_Read(BUF buf, void* dst, size_t size)
{
    size_t todo;

    /* peek to the callers data buffer, if non-NULL */
    if (dst)
        size = BUF_Peek(buf, dst, size);
    else if (!buf  ||  !buf->size  ||  !buf->list)
        return 0;
    if (!size)
        return 0;

    /* remove the read data from the buffer */ 
    todo = size;
    do {
        SBufChunk* head  = buf->list;
        size_t     avail = head->size - head->skip;
        if (todo < avail) {
            /* discard some of the chunk data */
            head->skip += todo;
            buf->size  -= todo;
            todo = 0;
            break;
        }
        /* discard the whole chunk */
        if (!(buf->list = head->next))
            buf->last = 0;
        if (head->base)
            free(head->base);
        free(head);
        buf->size -= avail;
        todo      -= avail;
    } while (todo  &&  buf->list);

    assert(size >= todo);
    return size - todo;
}


extern void BUF_Erase(BUF buf)
{
    if (buf) {
        while (buf->list) {
            SBufChunk* head = buf->list;
            buf->list = head->next;
            if (head->base)
                free(head->base);
            free(head);
        }
        buf->last = 0;
        buf->size = 0;
    }
}


extern int/*bool*/ BUF_Splice(BUF* dst, BUF src)
{
    if (!src  ||  !src->size)
        return 1/*success*/;
    /* init the buffer internals, if not init'd yet */
    if (!*dst  &&  !BUF_SetChunkSize(dst, 0))
        return 0/*false*/;
    if ((*dst)->last)
        (*dst)->last->next = src->list;
    else
        (*dst)->list       = src->list;
    (*dst)->last  = src->last;
    (*dst)->size += src->size;
    src->list = src->last = 0;
    src->size = 0;
    return 1/*success*/;
}


extern void BUF_Destroy(BUF buf)
{
    if (buf) {
        BUF_Erase(buf);
        free(buf);
    }
}
