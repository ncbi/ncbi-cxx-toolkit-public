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
 * Author:  Anton Lavrentiev
 *
 * Abstract:
 *
 * This is a simple heap manager with a primitive garbage collection.
 *
 * The heap contains blocks of data, stored in a common contiguous pool, each
 * block preceded with a SHEAP_Block structure.  The value of 'flag' is is odd
 * (LSB is set), when the block is in use, or even (LSB is 0), when the block
 * is vacant.  'Size' shows the length of the block in bytes, (uninterpreted)
 * data field of which is extended past the header (the header size IS counted
 * in the size of the block).
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
 * the base heap pointer.  'HEAP_Create' also takes the size of initial
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
 * as any information outside is not maintained by the heap manager.
 * Every heap operation guarantees that there are no adjacent free blocks,
 * only used blocks can follow each other sequentially.
 *
 * To discontinue to use the heap, 'HEAP_Destroy' or 'HEAP_Detach' can be
 * called.  The former deallocates the heap (by means of a call to 'resize'),
 * the latter just removes the heap handle, retaining the heap data intact.
 * Later, such a heap can be used again if attached with 'HEAP_Attach'.
 *
 * Note that an attached heap is always in read-only mode, that is nothing
 * can be allocated and/or freed in that heap, as well as an attempt to call
 * 'HEAP_Destroy' will not actually touch any heap data (but destroy the
 * handle only).
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

#define NCBI_USE_ERRCODE_X   Connect_HeapMgr

#if defined(NCBI_OS_MSWIN)  &&  defined(_WIN64)
/* Disable ptr->long conversion warning (even on explicit cast!) */
#  pragma warning (disable : 4311)
#endif /*NCBI_OS_MSWIN && _WIN64*/

#ifdef   abs
#  undef abs
#endif
#define  abs(a) ((a) < 0 ? (a) : -(a))

#ifdef NCBI_OS_LINUX
#  if NCBI_PLATFORM_BITS == 64
#     ifdef __GNUC__
#       define HEAP_PACKED  __attribute__ ((packed))
#     else
#       error "Don't know how to pack on this 64-bit platform"
#     endif
#  else
#     define HEAP_PACKED /* */
#  endif
#else
#  define HEAP_PACKED /* */
#endif


/* Heap's own block view */
typedef struct HEAP_PACKED {
    SHEAP_Block head;         /* Block head, a free block has the following: */
    TNCBI_Size  prevfree;     /* Heap index for prev (smaller) free block    */
    TNCBI_Size  nextfree;     /* Heap index for next (bigger) free block     */
} SHEAP_HeapBlock;


struct SHEAP_tag {
    SHEAP_HeapBlock* base;    /* Base of heap extent:         !base == !size */
    TNCBI_Size       size;    /* # blocks in the heap extent: !base == !size */
    TNCBI_Size       free;    /* Index of the largest free block (OOB=none)  */
    TNCBI_Size       last;    /* Index of the last block (RW heap only)      */
    TNCBI_Size       chunk;   /* Aligned (bytes);  0 when the heap is RO     */
    FHEAP_Resize     resize;  /* != NULL when resizeable (RW heap only)      */
    void*            auxarg;  /* Auxiliary argument to pass to "resize"      */
    unsigned int     refcnt;  /* Reference count (for heap copy, 0=original) */
    int              serial;  /* Serial number as assigned by (Attach|Copy)  */
};


static int/*bool*/ s_HEAP_fast = 1/*true*/;


#define _HEAP_ALIGN_EX(a, b)  ((((unsigned long)(a) + ((b) - 1)) / (b)) * (b))
#define _HEAP_ALIGN(a, b)     (( (unsigned long)(a) + ((b) - 1)) & ~((b) - 1))
#define _HEAP_SIZESHIFT       4
#define HEAP_BLOCKS(s)        ((s) >> _HEAP_SIZESHIFT)
#define HEAP_EXTENT(b)        ((b) << _HEAP_SIZESHIFT)
#define HEAP_ALIGN(a)         _HEAP_ALIGN(a, HEAP_EXTENT(1))
#define HEAP_MASK             (~(HEAP_EXTENT(1) - 1))
#define HEAP_LAST             0x80000000UL
#define HEAP_USED             1
#define HEAP_FREE             0
#define HEAP_SIZE(s)          ((s) &  HEAP_MASK)
#define HEAP_FLAG(s)          ((s) & ~HEAP_MASK)
#define HEAP_NEXT(b)          ((SHEAP_HeapBlock*)                       \
                               ((char*)(b) +           (b)->head.size))
#define HEAP_PREV(b)          ((SHEAP_HeapBlock*)                       \
                               ((char*)(b) - HEAP_SIZE((b)->head.flag)))
#define HEAP_INDEX(b, base)   ((TNCBI_Size)((b) - (base)))
#define HEAP_ISLAST(b)        ( (b)->head.flag & HEAP_LAST)
#define HEAP_ISFREE(b)        (((b)->head.flag & HEAP_USED) == HEAP_FREE)
#define HEAP_ISUSED(b)        (((b)->head.flag & HEAP_USED) != HEAP_FREE)


#if 0 /*FIXME*/
/* Performance / integrity improvements:
 * 1. flag is to keep byte-size of the previous block (instead of the magic);
 * 2. since sizes always have last nibble zero, use that in the flag field as
 *    the following:
 *    bit 0 -- set for used, unset for free block;
 *    bit 1 -- set for last block in the heap.
 * 3. bits 2 & 3 can be used as a parity checks for each size to compensate
 *    for discontinued use of the magic value (at least for used blocks).
 * With this scheme, block freeing will no longer need a lookup.
 * Yet keeping current size clean will still allow fast forward moves.
 * Remove:
 *   HEAP_FREE
 *   HEAP_ISFREE
 * Change:
 *   HEAP_LAST -> 2
 */

