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
 * Author:  Anton Lavrentiev
 *
 * Abstract:
 *
 * This is a simple heap manager with a primitive garbage collection.
 * The heap contains blocks of data, stored in the common contiguous pool,
 * each block preceded with a SHEAP_Block structure.  Low word of 'flag'
 * is either non-zero (True), when the block is in use, or zero (False),
 * when the block is vacant.  'Size' shows the length of the block in bytes,
 * (uninterpreted) data field of which is extended past the header
 * (the header size IS counted in the size of the block).
 *
 * When 'HEAP_Alloc' is called, the return value is either a heap pointer,
 * which points to the block header, marked as allocated and guaranteed
 * to have enough space to hold the requested data size; or 0 meaning, that the
 * heap has no more room to provide such a block (reasons for that:
 * heap is corrupted, heap has no provision to be expanded, expansion failed,
 * or the heap was attached read-only).
 *
 * An application program can then use the data field on its need,
 * providing not to overcome the size limit.  The current block header
 * can be used to find the next heap block with the use of 'size' member
 * (note, however, some restrictions below).
 *
 * The application program is NOT assumed to keep the returned block pointer,
 * as the garbage collection can occur on the next allocation attempt,
 * thus making any heap pointers invalid.  Instead, the application program
 * can keep track of the heap base (header of the very first heap block -
 * see 'HEAP_Create'), and the size of the heap, and can traverse the heap by
 * this means, or with call to 'HEAP_Walk' (described below). 
 *
 * While traversing, if the block found is no longer needed, it can be freed
 * with 'HEAP_Free' call, supplying the address of the block header
 * as an agrument.
 *
 * Prior the heap use, the initialization is required, which comprises
 * call to either 'HEAP_Create' or 'HEAP_Attach' with the information about
 * the base heap pointer. 'HEAP_Create' also requires the size of initial
 * heap area (if there is one), and size of chunk (usually, a page size)
 * to be used in heap expansions (defaults to 1 if provided as 0).
 * Additionally (but not compulsory) the application program can provide
 * heap manager with 'expand' routine, which is supposed to be called,
 * when no more room is available in the heap, or the heap was not
 * preallocated (base = 0 in 'HEAP_Create'), and given the arguments:
 * - current heap base address (or 0 if this is the very first heap alloc),
 * - new required heap size (or 0 if this is the very last call to deallocate
 * the entire heap). 
 * If successful, the expand routine must return the new heap base
 * address (if any) of expanded heap area, and where the exact copy of
 * the current heap is made.
 *
 * Note that all heap base pointers must be aligned on a 'double' boundary.
 * Please also be warned not to store pointers to the heap area, as a
 * garbage collection can clobber them.  Within a blocks, however,
 * it is possible to use local pointers (offsets), which remain same
 * regardless of garbage collections.
 *
 * For automatic traverse purposes there is a 'HEAP_Walk' call, which returns
 * the next block (either free, or used) from the heap.  Given a NULL-pointer,
 * this function returns the very first block, whareas all subsequent calls
 * with the argument being the last observed block results in the next block 
 * returned. NULL comes back when no more blocks exist in the heap.
 *
 * Note that for proper heap operations, no allocations should happen between
 * successive calls to 'HEAP_Walk', whereas deallocation of the seen block
 * is okay.
 *
 * Explicit heap traversing should not overcome the heap limit,
 * as any information above the limit is not maintained by the heap manager.
 * Every heap operation guarantees, that there are no adjacent free blocks,
 * only used blocks can follow each other sequentially.
 *
 * To discontinue to use the heap, 'HEAP_Destroy' or 'HEAP_Detach' can be
 * called.  The former deallocated the heap (by means of call to 'expand'),
 * the latter just removes the heap handle, retaining the heap data intact.
 * Later, such a heap could be used again if attached with 'HEAP_Attach'.
 *
 * Note that attached heap is in read-only mode, that is nothing can be
 * allocated and/or freed in that heap, as well as an attempt to call
 * 'HEAP_Destroy' will not destroy the heap data.
 *
 * Note also, that 'HEAP_Create' always does heap reset, that is the
 * memory area pointed by 'base' (if not 0) gets reformatted and lost
 * all previous contents.
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.6  2001/02/14 22:03:09  lavr
 * 0x... constants explicitly made unsigned
 *
 * Revision 6.5  2001/01/12 23:51:39  lavr
 * Message logging modified for use LOG facility only
 *
 * Revision 6.4  2000/05/23 21:41:07  lavr
 * Alignment changed to 'double'
 *
 * Revision 6.3  2000/05/17 14:22:30  lavr
 * Small cosmetic changes
 *
 * Revision 6.2  2000/05/16 15:06:05  lavr
 * Minor changes for format <-> argument correspondence in warnings
 *
 * Revision 6.1  2000/05/12 18:33:44  lavr
 * First working revision
 *
 * ==========================================================================
 */

