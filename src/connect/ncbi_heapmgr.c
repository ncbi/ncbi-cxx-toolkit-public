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
 * as an argument.
 *
 * Prior the heap use, the initialization is required, which comprises
 * call to either 'HEAP_Create' or 'HEAP_Attach' with the information about
 * the base heap pointer. 'HEAP_Create' also requires the size of initial
 * heap area (if there is one), and size of chunk (usually, a page size)
 * to be used in heap expansions (defaults to alignment if provided as 0).
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
 * garbage collection can clobber them. Within a block, however,
 * it is possible to use local pointers (offsets), which remain same
 * regardless of garbage collections.
 *
 * For automatic traverse purposes there is a 'HEAP_Walk' call, which returns
 * the next block (either free, or used) from the heap.  Given a NULL-pointer,
 * this function returns the very first block, whereas all subsequent calls
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
 * called. The former deallocates the heap (by means of a call to 'expand'),
 * the latter just removes the heap handle, retaining the heap data intact.
 * Later, such a heap could be used again if attached with 'HEAP_Attach'.
 *
 * Note that attached heap is in read-only mode, that is nothing can be
 * allocated and/or freed in that heap, as well as an attempt to call
 * 'HEAP_Destroy' will not destroy the heap data.
 *
 * Note also, that 'HEAP_Create' always does heap reset, that is the
 * memory area pointed by 'base' (if not 0) gets reformatted and lose
 * all previous contents.
 *
 */

#include "ncbi_priv.h"
#include <connect/ncbi_heapmgr.h>
#include <stdlib.h>
#include <string.h>


struct SHEAP_tag {
    void*         base;
    TNCBI_Size    size;
    TNCBI_Size    chunk;
    FHEAP_Expand  expand;
    void*         arg;
    int/*bool*/   copy;    /* (!=0) keeps user's serial number if provided */
};


#define _HEAP_ALIGN(a, b)     (((unsigned long)(a) + (b) - 1) & ~((b) - 1))
#define _HEAP_ALIGNMENT       sizeof(double)
#define HEAP_ALIGN(a)         _HEAP_ALIGN(a, _HEAP_ALIGNMENT)
#define HEAP_LAST             0x80000000UL
#define HEAP_USED             0x0DEAD2F0UL
#define HEAP_FREE             0
#define HEAP_ISFREE(b)        (((b)->flag & ~HEAP_LAST) == HEAP_FREE)
#define HEAP_ISUSED(b)        (((b)->flag & ~HEAP_LAST) == HEAP_USED)
#define HEAP_ISLAST(b)        ((b)->flag & HEAP_LAST)


HEAP HEAP_Create(void* base,       TNCBI_Size   size,
                 TNCBI_Size chunk, FHEAP_Expand expand, void* arg)
{
    SHEAP_Block* b;
    HEAP heap;

    if (!base != !size || !(heap = (HEAP) malloc(sizeof(*heap))))
        return 0;
    chunk = (TNCBI_Size) HEAP_ALIGN(chunk);
    if (!base) {
        size = (TNCBI_Size) _HEAP_ALIGN(sizeof(*b) + 1, chunk);
        if (!size || !expand || !(base = (*expand)(0, size, arg))) {
            CORE_LOGF(eLOG_Warning,
                      ("Heap Create: Cannot create (size = %u)",
                       (unsigned)(size ? size : sizeof(*b))));
            free(heap);
            return 0;
        }
    }
    if ((void*) HEAP_ALIGN(base) != base) {
        CORE_LOGF(eLOG_Warning,
                  ("Heap Create: Unaligned base (0x%08lX)", (long) base));
    }
    if (size < (TNCBI_Size) HEAP_ALIGN(sizeof(*b) + 1)) {
        CORE_LOGF(eLOG_Warning, ("Heap Create: Heap is too small (%u, %u)",
                                 (unsigned) size, (unsigned) sizeof(*b)));
    }
    heap->base   = base;
    heap->size   = size;
    heap->chunk  = chunk;
    heap->expand = expand;
    heap->arg    = expand ? arg : 0;
    heap->copy   = 0/*original*/;
    b = (SHEAP_Block*) heap->base;
    b->flag = HEAP_FREE | HEAP_LAST;
    b->size = size;
    return heap;
}


HEAP HEAP_AttachEx(const void* base, size_t size)
{
    HEAP heap;

    if (!base || !size || !(heap = malloc(sizeof(*heap))))
        return 0;
    if ((void*) HEAP_ALIGN(base) != base) {
        CORE_LOGF(eLOG_Warning,
                  ("Heap Attach: Unaligned base (0x%08lX)", (long) base));
    }
    heap->base   = (void*) base;
    heap->size   = size;
    heap->chunk  = 0/*read-only*/;
    heap->expand = 0;
    heap->arg    = 0;
    heap->copy   = 0/*original*/;
    return heap;
}