static int/*bool*/ x_Parity(unsigned int v)
{
#if 0
    v ^= v >> 1;
    v ^= v >> 2;
    v = (v & 0x11111111U) * 0x11111111U;
    return (v >> 28) & 1;
#else
    v ^= v >> 16;
    v ^= v >> 8;
    v ^= v >> 4;
    v &= 0xF;
    return (0x6996 >> v) & 1;
#endif
}
#endif /*0*/


HEAP HEAP_Create(void*      base,  TNCBI_Size   size,
                 TNCBI_Size chunk, FHEAP_Resize resize, void* auxarg)
{
    HEAP heap;

    assert(HEAP_EXTENT(1) == sizeof(SHEAP_HeapBlock));
    assert(_HEAP_ALIGN_EX(1, sizeof(SHEAP_Block)) ==
           _HEAP_ALIGN   (1, sizeof(SHEAP_Block)));

    if (!base != !size)
        return 0;
    if (size  &&  size < HEAP_EXTENT(1)) {
        CORE_LOGF_X(1, eLOG_Error,
                    ("Heap Create: Storage too small:"
                     " provided %u, required %u+",
                     size, HEAP_EXTENT(1)));
        return 0;
    }
    if (!(heap = (HEAP) malloc(sizeof(*heap))))
        return 0;
    heap->base   = (SHEAP_HeapBlock*) base;
    heap->size   = HEAP_BLOCKS(size);
    heap->free   = 0;
    heap->last   = 0;
    heap->chunk  = chunk        ? HEAP_ALIGN(chunk) : 0;
    heap->resize = heap->chunk  ? resize            : 0;
    heap->auxarg = heap->resize ? auxarg            : 0;
    heap->refcnt = 0/*original*/;
    heap->serial = 0;
    if (base) {
        SHEAP_HeapBlock* b = heap->base;
        /* Reformat the pre-allocated heap */
        if (_HEAP_ALIGN(base, sizeof(SHEAP_Block)) != (unsigned long) base) {
            CORE_LOGF_X(2, eLOG_Warning,
                        ("Heap Create: Unaligned base (0x%08lX)",
                         (long) base));
        }
        b->head.flag = HEAP_FREE | HEAP_LAST;
        b->head.size = HEAP_SIZE(size);
        b->nextfree  = 0;
        b->prevfree  = 0;
    }
    return heap;
}


HEAP HEAP_AttachFast(const void* base, TNCBI_Size size, int serial)
{
    HEAP heap;

    assert(HEAP_EXTENT(1) == sizeof(SHEAP_HeapBlock));
    assert(_HEAP_ALIGN_EX(1, sizeof(SHEAP_Block)) ==
           _HEAP_ALIGN   (1, sizeof(SHEAP_Block)));

    if (!base != !size  ||  !(heap = (HEAP) calloc(1, sizeof(*heap))))
        return 0;
    if (_HEAP_ALIGN(base, sizeof(SHEAP_Block)) != (unsigned long) base) {
        CORE_LOGF_X(3, eLOG_Warning,
                    ("Heap Attach: Unaligned base (0x%08lX)", (long) base));
    }
    heap->base   = (SHEAP_HeapBlock*) base;
    heap->size   = HEAP_BLOCKS(size);
    heap->free   = heap->size;
    heap->last   = heap->size;
    heap->serial = serial;
    if (size != HEAP_EXTENT(heap->size)) {
        CORE_LOGF_X(4, eLOG_Warning,
                    ("Heap Attach: Heap size truncation (%u->%u)"
                     " can result in missing data",
                     size, HEAP_EXTENT(heap->size)));
    }
    return heap;
}


HEAP HEAP_Attach(const void* base, TNCBI_Size maxsize, int serial)
{
    TNCBI_Size size = 0;

    if (base  &&  (!maxsize  ||  maxsize > sizeof(SHEAP_Block))) {
        const SHEAP_HeapBlock* b = (const SHEAP_HeapBlock*) base;
        for (;;) {
#if 0 /*FIXME*/
            if (!HEAP_ISUSED(b)  &&  !HEAP_ISFREE(b)) { /*FIXME*/
                CORE_LOGF_X(5, eLOG_Error,
                            ("Heap Attach: Heap corrupt @%u (0x%08X, %u)",
                             HEAP_INDEX(b, (SHEAP_HeapBlock*) base),
                             b->head.flag, b->head.size));
                return 0;
            }
#endif /*0*/
            size += b->head.size;
            if (maxsize  &&
                (maxsize < size  ||
                 (maxsize - size < sizeof(SHEAP_Block)  &&  !HEAP_ISLAST(b)))){
                CORE_LOGF_X(34, eLOG_Error,
                            ("Heap Attach: Runaway heap @%u (0x%08X, %u)"
                             " size=%u vs. maxsize=%u",
                             HEAP_INDEX(b, (SHEAP_HeapBlock*) base),
                             b->head.flag, b->head.size,
                             size, maxsize));
                return 0;
            }
            if (HEAP_ISLAST(b))
                break;
            b = HEAP_NEXT(b);
        }
    }
    return HEAP_AttachFast(base, size, serial);
}


static const char* s_HEAP_Id(char* buf, HEAP h)
{
    if (!h)
        return "";
    if (h->serial  &&  h->refcnt)
        sprintf(buf,"[C%d%sR%u]",abs(h->serial),&"-"[h->serial > 0],h->refcnt);
    else if (h->serial)
        sprintf(buf,"[C%d%s]", abs(h->serial), &"-"[h->serial > 0]);
    else if (h->refcnt)
        sprintf(buf,"[R%u]", h->refcnt);
    else
        strcpy(buf, "");
    return buf;
}


