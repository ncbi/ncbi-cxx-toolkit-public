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

#include <connect/ncbi_buffer.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifndef NDEBUG
/* NOTE: this conditional inclusion is only needed by assert.h on Darwin!
 * We do not want to include "ncbi_config.h" to additionally branch on
 * NCBI_OS_DAWRIN here because in C toolkit it in turn pulls ncbilcl.h,
 * which includes <stdio.h>, thus making this conditional unnecessary.
 */
#  include <stdio.h>
#endif


/* Buffer chunk
 */
typedef struct SBufChunkTag {
    struct SBufChunkTag* next;
    size_t size;       /* of data (including the discarded "n_skip" bytes)  */
    size_t alloc_size; /* maximum available (allocated) size of "data"      */
    size_t n_skip;     /* # of bytes already discarded(read) from the chunk */
    char*  data;       /* data stored in this chunk                         */
} SBufChunk;


/* Buffer
 */
typedef struct BUF_tag {
    size_t     chunk_size; /* this is actually a chunk size unit */
    SBufChunk* list;
    SBufChunk* last;
} BUF_struct;



extern size_t BUF_SetChunkSize(BUF* pBuf, size_t chunk_size)
{
    /* create buffer internals, if not created yet */
    if ( !*pBuf ) {
        *pBuf = (BUF_struct*) malloc(sizeof(BUF_struct));
        if ( !*pBuf )
            return 0;
        (*pBuf)->list = (*pBuf)->last = 0;
    }

    /* and set the min. mem. chunk size */
    (*pBuf)->chunk_size = chunk_size ? chunk_size : BUF_DEF_CHUNK_SIZE;
    return (*pBuf)->chunk_size;
}


extern size_t BUF_Size(BUF buf)
{
    size_t     size;
    SBufChunk* pChunk;
    if ( !buf )
        return 0;

    for (size = 0, pChunk = buf->list;  pChunk;  pChunk = pChunk->next) {
        assert(pChunk->size > pChunk->n_skip);
        size += pChunk->size - pChunk->n_skip;
    }
    return size;
}


/* Create a new chunk.
 * Allocate at least "chunk_size" bytes, but no less than "size" bytes.
 * Special case: "size" == 0 results in no data storage allocation.
 */
static SBufChunk* s_AllocChunk(size_t size, size_t chunk_size)
{
    size_t alloc_size = ((size + chunk_size - 1) / chunk_size) * chunk_size;
    SBufChunk* pChunk = (SBufChunk*) malloc(sizeof(*pChunk) + alloc_size);
    if ( !pChunk )
        return 0;

    pChunk->next       = 0;
    pChunk->size       = 0;
    pChunk->alloc_size = alloc_size;
    pChunk->n_skip     = 0;
    pChunk->data       = alloc_size ? (char*) pChunk + sizeof(*pChunk) : 0;
    return pChunk;
}


extern int/*bool*/ BUF_Append(BUF* pBuf, const void* data, size_t size)
{
    SBufChunk* pChunk;
    if ( !size )
        return 1/*true*/;

    /* init the buffer internals, if not init'd yet */
    if (!*pBuf  &&  !BUF_SetChunkSize(pBuf, 0))
        return 0/*false*/;

    pChunk = s_AllocChunk(0, (*pBuf)->chunk_size);
    if ( !pChunk )
        return 0/*false*/;

    assert( !pChunk->data );
    pChunk->alloc_size = size;
    pChunk->size       = size;
    pChunk->data       = (char*) data;

    if ( (*pBuf)->last )
        (*pBuf)->last->next = pChunk;
    else
        (*pBuf)->list = pChunk;
    (*pBuf)->last = pChunk;
    return 1/*true*/;
}


extern int/*bool*/ BUF_Prepend(BUF* pBuf, const void* data, size_t size)
{
    SBufChunk* pChunk;
    if ( !size )
        return 1/*true*/;

    /* init the buffer internals, if not init'd yet */
    if (!*pBuf  &&  !BUF_SetChunkSize(pBuf, 0))
        return 0/*false*/;

    pChunk = s_AllocChunk(0, (*pBuf)->chunk_size);
    if ( !pChunk )
        return 0/*false*/;

    assert( !pChunk->data );
    pChunk->alloc_size = size;
    pChunk->size       = size;
    pChunk->data       = (char*) data;

    if ( (*pBuf)->list )
        pChunk->next = (*pBuf)->list;
    (*pBuf)->list = pChunk;
    return 1/*true*/;
}


extern int/*bool*/ BUF_Write(BUF* pBuf, const void* data, size_t size)
{
    SBufChunk* pChunk, *pTail;
    if ( !size )
        return 1/*true*/;

    /* init the buffer internals, if not init'd yet */
    if (!*pBuf  &&  !BUF_SetChunkSize(pBuf, 0))
        return 0/*false*/;

    /* find the last allocated chunk */
    pTail = (*pBuf)->last;

    /* write to an unfilled space of the last allocated chunk, if any */
    if (pTail  &&  pTail->size != pTail->alloc_size) {
        size_t n_avail = pTail->alloc_size - pTail->size;
        size_t n_write = (size <= n_avail) ? size : n_avail;
        assert(pTail->size < pTail->alloc_size);
        memcpy(pTail->data + pTail->size, data, n_write);
        pTail->size += n_write;
        size -= n_write;
        data = (char*) data + n_write;
    }

    /* allocate and write to the new chunk, if necessary */
    if ( size ) {
        pChunk = s_AllocChunk(size, (*pBuf)->chunk_size);
        if ( !pChunk )
            return 0/*false*/;
        pChunk->n_skip = 0;
        pChunk->size   = size;
        memcpy(pChunk->data, data, size);

        /* add the new chunk to the list */
        if ( pTail )
            pTail->next = pChunk;
        else
            (*pBuf)->list = pChunk;
        (*pBuf)->last = pChunk;
    }
    return 1/*true*/;
}


