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

/** @file blast_traceback_mt_priv.h
 *  Private interface to support the multi-threaded traceback
 */

#ifndef ALGO_BLAST_CORE___BLAST_TRACEBACK_MT_PRIV__H
#define ALGO_BLAST_CORE___BLAST_TRACEBACK_MT_PRIV__H

#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/blast_hspstream.h>
#include <algo/blast/core/blast_parameters.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Data structure to support MT traceback: this encapsulates the data that
 * each thread modifies */
typedef struct SThreadLocalData {
    /*** Gapped alignment structure */
    BlastGapAlignStruct* gap_align;
    /** Scoring parameters, allocated anew in BLAST_GapAlignSetUp */
    BlastScoringParameters* score_params; 
    /** Gapped extension parameters, allocated anew in BLAST_GapAlignSetUp */
    BlastExtensionParameters* ext_params; 

    /* The 3 fields below are actually modified for bl2seq */
    /** Hit saving parameters*/
    BlastHitSavingParameters* hit_params;
    /** Parameters for effective lengths calculations */
    BlastEffectiveLengthsParameters* eff_len_params;
    /** The effective search space is modified */
    BlastQueryInfo* query_info;

    /** BlastSeqSrc so that each thread can set its own partial fetching */
    BlastSeqSrc* seqsrc;
    /** Structure to store results from this thread */
    BlastHSPResults* results;
} SThreadLocalData;

/** Allocate a new SThreadLocalData structure
 * @return NULL in case of memory allocation failure, otherwise a pointer to
 * the new structure
 */
NCBI_XBLAST_EXPORT
SThreadLocalData* SThreadLocalDataNew();

/** Deallocate the SThreadLocalData structure passed in
 * @param tld structure to deallocate [in]
 * @return NULL
 */
NCBI_XBLAST_EXPORT
SThreadLocalData* SThreadLocalDataFree(SThreadLocalData* tld);

/*** Array of data structures used by each thread */
typedef struct SThreadLocalDataArray {
    SThreadLocalData** tld; /***< Thread local data */
    Uint4 num_elems;        /***< This is also the number of threads */
} SThreadLocalDataArray;

/** Allocate a new SThreadLocalDataArray structure
 * @param num_threads size of the array structure to allocate [in]
 * @return NULL in case of memory allocation failure, otherwise a pointer to
 * the new structure
 */
NCBI_XBLAST_EXPORT
SThreadLocalDataArray* SThreadLocalDataArrayNew(Uint4 num_threads);

/** Deallocate the SThreadLocalDataArray structure passed in
 * @param array structure to deallocate [in]
 * @return NULL
 */
NCBI_XBLAST_EXPORT
SThreadLocalDataArray* SThreadLocalDataArrayFree(SThreadLocalDataArray* array);

/** Reduce the size of the array structure passed in to match the value of the
 * actual_num_threads parameter
 * @param array structure to modify [in|out]
 * @param actual_num_threads number of threads that will be used during the MT
 * traceback [in]
 */
NCBI_XBLAST_EXPORT
void SThreadLocalDataArrayTrim(SThreadLocalDataArray* array, Uint4 actual_num_threads);

/** Set up a newly allocated SThreadLocalDataArray object so it can be used by
 * multiple threads
 * @param array structure to modify [in|out]
 * @param program BLAST program type [in]
 * @param score_options scoring options to clone [in]
 * @param eff_len_options effective length options to clone [in]
 * @param ext_options extension options to clone [in]
 * @param hit_options hit options to clone [in]
 * @param query_info query_info structure to clone [in]
 * @param sbp BlastScoreBlk structure to refer to (not cloned) [in]
 * @param seqsrc sequence source structure to clone [in]
 * @return 0 on success, otherwise an error code
 */
NCBI_XBLAST_EXPORT
Int2 SThreadLocalDataArraySetup(SThreadLocalDataArray* array, 
                                EBlastProgramType program, 
                                const BlastScoringOptions* score_options, 
                                const BlastEffectiveLengthsOptions* eff_len_options,
                                const BlastExtensionOptions* ext_options,
                                const BlastHitSavingOptions* hit_options,
                                BlastQueryInfo* query_info,
                                BlastScoreBlk* sbp,
                                const BlastSeqSrc* seqsrc);

/** Extracts a single, consolidated BlastHSPResults structure from its input
 * for single threaded processing.
 * @param array structure to inspect and modify [in|out]
 * @return Consolidated structure or NULL in case of memory allocation failure
 */
NCBI_XBLAST_EXPORT
BlastHSPResults* SThreadLocalDataArrayConsolidateResults(SThreadLocalDataArray* array);

/** Identical in function to BLAST_ComputeTraceback, but this performs its task
 * in a multi-threaded manner if OpenMP is available.
 */
NCBI_XBLAST_EXPORT
Int2 
BLAST_ComputeTraceback_MT(EBlastProgramType program_number, 
   BlastHSPStream* hsp_stream, BLAST_SequenceBlk* query, 
   BlastQueryInfo* query_info,
   SThreadLocalDataArray* thread_data,
   const BlastDatabaseOptions* db_options,
   const PSIBlastOptions* psi_options, const BlastRPSInfo* rps_info, 
   SPHIPatternSearchBlk* pattern_blk, BlastHSPResults** results,
                        TInterruptFnPtr interrupt_search, SBlastProgress* progress_info);

#ifdef __cplusplus
}
#endif

#endif /* !ALGO_BLAST_CORE__BLAST_TRACEBACK_MT_PRIV__H */
