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

/** @file blast_hspstream_mt_utils.h
 *  Private interfaces to support the multi-threaded traceback in conjunction
 *  with the BlastHSPStream
 */

#ifndef ALGO_BLAST_CORE___BLAST_HSPSTREAM_MT_UTILS_PRIV__H
#define ALGO_BLAST_CORE___BLAST_HSPSTREAM_MT_UTILS_PRIV__H

#include <algo/blast/core/blast_hspstream.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Structure to extract the contents of the BlastHSPStream for MT traceback
 * processing */
typedef struct BlastHSPStreamResultsBatchArray {
    /** Array of batches, each corresponding to a single OID with BLAST hits */
    BlastHSPStreamResultBatch** array_of_batches;
    /** number of batches populated in the array_of_batches element */
    Uint4 num_batches;
    /** number of batches allocated in the array_of_batches element */
    Uint4 num_allocated;
} BlastHSPStreamResultsBatchArray;

/** Extracts all data from the BlastHSPStream into its output parameters.
 * @param hsp_stream The BlastHSPStream object [in]
 * @param batches Each batch contains the results for a single OID [in|out] 
 * kBlastHSPStream_Success on successful conversion, otherwise an error code which explains the problem.
 */
NCBI_XBLAST_EXPORT
int BlastHSPStreamToHSPStreamResultsBatch(BlastHSPStream* hsp_stream,
                                          BlastHSPStreamResultsBatchArray** batches);

/** Releases memory acquired in BlastHSPStreamToHSPStreamResultsBatch
 */
NCBI_XBLAST_EXPORT
BlastHSPStreamResultsBatchArray*
BlastHSPStreamResultsBatchArrayFree(BlastHSPStreamResultsBatchArray* batches);


#ifdef __cplusplus
}
#endif

#endif /* !ALGO_BLAST_CORE__BLAST_HSPSTREAM_MT_UTILS_PRIV__H */
