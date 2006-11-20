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
 * File Description:
 *   Simple random test of heap manager API. Not an interface example!
 *
 */

#include "../ncbi_priv.h"
#include <connect/ncbi_heapmgr.h>
#include <stdlib.h>
#include <time.h>
#if 0
#  define eLOG_Warning eLOG_Fatal
#  define eLOG_Error   eLOG_Fatal
#  include "../ncbi_heapmgr.c"
#endif
/* This header must go last */
#include "test_assert.h"


#define HEAP_ADDR(b, h) ((unsigned int)((char*) b - (char*) HEAP_Base(h)) >> 4)


#ifdef __cplusplus
extern "C" {
#endif

/*ARGSUSED*/
static void* s_Expand(void* base, TNCBI_Size size, void* arg)
{
    if (base  &&  size)
        return realloc(base, size);
    if (size)
        return malloc(size);
    if (base)
        free(base);
    return 0;
}

#ifdef __cplusplus
} /* extern "C" */
#endif


static void s_Walk(HEAP heap, const char* which)
{
    unsigned int i = 0;
    SHEAP_Block* blk = 0;
    TNCBI_Size total = 0;
    CORE_LOGF(eLOG_Note,
              ("Walking %s%sheap",
               which  &&  *which ? which : "", &" "[!which  ||  !*which]));
    while ((blk = HEAP_Walk(heap, blk)) != 0) {
        const char* flag = (int) blk->flag < 0 ? ", last" : "";
        TNCBI_Size size = blk->size;
        if ((short) blk->flag) {
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
        }
        total += size;
        i++;
    }
    CORE_LOGF(eLOG_Note,
              ("%d block%s total; total size %u", i, &"s"[i == 1], total));
}


