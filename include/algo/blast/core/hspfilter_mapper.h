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
 * Author:  Greg Boratyn
 *
 */

/** @file hspfilter_mapper.h
 * Implementation of a number of BlastHSPWriters to save the best chain 
 * of RNA-Seq hits to a genome.
 */

#ifndef ALGO_BLAST_CORE__HSPFILTER_MAPSPLICED__H
#define ALGO_BLAST_CORE__HSPFILTER_MAPSPLICED__H

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/core/blast_program.h>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_hspfilter.h>
#include <algo/blast/core/blast_hits.h>
#include <connect/ncbi_core.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ScoringOptions
{
    Int4 reward;
    Int4 penalty;
    Int4 gap_open;
    Int4 gap_extend;
    /* score for lack of splice signal */
    Int4 no_splice_signal;
} ScoringOptions;

/** Keeps prelim_hitlist_size and HitSavingOptions together. */
typedef struct BlastHSPMapperParams {
   EBlastProgramType program;/**< program type */
   ScoringOptions scoring_options; /**< scores for match, mismatch, and gap */
   Int4 hitlist_size;        /**< number of hits saved during preliminary
                                  part of search. */
   Boolean paired;           /**< mapping with paired reads */
   Boolean splice;           /**< mapping spliced reads (RNA-seq to a genome) */
} BlastHSPMapperParams;

/** Sets up parameter set for use by collector.
 * @param hit_options field hitlist_size and hsp_num_max needed, a pointer to 
 *      this structure will be stored on resulting structure.[in]
 * @param scoring_options needed for mismatch and gap penalties, the pointer
 *      will be stored [in]
 * @return the pointer to the allocated parameter
 */
NCBI_XBLAST_EXPORT
BlastHSPMapperParams*
BlastHSPMapperParamsNew(const BlastHitSavingOptions* hit_options,
                        const BlastScoringOptions* scoring_options);

/** Deallocates the BlastHSPMapperParams structure passed in
 * @param opts structure to deallocate [in]
 * @return NULL
 */
NCBI_XBLAST_EXPORT
BlastHSPMapperParams*
BlastHSPMapperParamsFree(BlastHSPMapperParams* opts);

/** WriterInfo to create a default writer: the collecter
 * @param params The collector parameters.
 * @return pointer to WriterInfo
 */
NCBI_XBLAST_EXPORT
BlastHSPWriterInfo* 
BlastHSPMapperInfoNew(BlastHSPMapperParams* params);

#ifdef __cplusplus
}
#endif

#endif /* !ALGO_BLAST_CORE__HSPFILTER_SPLICED__H */