/* Scan the heap and return a free block, whose size is the minimal to fit the
 * requested one (i.e. previous free block, if any, is smaller).  The block
 * returned is still linked into the list of free blocks.  Iff no suitable free
 * block can be found, *size gets updated with the total free space available.
 */
static SHEAP_HeapBlock* s_HEAP_Find(HEAP             heap,
                                    TNCBI_Size*      size,
                                    SHEAP_HeapBlock* hint)
{
    SHEAP_HeapBlock *f, *b, *e = heap->base + heap->free;
    TNCBI_Size find = size ? *size : 0;

    assert(!hint  ||  HEAP_ISFREE(hint));
    assert(heap->free < heap->size  &&  HEAP_ISFREE(e));
    if (!hint  &&  find < (e->head.size >> 1)) {
        /* begin from the smallest block */
        for (b = heap->base + e->nextfree;  ; b = heap->base + b->nextfree) {
            f = b;
            if (!s_HEAP_fast) {
                if (b < heap->base  ||  heap->base + heap->size <= b) {
                    b = 0;
                    goto err;
                }
                if (!HEAP_ISFREE(b))
                    goto err;
            }
            if (find <= b->head.size  ||  b == e)
                break;
        }
        assert(f);
    } else {
        TNCBI_Size free = (b = hint ? hint : e)->head.size;
        f = free < find ? 0 : b;  /* a bigger free block */
        for (b = heap->base + b->prevfree;  ; b = heap->base + b->prevfree) {
            if (!s_HEAP_fast) {
                if (b < heap->base  ||  heap->base + heap->size <= b) {
                    b = 0;
                    goto err;
                }
                if (!HEAP_ISFREE(b))
                    goto err;
            }
            if (!f) {
                if (b != e  &&  (free += b->head.size) < find)
                    continue;
                assert(size);
                *size = free;
                break;
            }
            if (b == e  ||  b->head.size < find)
                break;
            f = b;
        }
    }
    return f;

 err:
    {{
        char _id[32], msg[80];
        if (b)
            sprintf(msg, " (0x%08X, %u)", b->head.flag, b->head.size);
        else
            *msg = '\0';
        CORE_LOGF_X(8, eLOG_Error,
                    ("Heap Find%s: Heap corrupt @%u/%u%s",
                     s_HEAP_Id(_id, heap), HEAP_INDEX(b, heap->base),
                     heap->size, msg));
    }}
    if (size)
        *size = 0;
    return 0;
}


static void s_HEAP_Link(HEAP heap, SHEAP_HeapBlock* f, SHEAP_HeapBlock* hint)
{
    unsigned int free = HEAP_INDEX(f, heap->base);
    SHEAP_HeapBlock* b;

    assert(HEAP_ISFREE(f)  &&  (!hint  ||  HEAP_ISFREE(hint)));
    if (heap->free == heap->size) {
        assert(!hint);
        f->prevfree = free;
        f->nextfree = free;
        heap->free  = free;
        return;
    }
    assert(heap->free < heap->size);
    b = heap->base + heap->free;
    assert(HEAP_ISFREE(b));
    if (b->head.size < f->head.size) {
        assert(!hint);
        /* Link in AFTER b, and also set the new free head */
        f->nextfree = b->nextfree;
        f->prevfree = HEAP_INDEX(b, heap->base);
        heap->base[b->nextfree].prevfree = free;
        b->nextfree = free;
        heap->free  = free;
    } else {
        TNCBI_Size size = f->head.size;
        /* find a block "b" that is just bigger than "f" */
        assert(!hint  ||  size <= hint->head.size);
        b = s_HEAP_Find(heap, &size, hint);
        assert(b  &&  HEAP_ISFREE(b));
        /* Link in BEFORE b (so that f <= b) */
        f->nextfree = HEAP_INDEX(b, heap->base);
        f->prevfree = b->prevfree;
        heap->base[b->prevfree].nextfree = free;
        b->prevfree = free;
    }
}


#if defined(_DEBUG)  &&  !defined(NDEBUG)
/*ARGSUSED*/
#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static void s_HEAP_Unlink(SHEAP_HeapBlock* b)
{
    b->prevfree = b->nextfree = ~((TNCBI_Size) 0);
}
#else
#define s_HEAP_Unlink(b)  ((void) 0)
#endif /*_DEBUG && !NDEBUG*/


/* Collect garbage in the heap, moving all contents up to the top, and merging
 * all free blocks down at the end into a single free block (or until "need"
 * bytes become available).  Return pointer to that free block (which is
 * unlinked from the free chain), or NULL if there was no free space.
 * If the free block is returned its "flag" is set to the byte-size of the
 * previous block with LSB+1 set if it's the last block, clear otherwise.
 * FIXME: can be sped up with prev size in place.
 */