#include "ncbi_priv.h"
#include <connect/ncbi_heapmgr.h>
#include <stdlib.h>
#include <string.h>


struct SHEAP_tag {
    char         *base;
    size_t        size;
    size_t        chunk;
    FHEAP_Expand  expand;
};


#define HEAP_ALIGN(a, b)        (((unsigned long)(a) + (b) - 1) & ~((b) - 1))
#define HEAP_LAST               0x80000000UL
#define HEAP_USED               0x0DEAD2F0UL
#define HEAP_FREE               0
#define HEAP_ISFREE(b)          (((b)->flag & ~HEAP_LAST) == HEAP_FREE)
#define HEAP_ISUSED(b)          (((b)->flag & ~HEAP_LAST) == HEAP_USED)
#define HEAP_ISLAST(b)          ((b)->flag & HEAP_LAST)


HEAP HEAP_Create(char *base, size_t size, size_t chunk, FHEAP_Expand expand)
{
    SHEAP_Block *b;
    HEAP heap;

    if ((!base && size) || (base && !size) ||
        !(heap = (HEAP)malloc(sizeof(*heap))))
        return 0;
    if ((char *)HEAP_ALIGN(base, sizeof(double)) != base)
        CORE_LOGF(eLOG_Warning,
                  ("Heap Create: Unaligned base (0x%08lX)", (long)base));
    chunk = (chunk <= 1 ? 1 : chunk);
    if (!base) {
        size = HEAP_ALIGN(sizeof(*b) + 1, chunk);
        if (!(base = (*expand)(base, size))) {
            CORE_LOGF(eLOG_Warning,
                      ("Heap Create: Cannot create (size = %u)",
                       (unsigned)size));
            free(heap);
            return 0;
        }
    }
    if (size <= sizeof(*b))
        CORE_LOGF(eLOG_Warning, ("Heap Create: Heap too small (%u <= %u)",
                                 (unsigned)size, (unsigned)sizeof(*b)));
    heap->base = base;
    heap->size = size;
    heap->chunk = chunk;
    heap->expand = expand;
    b = (SHEAP_Block *)heap->base;
    b->flag = HEAP_FREE | HEAP_LAST;
    b->size = size;
    return heap;
}


HEAP HEAP_Attach(char *base)
{
    SHEAP_Block *b;
    HEAP heap;

    if (!base || !(heap = (HEAP)malloc(sizeof(*heap))))
        return 0;
    if ((char *)HEAP_ALIGN(base, sizeof(double)) != base)
        CORE_LOGF(eLOG_Warning,
                  ("Heap Attach: Unaligned base (0x%08lX)", (long)base));
    heap->size = 0;
    for (b = (SHEAP_Block *)base; ; b = (SHEAP_Block *)((char *)b + b->size)) {
        if (!HEAP_ISUSED(b) && !HEAP_ISFREE(b)) {
            CORE_LOGF(eLOG_Warning,
                      ("Heap Attach: Heap corrupted (0x%08X, %u)",
                       b->flag, (unsigned)b->size));
            free(heap);
            return 0;
        }
        heap->size += b->size;
        if (HEAP_ISLAST(b))
            break;
    }
    heap->base = base;
    heap->chunk = 0;
    heap->expand = 0;
    return heap;
}


