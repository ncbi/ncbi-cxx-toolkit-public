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
 * Author: Jason Papadopoulos
 *
 */

/** @file blast_sw.h
 *  Smith-Waterman alignment for use within the infrastructure of BLAST
 */

#ifndef ALGO_BLAST_CORE___BLAST_SW__H
#define ALGO_BLAST_CORE___BLAST_SW__H

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_program.h>
#include <algo/blast/core/blast_query_info.h>
#include <algo/blast/core/blast_parameters.h>
#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_diagnostics.h>

/** @addtogroup AlgoBlast
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Find all local alignments between two (unpacked) sequences, using 
 *  the Smith-Waterman algorithm, then save the list of alignments found. 
 *  The algorithm to recover all high-scoring local alignments, and not
 *  just the best one, is described in
 * <PRE>
 * Geoffrey J. Barton, "An Efficient Algorithm to Locate All
 * Locally Optimal Alignments Between Two Sequences Allowing for Gaps".
 * Computer Applications in the Biosciences, (1993), 9, pp. 729-734
 * </PRE>
 * @param program_number Blast program requesting traceback [in]
 * @param A The first sequence [in]
 * @param a_size Length of the first sequence [in]
 * @param B The second sequence [in]
 * @param b_size Length of the second sequence [in]
 * @param template_hsp Placeholder alignment, used only to
 *                    determine contexts and frames [in]
 * @param hsp_list Collection of alignments found so far [in][out]
 * @param score_params Structure containing gap penalties [in]
 * @param hit_params Structure used for percent identity calculation [in]
 * @param gap_align Auxiliary data for gapped alignment 
 *                 (used for score matrix info) [in]
 * @param start_shift Bias to be applied to subject offsets [in]
 * @param cutoff Alignments are saved if their score exceeds this value [in]
 */
void SmithWatermanScoreWithTraceback(EBlastProgramType program_number,
                                     Uint1 *A, Int4 a_size,
                                     Uint1 *B, Int4 b_size,
                                     BlastHSP *template_hsp,
                                     BlastHSPList *hsp_list,
                                     const BlastScoringParameters *score_params,
                                     const BlastHitSavingParameters *hit_params,
                                     BlastGapAlignStruct *gap_align,
                                     Int4 start_shift, Int4 cutoff);

/** Performs score-only Smith-Waterman gapped alignment of the subject
 * sequence with all contexts in the query.
 * @param program_number Type of BLAST program [in]
 * @param query The query sequence block [in]
 * @param query_info Query information structure, containing offsets into 
 *                   the concatenated sequence [in]
 * @param subject The subject sequence block [in]
 * @param gap_align The auxiliary structure for gapped alignment [in]
 * @param score_params Options and parameters related to scoring [in]
 * @param ext_params Options and parameters related to extensions [in]
 * @param hit_params Options related to saving hits [in]
 * @param init_hitlist List of initial HSPs (ignored)
 * @param hsp_list_ptr Structure containing all saved HSPs. Note that
 *               there will be at most one HSP for each context that
 *               contains a gapped alignment that exceeds a cutoff
 *               score. [out]
 * @param gapped_stats Return statistics (not filled if NULL) [out]
 */
Int2 BLAST_SmithWatermanGetGappedScore (EBlastProgramType program_number, 
        BLAST_SequenceBlk* query, BlastQueryInfo* query_info, 
        BLAST_SequenceBlk* subject, 
        BlastGapAlignStruct* gap_align,
        const BlastScoringParameters* score_params,
        const BlastExtensionParameters* ext_params,
        const BlastHitSavingParameters* hit_params,
        BlastInitHitList* init_hitlist,
        BlastHSPList** hsp_list_ptr, BlastGappedStats* gapped_stats);

#ifdef __cplusplus
}
#endif

/* @} */

#endif  /* ALGO_BLAST_CORE___BLAST_SW__H */