HEAP HEAP_Attach(const void* base)
{
    TNCBI_Size size;
    SHEAP_Block* b;

    if (!base)
        return 0;
    size = 0;
    for (b = (SHEAP_Block*) base; ; b = (SHEAP_Block*)((char*) b + b->size)) {
        if (!HEAP_ISUSED(b) && !HEAP_ISFREE(b)) {
            CORE_LOGF(eLOG_Warning,
                      ("Heap Attach: Heap corrupted (0x%08X, %u)",
                       b->flag, (unsigned) b->size));
            return 0;
        }
        size += b->size;
        if (HEAP_ISLAST(b))
            break;
    }
    return HEAP_AttachEx(base, size);
}


/* Check if a given block 'b' is adjacent to a free block, which
 * follows 'b', and/or to optionally passed previous block 'p'.
 * Join block(s) to form a larger free block, and return a pointer
 * to the next block.
 */
static SHEAP_Block* s_HEAP_Join(SHEAP_Block* p, SHEAP_Block* b)
{
    /* Block following 'b' */
    SHEAP_Block* n = (SHEAP_Block*)((char*) b + b->size);

    assert(HEAP_ISFREE(b));
    if (!HEAP_ISLAST(b) && HEAP_ISFREE(n)) {
        b->size += n->size;
        b->flag = n->flag;
        n = (SHEAP_Block*)((char*) n + n->size);
    }
    if (p && HEAP_ISFREE(p)) {
        p->size += b->size;
        p->flag = b->flag;
    }
    return n;
}


/* Collect garbage in the heap, moving all contents to the
 * top of the heap, and merging all free blocks at the end
 * in one large free block. Return pointer to that free block.
 */
static SHEAP_Block* s_HEAP_Collect(HEAP heap)
{
    SHEAP_Block* b = (SHEAP_Block*) heap->base, *f = 0;

    while ((char*) b < (char*) heap->base + heap->size) {
        if (HEAP_ISFREE(b))
            f = b;
        else if (HEAP_ISUSED(b) && f) {
            unsigned int last = b->flag & HEAP_LAST;
            TNCBI_Size save = f->size;

            memmove(f, b, b->size);
            f->flag &= ~HEAP_LAST;
            f = (SHEAP_Block*)((char*) f + f->size);
            f->flag = HEAP_FREE | last;
            f->size = save;
            b = s_HEAP_Join(0, f);
            continue;
        }
        b = (SHEAP_Block*)((char*) b + b->size);
    }
    return f;
}


/* Take the block 'b' (maybe split in two, if it's roomy enough)
 * for use of by at most 'size' bytes (including block header).
 * Return the block to use if taken okay; 0 otherwise.
 */
static SHEAP_Block* s_HEAP_Take(SHEAP_Block* b, TNCBI_Size size)
{
    unsigned int last = b->flag & HEAP_LAST;

    if (b->size >= size + sizeof(*b)) {
        SHEAP_Block* n = (SHEAP_Block*)((char*) b + size);

        n->flag = HEAP_FREE | last;
        n->size = b->size - size;
        b->flag = HEAP_USED;
        b->size = size;
        s_HEAP_Join(0, n);
    } else
        b->flag = HEAP_USED | last;
    return b;
}


SHEAP_Block* HEAP_Alloc(HEAP heap, TNCBI_Size size)
{
    SHEAP_Block* b, *p = 0;
    TNCBI_Size free = 0;

    if (!heap || size < 1) {
        if (size)
            CORE_LOG(eLOG_Warning, "Heap Alloc: Cannot alloc in NULL heap");
        return 0;
    }

    if (!heap->chunk) {
        CORE_LOG(eLOG_Warning, "Heap Alloc: Heap is read-only");
        return 0;
    }

    size = (TNCBI_Size) HEAP_ALIGN(sizeof(*b) + size);

    b = (SHEAP_Block*) heap->base;
    while ((char*) b < (char*) heap->base + heap->size) {
        if (HEAP_ISFREE(b)) {
            /* if an empty, large enough block found, then take it! */
            if (b->size >= size)
                return s_HEAP_Take(b, size);
            free += b->size;
        } else if (!HEAP_ISUSED(b)) {
            CORE_LOGF(eLOG_Warning,
                      ("Heap Alloc: Heap corrupted (0x%08X, %u)",
                       b->flag, (unsigned) b->size));
            return 0;
        }
        p = b;
        b = (SHEAP_Block*)((char*) b + b->size);
    }

    /* Heap exhausted, no free block found */
    if (free >= size)
        b = s_HEAP_Collect(heap);
    else if (!heap->expand)
        return 0;
    else {
        TNCBI_Size hsize =
            (TNCBI_Size) _HEAP_ALIGN(heap->size + size, heap->chunk);
        ptrdiff_t dp = (char*) p - (char*) heap->base;
        void* base;

        if (!(base = (*heap->expand)(heap->base, hsize, heap->arg)))
            return 0;
        p = (SHEAP_Block*)((char*) base + dp);
        if (!HEAP_ISLAST(p))
            CORE_LOG(eLOG_Warning, "Heap Alloc: Last block lost");
        if (HEAP_ISUSED(p)) {
            p->flag &= ~HEAP_LAST;
            /* New block is the very top on the heap */
            b = (SHEAP_Block*)((char*) base + heap->size);
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
    assert(b && HEAP_ISFREE(b) && b->size >= size);
    return s_HEAP_Take(b, size);
}


void HEAP_Free(HEAP heap, SHEAP_Block* ptr)
{
    SHEAP_Block* b, *p = 0;

    if (!heap || !ptr) {
        if (ptr)
            CORE_LOG(eLOG_Warning, "Heap Free: Cannot free in NULL heap");
        return;
    }

    if (!heap->chunk) {
        CORE_LOG(eLOG_Warning, "Heap Free: Heap is read-only");
        return;
    }

    b = (SHEAP_Block*) heap->base;
    while ((char*) b < (char*) heap->base + heap->size) {
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
                       b->flag, (unsigned) b->size));
            return;
        }
        p = b;
        b = (SHEAP_Block*)((char*) p + p->size);
    }
    CORE_LOG(eLOG_Warning, "Heap Free: Block not found");
}


