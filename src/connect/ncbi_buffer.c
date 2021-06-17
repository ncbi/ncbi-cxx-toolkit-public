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
 * Author:  Denis Vakatov, Anton Lavrentiev
 *
 * File Description:
 *   Memory-resident FIFO storage area (to be used e.g. in I/O buffering)
 *
 */

#include "ncbi_assert.h"
#include <connect/ncbi_buffer.h>
#include <stdlib.h>
#include <string.h>


#define _BUF_ALIGN(a, b)  (((a) + ((b) - 1)) & ~((b) - 1))
#define  BUF_ALIGN(s)     _BUF_ALIGN((s), sizeof(double))


/* Buffer chunk
 */
typedef struct SBufChunkTag {
    struct SBufChunkTag* next;
    void*  base;        /* base pointer of the chunk data if to be free()'d  */
    char*  data;        /* data stored in this chunk                         */
    size_t skip;        /* # of bytes already discarded(read) from the chunk */
    size_t size;        /* of data (including the discarded "skip" bytes)    */
    size_t extent;      /* total allocated size of "data" (0 if none/RO)     */
} SBufChunk;


/* Buffer
 */
struct SNcbiBuf {
    SBufChunk* list;    /* the singly-linked list of chunks                  */
    SBufChunk* last;    /* shortcut to the last chunk in the list            */
    size_t     unit;    /* chunk size unit                                   */
    size_t     size;    /* total buffer size; must be consistent at all times*/
};


extern size_t BUF_SetChunkSize(BUF* buf, size_t chunk_size)
{
    assert(!(sizeof(double) & (sizeof(double) - 1)));
    assert(!(BUF_ALIGN(BUF_DEF_CHUNK_SIZE) ^ BUF_DEF_CHUNK_SIZE));

    /* create the buffer internals, if not done yet */
    if (!*buf) {
        if (!(*buf = (struct SNcbiBuf*) malloc(sizeof(**buf))))
            return 0;
        (*buf)->list = (*buf)->last = 0;
        (*buf)->size = 0;
    }

    /* and set the minimal memory chunk unit size */
    (*buf)->unit = chunk_size ? BUF_ALIGN(chunk_size) : BUF_DEF_CHUNK_SIZE;
    return (*buf)->unit;
}


extern size_t BUF_Size(BUF buf)
{
#if defined(_DEBUG)  &&  !defined(NDEBUG)  
    size_t     size;
    SBufChunk* chunk;

    if (!buf)
        return 0;
    assert(!buf->list == !buf->last
           &&  !buf->size == !(buf->list  ||  buf->last));

    for (size = 0, chunk = buf->list;  chunk;  chunk = chunk->next) {
        /* NB: no empty chunks allowed within the list */
        assert(chunk->size > chunk->skip);
        size += chunk->size - chunk->skip;
        assert(chunk != buf->last  ||  !chunk->next);
    }
    assert(size == buf->size);
    return size;
#else
    return buf ? buf->size : 0;
#endif /*_DEBUG && !NDEBUG*/
}


/* Create a new buffer chunk.
 * Allocate at least "unit_size" bytes, but no less than "data_size" bytes.
 * Special case: "data_size" == 0 results in no data storage allocation.
 */
static SBufChunk* s_BUF_AllocChunk(size_t data_size, size_t unit_size)
{
    size_t chunk_size, alloc_size;
    SBufChunk* chunk;
    if (!data_size) {
        chunk_size = sizeof(*chunk);
        alloc_size = 0;
    } else {
        chunk_size = BUF_ALIGN(sizeof(*chunk));
        alloc_size = ((data_size + unit_size - 1) / unit_size) * unit_size;
    }
    assert(!data_size == !alloc_size);
    if (!(chunk = (SBufChunk*) malloc(chunk_size + alloc_size)))
        return 0/*failure*/;

    /* NB: chunk->next is left uninited! */
    chunk->base   = 0;
    chunk->data   = alloc_size ? (char*) chunk + chunk_size : 0;
    chunk->skip   = 0;
    chunk->size   = 0;
    chunk->extent = alloc_size;
    return chunk/*success*/;
}


