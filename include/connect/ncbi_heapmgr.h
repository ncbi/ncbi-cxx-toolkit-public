#ifndef CONNECT___NCBI_HEAPMGR__H
#define CONNECT___NCBI_HEAPMGR__H

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
 * Authors:  Anton Lavrentiev, Denis Vakatov
 *
 * File Description:
 *   Simple heap manager with a primitive garbage collection
 *
 */

#include <connect/ncbi_types.h>


/** @addtogroup ServiceSupport
 *
 * @{
 */


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
    unsigned int flag;  /* (short)flag == 0 if block is vacant              */
    TNCBI_Size   size;  /* size of the block (including the block header)   */
} SHEAP_Block;


/* Callback to resize the heap (a la 'realloc').
 * NOTE: the returned address must be aligned with the 'double' boundary!
 *
 *   old_base  |  new_size  |  Expected result
 * ------------+------------+--------------------------------------------------
 *   non-NULL  |     0      | Deallocate old_base and return 0
 *   non-NULL  |  non-zero  | Reallocate to the requested size, return new base
 *      0      |  non-zero  | Allocate (anew) and return base
 *      0      |     0      | Do nothing, return 0
 * ------------+------------+--------------------------------------------------
 * Note that reallocation can request either to expand or to shrink the
 * heap extent.  When (re-)allocation fails, the callback should return 0
 * (and must not change the original heap extent / content, if any).
 * When expected to return 0, this callback must always do so.
 */
typedef void* (*FHEAP_Resize)
(void*      old_base,  /* current base of the heap to be expanded           */
 TNCBI_Size new_size,  /* requested new heap size (zero to deallocate heap) */
 void*      auxarg     /* user-supplied argument, see HEAP_Create() below   */
 );


/* Create new heap.
 * NOTE: the initial heap base must be aligned at a 'double' boundary!
 */
extern NCBI_XCONNECT_EXPORT HEAP HEAP_Create
(void*        base,        /* initial heap base (use "resize" if NULL) */
 TNCBI_Size   size,        /* initial heap size                        */
 TNCBI_Size   chunk_size,  /* minimal increment size                   */
 FHEAP_Resize resize,      /* NULL if not resizeable                   */
 void*        auxarg       /* a user argument to pass to "resize"      */
 );


/* Attach to an already existing heap (in read-only mode).
 */
extern NCBI_XCONNECT_EXPORT HEAP HEAP_Attach
(const void* base,         /* base of the heap to attach to */
 int         serial        /* serial number to assign       */
 );

/* Expedited HEAP_Attach() that does not calculate heap size on its own */
extern NCBI_XCONNECT_EXPORT HEAP HEAP_AttachFast
(const void* base,         /* base of the heap to attach to                  */
 TNCBI_Size  size,         /* heap extent -- must be non-0 for non-NULL base */
 int         serial        /* serial number to assign                        */
 );


/* Allocate a new block of memory in the heap.
 */
extern NCBI_XCONNECT_EXPORT SHEAP_Block* HEAP_Alloc
(HEAP       heap,          /* heap handle                          */
 TNCBI_Size size           /* data size of the block to accomodate */
 );


/* Allocate a new block of memory in the heap
 * (faster than HEAP_Alloc() but inverses the insertion order).
 */
extern NCBI_XCONNECT_EXPORT SHEAP_Block* HEAP_AllocFast
(HEAP       heap,          /* heap handle                          */
 TNCBI_Size size           /* data size of the block to accomodate */
 );


/* Deallocate a block pointed to by "ptr".
 */
extern NCBI_XCONNECT_EXPORT void HEAP_Free
(HEAP         heap,        /* heap handle         */
 SHEAP_Block* ptr          /* block to deallocate */
 );


/* Deallocate a block pointed to by "ptr" and having "prev" as its predecessor
 * (NULL if "ptr" is the first on the heap) -- a faster variant of HEAP_Free().
 * NOTE:  Since the block pointed to by "ptr" may cause free blocks to
 * coalesce, to use this call again while walking the following rule must
 * be utilized:  If "prev" was free, "prev" must not get advanced;
 * otherwise, "prev" must be updated with "ptr"'s value.
 */
extern NCBI_XCONNECT_EXPORT void HEAP_FreeFast
(HEAP               heap,  /* heap handle         */
 SHEAP_Block*       ptr,   /* block to deallocate */
 const SHEAP_Block* prev   /* block's predecessor */
 );


