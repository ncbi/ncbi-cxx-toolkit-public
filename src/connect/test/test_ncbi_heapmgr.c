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
 * --------------------------------------------------------------------------
 * $Log$
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

#include <connect/ncbi_heapmgr.h>
#include "../ncbi_misc.h"
#include <stdlib.h>
#include <time.h>
#include <unistd.h>


#ifdef __cplusplus
extern "C" {
#endif

static char *s_Expand(char *base, size_t size)
{
    if (base && size)
        return (char *)realloc(base, size);
    else if (size)
        return (char *)malloc(size);
    else
        free(base);
    return 0;
}

#ifdef __cplusplus
} /* extern "C" */
#endif


int main(void)
{
    SHEAP_Block *blk;
    int r, j, i;
    HEAP heap;
    char *c;

    for (j = 1; j <= 3; j++) {
        srand((int)time(0) + (int)getpid());
        Message("Creating heap %d\n", j);
        heap = HEAP_Create(0, 0, getpagesize(), s_Expand);
        while (rand() != 12345) {
            r = rand() & 7;
            if (r == 2 || r == 4) {
                i = rand() & 0xFF;
                if (i) {
                    Message("Allocating %d bytes", i);
                    if (!(blk = HEAP_Alloc(heap, i)))
                        Warning(0, "Allocation failed");
                    else
                        Message("Done\n");
                    c = (char *)blk + sizeof(*blk);
                    while (i--)
                        *c++ = rand();
                }
            } else if (r == 1 || r == 3 || r == 5) {
                blk = 0;
                i = 0;
                do {
                    blk = HEAP_Walk(heap, blk);
                    if (!blk)
                        break;
                    i++;
                } while (rand() & 0x7);
                if (blk && (short)blk->flag) {
                    Message("Deleting block #%d, size %u", i,
                            (unsigned)(blk->size - sizeof(*blk)));
                    HEAP_Free(heap, blk);
                    Message("Done\n");
                }
            } else if (r == 7) {
                blk = 0;
                i = 0;
                Message("Walking the heap");
                do {
                    blk = HEAP_Walk(heap, blk);
                    if (blk)
                        Message("Block #%d (%s), size %u", ++i,
                                (short)blk->flag ? "used" : "free",
                                (unsigned)(blk->size - sizeof(*blk)));
                } while (blk);
                Message("Total of %d block%s\n", i, i == 1 ? "" : "s");
            } else if (r == 6) {
                HEAP newheap = HEAP_Attach(/* HACK! */*(char **)heap);

                blk = 0;
                i = 0;
                Message("Walking the newheap");
                do {
                    blk = HEAP_Walk(newheap, blk);
                    if (blk)
                        Message("Block #%d (%s), size %u", ++i,
                                (short)blk->flag ? "used" : "free",
                                (unsigned)(blk->size - sizeof(*blk)));
                } while (blk);
                Message("Total of %d block%s\n", i, i == 1 ? "" : "s");
                HEAP_Detach(newheap);
            }
        }
        HEAP_Destroy(heap);
        Message("Heap %d done\n", j);
    }
    Message("Test completed");
    return 0;
}
