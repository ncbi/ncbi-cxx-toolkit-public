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
 * heap is corrupt, heap has no provision to be expanded, expansion failed,
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
 * Prior to the heap use, the initialization is required, which comprises
 * call to either 'HEAP_Create' or 'HEAP_Attach' with the information about
 * the base heap pointer. 'HEAP_Create' also takes the size of initial
 * heap area (if there is one), and size of chunk (usually, a page size)
 * to be used in heap expansions (defaults to alignment if provided as 0).
 * Additionally (but not compulsory) the application program can provide
 * heap manager with 'resize' routine, which is supposed to be called,
 * when no more room is available in the heap, or the heap has not been
 * preallocated (base = 0 in 'HEAP_Create'), and given the arguments:
 * - current heap base address (or 0 if this is the very first heap alloc),
 * - new required heap size (or 0 if this is the last call to deallocate
 * the entire heap). 
 * If successful, the resize routine must return the new heap base
 * address (if any) of expanded heap area, and where the exact copy of
 * the current heap is made.
 *
 * Note that all heap base pointers must be aligned on a 'double' boundary.
 * Please also be warned not to store pointers to the heap area, as a
 * garbage collection can clobber them.  Within a block, however,
 * it is possible to use local pointers (offsets), which remain same
 * regardless of garbage collections.
 *
 * For automatic traverse purposes there is a 'HEAP_Walk' call, which returns
 * the next block (either free, or used) from the heap.  Given a NULL-pointer,
 * this function returns the very first block, whereas all subsequent calls
 * with the argument being the last observed block results in the next block 
 * returned.  NULL comes back when no more blocks exist in the heap.
 *
 * Note that for proper heap operations, no allocation(s) should happen between
 * successive calls to 'HEAP_Walk', whereas deallocation of the seen block
 * is okay.
 *
 * Explicit heap traversing should not overcome the heap limit,
 * as any information above the limit is not maintained by the heap manager.
 * Every heap operation guarantees that there are no adjacent free blocks,
 * only used blocks can follow each other sequentially.
 *
 * To discontinue to use the heap, 'HEAP_Destroy' or 'HEAP_Detach' can be
 * called.  The former deallocates the heap (by means of a call to 'resize'),
 * the latter just removes the heap handle, retaining the heap data intact.
 * Later, such a heap could be used again if attached with 'HEAP_Attach'.
 *
 * Note that attached heap is in read-only mode, that is nothing can be
 * allocated and/or freed in that heap, as well as an attempt to call
 * 'HEAP_Destroy' will not destroy the heap data.
 *
 * Note also, that 'HEAP_Create' always does heap reset, that is the
 * memory area pointed to by 'base' (if not 0) gets reformatted and lose
 * all previous contents.
 *
 */

#include "ncbi_priv.h"
#include <connect/ncbi_heapmgr.h>
#include <stdlib.h>
#include <string.h>

#if defined(NCBI_OS_MSWIN)  &&  defined(_WIN64)
/* Disable ptr->long conversion warning (even on explicit cast!) */
#  pragma warning (disable : 4311)
#endif /*NCBI_OS_MSWIN && _WIN64*/


struct SHEAP_tag {
    void*          base;    /* Current base of heap extent:  !base == !size  */
    TNCBI_Size     size;    /* Current size of heap extent:  !base == !size  */
    TNCBI_Size     chunk;   /* == 0 when the heap is read-only               */
    FHEAP_Resize   resize;  /* != NULL when resizeable (only for non-RO heaps*/
    void*          arg;     /* Aux argument to pass to "resize"              */
    unsigned int   refc;    /* Reference counter (copy heaps only,0=original)*/
    int            serial;  /* Serial number as assigned by user(Attach|Copy)*/
};


#define _HEAP_ALIGN(a, b)     (((unsigned long)(a) + (b) - 1) & ~((b) - 1))
#define _HEAP_ALIGNMENT       sizeof(double)
#define HEAP_ALIGN(a)         _HEAP_ALIGN(a, _HEAP_ALIGNMENT)
#define HEAP_LAST             0x80000000UL
#define HEAP_USED             0x0DEAD2F0UL
#define HEAP_FREE             0
#define HEAP_ISFREE(b)        (((b)->flag & ~HEAP_LAST) == HEAP_FREE)
#define HEAP_ISUSED(b)        (((b)->flag & ~HEAP_LAST) == HEAP_USED)
#define HEAP_ISLAST(b)        ( (b)->flag &  HEAP_LAST)


