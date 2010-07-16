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

/** @file blast_hspstream.h
 * Declaration of ADT to save and retrieve lists of HSPs in the BLAST engine.
 */

#ifndef ALGO_BLAST_CORE__BLAST_HSPSTREAM_H
#define ALGO_BLAST_CORE__BLAST_HSPSTREAM_H

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/core/blast_export.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/split_query.h>
#include <algo/blast/core/blast_program.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_hspfilter.h>
#include <connect/ncbi_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Auxiliary structure to allow sorting the results by score for the
 * composition-based statistics code */
typedef struct SSortByScoreStruct {
    Boolean sort_on_read;    /**< Should the results be sorted on the first read
                               call? */
    Int4 first_query_index;  /**< Index of the first query to try getting
                               results from */
} SSortByScoreStruct;

/** structure used to hold a collection of hits
    retrieved from the HSPStream */
typedef struct BlastHSPStreamResultBatch {
    Int4 num_hsplists;          /**< number of lists of HSPs returned */
    BlastHSPList **hsplist_array;  /**< array of HSP lists returned */
} BlastHSPStreamResultBatch;

/** create a new batch to hold HSP results
 * @param num_hsplists Maximum number of results to hold
 * @return Pointer to newly allocated structure
 */
NCBI_XBLAST_EXPORT
BlastHSPStreamResultBatch * Blast_HSPStreamResultBatchInit(
                                            Int4 num_hsplists);

/** free a batch of HSP results. Note that the HSPLists
 * themselves are not freed
 * @param batch Structure to free
 * @return Always NULL
 */
NCBI_XBLAST_EXPORT
BlastHSPStreamResultBatch * Blast_HSPStreamResultBatchFree(
                                  BlastHSPStreamResultBatch *batch);

/** free the list of HSPLists within a batch
 * @param batch Structure to reset
 */
NCBI_XBLAST_EXPORT
void Blast_HSPStreamResultBatchReset(BlastHSPStreamResultBatch *batch);

/** Default implementation of BlastHSPStream */
typedef struct BlastHSPStream {
   EBlastProgramType program;           /**< BLAST program type */
   Int4 num_hsplists;          /**< number of HSPlists saved */
   Int4 num_hsplists_alloc;    /**< number of entries in sorted_hsplists */
   BlastHSPList **sorted_hsplists; /**< list of all HSPlists from 'results'
                                       combined, sorted in order of
                                       decreasing subject OID */
   BlastHSPResults* results;/**< Structure for saving HSP lists */
   Boolean results_sorted;  /**< Have the results already been sorted?
                               Set to true after the first read call. */
   /**< Non-NULL if the results should be sorted by score as opposed to subject
    * OID. This is necessary to meet a pre-condition of the composition-based
    * statistics processing */
   SSortByScoreStruct* sort_by_score;
   MT_LOCK x_lock;   /**< Mutex for writing and reading results. */
   /* support for writer and pipes */
   BlastHSPWriter* writer;         /**< writer to be applied when writing*/
   Boolean writer_initialized;     /**< Is writer already initialized? */
   Boolean writer_finalized;       /**< Is writer ever finalized? */
   BlastHSPPipe *pre_pipe;         /**< registered preliminary pipeline (unused
                                    for now) */
   BlastHSPPipe *tback_pipe;       /**< registered traceback pipeline */
} BlastHSPStream;

/*****************************************************************************/
/** Initialize the HSP stream. 
 * @param program Type of BlAST program [in]
 * @param extn_opts Extension options to determine composition-based statistics
 * mode [in]
 * @param sort_on_read Should results be sorted on the first read call? Only
 * applicable if composition-based statistics is on [in]
 * @param num_queries Number of query sequences in this BLAST search [in]
 * @param writer Writer to be registered [in]
 */
NCBI_XBLAST_EXPORT
BlastHSPStream* BlastHSPStreamNew(EBlastProgramType program,
                             const BlastExtensionOptions* extn_opts,
                             Boolean sort_on_read,
                             Int4 num_queries,
                             BlastHSPWriter* writer);

/** Frees the BlastHSPStream structure by invoking the destructor function set
 * by the user-defined constructor function when the structure is initialized
 * (indirectly, by BlastHSPStreamNew). If the destructor function pointer is not
 * set, a memory leak could occur.
 * @param hsp_stream BlastHSPStream to free [in]
 * @return NULL
 */
NCBI_XBLAST_EXPORT
BlastHSPStream* BlastHSPStreamFree(BlastHSPStream* hsp_stream);

