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
    unsigned int flag;  /* (flag & 1) == 0 if the block is vacant;          *
                         * other bits reserved, do not assume their values! */
    TNCBI_Size   size;  /* size of the block (including this header), bytes */
} SHEAP_Block;


/* Callback to resize the heap (a la 'realloc').
 * NOTE: the returned address must be aligned at the 'double' boundary!
 *
 *   old_base  |  new_size  |  Expected result
 * ------------+------------+--------------------------------------------------
 *   non-NULL  |     0      | Deallocate old_base, return 0
 *   non-NULL  |  non-zero  | Reallocate to the requested size, return new base
 *      0      |  non-zero  | Allocate anew, return base (return 0 on error)
 *      0      |     0      | Do nothing, return 0
 * ------------+------------+--------------------------------------------------
 * Note that reallocation can request either to expand or to shrink the heap
 * extent.  When (re-)allocation fails, the callback should return 0 (and must
 * retain the original heap extent / content, if any).  When expected to return
 * 0, this callback must always do so.
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


/* Attach to an already existing heap (in read-only mode), calculating the heap
 * extent by a heap end marker (but only within the maximal specified size).
 */
extern NCBI_XCONNECT_EXPORT HEAP HEAP_Attach
(const void* base,         /* base of the heap to attach to                  */
 TNCBI_Size  maxsize,      /* maximal heap extent, or 0 for unlimited search */
 int         serial        /* serial number to assign                        */
 );

/* Expedited HEAP_Attach() that does not calculate heap extent on its own */
extern NCBI_XCONNECT_EXPORT HEAP HEAP_AttachFast
(const void* base,         /* base of the heap to attach to                  */
 TNCBI_Size  size,         /* heap extent -- must be non-0 for non-NULL base */
 int         serial        /* serial number to assign                        */
 );


/* Allocate a new block of memory in the heap toward either the base or the end
 * of the heap, depending on the "hint" parameter:
 * "hint" == 0 for base;
 * "hint" != 0 for end.
 * Return NULL if allocation has failed.
 */