extern int/*bool*/ BUF_AppendEx(BUF* buf, void* base, size_t alloc_size,
                                void* data, size_t size)
{
    SBufChunk* chunk;

    if (!size) {
        if (base)
            free(base);
        return 1/*success*/;
    }
    if (!data)
        return 0/*failure*/;

    /* init the buffer internals, if not init'd yet */
    if (!*buf  &&  !BUF_SetChunkSize(buf, 0))
        return 0/*failure*/;

    if (!(chunk = s_BUF_AllocChunk(0, 0)))
        return 0/*failure*/;

    assert(!chunk->data);
    chunk->next   = 0;
    chunk->base   = base;
    chunk->data   = (char*) data;
    chunk->size   = size;
    chunk->extent = alloc_size;

    if ((*buf)->last)
        (*buf)->last->next = chunk;
    else
        (*buf)->list       = chunk;
    (*buf)->last  = chunk;
    (*buf)->size += size;
    return 1/*success*/;
}


extern int/*bool*/ BUF_Append(BUF* buf, const void* data, size_t size)
{
    return BUF_AppendEx(buf, 0, 0, (void*) data, size);
}


extern int/*bool*/ BUF_PrependEx(BUF* buf, void* base, size_t alloc_size,
                                 void* data, size_t size)
{
    SBufChunk* chunk;

    if (!size) {
        if (base)
            free(base);
        return 1/*success*/;
    }
    if (!data)
        return 0/*failure*/;
    
    /* init the buffer internals, if not init'd yet */
    if (!*buf  &&  !BUF_SetChunkSize(buf, 0))
        return 0/*failure*/;

    if (!(chunk = s_BUF_AllocChunk(0, 0)))
        return 0/*failure*/;

    assert(!chunk->data);
    chunk->next   = (*buf)->list;
    chunk->base   = base;
    chunk->data   = (char*) data;
    chunk->size   = size;
    chunk->extent = alloc_size;

    if (!(*buf)->last) {
        assert(!chunk->next);
        (*buf)->last = chunk;
    }
    (*buf)->list  = chunk;
    (*buf)->size += size;
    return 1/*success*/;
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
        return 1/*success*/;
    if (!src)
        return 0/*failure*/;

    /* init the buffer internals, if not init'd yet */
    if (!*buf  &&  !BUF_SetChunkSize(buf, 0))
        return 0/*failure*/;

    /* find the last allocated chunk */
    tail = (*buf)->last;
    assert(!(*buf)->list == !tail  &&  !(*buf)->size == !tail);

    /* schedule to write to an unfilled space of the last allocated chunk */
    if (tail  &&  tail->extent > tail->size) {
        pending = tail->extent - tail->size;
        if (pending > size)
            pending = size;
        size -= pending;
    } else
        pending = 0;

    /* if necessary, allocate a new chunk and write remaining data into it */
    if (size) {
        SBufChunk* next;
        if (!(next = s_BUF_AllocChunk(size, (*buf)->unit)))
            return 0/*false*/;
        assert(next->data);
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

    /* finish up with the pending portion of the data (from the beginning) */
    if (pending) {
        void* dst = tail->data + tail->size;
        if (dst != src)
            memmove(dst, src, pending);
        tail->size += pending;
    }
    (*buf)->size += pending + size;
    assert((*buf)->size  &&  (*buf)->list  &&  (*buf)->last);
    return 1/*success*/;
}


extern int/*bool*/ BUF_Pushback(BUF* buf, const void* src, size_t size)
{
    SBufChunk* head;
    void* dst;

    if (!size)
        return 1/*success*/;
    if (!src)
        return 0/*failure*/;

    /* init the buffer internals, if not init'd yet */
    if (!*buf  &&  !BUF_SetChunkSize(buf, 0))
        return 0/*failure*/;

    head = (*buf)->list;

    /* allocate and link a new chunk at the beginning of the chunk list */
    if (!head  ||  !head->extent  ||  size > head->skip) {
        size_t     skip = head  &&  head->extent ? head->skip : 0;
        SBufChunk* next = head;
        size -= skip;
        assert(size);
        if (!(head = s_BUF_AllocChunk(size, (*buf)->unit)))
            return 0/*failure*/;
        if (skip) {
            /* fill up the skip area */
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

    /* write remaining data */
    assert(head->data  &&  head->skip >= size);
    head->skip -= size;
    dst = head->data + head->skip;
    if (dst != src)
        memmove(dst, src, size);
    (*buf)->size += size;
    assert((*buf)->size  &&  (*buf)->list  &&  (*buf)->last);
    return 1/*success*/;
}


extern size_t BUF_PeekAtCB(BUF      buf,
                           size_t   pos,
                           size_t (*callback)(void*, const void*, size_t),
                           void*    cbdata,
                           size_t   size)
{
    size_t     todo;
    SBufChunk* chunk;

    assert(!buf  ||  (!buf->list == !buf->last
                      &&  !buf->size == !(buf->list  ||  buf->last)));

    if (!size  ||  !buf  ||  buf->size <= pos/*NB: includes !buf->size*/)
        return 0/*nothing*/;
    assert(buf->list  &&  buf->last);

    /* special treatment for NULL callback */
    if (!callback) {
        todo = buf->size - pos;
        return todo < size ? todo : size;
    }

    /* skip "pos" bytes, by fast tracking for last chunk first if possible */
    chunk = buf->last; 
    assert(chunk->size > chunk->skip/*i.e. chunk->size > 0*/);
    if (pos + (todo = chunk->size - chunk->skip) < buf->size) {
        for (chunk = buf->list;  chunk;  chunk = chunk->next) {
            todo = chunk->size - chunk->skip;
            assert(chunk->size > chunk->skip/*i.e. chunk->size > 0*/);
            if (todo > pos)
                break;
            pos -= todo;
        }
        assert(chunk != buf->last);
    } else
        pos -= buf->size - todo;

    /* process the data being "peeked into" */
    for (todo = size;  todo  &&  chunk;  chunk = chunk->next, pos = 0) {
        size_t skip = chunk->skip + pos;
        size_t peek = chunk->size - skip;
        assert(chunk->size > skip/*i.e. chunk->size > 0*/);
        if (peek > todo)
            peek = todo;
        assert(peek);
        skip  = callback(cbdata, (const char*) chunk->data + skip, peek);
        assert(skip <= peek);
        todo -= skip;
        if (skip < peek)
            break;
    }

    assert(size >= todo);
    return size - todo;
}


static size_t x_BUF_MemcpyCB(void* cbdata, const void* data, size_t size)
{
    char** dst = (char**) cbdata;
    memcpy(*dst, data, size);
    *dst += size;
    return size;
}


extern size_t BUF_PeekAt(BUF buf, size_t pos, void* dst, size_t size)
{
    return BUF_PeekAtCB(buf, pos, dst ? x_BUF_MemcpyCB : 0, &dst, size);
}


extern size_t BUF_Peek(BUF buf, void* dst, size_t size)
{
    return BUF_PeekAt(buf, 0, dst, size);
}


extern size_t BUF_Read(BUF buf, void* dst, size_t size)
{
    size_t todo = size;

    assert(!buf  ||  (!buf->list == !buf->last
                      &&  !buf->size == !(buf->list  ||  buf->last)));
           
    /* first, peek to the caller's data buffer, if non-NULL */
    if (dst)
        size = BUF_Peek(buf, dst, todo);
    else if (!buf  ||  !buf->size)
        return 0/*nothing*/;
    if (!size)
        return 0/*nothing*/;

    /* then safely remove the peeked data (making it "read") from the buffer */
    assert(size <= todo);
    todo = size;

    assert(buf  &&  buf->list  &&  buf->last);
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
        /* discard the entire chunk */
        if (!(buf->list = head->next))
            buf->last = 0;
        if (head->base)
            free(head->base);
        free(head);
        buf->size -= avail;
        todo      -= avail;
    } while (todo  &&  buf->list);

    assert(!buf->list == !buf->last
           &&  !buf->size == !(buf->list  ||  buf->last));
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
    assert(src->list  &&  src->last);
    /* init the buffer internals, if not init'd yet */
    if (!*dst  &&  !BUF_SetChunkSize(dst, 0))
        return 0/*failure*/;
    assert(!(*dst)->list == !(*dst)->last
           &&  !(*dst)->size == !((*dst)->list  ||  (*dst)->last));
    if ((*dst)->last)
        (*dst)->last->next = src->list;
    else
        (*dst)->list       = src->list;
    (*dst)->last  = src->last;
    (*dst)->size += src->size;
    src->list = src->last = 0;
    src->size = 0;
    assert((*dst)->size  &&  (*dst)->list  &&  (*dst)->last);
    return 1/*success*/;
}


extern void BUF_Destroy(BUF buf)
{
    if (buf) {
        BUF_Erase(buf);
        free(buf);
    }
}