/* Iterate through the heap blocks.
 * Return pointer to the block following block "prev_block".
 * Return NULL if "prev_block" is the last block of the heap.
 */
extern NCBI_XCONNECT_EXPORT SHEAP_Block* HEAP_Walk
(const HEAP         heap,  /* heap handle                                  */
 const SHEAP_Block* prev   /* (if 0, then get the first block of the heap) */
 );


/* Trim the heap, making garbage collection first.  Returned is
 * the resultant heap, which has its last block (if any) trimmed to the
 * size of the heap chunk size as specified at the time of the heap creation.
 * No change in size is made if the last block is not free or large
 * enough to allow the trimming.  NULL gets returned on NULL or read-only
 * heaps, or if a resize error has occurred.
 * Note that trimming can cause the entire heap extent (of an empty heap)
 * to deallocate (so that HEAP_Base() and HEAP_Size() will return 0).
 */
extern NCBI_XCONNECT_EXPORT HEAP HEAP_Trim(HEAP heap);


/* Make a snapshot of a given heap.  Return a read-only heap
 * (like the one after HEAP_Attach[Fast]), which must be freed by a call
 * to either HEAP_Detach() or HEAP_Destroy() when no longer needed.
 * A copy is created reference-counted (with the initial ref.count set to 1).
 */
extern NCBI_XCONNECT_EXPORT HEAP HEAP_Copy
(const HEAP orig,          /* original heap to copy from               */
 size_t     extra,         /* extra amount to add past the heap extent */
 int        serial         /* serial number to assign                  */
 );


/* Add reference counter to the given copy heap (no effect on
 * a heap, which have been HEAP_Create()'d or HEAP_Attach[Fast]()'d).
 * The heap handle then will be destroyed only when the internal
 * reference counter reaches 0.  No internal locking is provided.
 * Return the resultant value of the reference counter.
 */
extern NCBI_XCONNECT_EXPORT unsigned int HEAP_AddRef(HEAP heap);


/* Detach heap (previously attached by HEAP_Attach[Fast]).
 * For copy heap, it decrements an internal ref. counter by one, and
 * destroys the heap handle if and only if the counter has reached 0.
 * No internal locking of the reference counter is provided.
 * For heaps that are results of the HEAP_Copy() call,
 * both HEAP_Detach() and HEAP_Destroy() can be used interchangeably.
 * Return the remaining value of the reference counter (0 if the heap is gone).
 */
extern NCBI_XCONNECT_EXPORT unsigned int HEAP_Detach(HEAP heap);


/* Destroy heap (previously created by HEAP_Create()).
 * For copy heaps -- see comments for HEAP_Detach() above.
 * Return the remaining value of the reference counter (0 if the heap is gone).
 */
extern NCBI_XCONNECT_EXPORT unsigned int HEAP_Destroy(HEAP heap);


/* Get base address of the heap.
 * Return NULL if heap is passed as NULL, or when the heap is completely empty.
 */
extern NCBI_XCONNECT_EXPORT void* HEAP_Base(const HEAP heap);


/* Get the extent of the heap.
 * Return 0 if heap is passed as NULL, or when the heap is completely empty.
 */
extern NCBI_XCONNECT_EXPORT TNCBI_Size HEAP_Size(const HEAP heap);


/* Get a serial number of the heap as assigned by Attach or Copy.
 * Return 0 if the heap is not a copy but the original, or passed as NULL.
 */
extern NCBI_XCONNECT_EXPORT int HEAP_Serial(const HEAP heap);


/* Set heap access speed and check level while walking:
 * fast == eOn  turns on fast heap operations (default);
 * fast == eOff turns off fast heap operations (more checks, slower);
 * fast == eDefault does not change current setting;
 * newalk == eOn turns on new heap integrity checks while walking slow;
 * newalk == eOff turns off new heap integrity checks (default);
 * newalk == eDefault keeps current setting.
 * This call is intended for internal uses; and default settings (fast ops
 * w/o new structure integrity checks) should suffice for most users.
 * Note that in current implementation, new heap walk is only coupled with
 * slow heap operations;  that is, if heap access is set to fast==eOn, then
 * newalk setting is read but ignored.
 */
extern NCBI_XCONNECT_EXPORT void HEAP_Options(ESwitch fast, ESwitch newalk);


#ifdef __cplusplus
} /* extern "C" */
#endif


/* @} */

#endif /* CONNECT___NCBI_HEAPMGR__H */