HEAP HEAP_Create(void* base,       TNCBI_Size   size,
                 TNCBI_Size chunk, FHEAP_Resize resize, void* arg)
{
    TNCBI_Size min_size;
    SHEAP_Block* b;
    HEAP heap;

    if (!base != !size)
        return 0;
    min_size = size ? (TNCBI_Size) HEAP_ALIGN(sizeof(*b) + 1) : 0;
    if (size < min_size) {
        CORE_LOGF(eLOG_Error, ("Heap Create: Storage too small "
                               "(provided %u, required %u+)",
                               (unsigned) size, (unsigned) min_size));
        return 0;
    }
    if (!(heap = (HEAP) malloc(sizeof(*heap))))
        return 0;
    heap->base   = base;
    heap->size   = size;
    heap->chunk  = chunk        ? (TNCBI_Size) HEAP_ALIGN(chunk) : 0;
    heap->resize = heap->chunk  ? resize                         : 0;
    heap->arg    = heap->resize ? arg                            : 0;
    heap->refc   = 0/*original*/;
    heap->serial = 0;
    if (base) {
        /* Reformat the pre-allocated heap */
        if (HEAP_ALIGN(base) != (unsigned long) base) {
            CORE_LOGF(eLOG_Warning,
                     ("Heap Create: Unaligned base (0x%08lX)", (long) base));
        }
        b = (SHEAP_Block*) base;
        b->flag = HEAP_FREE | HEAP_LAST;
        b->size = size;
    }
    return heap;
}


HEAP HEAP_AttachEx(const void* base, TNCBI_Size size, int serial)
{
    HEAP heap;

    if (!base != !size || !(heap = (HEAP) malloc(sizeof(*heap))))
        return 0;
    if (HEAP_ALIGN(base) != (unsigned long) base) {
        CORE_LOGF(eLOG_Warning,
                  ("Heap Attach: Unaligned base (0x%08lX)", (long) base));
    }
    heap->base   = (void*) base;
    heap->size   = size;
    heap->chunk  = 0/*read-only*/;
    heap->resize = 0;
    heap->arg    = 0;
    heap->refc   = 0/*original*/;
    heap->serial = serial;
    return heap;
}


HEAP HEAP_Attach(const void* base, int serial)
{
    TNCBI_Size size = 0;

    if (base) {
        const SHEAP_Block* b = (const SHEAP_Block*) base;
        unsigned int n;
        for (n = 0; ; n++) {
            if (!HEAP_ISUSED(b) && !HEAP_ISFREE(b)) {
                CORE_LOGF(eLOG_Warning,
                          ("Heap Attach: Heap corrupted (%u, 0x%08X, %u)",
                           n, b->flag, (unsigned) b->size));
                return 0;
            }
            size += b->size;
            if (HEAP_ISLAST(b))
                break;
            b = (const SHEAP_Block*)((const char*) b + b->size);
        }
    }
    return HEAP_AttachEx(base, size, serial);
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
        b->flag  = n->flag;
        n = (SHEAP_Block*)((char*) n + n->size);
    }
    assert(!p || (SHEAP_Block*)((char*) p + p->size) == b);
    if (p && HEAP_ISFREE(p)) {
        p->size += b->size;
        p->flag  = b->flag;
    }
    return n;
}


/* Collect garbage in the heap, moving all contents to the
 * top of, and merging all free blocks at the end into one
 * large free block.  Return pointer to that free block, or
 * NULL if there is no free space in the heap.
 */