extern NCBI_XCONNECT_EXPORT SHEAP_Block* HEAP_Alloc
(HEAP        heap,         /* heap handle                           */
 TNCBI_Size  size,         /* data size of the block to accommodate */
 int/*bool*/ hint
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
 * coalesce, to use this call again while walking the heap, the following rule
 * must be utilized:  If "prev" was free, "prev" must not get advanced;
 * otherwise, "prev" must be updated with "ptr"'s value.
 * As an exception, if "prev" points to a used block and "prev"'s next block is
 * not "ptr", but a free block, whose next is "ptr", then "prev" gets bumped up
 * internally to that free block's pointer value.
 * NOTE:  This call will be removed.
 */
extern NCBI_XCONNECT_EXPORT void HEAP_FreeFast
(HEAP               heap,  /* heap handle         */
 SHEAP_Block*       ptr,   /* block to deallocate */
 const SHEAP_Block* prev   /* block's predecessor */
 );


/* Iterate through the heap blocks.
 * Return pointer to the block following the block "prev".
 * Return NULL if "prev" is the last block of the heap.
 */
extern NCBI_XCONNECT_EXPORT SHEAP_Block* HEAP_Walk
(const HEAP         heap,  /* heap handle                         */
 const SHEAP_Block* prev   /* if 0, then get the first heap block */
 );


/* Same as HEAP_Walk() but skip over free blocks so that only used blocks show
 * (nevertheless it can also accept prev pointing to a free block).
 */
extern NCBI_XCONNECT_EXPORT SHEAP_Block* HEAP_Next
(const HEAP         heap,  /* heap handle                              */
 const SHEAP_Block* prev   /* if 0, then get the first used heap block */
 );


/* Trim the heap, making garbage collection first.  Returned is the resultant
 * heap, which has its last block (if any) trimmed to the size of the heap
 * chunk size as specified at the time of the heap creation.  No change in size
 * is made if the last block is not free or not large enough to allow the
 * trimming.  NULL gets returned on NULL or read-only heaps, or on a resize
 * error.
 * Note that trimming may cause the entire heap extent (of an empty heap) to
 * deallocate (so that HEAP_Base() and HEAP_Size() will both return 0).
 */
extern NCBI_XCONNECT_EXPORT HEAP HEAP_Trim(HEAP heap);


/* Make a snapshot of a given heap.  Return a read-only heap (like the one
 * after HEAP_Attach[Fast]()), which must be freed by a call to either
 * HEAP_Detach() or HEAP_Destroy() when no longer needed.
 * A copy is created reference-counted (with the initial ref.count set to 1).
 */
extern NCBI_XCONNECT_EXPORT HEAP HEAP_Copy
(const HEAP orig,          /* original heap to copy from               */
 size_t     extra,         /* extra amount to add past the heap extent */
 int        serial         /* serial number to assign                  */
 );


/* Add reference counter to the given copy heap (no effect on a heap, which has
 * been HEAP_Create()'d or HEAP_Attach[Fast]()'d).  The heap handle then will
 * be destroyed only when the internal reference counter reaches 0.
 * @warning No internal locking is provided.
 * Return the resultant value of the reference counter.
 */
extern NCBI_XCONNECT_EXPORT unsigned int HEAP_AddRef(HEAP heap);


/* Detach a heap (previously attached by HEAP_Attach[Fast]).  For a copy heap,
 * it decrements an internal ref. counter by one, and actually destroys the
 * heap handle if and only if the counter has reached 0.
 * @warning No internal locking of the reference counter is provided.
 * For heaps that are results of the HEAP_Copy() call, both HEAP_Detach() and
 * HEAP_Destroy() can be used interchangeably.
 * Return the remaining value of the reference counter (0 if the heap is gone).
 */
extern NCBI_XCONNECT_EXPORT unsigned int HEAP_Detach(HEAP heap);


/* Destroy the heap (previously created by HEAP_Create()).
 * For copy heaps -- see comments for HEAP_Detach() above.
 * Return the remaining value of the reference counter (0 if the heap is gone).
 */
extern NCBI_XCONNECT_EXPORT unsigned int HEAP_Destroy(HEAP heap);


/* Get the base address of the heap.  Return NULL if heap is passed as NULL, or
 * when the heap is completely empty.
 */
extern NCBI_XCONNECT_EXPORT void* HEAP_Base(const HEAP heap);


/* Get the extent of the heap.  This is a very fast routine.  Return 0 if heap
 * is passed as NULL, or when the heap is completely empty.
 */
extern NCBI_XCONNECT_EXPORT TNCBI_Size HEAP_Size(const HEAP heap);


/* Return the total used space on the heap.  This is a very fast routine.
 */
extern NCBI_XCONNECT_EXPORT TNCBI_Size HEAP_Used(const HEAP heap);


/* Return total vacant usable space on the heap.  Involves heap walking.
 */
extern NCBI_XCONNECT_EXPORT TNCBI_Size HEAP_Idle(const HEAP heap);


/* Get a serial number of the heap as assigned by Attach or Copy.
 * Return 0 if the heap is not a copy but the original, or passed as NULL.
 */
extern NCBI_XCONNECT_EXPORT int HEAP_Serial(const HEAP heap);


/* Set heap access speed (and ignore second parameter):
 * fast == eOn  turns on fast heap operations (default);
 * fast == eOff turns off fast heap operations (more checks, slower);
 * fast == eDefault does not change the current setting.
 * This call is intended for internal uses;  and default settings (fast ops
 * w/o structure integrity checks) should suffice for most users.
 */
extern NCBI_XCONNECT_EXPORT void HEAP_Options(ESwitch fast, ESwitch unused);


#ifdef __cplusplus
} /* extern "C" */
#endif


/* @} */

#endif /* CONNECT___NCBI_HEAPMGR__H */