int main(int argc, const char* argv[])
{
    SHEAP_Block* blk;
    int r, j, i, n;
    HEAP heap;
    char* c;

    /* CORE_SetLOGFormatFlags(fLOG_None | fLOG_Level | fLOG_OmitNoteLevel); */
    CORE_SetLOGFILE(stderr, 0/*false*/);
    if (argc > 1)
        g_NCBI_ConnectRandomSeed = atoi(argv[1]);
    else
        g_NCBI_ConnectRandomSeed = (int) time(0) ^ NCBI_CONNECT_SRAND_ADDEND;
    CORE_LOGF(eLOG_Note, ("Using seed %d", g_NCBI_ConnectRandomSeed));
    srand(g_NCBI_ConnectRandomSeed);
    for (j = 1;  j <= 3;  j++) {
        CORE_LOGF(eLOG_Note, ("Creating heap %d", j));
        if (!(heap = HEAP_Create(0, 0, 4096, s_Expand, 0)))
            CORE_LOG(eLOG_Error, "Cannot create heap");
        for (n = 0; n < 1000 && (rand() & 0xFFFF) != 0x1234; n++) {
            r = rand() & 7;
            if (r == 1  ||  r == 3) {
                i = rand() & 0xFF;
                if (i) {
                    CORE_LOGF(eLOG_Note, ("Allocating %d data size", i));
                    blk = HEAP_Alloc(heap, i);
                    if (blk) {
                        CORE_LOGF(eLOG_Note,
                                  ("Done @%u, size %u",
                                   HEAP_ADDR(blk, heap), blk->size));
                    } else
                        CORE_LOG(eLOG_Error, "Allocation failed");
                    c = (char*) blk + sizeof(*blk);
                    while (i--)
                        *c++ = rand();
                    s_Walk(heap, 0);
                } else
                    assert(!HEAP_Alloc(heap, i));
            } else if (r == 2  ||  r == 4) {
                blk = 0;
                do {
                    if (!(blk = HEAP_Walk(heap, blk)))
                        break;
                } while (rand() & 7);
                if (blk  &&  (short) blk->flag) {
                    unsigned size = blk->size;
                    unsigned data_size = size - sizeof(*blk);
                    CORE_LOGF(eLOG_Note,
                              ("Freeing block @%u, size %u, data size %u",
                               HEAP_ADDR(blk, heap), size, data_size));
                    assert(data_size);
                    HEAP_Free(heap, blk);
                    CORE_LOG(eLOG_Note, "Done");
                    s_Walk(heap, 0);
                }
            } else if (r == 5) {
                unsigned ok = 0;
                blk = 0;
                for (;;) {
                    if (!(blk = HEAP_Walk(heap, blk)))
                        break;
                    if ((short) blk->flag  &&  (rand() & 7)) {
                        unsigned size = blk->size;
                        unsigned data_size = size - sizeof(*blk);
                        CORE_LOGF(eLOG_Note,
                                  ("Freeing block @%u while walking, size %u,"
                                   " data size %u", HEAP_ADDR(blk, heap),
                                   size, data_size));
                        assert(data_size);
                        HEAP_Free(heap, blk);
                        CORE_LOG(eLOG_Note, "Done");
                        s_Walk(heap, 0);
                        ok = 1;
                    }
                }
                if (!ok)
                    s_Walk(heap, "the");
            } else if (r == 6  ||  r == 7) {
                HEAP newheap;

                if (r == 6) {
                    CORE_LOG(eLOG_Note, "Attaching heap");
                    newheap = HEAP_Attach(HEAP_Base(heap), 0);
                } else {
                    CORE_LOG(eLOG_Note, "Copying heap");
                    newheap = HEAP_Copy(heap, 0, 0);
                }

                if (!newheap) {
                    CORE_LOGF(eLOG_Error,
                              ("%s failed", r == 6 ? "Attach" : "Copy"));
                } else
                    s_Walk(newheap, r == 6 ? "attached" : "copied");
                HEAP_Detach(newheap);
            } else {
                TNCBI_Size size = HEAP_Size(heap);
                HEAP newheap;

                CORE_LOG(eLOG_Note, "Trimming heap");
                newheap = HEAP_Trim(heap);
                CORE_LOGF(eLOG_Note,
                          ("Heap %strimmed: %u -> %u", newheap ? "" : "NOT ",
                           size, HEAP_Size(newheap)));
                if (newheap) {
                    heap = newheap;
                    s_Walk(heap, "trimmed");
                }
            }
        }
        HEAP_Destroy(heap);
        CORE_LOGF(eLOG_Note, ("Heap %d done", j));
    }
    CORE_LOG(eLOG_Note, "Test completed");
    CORE_SetLOG(0);
    return 0;
}


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.16  2006/11/20 16:42:31  lavr
 * Test extended to be more thorough
 *
 * Revision 6.15  2006/03/05 17:42:10  lavr
 * Adjust for revised API
 *
 * Revision 6.14  2005/07/11 18:24:46  lavr
 * Spell ADDEND
 *
 * Revision 6.13  2005/05/02 16:12:32  lavr
 * Use global random seed
 *
 * Revision 6.12  2003/08/25 14:58:10  lavr
 * Adjust test to take advantage of modified API
 *
 * Revision 6.11  2003/07/31 17:54:16  lavr
 * +HEAP_Trim() test
 *
 * Revision 6.10  2003/02/27 15:34:35  lavr
 * Log moved to end
 *
 * Revision 6.9  2002/04/15 19:21:44  lavr
 * +#include "../test/test_assert.h"
 *
 * Revision 6.8  2001/07/03 20:53:38  lavr
 * HEAP_Copy() test added
 *
 * Revision 6.7  2001/06/19 19:12:04  lavr
 * Type change: size_t -> TNCBI_Size; time_t -> TNCBI_Time
 *
 * Revision 6.6  2001/01/23 23:22:05  lavr
 * Patched logging (in a few places)
 *
 * Revision 6.5  2001/01/12 23:59:53  lavr
 * Message logging modified for use LOG facility only
 *
 * Revision 6.4  2000/12/29 18:23:42  lavr
 * getpagesize() replaced by a constant 4096, which is "more portable".
 *
 * Revision 6.3  2000/05/31 23:12:32  lavr
 * First try to assemble things together to get working service mapper
 *
 * Revision 6.2  2000/05/16 15:21:03  lavr
 * Cleaned up with format - argument correspondence; #include <time.h> added
 *
 * Revision 6.1  2000/05/12 19:35:13  lavr
 * First working revision
 *
 * ==========================================================================
 */