SHEAP_Block* HEAP_Walk(const HEAP heap, const SHEAP_Block* p)
{
    SHEAP_Block* b;

    if (!heap) {
        CORE_LOG(eLOG_Warning, "Heap Walk: NULL heap");
        return 0;
    }
    if (!p ||
        ((char*) p >= (char*) heap->base &&
         (char*) p <  (char*) heap->base + heap->size)) {
        b = (SHEAP_Block*)(p ? (char*) p + p->size : (char*) heap->base);
        if ((char*) b < (char*) heap->base + heap->size) {
            if (b->size >= sizeof(*b) &&
                b->size == (TNCBI_Size) HEAP_ALIGN(b->size) &&
                (char*) b + b->size <= (char*) heap->base + heap->size &&
                (HEAP_ISFREE(b) || HEAP_ISUSED(b))) {
                /* Block 'b' seems valid, but... */
                if (!p)
                    return b;
                if (HEAP_ISLAST(p))
                    CORE_LOG(eLOG_Warning, "Heap Walk: Misplaced last block");
                else if (HEAP_ISFREE(b) && HEAP_ISFREE(p)) {
                    const SHEAP_Block* c = (const SHEAP_Block*) heap->base;
                    while ((char*) c < (char*) p) {
                        if (HEAP_ISFREE(c) &&
                            (char*) c + c->size >= (char*) b + b->size)
                            break;
                        c = (SHEAP_Block*)((char*) c + c->size);
                    }
                    if ((char*) c < (char*) p)
                        return b;
                    CORE_LOG(eLOG_Warning, "Heap Walk: Adjacent free blocks");
                } else
                    return b;
            } else
                CORE_LOGF(eLOG_Warning,
                          ("Heap Walk: Heap corrupted (0x%08X, %u)",
                           b->flag, (unsigned) b->size));
        } else if ((char*) b > (char*) heap->base + heap->size)
            CORE_LOG(eLOG_Warning, "Heap Walk: Heap corrupted");
        else if (!HEAP_ISLAST(p))
            CORE_LOG(eLOG_Warning, "Heap Walk: Last block lost");
    } else
        CORE_LOG(eLOG_Warning, "Heap Walk: Alien pointer passed");
    return 0;
}


TNCBI_Size HEAP_Trim(HEAP heap)
{
    TNCBI_Size   size, last_size;
    SHEAP_Block* f;

    if (!heap)
        return 0;
    if (!heap->chunk) {
        CORE_LOG(eLOG_Error, "Heap Trim: Heap is read-only");
        return 0;
    }

    if (!(f =s_HEAP_Collect(heap)) || HEAP_ISUSED(f) || f->size < heap->chunk){
        last_size = 0;
        size      = heap->size;
    } else if ((char*) f != heap->base) {
        assert(f->size >= _HEAP_ALIGNMENT);
        last_size = f->size % heap->chunk;
        if (last_size) {
            if (last_size < _HEAP_ALIGNMENT)
                last_size += heap->chunk;
            size = heap->size - f->size + last_size;
        } else {
            SHEAP_Block* b = (SHEAP_Block*) heap->base, *p = 0;
            while (b != f) {
                p = b;
                b = (SHEAP_Block*)((char*) b + b->size);
            }
            size = heap->size - f->size;
            assert(p);
            f = p;
        }
    } else {
        last_size = heap->chunk;
        size      = heap->chunk;
    }

    assert(size % heap->chunk == 0);
    if (heap->expand) {
        void* base = (*heap->expand)(heap->base, size, heap->arg);
        if (!base)
            return 0;
        if (f) {
            ptrdiff_t dp = (char*) f - (char*) heap->base;
            f = (SHEAP_Block*)((char*) base + dp);
            f->flag |= HEAP_LAST;
            if (last_size)
                f->size = last_size;
        }
        heap->base = base;
        heap->size = size;
    } else if (size != heap->size)
        CORE_LOG(eLOG_Error, "Heap Trim: Heap is not trimmable");
    return heap->size;
}