static SHEAP_HeapBlock* s_HEAP_Collect(HEAP heap, TNCBI_Size need)
{
    const SHEAP_HeapBlock* e = heap->base + heap->size;
    SHEAP_HeapBlock* f = 0, *u = 0, *p = 0;
    SHEAP_HeapBlock* b = heap->base;
    unsigned int last = 0;
    TNCBI_Size free = 0;

    do {
        SHEAP_HeapBlock* n = b == e ? 0 : HEAP_NEXT(b);
        assert(!n  ||  HEAP_SIZE(b->head.size) == b->head.size);
        if (n)
            last = HEAP_ISLAST(b);
        if (!n  ||  HEAP_ISFREE(b)) {
            if (n) {
                assert(!need  ||  b->head.size < need);
                assert(n == e  ||  !HEAP_ISFREE(n));
                free += b->head.size;
            }
            if (f) {
                assert(f != p  &&  (u  ||  (!n  &&  !need)));
                assert(!u  ||  HEAP_NEXT(f) == u);
                if (n) {
                    heap->base[b->prevfree].nextfree = b->nextfree;
                    heap->base[b->nextfree].prevfree = b->prevfree;
                    if (b == heap->base + heap->free)
                        heap->free = b->prevfree;
                    assert(heap->base + heap->size != b); /*because of "f"*/
                    s_HEAP_Unlink(b);
                }
                if (f != heap->base + heap->free) {
                    heap->base[f->prevfree].nextfree = f->nextfree;
                    heap->base[f->nextfree].prevfree = f->prevfree;
                } else if (heap->free != f->prevfree) {
                    heap->base[f->prevfree].nextfree = f->nextfree;
                    heap->base[f->nextfree].prevfree = f->prevfree;
                    heap->free = f->prevfree;
                } else {
                    assert(f->prevfree == f->nextfree);
                    heap->free = heap->size;
                }
                if (u) {
                    TNCBI_Size size = HEAP_BLOCKS(f->head.size);
                    TNCBI_Size used = (TNCBI_Size)(b - u);
                    assert(size == (TNCBI_Size)(u - f));
                    memmove(f, u, HEAP_EXTENT(used));
                    assert(p);
                    p -= size;
                    p->head.flag &= ~HEAP_LAST;
                    f += used;
                    f->head.flag = HEAP_FREE | last;
                    f->head.size = free;
                    s_HEAP_Unlink(f);
                    if (last)
                        heap->last = HEAP_INDEX(f, heap->base);
                    u = 0;
                } else
                    s_HEAP_Unlink(f);
                if (need  &&  need <= free)
                    return f;
                if (n)
                    s_HEAP_Link(heap, f, 0);
            } else if (n)
                f = b;
        } else {
            if (f  &&  !u)
                u = b;
            p = b;
        }
        b = n;
    } while (b);

    if (f) {
        /*FIXME*/
        assert(last);
        f->head.flag = (p ? p->head.size : 0) | 2/*HEAP_LAST*/;
    }
    return f;
}


/* Book 'size' bytes (aligned, and block header included) within the given
 * free block 'b' of an adequate size (perhaps causing the block to be split
 * in two, if it's roomy enough, and the remaining part marked as a new
 * free block).  Non-zero 'tail' parameter inverses the order of locations of
 * occupied blocks in successive allocations.  Return the block to use.
 */
static SHEAP_Block* s_HEAP_Take(HEAP heap,
                                SHEAP_HeapBlock* b, SHEAP_HeapBlock* n,
                                TNCBI_Size size, TNCBI_Size need,
                                int/*bool*/ tail)
{
    assert(HEAP_SIZE(size) == size);
    assert(HEAP_ISFREE(b)  &&  size <= b->head.size);
    if (size + HEAP_EXTENT(1) <= b->head.size) {
        /* the block is to used partially */
        unsigned int last = HEAP_ISLAST(b);
        SHEAP_HeapBlock* f;
        if (tail) {
            f = b;
            f->head.flag &= ~HEAP_LAST;
            f->head.size -= size;
            b = HEAP_NEXT(f);
            b->head.flag  = HEAP_USED | last;
            b->head.size  = size;
            if (last)
                heap->last = HEAP_INDEX(b, heap->base);            
        } else {
            TNCBI_Size save = b->head.size;
            b->head.size  = size;
            f = HEAP_NEXT(b);
            f->head.flag  = b->head.flag;
            f->head.size  = save - size;
            b->head.flag  = HEAP_USED;
            if (last)
                heap->last = HEAP_INDEX(f, heap->base);
        }
        s_HEAP_Unlink(f);
        s_HEAP_Link(heap, f, n);
    } else
        b->head.flag |= HEAP_USED;
    if (size -= need)
        memset((char*) b + need, 0, size); /* block padding (if any) zeroed */
    return &b->head;
}