/** Closes the BlastHSPStream structure for writing. Any subsequent attempt
 * to write to the stream will return error.
 * @param hsp_stream The stream to close [in] [out]
 */
NCBI_XBLAST_EXPORT
void BlastHSPStreamClose(BlastHSPStream* hsp_stream);

/** Closes the BlastHSPStream structure after traceback. 
 * This is mainly to provide a chance to apply post-traceback pipes.
 * @param hsp_stream The stream to close [in] [out]
 * @param results The traceback results [in] [out]
 */
NCBI_XBLAST_EXPORT
void BlastHSPStreamTBackClose(BlastHSPStream* hsp_stream,
                              BlastHSPResults* results);

/** Moves the HSPlists from an HSPStream into the list contained
 * by a second HSPStream
 * @param squery_blk Information needed to map HSPs from one HSPstream
 *                   to the combined HSPstream [in]
 * @param chunk_num Used to choose a subset of the information in
 *                  squery_blk [in]
 * @param hsp_stream The stream to merge [in][out]
 * @param combined_hsp_stream The stream that will contain the
 *         HSPLists of the first stream [in][out]
 */
NCBI_XBLAST_EXPORT
int BlastHSPStreamMerge(SSplitQueryBlk* squery_blk,
                        Uint4 chunk_num,
                        BlastHSPStream* hsp_stream,
                        BlastHSPStream* combined_hsp_stream);

/** Standard error return value for BlastHSPStream methods */
NCBI_XBLAST_EXPORT
extern const int kBlastHSPStream_Error;

/** Standard success return value for BlastHSPStream methods */
NCBI_XBLAST_EXPORT
extern const int kBlastHSPStream_Success;

/** Return value when the end of the stream is reached (applicable to read
 * method only) */
NCBI_XBLAST_EXPORT
extern const int kBlastHSPStream_Eof;

/** Invokes the user-specified write function for this BlastHSPStream
 * implementation.
 * @param hsp_stream The BlastHSPStream object [in]
 * @param hsp_list List of HSPs for the HSPStream to keep track of. The caller
 * releases ownership of the hsp_list [in]
 * @return kBlastHSPStream_Success on success, otherwise kBlastHSPStream_Error
 */
NCBI_XBLAST_EXPORT
int BlastHSPStreamWrite(BlastHSPStream* hsp_stream, BlastHSPList** hsp_list);

/** Invokes the user-specified read function for this BlastHSPStream
 * implementation.
 * @param hsp_stream The BlastHSPStream object [in]
 * @param hsp_list List of HSPs for the HSPStream to return. The caller
 * acquires ownership of the hsp_list [out]
 * @return kBlastHSPStream_Success on success, kBlastHSPStream_Error, or
 * kBlastHSPStream_Eof on end of stream
 */
NCBI_XBLAST_EXPORT
int BlastHSPStreamRead(BlastHSPStream* hsp_stream, BlastHSPList** hsp_list);

/** Invokes the user-specified batch read function for this BlastHSPStream
 * implementation.
 * @param hsp_stream The BlastHSPStream object [in]
 * @param batch List of HSP listss for the HSPStream to return. The caller
 * acquires ownership of all HSP lists returned [out]
 * @return kBlastHSPStream_Success on success, kBlastHSPStream_Error, or
 * kBlastHSPStream_Eof on end of stream
 */
NCBI_XBLAST_EXPORT
int BlastHSPStreamBatchRead(BlastHSPStream* hsp_stream,
                            BlastHSPStreamResultBatch* batch);

/** Attach a mutex lock to a stream to protect multiple access during writing
 * @param hsp_stream  The stream to attach [in]
 * @param lock        Pointer to locking structure for writing by multiple
 *                    threads. Locking will not be performed if NULL. [in]
 */
NCBI_XBLAST_EXPORT
int BlastHSPStreamRegisterMTLock(BlastHSPStream* hsp_stream,
                                 MT_LOCK lock);

/** Insert the user-specified pipe to the *end* of the pipeline.
 * @param hsp_stream The BlastHSPStream object [in]
 * @param pipe The pipe to be registered [in]
 * @param stage At what stage should this pipeline be applied [in]
 */
NCBI_XBLAST_EXPORT
int BlastHSPStreamRegisterPipe(BlastHSPStream* hsp_stream,
                               BlastHSPPipe* pipe,
                               EBlastStage stage);

#ifdef __cplusplus
}
#endif

#endif /* ALGO_BLAST_CORE__BLAST_HSPSTREAM_H */
