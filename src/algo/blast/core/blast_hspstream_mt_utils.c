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
 * Author:  Christiam Camacho
 *
 */

/** @file blast_hspstream_mt_utils.c
 *  Private interfaces to support the multi-threaded traceback in conjunction
 *  with the BlastHSPStream
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include "blast_hspstream_mt_utils.h"

/** Reset and free the BlastHSPStreamResultBatch structures contained in the
 * object manipulated by this function
 * @param it object to operate on [in|out]
 */
static void
s_BlastHSPStreamResultsBatchArrayReset(BlastHSPStreamResultsBatchArray* it)
{
    Uint4 i;
    if ( !it ) {
        return;
    }

    for (i = 0; i < it->num_batches; i++) {
        it->array_of_batches[i] = Blast_HSPStreamResultBatchReset(it->array_of_batches[i]);
        it->array_of_batches[i] = Blast_HSPStreamResultBatchFree(it->array_of_batches[i]);
    }
    it->num_batches = 0;
}

BlastHSPStreamResultsBatchArray*
BlastHSPStreamResultsBatchArrayFree(BlastHSPStreamResultsBatchArray* it)
{
    if (it) {
        s_BlastHSPStreamResultsBatchArrayReset(it);
        if (it->array_of_batches)
            sfree(it->array_of_batches);
        sfree(it);
    }
    return NULL;
}

/** Allocate a new BlastHSPStreamResultsBatchArray with the specified capacity
 * @param num_elements capacity for the allocated structure [in]
 */
static BlastHSPStreamResultsBatchArray*
s_BlastHSPStreamResultsBatchArrayNew(Uint4 num_elements)
{
    BlastHSPStreamResultsBatchArray* retval = NULL;

    retval = (BlastHSPStreamResultsBatchArray*)
        calloc(1, sizeof(BlastHSPStreamResultsBatchArray));
    if ( !retval ) {
        return BlastHSPStreamResultsBatchArrayFree(retval);
    }
    num_elements = !num_elements ? 10 : num_elements;
    retval->array_of_batches = (BlastHSPStreamResultBatch**)
        calloc(num_elements, sizeof(*retval->array_of_batches));
    if ( !retval->array_of_batches ) {
        return BlastHSPStreamResultsBatchArrayFree(retval);
    }
    retval->num_batches = 0;
    retval->num_allocated = num_elements;
    return retval;
}

/** Append a value to the array, reallocating it if necessary
 * @param batches array to append data to [in|out]
 * @param batch value to append to array [in]
 * @return 0 on success, otherwise non-zero */
static int
s_BlastHSPStreamResultsBatchArrayAppend(BlastHSPStreamResultsBatchArray* batches, 
                                        BlastHSPStreamResultBatch* batch)
{
    const size_t kResizeFactor = 2;

    if ( !batches || !batch ) {
        return BLASTERR_INVALIDPARAM;
    }

    if ((batches->num_batches+1) > batches->num_allocated) {
        /* re-allocate */
        BlastHSPStreamResultBatch** reallocation = (BlastHSPStreamResultBatch**)
            realloc(batches->array_of_batches,
                    kResizeFactor * batches->num_allocated *
                    sizeof(*batches->array_of_batches));
        if ( !reallocation ) {
            return BLASTERR_MEMORY;
        }
        batches->num_allocated *= kResizeFactor;
        batches->array_of_batches = reallocation;
    }
    batches->array_of_batches[batches->num_batches] = batch;
    batches->num_batches++;
    return 0;
}

static Uint4
s_BlastHSPStreamCountNumOids(const BlastHSPStream* hsp_stream)
{
    Uint4 retval = 0;
    if (hsp_stream && hsp_stream->num_hsplists > 0) {
        Int4 current_oid = -1, i;
        for (i = hsp_stream->num_hsplists - 1; i >= 0; i--) {
            if (current_oid != hsp_stream->sorted_hsplists[i]->oid) {
                current_oid = hsp_stream->sorted_hsplists[i]->oid;
                retval++;
            }
        }
    }
    return retval;
}

int BlastHSPStreamToHSPStreamResultsBatch(BlastHSPStream* hsp_stream,
                                          BlastHSPStreamResultsBatchArray** batches)
{
    BlastHSPStreamResultBatch *batch = NULL;

    if (!batches || !hsp_stream) {
        return BLASTERR_INVALIDPARAM;
    }

    *batches =
        s_BlastHSPStreamResultsBatchArrayNew(s_BlastHSPStreamCountNumOids(hsp_stream));
    if ( !*batches ) {
        return BLASTERR_MEMORY;
    }

    for (batch = Blast_HSPStreamResultBatchInit(hsp_stream->results->num_queries);
         BlastHSPStreamBatchRead(hsp_stream, batch) != kBlastHSPStream_Eof;
         batch = Blast_HSPStreamResultBatchInit(hsp_stream->results->num_queries)) {
        if (s_BlastHSPStreamResultsBatchArrayAppend(*batches, batch) != 0) {
            s_BlastHSPStreamResultsBatchArrayReset(*batches);
            *batches = BlastHSPStreamResultsBatchArrayFree(*batches);
            return BLASTERR_MEMORY;
        }
    }
    batch = Blast_HSPStreamResultBatchFree(batch);
    return kBlastHSPStream_Success;
}