static SHEAP_Block *s_HEAP_Join(SHEAP_Block *p, SHEAP_Block *b)
{
    /* Block followed by 'b' */
    SHEAP_Block *n = (SHEAP_Block *)((char *)b + b->size);

    if (!HEAP_ISFREE(b))
        CORE_LOG(eLOG_Warning, "Heap Join: Block is not free");
    else {
        if (!HEAP_ISLAST(b) && HEAP_ISFREE(n)) {
            b->size += n->size;
            b->flag = n->flag;
            n = (SHEAP_Block *)((char *)n + n->size);
        }
        if (p && HEAP_ISFREE(p)) {
            p->size += b->size;
            p->flag = b->flag;
        }
    }
    return n;
}


static SHEAP_Block *s_HEAP_Squeeze(HEAP heap)
{
    SHEAP_Block *b = (SHEAP_Block *)heap->base, *f = 0;

    while ((char *)b < heap->base + heap->size) {
        if (HEAP_ISFREE(b))
            f = b;
        else if (HEAP_ISUSED(b) && f) {
            unsigned last = b->flag & HEAP_LAST;
            size_t save = f->size;

            memmove(f, b, b->size);
            f->flag &= ~HEAP_LAST;
            f = (SHEAP_Block *)((char *)f + f->size);
            f->flag = HEAP_FREE | last;
            f->size = save;
            b = s_HEAP_Join(0, f);
            continue;
        }
        b = (SHEAP_Block *)((char *)b + b->size);
    }
    return f;
}


SHEAP_Block *s_HEAP_Take(SHEAP_Block *b, size_t size)
{
    unsigned last = b->flag & HEAP_LAST;

    if (HEAP_ISUSED(b))
        CORE_LOG(eLOG_Warning, "Heap Take: Block is not free");
    if (b->size > size + sizeof(*b)) {
        SHEAP_Block *n = (SHEAP_Block *)((char *)b + size);

        n->flag = HEAP_FREE | last;
        n->size = b->size - size;
        b->flag = HEAP_USED;
        b->size = size;
        s_HEAP_Join(0, n);
    } else
        b->flag = HEAP_USED | last;
    return b;
}


SHEAP_Block *HEAP_Alloc(HEAP heap, size_t size)
{
    SHEAP_Block *b, *p = 0;
    size_t free = 0;

    if (!heap || !size)
        return 0;

    if (!heap->chunk) {
        CORE_LOG(eLOG_Warning, "Heap Alloc: Heap is read-only");
        return 0;
    }
    
    size = (size_t)HEAP_ALIGN(size + sizeof(*b), sizeof(double));

    b = (SHEAP_Block *)heap->base;
    while ((char *)b < heap->base + heap->size) {
        if (HEAP_ISFREE(b)) {
            if (b->size >= size)
                /* Ok, empty, large enough block found */
                return s_HEAP_Take(b, size);
            free += b->size;
        } else if (!HEAP_ISUSED(b)) {
            CORE_LOGF(eLOG_Warning,
                      ("Heap Alloc: Heap corrupted (0x%08X, %u)",
                       b->flag, (unsigned)b->size));
            return 0;
        }
        p = b;
        b = (SHEAP_Block *)((char *)b + b->size);
    }

    /* Heap exhausted, no free block found */
    if (free >= size)
        b = s_HEAP_Squeeze(heap);
    else if (!heap->expand)
        return 0;
    else {
        size_t hsize = (size_t)HEAP_ALIGN(heap->size + size, heap->chunk);
        ptrdiff_t dp = (char *)p - heap->base;
        char *base;

        if (!(base = (*heap->expand)(heap->base, hsize)))
            return 0;
        p = (SHEAP_Block *)(base + dp);
        if (!HEAP_ISLAST(p))
            CORE_LOG(eLOG_Warning, "Heap Alloc: Last block lost");
        if (HEAP_ISUSED(p)) {
            p->flag &= ~HEAP_LAST;
            /* New block is the very top on the heap */
            b = (SHEAP_Block *)(base + heap->size);
            b->size = hsize - heap->size;
            b->flag = HEAP_FREE | HEAP_LAST;
        } else {
            /* Extend last free block */
            p->size += hsize - heap->size;
            b = p;
        }
        heap->base = base;
        heap->size = hsize;
    }
    return s_HEAP_Take(b, size);
}