HEAP HEAP_CopySerial(const HEAP heap, size_t extra, int serial)
{
    HEAP   newheap;
    void*  newbase;

    extra = HEAP_ALIGN(extra);
    if (!heap ||
        !(newbase = malloc(HEAP_ALIGN(heap->size) + extra + sizeof(*newheap))))
        return 0;
    memcpy(newbase, heap->base, heap->size);
    newheap = (HEAP)((char*) newbase + HEAP_ALIGN(heap->size) + extra);
    newheap->base   = newbase;
    newheap->size   = heap->size;
    newheap->chunk  = 0/*read-only*/;
    newheap->expand = 0;
    newheap->arg    = 0;
    newheap->copy   = serial ? serial : 1/*copy*/;
    return newheap;
}


void HEAP_Detach(HEAP heap)
{
    if (heap)
        free(heap->copy ? heap->base : heap);
}


void HEAP_Destroy(HEAP heap)
{
    if (heap) {
        if (!heap->chunk && !heap->copy)
            CORE_LOG(eLOG_Warning, "Heap Destroy: Heap is read-only");
        else if (heap->expand/*NB: false for heap copies*/)
            (*heap->expand)(heap->base, 0, heap->arg);
        HEAP_Detach(heap);
    }
}


void* HEAP_Base(const HEAP heap)
{
    return heap ? heap->base : 0;
}


TNCBI_Size HEAP_Size(const HEAP heap)
{
    return heap ? heap->size : 0;
}


int HEAP_Serial(const HEAP heap)
{
    return heap ? heap->copy : 0;
}


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.24  2003/09/23 21:06:30  lavr
 * +HEAP_AttachEx()
 *
 * Revision 6.23  2003/08/28 21:09:58  lavr
 * Accept (and allocate) additional heap extent in HEAP_CopySerial()
 *
 * Revision 6.22  2003/08/25 16:53:37  lavr
 * Add/remove spaces here and there to comply with coding rules...
 *
 * Revision 6.21  2003/08/25 16:47:08  lavr
 * Fix in pointer arith since the base changed from "char*" to "void*"
 *
 * Revision 6.20  2003/08/25 14:50:50  lavr
 * Heap arena ptrs changed to be "void*";  expand routine to take user arg
 *
 * Revision 6.19  2003/08/11 19:08:04  lavr
 * HEAP_Attach() reimplemented via HEAP_AttachEx() [not public yet]
 * HEAP_Trim() fixed to call expansion routine where applicable
 *
 * Revision 6.18  2003/07/31 17:54:03  lavr
 * +HEAP_Trim()
 *
 * Revision 6.17  2003/03/24 19:45:15  lavr
 * Added few minor changes and comments
 *
 * Revision 6.16  2002/08/16 15:37:22  lavr
 * Warn if allocation attempted on a NULL heap
 *
 * Revision 6.15  2002/08/12 15:15:15  lavr
 * More thorough check for the free-in-the-middle heap blocks
 *
 * Revision 6.14  2002/04/13 06:33:52  lavr
 * +HEAP_Base(), +HEAP_Size(), +HEAP_Serial(), new HEAP_CopySerial()
 *
 * Revision 6.13  2001/07/31 15:07:58  lavr
 * Added paranoia log message: freeing a block in a NULL heap
 *
 * Revision 6.12  2001/07/13 20:09:27  lavr
 * If remaining space in a block is equal to block header,
 * do not leave this space as a padding of the block been allocated,
 * but instead form a new block consisting only of the header.
 * The block becomes a subject for a later garbage collecting.
 *
 * Revision 6.11  2001/07/03 20:24:03  lavr
 * Added function: HEAP_Copy()
 *
 * Revision 6.10  2001/06/25 15:32:41  lavr
 * Typo fixed
 *
 * Revision 6.9  2001/06/19 22:22:56  juran
 * Heed warning:  Make s_HEAP_Take() static
 *
 * Revision 6.8  2001/06/19 19:12:01  lavr
 * Type change: size_t -> TNCBI_Size; time_t -> TNCBI_Time
 *
 * Revision 6.7  2001/03/02 20:08:26  lavr
 * Typos fixed
 *
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