static SHEAP_Block* s_HEAP_Collect(HEAP heap)
{
    SHEAP_Block* b = (SHEAP_Block*) heap->base, *f = 0;

    while ((char*) b < (char*) heap->base + heap->size) {
        if (HEAP_ISFREE(b))
            f = b;
        else if (HEAP_ISUSED(b) && f) {
            unsigned int last = b->flag & HEAP_LAST;
            TNCBI_Size   size = f->size;

            memmove(f, b, b->size);
            f->flag &= ~HEAP_LAST;
            f = (SHEAP_Block*)((char*) f + f->size);
            f->flag = HEAP_FREE | last;
            f->size = size;
            b = s_HEAP_Join(0, f);
            continue;
        }
        b = (SHEAP_Block*)((char*) b + b->size);
    }
    return f;
}


/* Take the block 'b' (maybe split in two, if it's roomy enough)
 * for use of by at most 'size' bytes (aligned, and block header included).
 * Return the block to use.
 */
static SHEAP_Block* s_HEAP_Take(SHEAP_Block* b, TNCBI_Size size)
{
    unsigned int last = b->flag & HEAP_LAST;

    assert(b->size >= size);
    if (b->size > size + sizeof(*b)) {
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


static const char* s_HEAP_Id(char* buf, HEAP h)
{
    if (!h  || !h->refc)
        return "";
    sprintf(buf, "[%u]", h->refc);
    return buf;
}


SHEAP_Block* HEAP_Alloc(HEAP heap, TNCBI_Size size)
{
    SHEAP_Block* b, *p = 0;
    TNCBI_Size free = 0;
    unsigned int n;
    char _id[32];

    if (!heap) {
        CORE_LOG(eLOG_Warning, "Heap Alloc: Cannot alloc in NULL heap");
        return 0;
    }
    assert(!heap->base == !heap->size);
    if (size < 1)
        return 0;

    if (!heap->chunk) {
        CORE_LOGF(eLOG_Warning, ("Heap Alloc%s: Heap is read-only",
                                 s_HEAP_Id(_id, heap)));
        return 0;
    }

    size = (TNCBI_Size) HEAP_ALIGN(sizeof(*b) + size);

    b = (SHEAP_Block*) heap->base;
    for (n = 0; (char*) b < (char*) heap->base + heap->size; n++) {
        if (HEAP_ISFREE(b)) {
            /* if an empty, large enough block found, then take it! */
            if (b->size >= size)
                return s_HEAP_Take(b, size);
            free += b->size;
        } else if (!HEAP_ISUSED(b)) {
            CORE_LOGF(eLOG_Warning,
                      ("Heap Alloc%s: Heap corrupted (%u, 0x%08X, %u)",
                       s_HEAP_Id(_id, heap), n, b->flag, (unsigned) b->size));
            return 0;
        }
        p = b;
        b = (SHEAP_Block*)((char*) b + b->size);
    }
    assert(!p || HEAP_ISLAST(p));

    /* Heap exhausted, no free block found */
    if (free >= size)
        b = s_HEAP_Collect(heap);
    else if (!heap->resize)
        return 0;
    else {
        TNCBI_Size hsize =
            (TNCBI_Size) _HEAP_ALIGN(heap->size + size, heap->chunk);
        ptrdiff_t dp = (char*) p - (char*) heap->base;
        void* base;

        if (!(base = (*heap->resize)(heap->base, hsize, heap->arg)))
            return 0;
        p = (SHEAP_Block*)((char*) base + dp);
        if (!heap->base) {
            p->flag = HEAP_FREE | HEAP_LAST;
            p->size = hsize;
            b = p;
        } else {
            if (!HEAP_ISLAST(p)) {
                CORE_LOGF(eLOG_Warning, ("Heap Alloc%s: Last block marker "
                                         "lost", s_HEAP_Id(_id, heap)));
            }
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
        }
        heap->base = base;
        heap->size = hsize;
    }
    assert(b && HEAP_ISFREE(b));
    return s_HEAP_Take(b, size);
}


void HEAP_Free(HEAP heap, SHEAP_Block* ptr)
{
    SHEAP_Block* b, *p;
    unsigned int n;
    char _id[32];

    if (!heap) {
        CORE_LOG(eLOG_Warning, "Heap Free: Cannot free in NULL heap");
        return;
    }
    assert(!heap->base == !heap->size);
    if (!ptr)
        return;

    if (!heap->chunk) {
        CORE_LOGF(eLOG_Warning, ("Heap Free%s: Heap is read-only",
                                 s_HEAP_Id(_id, heap)));
        return;
    }

    b = (SHEAP_Block*) heap->base;
    for (p = 0, n = 0; (char*) b < (char*) heap->base + heap->size; n++) {
        if (HEAP_ISFREE(b)) {
            if (b == ptr) {
                CORE_LOGF(eLOG_Warning, ("Heap Free%s: Freeing free block",
                                         s_HEAP_Id(_id, heap)));
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
                      ("Heap Free%s: Heap corrupted (%u, 0x%08X, %u)",
                       s_HEAP_Id(_id, heap), n, b->flag, (unsigned) b->size));
            return;
        }
        p = b;
        b = (SHEAP_Block*)((char*) p + p->size);
    }
    CORE_LOGF(eLOG_Warning, ("Heap Free%s: Block not found",
                             s_HEAP_Id(_id, heap)));
}


SHEAP_Block* HEAP_Walk(const HEAP heap, const SHEAP_Block* p)
{
    SHEAP_Block* b;
    char _id[32];

    if (!heap) {
        CORE_LOG(eLOG_Warning, "Heap Walk: NULL heap");
        return 0;
    }
    assert(!heap->base == !heap->size);

    if (!p || ((char*) p >= (char*) heap->base &&
               (char*) p <  (char*) heap->base + heap->size)) {
        b = (SHEAP_Block*)(p ? (char*) p + p->size : (char*) heap->base);
        if ((char*) b < (char*) heap->base + heap->size) {
            if (b->size >= sizeof(*b)/*compatibility: ">" should be here now*/
                && b->size == (TNCBI_Size) HEAP_ALIGN(b->size)
                && (char*) b + b->size <= (char*) heap->base + heap->size
                && (HEAP_ISFREE(b) || HEAP_ISUSED(b))) {
                /* Block 'b' seems valid, but... */
                if (!p)
                    return b;
                if (HEAP_ISLAST(p)) {
                    CORE_LOGF(eLOG_Warning, ("Heap Walk%s: Misplaced last "
                                             "block", s_HEAP_Id(_id, heap)));
                } else if (HEAP_ISFREE(p) && HEAP_ISFREE(b)) {
                    const SHEAP_Block* c = (const SHEAP_Block*) heap->base;
                    while ((char*) c < (char*) p) {
                        if (HEAP_ISFREE(c) &&
                            (char*) c + c->size >= (char*) b + b->size)
                            break;
                        c = (SHEAP_Block*)((char*) c + c->size);
                    }
                    if ((char*) c < (char*) p)
                        return b;
                    CORE_LOGF(eLOG_Warning, ("Heap Walk%s: Adjacent free "
                                             "blocks", s_HEAP_Id(_id, heap)));
                } else
                    return b;
            } else {
                CORE_LOGF(eLOG_Warning,
                          ("Heap Walk%s: Heap corrupted (0x%08X, %u)",
                           s_HEAP_Id(_id, heap), b->flag, (unsigned) b->size));
            }
        } else if ((char*) b > (char*) heap->base + heap->size) {
            CORE_LOGF(eLOG_Warning, ("Heap Walk%s: Heap corrupted",
                                     s_HEAP_Id(_id, heap)));
        } else if (b && !HEAP_ISLAST(p)) {
            CORE_LOGF(eLOG_Warning, ("Heap Walk%s: Last block lost",
                                     s_HEAP_Id(_id, heap)));
        }
    } else {
        CORE_LOGF(eLOG_Warning, ("Heap Walk%s: Alien pointer",
                                 s_HEAP_Id(_id, heap)));
    }
    return 0;
}


HEAP HEAP_Trim(HEAP heap)
{
    TNCBI_Size   size, last_size;
    SHEAP_Block* f;

    if (!heap)
        return 0;

    if (!heap->chunk) {
        CORE_LOG(eLOG_Error, "Heap Trim: Heap is read-only");
        return 0;
    }

    if (!(f = s_HEAP_Collect(heap)) || f->size < heap->chunk) {
        assert(!f || HEAP_ISFREE(f));
        last_size = 0;
        size = heap->size;
    } else if (!(last_size = f->size % heap->chunk)) {
        SHEAP_Block* b = (SHEAP_Block*) heap->base, *p = 0;
        while (b != f) {
            p = b;
            b = (SHEAP_Block*)((char*) b + b->size);
        }
        size = heap->size - f->size;
        f = p;
    } else {
        if (last_size <= sizeof(*f))
            last_size = _HEAP_ALIGN(sizeof(*f) + 1, heap->chunk);
        size = heap->size - f->size + last_size;
    }

    if (heap->resize) {
        void* base = (*heap->resize)(heap->base, size, heap->arg);
        if (!size)
            assert(!base);
        else if (!base)
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

    assert(!heap->base == !heap->size);
    return heap;
}


HEAP HEAP_Copy(const HEAP heap, size_t extra, int serial)
{
    HEAP   newheap;
    size_t size;

    if (!heap)
        return 0;
    assert(!heap->base == !heap->size);

    size  = HEAP_ALIGN(heap->size);
    extra = HEAP_ALIGN(extra);
    if (!(newheap = (HEAP)malloc(HEAP_ALIGN(sizeof(*newheap)) + size + extra)))
        return 0;
    newheap->base   = (heap->base
                       ? (char*) newheap + HEAP_ALIGN(sizeof(*newheap))
                       : 0);
    newheap->size   = heap->size;
    newheap->chunk  = 0/*read-only*/;
    newheap->resize = 0;
    newheap->arg    = 0;
    newheap->refc   = 1/*copy*/;
    newheap->serial = serial;
    if (heap->size) {
        memcpy((char*) newheap->base,  heap->base,           heap->size);
        memset((char*) newheap->base + heap->size, 0, size - heap->size);
    }
    return newheap;
}


HEAP HEAP_CopySerial(const HEAP heap, size_t extra, int serial)
{
    return HEAP_Copy(heap, extra, serial);
}


void HEAP_AddRef(HEAP heap)
{
    if (!heap)
        return;
    assert(!heap->base == !heap->size);
    if (heap->refc) {
        heap->refc++;
        assert(heap->refc);
    }
}


void HEAP_Detach(HEAP heap)
{
    if (!heap)
        return;
    assert(!heap->base == !heap->size);
    if (!heap->refc  ||  !--heap->refc) {
        memset(heap, 0, sizeof(*heap));
        free(heap);
    }
}


void HEAP_Destroy(HEAP heap)
{
    if (!heap)
        return;
    assert(!heap->base == !heap->size);
    if (!heap->chunk  &&  !heap->refc)
        CORE_LOG(eLOG_Warning, "Heap Destroy: Heap is read-only");
    else if (heap->resize/*NB: NULL for heap copies*/)
        verify((*heap->resize)(heap->base, 0, heap->arg) == 0);
    HEAP_Detach(heap);
}


void* HEAP_Base(const HEAP heap)
{
    if (!heap)
        return 0;
    assert(!heap->base == !heap->size);
    return heap->base;
}


TNCBI_Size HEAP_Size(const HEAP heap)
{
    if (!heap)
        return 0;
    assert(!heap->base == !heap->size);
    return heap->size;
}


int HEAP_Serial(const HEAP heap)
{
    if (!heap)
        return 0;
    assert(!heap->base == !heap->size);
    return heap->serial;
}


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.31  2006/04/28 16:19:44  lavr
 * Disable W4311 for MSVC/W64
 *
 * Revision 6.30  2006/03/06 20:26:00  lavr
 * Added a paranoid assert() to check ref.-count overflow
 *
 * Revision 6.29  2006/03/06 14:22:48  lavr
 * Cast (void*) to (char*) to allow ptr arithmetics
 *
 * Revision 6.28  2006/03/05 17:35:56  lavr
 * API revised to allow to create ref-counted heap copies
 *
 * Revision 6.27  2005/01/27 19:00:17  lavr
 * Explicit cast of malloc()ed memory
 *
 * Revision 6.26  2003/10/02 14:52:23  lavr
 * Wrapped long lines in the change log
 *
 * Revision 6.25  2003/09/24 02:56:55  ucko
 * HEAP_AttachEx: size_t -> TNCBI_Size per prototype (needed on 64-bit archs)
 *
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