SHEAP_Block* HEAP_Alloc(HEAP        heap,
                        TNCBI_Size  size,
                        int/*bool*/ tail)
{
    SHEAP_HeapBlock *f, *n;
    TNCBI_Size need;
    char _id[32];

    if (!heap) {
        CORE_LOG_X(6, eLOG_Warning, "Heap Alloc: NULL heap");
        return 0;
    }
    assert(!heap->base == !heap->size);

    if (!heap->chunk) {
        CORE_LOGF_X(7, eLOG_Error,
                    ("Heap Alloc%s: Heap read-only", s_HEAP_Id(_id, heap)));
        return 0;
    }
    if (size < 1)
        return 0;

    size += (TNCBI_Size) sizeof(SHEAP_Block);
    need  = HEAP_ALIGN(size);

    if (heap->free < heap->size) {
        TNCBI_Size free = need;
        if ((f = s_HEAP_Find(heap, &free, 0)) != 0) {
            /*NB: f is linked, unlink*/
            n = heap->base + f->nextfree;
            if (n == f) {
                assert(f == heap->base + heap->free);
                assert(f->prevfree == f->nextfree);
                heap->free = heap->size;
                n = 0;
            } else {
                n->prevfree = f->prevfree;
                heap->base[f->prevfree].nextfree = f->nextfree;
                if (f == heap->base + heap->free) {
                    heap->free = f->prevfree;
                    n = 0;
                }
            }
            s_HEAP_Unlink(f);
        } else if (need <= free) {
            /*NB: f is unlinked*/
            f = s_HEAP_Collect(heap, need);
            assert(f  &&  HEAP_ISFREE(f)  &&  need <= f->head.size);
            n = 0;
        } else
            f = n = 0;
    } else
        f = n = 0;
    if (!f) {
        TNCBI_Size dsize = HEAP_EXTENT(heap->size);
        TNCBI_Size hsize = _HEAP_ALIGN_EX(dsize + need, heap->chunk);
        SHEAP_HeapBlock* base = (SHEAP_HeapBlock*)
            heap->resize(heap->base, hsize, heap->auxarg);
        if (_HEAP_ALIGN(base, sizeof(SHEAP_Block)) != (unsigned long) base) {
            CORE_LOGF_X(9, eLOG_Warning,
                        ("Heap Alloc%s: Unaligned base (0x%08lX)",
                         s_HEAP_Id(_id, heap), (long) base));
        }
        if (!base)
            return 0;
        dsize = hsize - dsize;
        memset(base + heap->size, 0, dsize); /* security */
        f = base + heap->last;
        if (!heap->base) {
            assert(!heap->last  &&  !heap->free);
            f->head.flag = HEAP_FREE | HEAP_LAST;
            f->head.size = hsize;
            hsize = HEAP_BLOCKS(hsize);
            heap->free = hsize;
        } else {
            assert(base <= f  &&  f < base + heap->size  &&  HEAP_ISLAST(f));
            hsize = HEAP_BLOCKS(hsize);
            if (!HEAP_ISFREE(f)) {
                f->head.flag &= ~HEAP_LAST;
                /* New block is at the very top of the heap */
                heap->last = heap->size;
                f = base + heap->size;
                f->head.flag  = HEAP_FREE | HEAP_LAST;
                f->head.size  = dsize;
                if (heap->free == heap->size)
                    heap->free  = hsize;
            } else {
                if (f != base + heap->free) {
                    base[f->nextfree].prevfree = f->prevfree;
                    base[f->prevfree].nextfree = f->nextfree;
                } else if (heap->free != f->prevfree) {
                    base[f->nextfree].prevfree = f->prevfree;
                    base[f->prevfree].nextfree = f->nextfree;
                    heap->free = f->prevfree;
                } else {
                    assert(f->prevfree == f->nextfree);
                    heap->free = hsize;
                }
                f->head.size += dsize;
            }
            s_HEAP_Unlink(f);
        }
        heap->base = base;
        heap->size = hsize;
        assert(!n);
    }
    assert(f  &&  HEAP_ISFREE(f)  &&  need <= f->head.size);
    return s_HEAP_Take(heap, f, n, need, size, tail);
}


static void s_HEAP_Free(HEAP             heap,
                        SHEAP_HeapBlock* p,
                        SHEAP_HeapBlock* b,
                        SHEAP_HeapBlock* n)
{
    /* NB: in order to maintain HEAP_Walk() "b" must have "size" updated
     * so that the next heap block could be located correctly, and also
     * "flag" must keep its HEAP_LAST bit so that it can be verified. */
    unsigned int last = HEAP_ISLAST(b);

    s_HEAP_Unlink(b);
    b->head.flag = HEAP_FREE | last;
    if (!last  &&  HEAP_ISFREE(n)) {
        b->head.size += n->head.size;
        if (HEAP_ISLAST(n)) {
            last = HEAP_LAST;
            b->head.flag |= HEAP_LAST;
            heap->last = HEAP_INDEX(b, heap->base);
        }
        if (n == heap->base + heap->free) {
            if (heap->free == n->prevfree) {
                assert(!p  ||  !HEAP_ISFREE(p));
                assert(n->prevfree == n->nextfree);
                heap->free = HEAP_INDEX(b, heap->base);
                b->prevfree = heap->free;
                b->nextfree = heap->free;
                return;
            }
            heap->free = n->prevfree;
        }
        heap->base[n->nextfree].prevfree = n->prevfree;
        heap->base[n->prevfree].nextfree = n->nextfree;
        s_HEAP_Unlink(n);
    }
    if (p  &&  HEAP_ISFREE(p)) {
        p->head.size += b->head.size;
        if (last) {
            assert(!HEAP_ISLAST(p));
            p->head.flag |= HEAP_LAST;
            heap->last = HEAP_INDEX(p, heap->base);
        }
        if (p == heap->base + heap->free) {
            if (heap->free == p->prevfree) {
                assert(p->prevfree == p->nextfree);
                return;
            }
            heap->free = p->prevfree;
        }
        heap->base[p->nextfree].prevfree = p->prevfree;
        heap->base[p->prevfree].nextfree = p->nextfree;
        s_HEAP_Unlink(p);
        b = p;
    }
    s_HEAP_Link(heap, b, 0);
}


void HEAP_Free(HEAP heap, SHEAP_Block* ptr)
{
    const SHEAP_HeapBlock* e;
    SHEAP_HeapBlock *b, *p;
    char _id[32];

    if (!heap) {
        CORE_LOG_X(10, eLOG_Warning, "Heap Free: NULL heap");
        return;
    }
    assert(!heap->base == !heap->size);

    if (!heap->chunk) {
        CORE_LOGF_X(11, eLOG_Error,
                    ("Heap Free%s: Heap read-only", s_HEAP_Id(_id, heap)));
        return;
    }
    if (!ptr)
        return;

    p = 0;
    b = heap->base;
    e = b + heap->size;
    while (b < e) {
        SHEAP_HeapBlock* n = HEAP_NEXT(b);
        if (n <= e) {
            if (&b->head == ptr) {
                if (!HEAP_ISFREE(b)) {
                    s_HEAP_Free(heap, p, b, n);
                    return;
                }
                if (HEAP_ISFREE(b)) {
                    CORE_LOGF_X(12, eLOG_Warning,
                                ("Heap Free%s: Freeing free block @%u",
                                 s_HEAP_Id(_id, heap),
                                 HEAP_INDEX(b, heap->base)));
                    return;
                }
            } else {
                p = b;
                b = n;
                continue;
            }
        }
        CORE_LOGF_X(13, eLOG_Error,
                    ("Heap Free%s: Heap corrupt @%u/%u (0x%08X, %u)",
                     s_HEAP_Id(_id, heap), HEAP_INDEX(b, heap->base),
                     heap->size, b->head.flag, b->head.size));
        return;
    }

    CORE_LOGF_X(14, eLOG_Error,
                ("Heap Free%s: Block not found", s_HEAP_Id(_id, heap)));
}


