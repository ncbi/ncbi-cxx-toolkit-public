#ifndef NCBI_HEAPMGR__H
#define NCBI_HEAPMGR__H

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
 * Author:  Anton Lavrentiev, Denis Vakatov
 *
 * File Description:
 *   Simple heap manager with a primitive garbage collection.
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.2  2000/05/09 15:31:28  lavr
 * Minor changes
 *
 * Revision 6.1  2000/05/05 20:23:59  lavr
 * Initial revision
 *
 * ==========================================================================
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


/* Heap handle
 */
struct SHEAP_tag;
typedef struct SHEAP_tag* HEAP;


/* Header of a heap block
 */
typedef struct {
    unsigned flag;  /* (short)flag == 0 if block is vacant */
    size_t   size;  /* size of the block (including the block header) */
} SHEAP_Block;


/* Callback to expand the heap (a la 'realloc').
 * NOTE: the returned address must be aligned on a 'long' boundary!
 */
typedef char* (*FHEAP_Expand)
(char*  old_base,  /* current base of the heap to be expanded */
 size_t new_size   /* requested new heap size (zero to deallocate all heap) */
 );


/* Create new heap.
 * NOTE: the initial heap base must be aligned on a 'long' boundary!
 */
HEAP HEAP_Create
(char*        base,        /* initial heap base (use "expand" if NULL) */
 size_t       size,        /* initial heap size */
 size_t       chunk_size,  /* minimal increment size */
 FHEAP_Expand expand       /* NULL if not expandable */
 );


/* Attach to an already existing heap (in read-only mode).
 */
HEAP HEAP_Attach
(char* base  /* base of the heap to attach to */
 );


/* Allocate a new block of memory in the heap
 */
SHEAP_Block* HEAP_Alloc
(HEAP   heap,  /* heap handle */
 size_t size   /* # of bytes to allocate for the new block */
 );


/* Deallocate block pointed by "block_ptr"
 */
void HEAP_Free
(HEAP         heap,      /* heap handle */
 SHEAP_Block* block_ptr  /* block to deallocate */
 );


/* Iterate through the heap blocks.
 * Return pointer to the block following block "prev_block".
 * Return NULL if "prev_block" is the last block of the heap.
 */
SHEAP_Block* HEAP_Walk
(HEAP               heap, /* heap handle */
 const SHEAP_Block* prev  /* (if NULL, then get the first block of the heap) */
 );


/* Detach from the heap (previously attached to by HEAP_Attach)
 */
void HEAP_Detach(HEAP heap);


/* Destroy heap
 */
void HEAP_Destroy(HEAP heap);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NCBI_HEAPMGR__H */
