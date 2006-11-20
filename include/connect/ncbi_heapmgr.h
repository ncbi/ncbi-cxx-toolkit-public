#ifndef CONNECT___NCBI_HEAPMGR__H
#define CONNECT___NCBI_HEAPMGR__H

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
 void*      arg        /* user-supplied argument, see HEAP_Create() below   */
 );


/* Create new heap.
 * NOTE: the initial heap base must be aligned with a 'double' boundary!
 */
extern NCBI_XCONNECT_EXPORT HEAP HEAP_Create
(void*        base,        /* initial heap base (use "resize" if NULL) */
 TNCBI_Size   size,        /* initial heap size                        */
 TNCBI_Size   chunk_size,  /* minimal increment size                   */
 FHEAP_Resize resize,      /* NULL if not resizeable                   */
 void*        arg          /* user argument to pass to "resize"        */
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


/* Deallocate a block pointed to by "ptr".
 */
extern NCBI_XCONNECT_EXPORT void HEAP_Free
(HEAP         heap,        /* heap handle         */
 SHEAP_Block* ptr          /* block to deallocate */
 );


/* Deallocate a block pointed to by "ptr" and having "prev" as its predecessor
 * (NULL if "ptr" is the first on the heap) -- faster variant of HEAP_Free()
 * NOTE:  Since "ptr" gets invalidated by this call, it cannot be used in
 * successive calls to HEAP_FreeFast() as a "previous" block pointer (keep
 * using older "ptr"'s previous pointer if needed;  however, "ptr" still can
 * be used for advancing in HEAP_Walk().
 * NOTE:  This call may invalidate "prev" pointer if the "prev" block was
 * already free.  So, it is recommended that HEAP_FreeFast() is to be used
 * only once per a heap walk, to lessen the code complication.
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
 * size of heap chunk size as specified at the time of the heap creation.
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
 */
extern NCBI_XCONNECT_EXPORT void HEAP_AddRef(HEAP heap);


/* Detach heap (previously attached by HEAP_Attach[Fast]).
 * For copy heap, it decrements an internal ref. counter by one, and
 * destroys the heap handle if and only if the counter has reached 0.
 * No internal locking of the reference counter is provided.
 * For heaps that are results of HEAP_Copy() call,
 * both HEAP_Detach() and HEAP_Destroy() can be used interchangeably.
 */
extern NCBI_XCONNECT_EXPORT void HEAP_Detach(HEAP heap);


/* Destroy heap (previously created by HEAP_Create()).
 * For copy heaps -- see comments for HEAP_Detach() above.
 */
extern NCBI_XCONNECT_EXPORT void HEAP_Destroy(HEAP heap);


/* Get base address of the heap.
 * Return NULL if heap is passed as NULL, or when the heap is completely empty.
 */
extern NCBI_XCONNECT_EXPORT void* HEAP_Base(const HEAP heap);


/* Get the extent of the heap.
 * Return 0 if heap is passed as NULL, or when the heap is completely empty.
 */
extern NCBI_XCONNECT_EXPORT TNCBI_Size HEAP_Size(const HEAP heap);


/* Get non-zero serial number of the heap.
 * Return 0 if heap is passed as NULL,
 * or the heap is not a copy but the original.
 */
extern NCBI_XCONNECT_EXPORT int HEAP_Serial(const HEAP heap);


/* Set heap access speed and check level while walking:
 * fast == eOn  turns on fast heap operations (default);
 * fast == eOff turns off fast heap operations (more checks, slower);
 * fast == eDefault does not change current setting;
 * newalk == eOn turns on new heap integrity checks while walking;
 * newalk == eOff turns off new heap integrity checks (default);
 * newalk == eDefault keeps current setting.
 * This call is intended for internal uses; and default settings (fast ops
 * w/o new structure integrity checks) should suffice for most users.
 */
extern NCBI_XCONNECT_EXPORT void HEAP_Options(ESwitch fast, ESwitch newalk);


#ifdef __cplusplus
} /* extern "C" */
#endif


/* @} */


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.24  2006/11/20 17:26:20  lavr
 * HEAP_FreeFast() better documented (with all side effects)
 *
 * Revision 6.23  2006/11/20 17:01:52  lavr
 * Added missing declaration of newly added HEAP_FreeFast()
 *
 * Revision 6.22  2006/11/20 16:38:15  lavr
 * Faster heap with free blocks linked into a list.
 * HEAP_AttachEx() -> HEAP_AttachFast()
 * +HEAP_FreeFast(), +HEAP_Options()
 *
 * Revision 6.21  2006/03/05 17:32:35  lavr
 * Revised API to allow to create ref-counted heap copies
 *
 * Revision 6.20  2004/07/08 14:11:11  lavr
 * Fix few inline descriptions
 *
 * Revision 6.19  2003/09/23 21:00:53  lavr
 * +HEAP_AttachEx()
 *
 * Revision 6.18  2003/09/02 20:45:45  lavr
 * -<connect/connect_export.h> -- now included from <connect/ncbi_types.h>
 *
 * Revision 6.17  2003/08/28 21:09:37  lavr
 * Accept (and allocate) additional heap extent in HEAP_CopySerial()
 *
 * Revision 6.16  2003/08/25 14:50:10  lavr
 * Heap arena ptrs changed to be "void*";  expand routine to take user arg
 *
 * Revision 6.15  2003/07/31 17:53:43  lavr
 * +HEAP_Trim()
 *
 * Revision 6.14  2003/04/09 17:58:51  siyan
 * Added doxygen support
 *
 * Revision 6.13  2003/01/08 01:59:32  lavr
 * DLL-ize CONNECT library for MSVC (add NCBI_XCONNECT_EXPORT)
 *
 * Revision 6.12  2002/09/25 20:08:43  lavr
 * Added table to explain expand callback inputs and outputs
 *
 * Revision 6.11  2002/09/19 18:00:58  lavr
 * Header file guard macro changed; log moved to the end
 *
 * Revision 6.10  2002/04/13 06:33:22  lavr
 * +HEAP_Base(), +HEAP_Size(), +HEAP_Serial(), new HEAP_CopySerial()
 *
 * Revision 6.9  2001/07/03 20:23:46  lavr
 * Added function: HEAP_Copy()
 *
 * Revision 6.8  2001/06/19 20:16:19  lavr
 * Added #include <connect/ncbi_types.h>
 *
 * Revision 6.7  2001/06/19 19:09:35  lavr
 * Type change: size_t -> TNCBI_Size; time_t -> TNCBI_Time
 *
 * Revision 6.6  2000/10/05 21:25:45  lavr
 * ncbiconf.h removed
 *
 * Revision 6.5  2000/10/05 21:09:52  lavr
 * ncbiconf.h included
 *
 * Revision 6.4  2000/05/23 21:41:05  lavr
 * Alignment changed to 'double'
 *
 * Revision 6.3  2000/05/12 18:28:40  lavr
 * First working revision
 *
 * ==========================================================================
 */

#endif /* CONNECT___NCBI_HEAPMGR__H */