/*FIXME: to remove*/
void HEAP_FreeFast(HEAP heap, SHEAP_Block* ptr, const SHEAP_Block* prev)
{
    SHEAP_HeapBlock *b, *p, *n;
    char _id[32];

    if (!heap) {
        CORE_LOG_X(15, eLOG_Warning, "Heap Free: NULL heap");
        return;
    }
    assert(!heap->base == !heap->size);

    if (!heap->chunk) {
        CORE_LOGF_X(16, eLOG_Error,
                    ("Heap Free%s: Heap read-only", s_HEAP_Id(_id, heap)));
        return;
    }
    if (!ptr)
        return;

    p = (SHEAP_HeapBlock*) prev;
    b = (SHEAP_HeapBlock*) ptr;
    n = HEAP_NEXT(b);

    if (!s_HEAP_fast) {
        const SHEAP_HeapBlock* e = heap->base + heap->size;
        if (b < heap->base  ||  e < n) {
            CORE_LOGF_X(17, eLOG_Error,
                        ("Heap Free%s: Alien block", s_HEAP_Id(_id, heap)));
            return;
        }
        if ((!p  &&  b != heap->base)  ||
            ( p  &&  (p < heap->base  ||  b != HEAP_NEXT(p)))) {
            char h[40];
            if (!p  ||  p < heap->base  ||  e <= p)
                *h = '\0';
            else
                sprintf(h, "(%u)", HEAP_INDEX(p, heap->base));
            CORE_LOGF_X(18, eLOG_Warning,
                        ("Heap Free%s: Lame hint%s for block @%u",
                         s_HEAP_Id(_id, heap), h, HEAP_INDEX(b, heap->base)));
            HEAP_Free(heap, ptr);
            return;
        }
        if (HEAP_ISFREE(b)) {
            CORE_LOGF_X(19, eLOG_Warning,
                        ("Heap Free%s: Freeing free block @%u",
                         s_HEAP_Id(_id, heap), HEAP_INDEX(b, heap->base)));
            return;
        }
    }

    s_HEAP_Free(heap, p, b, n);
}


HEAP HEAP_Trim(HEAP heap)
{
    TNCBI_Size hsize, size, prev;
    SHEAP_HeapBlock* b;
    char _id[32];

    if (!heap)
        return 0;
    assert(!heap->base == !heap->size);
 
    if (!heap->chunk) {
        CORE_LOGF_X(30, eLOG_Error,
                    ("Heap Trim%s: Heap read-only", s_HEAP_Id(_id, heap)));
        return 0;
    }
    if (s_HEAP_fast  &&  heap->size == heap->free)
        return heap/*no free space, nothing to do*/;

    b = s_HEAP_Collect(heap, 0);

    if (b) {
        assert(HEAP_ISFREE(b)  &&  (b->head.flag & 2/*HEAP_LAST*/));
        prev = HEAP_BLOCKS(b->head.flag);
        b->head.flag = HEAP_FREE | HEAP_LAST;
    } else
        prev = 0;
    if (!b  ||  b->head.size < heap->chunk) {
        hsize = HEAP_EXTENT(heap->size);
        size  = 0;
    } else if (!(size = b->head.size % heap->chunk)) {
        hsize = HEAP_EXTENT(heap->size) - b->head.size;
        b    -= prev;
        assert(!prev  ||  !HEAP_ISFREE(b));
    } else {
        assert(HEAP_EXTENT(1) <= size);
        hsize = HEAP_EXTENT(heap->size) - b->head.size + size;
    }

    if (heap->resize) {
        SHEAP_HeapBlock* base = (SHEAP_HeapBlock*)
            heap->resize(heap->base, hsize, heap->auxarg);
        if (!hsize)
            assert(!base);
        else if (!base)
            return 0;
        if (_HEAP_ALIGN(base, sizeof(SHEAP_Block)) != (unsigned long) base) {
            CORE_LOGF_X(31, eLOG_Warning,
                        ("Heap Trim%s: Unaligned base (0x%08lX)",
                         s_HEAP_Id(_id, heap), (long) base));
        }
        prev = HEAP_INDEX(b, heap->base);
        hsize = HEAP_BLOCKS(hsize);
        if (heap->free == heap->size)
            heap->free  = hsize;
        heap->base = base;
        heap->size = hsize;
        if (base  &&  b) {
            b = base + prev;
            if (!HEAP_ISFREE(b)) {
                assert(heap->free == hsize);
                b->head.flag |= HEAP_LAST;
                heap->last = prev;
            } else {
                assert(HEAP_ISLAST(b));
                if (size)
                    b->head.size = size;
                s_HEAP_Link(heap, b, 0);
            }
        }
        assert(HEAP_EXTENT(hsize) % heap->chunk == 0);
        assert(heap->free <= heap->size);
        assert(heap->last <= heap->size);
    } else if (hsize != HEAP_EXTENT(heap->size)) {
        CORE_LOGF_X(32, eLOG_Error,
                    ("Heap Trim%s: Heap not trimmable", s_HEAP_Id(_id, heap)));
    }

    assert(!heap->base == !heap->size);
    return heap;
}


