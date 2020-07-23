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
 * File Description:
 *   Simple random test of heap manager API. Not an interface example!
 *
 */

#include <connect/ncbi_heapmgr.h>
#include "../ncbi_priv.h"               /* CORE logging facilities */
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "test_assert.h"  /* This header must go last */

#define HEAP_ADDR(b, h) ((unsigned int)((char*) b - (char*) HEAP_Base(h)) >> 4)


#ifdef __cplusplus
extern "C" {
#endif

/*ARGSUSED*/
static void* s_Expand(void* base, TNCBI_Size size, void* arg)
{
    if (base  &&  size) {
        CORE_LOGF(eLOG_Note, ("Resizing heap, size %u", size));
        return realloc(base, size);
    }
    if (size) {
        CORE_LOGF(eLOG_Note, ("Allocating heap, size %u", size));
        return malloc(size);
    }
    if (base) {
        CORE_LOG(eLOG_Note, "Freeing heap");
        free(base);
    } else
        CORE_LOG(eLOG_Note, "NULL heap op");
    return 0;
}

#ifdef __cplusplus
} /* extern "C" */
#endif


static void s_Walk(HEAP heap, const char* which, int/*bool*/ check)
{
    unsigned int n = 0;
    SHEAP_Block* blk = 0;
    TNCBI_Size free_size[2];
    TNCBI_Size total_size[2];
    CORE_LOGF(eLOG_Note,
              ("Walking %s%sheap",
               which  &&  *which ? which : "", &" "[!which  ||  !*which]));
    free_size[0] = total_size[0] = 0;
    while ((blk = HEAP_Walk(heap, blk)) != 0) {
        const char* flag = (int) blk->flag < 0 ? ", last" : "";
        TNCBI_Size size = blk->size;
        if (blk->flag & 1) {
            TNCBI_Size data_size = size - sizeof(*blk);
            CORE_LOGF(eLOG_Note,
                      ("Used%s @%u, size %u, data size %u",
                       flag, HEAP_ADDR(blk, heap), size, data_size));
            assert(data_size);
        } else {
            unsigned int* ptr = (unsigned int*) blk;
            CORE_LOGF(eLOG_Note,
                      ("Free%s @%u, size %u, <-%u, %u->",
                       flag, HEAP_ADDR(blk, heap), size, ptr[2], ptr[3]));
            free_size[0] += size;
        }
        total_size[0] += size;
        ++n;
    }
    free_size[1] = HEAP_Idle(heap);
    total_size[1] = HEAP_Size(heap);
    CORE_LOGF(eLOG_Note,
              ("%d block%s: free %u/%u, total %u/%u", n, &"s"[n == 1],
               free_size[0], free_size[1], total_size[0], total_size[1]));
    if (check) {
        if (free_size[0] != free_size[1])
            CORE_LOG(eLOG_Fatal, "Free size mismatch");
        if (total_size[0] - free_size[0] != HEAP_Used(heap))
            CORE_LOG(eLOG_Fatal, "Used size mismatch");
    }
    if (total_size[0] != total_size[1])
        CORE_LOG(eLOG_Fatal, "Heap size mismatch");
}