void HEAP_Free(HEAP heap, SHEAP_Block *ptr)
{
    SHEAP_Block *b, *p = 0;

    if (!heap || !ptr)
        return;

    if (!heap->chunk) {
        CORE_LOG(eLOG_Warning, "Heap Free: Heap is read-only");
        return;
    }
    
    b = (SHEAP_Block *)heap->base;
    while ((char *)b < heap->base + heap->size) {
        if (HEAP_ISFREE(b)) {
            if (b == ptr) {
                CORE_LOG(eLOG_Warning, "Heap Free: Freeing free block");
                return;
            }
        } else if (HEAP_ISUSED(b)) {
            if (b == ptr) {
                b->flag = HEAP_FREE | (b->flag & HEAP_LAST);
                s_HEAP_Join(p, b);
                return;
            }
        } else {
            CORE_LOGF(eLOG_Warning,
                      ("Heap Free: Heap corrupted (0x%08X, %u)",
                       b->flag, (unsigned)b->size));
            return;
        }
        p = b;
        b = (SHEAP_Block *)((char *)b + b->size);
    }
    CORE_LOG(eLOG_Warning, "Heap Free: Block not found");
}


SHEAP_Block *HEAP_Walk(HEAP heap, const SHEAP_Block *p)
{
    SHEAP_Block *b;
    
    if (!p ||
        ((char *)p >= heap->base && (char *)p < heap->base + heap->size)) {
        b = (SHEAP_Block *)(p ? (char *)p + p->size : heap->base);
        if ((char *)b < heap->base + heap->size) {
            if (b->size > sizeof(*b) &&
                b->size == HEAP_ALIGN(b->size, sizeof(double)) &&
                (char *)b + b->size <= heap->base + heap->size &&
                (HEAP_ISFREE(b) || HEAP_ISUSED(b))) {
                /* Block 'b' seems valid, but... */
                if (!p)
                    return b;
                if (HEAP_ISLAST(p))
                    CORE_LOG(eLOG_Warning, "Heap Walk: Misplaced last block");
                else if (HEAP_ISFREE(b) && HEAP_ISFREE(p))
                    CORE_LOG(eLOG_Warning, "Heap Walk: Adjacent free blocks");
                else
                    return b;
            } else
                CORE_LOGF(eLOG_Warning,
                          ("Heap Walk: Heap corrupted (0x%08X, %u)",
                           b->flag, (unsigned)b->size));
        } else if ((char *)b > heap->base + heap->size)
            CORE_LOG(eLOG_Warning, "Heap Walk: Heap corrupted");
        else if (!HEAP_ISLAST(p))
            CORE_LOG(eLOG_Warning, "Heap Walk: Last block lost");
    } else
        CORE_LOG(eLOG_Warning, "Heap Walk: Alien pointer passed");
    return 0;
}


void HEAP_Detach(HEAP heap)
{
    if (heap)
        free(heap);
}


void HEAP_Destroy(HEAP heap)
{
    if (heap) {
        if (!heap->chunk) {
            CORE_LOG(eLOG_Warning, "Heap Destroy: Heap is read-only");
            return;
        }
        if (heap->expand)
            (*heap->expand)(heap->base, 0);
        HEAP_Detach(heap);
    }
}