extern int/*bool*/ BUF_PushBack(BUF* pBuf, const void* data, size_t size)
{
    SBufChunk* pChunk;
    if ( !size )
        return 1/*true*/;

    /* init the buffer internals, if not init'd yet */
    if (!*pBuf  &&  !BUF_SetChunkSize(pBuf, 0) )
        return 0/*false*/;

    pChunk = (*pBuf)->list;

    /* allocate and link a new chunk to the beginning of the chunk list */
    if (!pChunk  ||  size > pChunk->n_skip) {
        pChunk = s_AllocChunk(size, (*pBuf)->chunk_size);
        if ( !pChunk )
            return 0/*false*/;
        pChunk->n_skip = pChunk->size = pChunk->alloc_size;
        pChunk->next  = (*pBuf)->list;
        (*pBuf)->list = pChunk;
        if ( !(*pBuf)->last ) {
            (*pBuf)->last = pChunk;
        }
    }

    /* write data */
    assert(pChunk->n_skip >= size);
    pChunk->n_skip -= size;
    memcpy(pChunk->data + pChunk->n_skip, data, size);
    return 1/*true*/;
}


extern size_t BUF_Peek(BUF buf, void* data, size_t size)
{
    return BUF_PeekAt(buf, 0, data, size);
}


extern size_t BUF_PeekAt(BUF buf, size_t pos, void* data, size_t size)
{
    size_t     n_todo = size;
    size_t     n_extra_skip = 0;
    SBufChunk* pChunk;

    if (!size  ||  !buf  ||  !buf->list)
        return 0;

    /* special treatment for NULL data buffer */
    if ( !data ) {
        size_t buf_size = BUF_Size(buf);
        if (buf_size <= pos)
            return 0;
        buf_size -= pos;
        return buf_size < size ? buf_size : size;
    }

    /* skip "pos" bytes */
    for (pChunk = buf->list;  pChunk;  pChunk = pChunk->next) {
        size_t chunk_size = pChunk->size - pChunk->n_skip;
        assert(pChunk->size > pChunk->n_skip);
        if (chunk_size > pos) {
            n_extra_skip = pos;
            break;
        }
        pos -= chunk_size;
    }

    /* copy the peeked data to "data" */
    for ( ;  n_todo  &&  pChunk;  pChunk = pChunk->next, n_extra_skip = 0) {
        size_t n_skip = pChunk->n_skip + n_extra_skip;
        size_t n_copy = pChunk->size - n_skip;
        assert(pChunk->size > n_skip);
        if (n_copy > n_todo)
            n_copy = n_todo;

        memcpy(data, (char*) pChunk->data + n_skip, n_copy);
        data = (char*) data + n_copy;
        n_todo -= n_copy;
    }

    assert(size >= n_todo);
    return (size - n_todo);
}


extern size_t BUF_PeekAtCB(BUF buf,
                           size_t pos,
                           void (*callback)(void*, void*, size_t),
                           void* data,
                           size_t size)
{
    size_t     n_todo = size;
    size_t     n_extra_skip = 0;
    SBufChunk* pChunk;

    if (!size  ||  !buf  ||  !buf->list)
        return 0;

    /* special treatment for NULL callback */
    if ( !callback ) {
        size_t buf_size = BUF_Size(buf);
        if (buf_size <= pos)
            return 0;
        buf_size -= pos;
        return buf_size < size ? buf_size : size;
    }

    /* skip "pos" bytes */
    for (pChunk = buf->list;  pChunk;  pChunk = pChunk->next) {
        size_t chunk_size = pChunk->size - pChunk->n_skip;
        assert(pChunk->size > pChunk->n_skip);
        if (chunk_size > pos) {
            n_extra_skip = pos;
            break;
        }
        pos -= chunk_size;
    }

    /* process the peeked data */
    for ( ;  n_todo  &&  pChunk;  pChunk = pChunk->next, n_extra_skip = 0) {
        size_t n_skip = pChunk->n_skip + n_extra_skip;
        size_t n_copy = pChunk->size - n_skip;
        assert(pChunk->size > n_skip);
        if (n_copy > n_todo)
            n_copy = n_todo;

        callback(data, (char*) pChunk->data + n_skip, n_copy);
        n_todo -= n_copy;
    }

    assert(size >= n_todo);
    return (size - n_todo);
}


extern size_t BUF_Read(BUF buf, void* data, size_t size)
{
    size_t n_todo;
    if (!buf  ||  !size)
        return 0;

    /* peek to the callers data buffer, if non-NULL */
    if ( data )
        size = BUF_PeekAt(buf, 0, data, size);

    /* remove the read data from the buffer */ 
    n_todo = size;
    while (n_todo  &&  buf->list) {
        SBufChunk* pHead = buf->list;
        size_t     n_avail = pHead->size - pHead->n_skip;
        if (n_todo >= n_avail) { /* discard the whole chunk */
            buf->list = pHead->next;
            if ( !buf->list ) {
                buf->last = 0;
            }
            free(pHead);
            n_todo -= n_avail;
        } else { /* discard some of the chunk data */
            pHead->n_skip += n_todo;
            n_todo = 0;
        }
    }

    assert(size >= n_todo);
    return (size - n_todo);
}


extern void BUF_Erase(BUF buf)
{
    if ( !buf )
        return;

    while ( buf->list ) {
        SBufChunk* pChunk = buf->list;
        buf->list = pChunk->next;
        free(pChunk);
    }
    buf->last = 0;
}


extern void BUF_Destroy(BUF buf)
{
    if (buf) {
        BUF_Erase(buf);
        free(buf);
    }
}