static SHEAP_Block* s_HEAP_Walk(const HEAP heap, const SHEAP_Block* ptr)
{
    SHEAP_HeapBlock *n, *b, *p = (SHEAP_HeapBlock*) ptr;
    const SHEAP_HeapBlock* e = heap->base + heap->size;
    char msg[80];
    char _id[32];
    size_t i;

    if (p) {
        if (p < heap->base  ||  e <= p
            ||  p->head.size <= sizeof(SHEAP_Block)
            ||  HEAP_ALIGN(p->head.size) != p->head.size
            /*FIXME  ||  (!HEAP_ISFREE(p)  &&  !HEAP_ISUSED(p))*/) {
            CORE_LOGF_X(28, eLOG_Error,
                        ("Heap Walk%s: Alien pointer", s_HEAP_Id(_id, heap)));
            return 0;
        }
        b = HEAP_NEXT(p);
    } else
        b = heap->base;

    if (e <= b  ||  b <= p
        ||  b->head.size <= sizeof(SHEAP_Block)
        ||  HEAP_ALIGN(b->head.size) != b->head.size
        /*FIXME  ||  (!HEAP_ISFREE(b)  &&  !HEAP_ISUSED(b))*/
        ||  e < (n = HEAP_NEXT(b))  ||  n <= b) {
        if (b != e  ||  (b  &&  !p)) {
            CORE_LOGF_X(26, eLOG_Error,
                        ("Heap Walk%s: Heap corrupt", s_HEAP_Id(_id, heap)));
        } else if (b/*== e*/  &&  !HEAP_ISLAST(p)) {
            CORE_LOGF_X(27, eLOG_Error,
                        ("Heap Walk%s: Last block lost",s_HEAP_Id(_id, heap)));
        }
        return 0;
    }

    if (HEAP_ISLAST(b)  &&
        (n < e  ||  (heap->chunk/*RW heap*/ &&  b < heap->base + heap->last))){
        if (heap->chunk/*RW heap*/)
            sprintf(msg, " expected @%u", heap->last);
        else
            *msg = '\0';
        CORE_LOGF_X(23, eLOG_Error,
                    ("Heap Walk%s: Last block @%u%s", s_HEAP_Id(_id, heap),
                     HEAP_INDEX(b, heap->base), msg));
    }

    if (HEAP_ISFREE(b)) {
        const SHEAP_HeapBlock* c;
        if (heap->chunk/*RW heap*/) {
            /* Free blocks are tricky!  They can be left-overs from
               merging of freed blocks while walking.  Careful here! */
            int/*bool*/ ok = 0/*false*/;
            c = heap->base + heap->free;
            for (i = 0;  i < heap->size;  ++i) {
                const SHEAP_HeapBlock* s;
                int/*bool*/ self;
                if (e <= c
                    ||  heap->size <= c->prevfree
                    ||  heap->size <= c->nextfree
                    ||  !HEAP_ISFREE(heap->base + c->prevfree)
                    ||  !HEAP_ISFREE(heap->base + c->nextfree)
                    ||  e < (s = HEAP_NEXT(c))) {
                    c = 0;
                    break;
                }
                self = (c->nextfree == c->prevfree  &&
                        c == heap->base + c->prevfree);
                if (self  ||  (c <= b  &&  b <= s)){
                    if (ok  ||  s < n) {
                        c = 0;
                        break;
                    }
                    if (self/*the only single free block in heap*/) {
                        ok = c <= b  &&  c == heap->base + heap->free;
                        break;
                    }
                    ok = 1/*true*/;
                }
                s = heap->base + c->prevfree;
                if (s == c  ||  c != heap->base + s->nextfree) {
                    b = 0;
                    break;
                }
                if (s == heap->base + heap->free)
                    break;
                if (c->head.size < s->head.size) {
                    b = 0;
                    break;
                }
                c = s;
            }
            if (!ok)
                b = 0;
            else if (c  &&  b)
                c = b;
        } else {
            /*FIXME: check for monotonic increase in sizes*/
            /* RO heap may not have any free block peculiarities */
            c = b;
            if (heap->size <= b->prevfree  ||
                heap->size <= b->nextfree
                ||  !HEAP_ISFREE(heap->base + b->prevfree)
                ||  !HEAP_ISFREE(heap->base + b->nextfree)) {
                b = 0;
            } else if (b->prevfree != b->nextfree  ||
                       b != heap->base + b->nextfree) {
                for (i = 0;  i < heap->size;  ++i) {
                    const SHEAP_HeapBlock* s = b;
                    b = heap->base + b->nextfree;
                    if (!HEAP_ISFREE(b)  ||  b == s  ||
                        heap->size <= b->nextfree    ||
                        s != heap->base + b->prevfree) {
                        b = 0;
                        break;
                    }
                    if (b == c)
                        break;
                }
            } else
                b = heap->base + b->nextfree;  /* NB: must not move */
        }
        if (!c  ||  !b  ||  b != c) {
            if (c) {
                sprintf(msg, " @%u/%u (%u, <-%u, %u->)",
                         HEAP_INDEX(c, heap->base), heap->size,
                         c->head.size, c->prevfree, c->nextfree);
            } else
                *msg = '\0';
            CORE_LOGF_X(21, eLOG_Error,
                        ("Heap Walk%s: Free list %s%s", s_HEAP_Id(_id, heap),
                         b  &&  c ? "broken" : "corrupt", msg));
            return 0;
        }
    }

    if (HEAP_ISUSED(b)  &&  heap->chunk/*RW heap*/) {
        const SHEAP_HeapBlock* c = heap->base + heap->free;
        /* check that a used block is not within the chain of free
           blocks but ignore any inconsistencies in the chain */
        for (i = 0;  c < e  &&  i < heap->size;  ++i) {
            if (!HEAP_ISFREE(c))
                break;
            if (c <= b  &&  b < HEAP_NEXT(c)) {
                CORE_LOGF_X(20, eLOG_Error,
                            ("Heap Walk%s: Used block @%u within"
                             " a free one @%u", s_HEAP_Id(_id, heap),
                             HEAP_INDEX(b, heap->base),
                             HEAP_INDEX(c, heap->base)));
                return 0;
            }
            if (c == heap->base + c->nextfree)
                break;
            c = heap->base + c->nextfree;
            if (c == heap->base + heap->free)
                break;
        }
    }

    /* Block 'b' seems okay for walking onto, but... */
    if (p) {
        if (HEAP_ISLAST(p)) {
            CORE_LOGF_X(22, eLOG_Error,
                        ("Heap Walk%s: Last block @%u", s_HEAP_Id(_id,heap),
                         HEAP_INDEX(p, heap->base)));
        } else if (HEAP_ISFREE(p)  &&  HEAP_ISFREE(b)) {
            const SHEAP_HeapBlock* c = heap->base;
            while (c < p) {
                if (HEAP_ISFREE(c)  &&  n <= HEAP_NEXT(c))
                    break;
                c = HEAP_NEXT(c);
            }
            if (p <= c) {
                CORE_LOGF_X(24, eLOG_Error,
                            ("Heap Walk%s: Adjacent free blocks"
                             " @%u and @%u", s_HEAP_Id(_id, heap),
                             HEAP_INDEX(p, heap->base),
                             HEAP_INDEX(b, heap->base)));
            }
        }
    }

    return &b->head;
}