int main(int argc, const char* argv[])
{
    SHEAP_Block* blk;
    int r, j, i, n;
    HEAP heap;
    char* c;

    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    /* the first init needed to clear up uninited mutex, if any */
    CORE_SetLOGFILE_Ex(stderr, eLOG_Trace, eLOG_Fatal, 0/*false*/);
    CORE_SetLOGFILE_Ex(stderr, eLOG_Trace, eLOG_Warning, 0/*false*/);
    if (argc > 1)
        g_NCBI_ConnectRandomSeed = (unsigned int) atoi(argv[1]);
    else {
        g_NCBI_ConnectRandomSeed
            = (unsigned int) time(0) ^ NCBI_CONNECT_SRAND_ADDEND;
    }
    CORE_LOGF(eLOG_Note, ("Using seed %d", g_NCBI_ConnectRandomSeed));
    HEAP_Options(eOff/*slow*/, eDefault);
    srand(g_NCBI_ConnectRandomSeed);
    for (j = 1;  j <= 3;  j++) {
        CORE_LOGF(eLOG_Note, ("Creating heap %d", j));
        if (!(heap = HEAP_Create(0, 0, 128, s_Expand, 0)))
            CORE_LOG(eLOG_Error, "Cannot create heap");
        for (n = 0;  n < 1000  && (rand() & 0xFFFF) != 0x1234;  n++) {
            r = rand() & 7;
            if (r == 1  ||  r == 2) {
                int/*bool*/ tail = rand() & 1;
                i = rand() & 0xFF;
                if (i) {
                    CORE_LOGF(eLOG_Note,
                              ("Allocating %s data size %d",
                               tail ? "tail" : "head", i));
                    blk = HEAP_Alloc(heap, i, tail);
                    if (blk) {
                        CORE_LOGF(eLOG_Note,
                                  ("Done @%u, size %u",
                                   HEAP_ADDR(blk, heap), blk->size));
                    } else
                        CORE_LOG(eLOG_Fatal, "Allocation failed");
                    c = (char*) blk + sizeof(*blk);
                    while (i--)
                        *c++ = rand();
                    s_Walk(heap, 0, 1);
                } else
                    assert(!HEAP_Alloc(heap, i, tail));
            } else if (r == 3  ||  r == 4) {
                blk = 0;
                do {
                    if (!(blk = HEAP_Walk(heap, blk)))
                        break;
                } while (rand() & 7);
                if (blk  &&  (blk->flag & 1)) {
                    unsigned size = blk->size;
                    unsigned data_size = size - sizeof(*blk);
                    CORE_LOGF(eLOG_Note,
                              ("Freeing @%u, size %u, data size %u",
                               HEAP_ADDR(blk, heap), size, data_size));
                    assert(data_size);
                    HEAP_Free(heap, blk);
                    CORE_LOG(eLOG_Note, "Done");
                    s_Walk(heap, 0, 1);
                }
            } else if (r == 5) {
                const SHEAP_Block* prev = 0;
                unsigned ok = 0;
                blk = 0;
                for (;;) {
                    if (!(blk = HEAP_Walk(heap, blk)))
                        break;
                    if ((blk->flag & 1)  &&  (rand() & 7)) {
                        char buf[32];
                        unsigned size = blk->size;
                        int/*bool*/ fast = rand() & 1;
                        unsigned data_size = size - sizeof(*blk);
                        if (!fast  ||  !prev)
                            *buf = '\0';
                        else
                            sprintf(buf, ", prev @%u", HEAP_ADDR(prev, heap));
                        CORE_LOGF(eLOG_Note,
                                  ("Freeing%s%s @%u%s in walk,"
                                   " size %u, data size %u",
                                   fast ? " fast" : "", ok ? " more" : "",
                                   HEAP_ADDR(blk,heap), buf, size, data_size));
                        assert(data_size);
                        if (fast)
                            HEAP_FreeFast(heap, blk, prev);
                        else
                            HEAP_Free(heap, blk);
                        CORE_LOG(eLOG_Note, "Done");
                        s_Walk(heap, 0, 1);
                        ok = 1;
                        if (prev  &&  !(prev->flag & 1))
                            continue;
                    }
                    prev = blk;
                }
                if (!ok)
                    s_Walk(heap, "the", 1);
                else
                    CORE_LOG(eLOG_Note, "Done with freeing while walking");
            } else if (r == 6  ||  r == 7) {
                HEAP newheap;

                if (r == 6) {
                    int/*bool*/ fast = rand() & 1;
                    if (fast) {
                        CORE_LOG(eLOG_Note, "Attaching heap fast");
                        newheap = HEAP_AttachFast(HEAP_Base(heap),
                                                  HEAP_Size(heap), 0);
                    } else {
                        CORE_LOG(eLOG_Note, "Attaching heap");
                        newheap = HEAP_Attach(HEAP_Base(heap), 0, 0);
                    }
                } else {
                    CORE_LOG(eLOG_Note, "Copying heap");
                    newheap = HEAP_Copy(heap, 0, 0);
                }

                if (!newheap) {
                    CORE_LOGF(eLOG_Error,
                              ("%s failed", r == 6 ? "Attach" : "Copy"));
                } else
                    s_Walk(newheap, r == 6 ? "attached" : "copied", !(r == 6));
                HEAP_Detach(newheap);
            } else {
                TNCBI_Size size = HEAP_Size(heap);
                HEAP newheap;

                CORE_LOG(eLOG_Note, "Trimming heap");
                newheap = HEAP_Trim(heap);
                CORE_LOGF(newheap ? eLOG_Note : eLOG_Error,
                          ("Heap %strimmed: %u -> %u", newheap ? "" : "NOT ",
                           size, HEAP_Size(newheap)));
                if (newheap) {
                    heap = newheap;
                    s_Walk(heap, "trimmed", 1);
                }
            }
        }
        HEAP_Destroy(heap);
        CORE_LOGF(eLOG_Note, ("Heap %d done", j));
    }
    CORE_LOG(eLOG_Note, "TEST completed successfully");
    CORE_SetLOG(0);
    return 0;
}
