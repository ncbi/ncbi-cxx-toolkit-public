/*  $Id$
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
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.4  1999/11/19 19:59:08  vakatov
 * Get rid of an insignificant compiler warning
 *
 * Revision 6.3  1999/10/18 18:01:43  vakatov
 * Use double-quotes (rather than angles) to #include ncbi_buffer.h
 * (this kludge is to ease compilation in the NCBI C++ tree).
 *
 * Revision 6.2  1999/10/12 16:32:14  vakatov
 * Moved all TEST suite code from "ncbi_buffer.c" to "test/test_ncbi_buffer.c"
 *
 * Revision 6.1  1999/08/17 19:45:22  vakatov
 * Moved all real code from NCBIBUF to NCBI_BUFFER;  the code has been cleaned
 * from the NCBI C toolkit specific types and API calls.
 * NCBIBUF module still exists for the backward compatibility -- it
 * provides old NCBI-wise interface.
 * ===========================================================================
 */

#include "ncbi_buffer.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>


/* Buffer chunk
 */
typedef struct SBufChunkTag {
  struct SBufChunkTag* next;
  size_t size;       /* of data (including the discarded "n_skip" bytes) */
  size_t alloc_size; /* maximum avail.(allocated) size of "data" */
  size_t n_skip;     /* # of bytes already discarded(read) from the chunk */
  char   data[1];    /* data stored in this chunk */
} SBufChunk;


/* Buffer
 */
typedef struct BUF_tag {
  size_t     chunk_size;
  SBufChunk* list;
} BUF_struct;



extern size_t BUF_SetChunkSize(BUF* pBuf, size_t chunk_size)
{
  /* create buffer internals, if not created yet */
  if ( !*pBuf ) {
    *pBuf = (BUF_struct*) malloc(sizeof(BUF_struct));
    if ( !*pBuf )
      return 0;
    (*pBuf)->list = 0;
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
 */
static SBufChunk* s_AllocChunk(size_t size, size_t chunk_size)
{
  size_t alloc_size = ((size + chunk_size - 1) / chunk_size) * chunk_size;
  SBufChunk* pChunk = (SBufChunk*)
    malloc((size_t) (&(((SBufChunk*)0)->data)) + alloc_size);
  if ( !pChunk )
    return 0;

  pChunk->next       = 0;
  pChunk->size       = 0;
  pChunk->alloc_size = alloc_size;
  pChunk->n_skip     = 0;
  return pChunk;
}


extern int/*bool*/ BUF_Write(BUF* pBuf, const void* data, size_t size)
{
  SBufChunk *pChunk, *pTail;
  if ( !size )
    return 1 /* true */;

  /* init the buffer internals, if not init'd yet */
  if (!*pBuf  &&  !BUF_SetChunkSize(pBuf, 0) )
    return 0 /* false */;

  /* find the last allocated chunk */
  for (pTail = (*pBuf)->list;  pTail  &&  pTail->next;  pTail = pTail->next)
    continue;

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
      return 0 /* false */;
    pChunk->n_skip = 0;
    pChunk->size   = size;
    memcpy(pChunk->data, data, size);

    /* add the new chunk to the list */
    if ( pTail )
      pTail->next = pChunk;
    else
      (*pBuf)->list = pChunk;
  }
  return 1 /* true */;
}


extern int/*bool*/ BUF_PushBack(BUF* pBuf, const void* data, size_t size)
{
  SBufChunk* pChunk;
  if ( !size )
    return 1 /* true */;

  /* init the buffer internals, if not init'd yet */
  if (!*pBuf  &&  !BUF_SetChunkSize(pBuf, 0) )
    return 0 /* false */;

  pChunk = (*pBuf)->list;

  /* allocate and link a new chunk to the beginning of the chunk list */
  if (!pChunk  ||  size > pChunk->n_skip) {
    pChunk = s_AllocChunk(size, (*pBuf)->chunk_size);
    if ( !pChunk )
      return 0 /* false */;
    pChunk->n_skip = pChunk->size = pChunk->alloc_size;
    pChunk->next  = (*pBuf)->list;
    (*pBuf)->list = pChunk;
  }

  /* write data */
  assert(pChunk->n_skip >= size);
  pChunk->n_skip -= size;
  memcpy(pChunk->data + pChunk->n_skip, data, size);
  return 1 /* true */;
}


extern size_t BUF_Peek(BUF buf, void* data, size_t size)
{
  size_t     n_todo = size;
  SBufChunk* pChunk;

  if (!data  ||  !size  ||  !buf  ||  !buf->list)
    return 0;

  for (pChunk = buf->list;  n_todo  &&  pChunk;  pChunk = pChunk->next) {
    size_t n_copy = pChunk->size - pChunk->n_skip;
    assert(pChunk->size > pChunk->n_skip);
    if (n_copy > n_todo)
      n_copy = n_todo;

    memcpy(data, (char*)pChunk->data + pChunk->n_skip, n_copy);
    data = (char*)data + n_copy;
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
    size = BUF_Peek(buf, data, size);

  /* remove the read data from the buffer */ 
  n_todo = size;
  while (n_todo  &&  buf->list) {
    SBufChunk* pHead = buf->list;
    size_t     n_avail = pHead->size - pHead->n_skip;
    if (n_todo >= n_avail) { /* discard the whole chunk */
      buf->list = pHead->next;
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


extern BUF BUF_Destroy(BUF buf)
{
  if ( !buf )
    return 0;

  while ( buf->list ) {
    SBufChunk* pChunk = buf->list;
    buf->list = pChunk->next;
    free(pChunk);
  }

  free(buf);
  return 0;
}