SHEAP_Block* HEAP_Walk(const HEAP heap, const SHEAP_Block* ptr)
{
    if (!heap) {
        CORE_LOG_X(29, eLOG_Warning, "Heap Walk: NULL heap");
        return 0;
    }
    assert(!heap->base == !heap->size);

    if (s_HEAP_fast) {
        SHEAP_HeapBlock* b;
        if (!ptr)
            return &heap->base->head;
        b = HEAP_NEXT((SHEAP_HeapBlock*) ptr);
        return ptr < &b->head  &&  b < heap->base + heap->size ? &b->head : 0;
    }
    return s_HEAP_Walk(heap, ptr);
}


SHEAP_Block* HEAP_Next(const HEAP heap, const SHEAP_Block* ptr)
{
    SHEAP_Block* next;
    for (next = HEAP_Walk(heap, ptr);  next;  next = HEAP_Walk(heap, next)) {
        if ((short) next->flag) /*FIXME; now for compatibility*/
            break;
    }
    return next;
}


HEAP HEAP_Copy(const HEAP heap, size_t extra, int serial)
{
    HEAP       copy;
    TNCBI_Size size;

    assert(_HEAP_ALIGN_EX(1, sizeof(SHEAP_Block)) ==
           _HEAP_ALIGN   (1, sizeof(SHEAP_Block)));

    if (!heap)
        return 0;
    assert(!heap->base == !heap->size);

    size = HEAP_EXTENT(heap->size);
    copy = (HEAP) malloc(sizeof(*copy) +
                         (size ? sizeof(SHEAP_Block) - 1 + size : 0) + extra);
    if (!copy)
        return 0;
    if (size) {
        char* base = (char*) copy + sizeof(*copy);
        base += _HEAP_ALIGN(base, sizeof(SHEAP_Block)) - (unsigned long) base;
        assert(_HEAP_ALIGN(base, sizeof(SHEAP_Block)) == (unsigned long) base);
        copy->base = (SHEAP_HeapBlock*) base;
    } else
        copy->base = 0;
    copy->size   = heap->size;
    copy->free   = heap->size;
    copy->last   = heap->size;
    copy->chunk  = 0/*read-only*/;
    copy->resize = 0;
    copy->auxarg = 0;
    copy->refcnt = 1/*copy*/;
    copy->serial = serial;
    if (size) {
        memcpy(copy->base, heap->base, size);
        assert(memset((char*) copy->base + size, 0, extra));
    }
    return copy;
}


unsigned int HEAP_AddRef(HEAP heap)
{
    if (!heap)
        return 0;
    assert(!heap->base == !heap->size);
    if (heap->refcnt) {
        heap->refcnt++;
        assert(heap->refcnt);
    }
    return heap->refcnt;
}


unsigned int HEAP_Detach(HEAP heap)
{
    if (!heap)
        return 0;
    assert(!heap->base == !heap->size);
    if (heap->refcnt  &&  --heap->refcnt)
        return heap->refcnt;
    memset(heap, 0, sizeof(*heap));
    free(heap);
    return 0;
}


unsigned int HEAP_Destroy(HEAP heap)
{
    char _id[32];

    if (!heap)
        return 0;
    assert(!heap->base == !heap->size);
    if (!heap->chunk  &&  !heap->refcnt) {
        CORE_LOGF_X(33, eLOG_Error,
                    ("Heap Destroy%s: Heap read-only", s_HEAP_Id(_id, heap)));
    } else if (heap->resize/*NB: NULL for heap copies*/)
        verify(heap->resize(heap->base, 0, heap->auxarg) == 0);
    return HEAP_Detach(heap);
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
    return HEAP_EXTENT(heap->size);
}


int HEAP_Serial(const HEAP heap)
{
    if (!heap)
        return 0;
    assert(!heap->base == !heap->size);
    return heap->serial;
}


/*ARGSUSED*/
void HEAP_Options(ESwitch fast, ESwitch ignored)
{
    switch (fast) {
    case eOff:
        s_HEAP_fast = 0/*false*/;
        break;
    case eOn:
        s_HEAP_fast = 1/*true*/;
        break;
    default:
        break;
    }
}
