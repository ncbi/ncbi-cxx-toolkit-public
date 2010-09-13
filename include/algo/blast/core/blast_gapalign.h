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
 * Author:  Ilya Dondoshansky
 *
 */

/** @file blast_gapalign.h
 * Structures and functions prototypes used for BLAST gapped extension
 * @todo FIXME: elaborate on contents.
 */

#ifndef ALGO_BLAST_CORE__BLAST_GAPALIGN__H
#define ALGO_BLAST_CORE__BLAST_GAPALIGN__H

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/core/blast_export.h>
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_extend.h>
#include <algo/blast/core/blast_query_info.h>
#include <algo/blast/core/blast_parameters.h>
#include <algo/blast/core/gapinfo.h>
#include <algo/blast/core/greedy_align.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_diagnostics.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Split subject sequences if longer than this */
#define MAX_DBSEQ_LEN 5000000 

/** Auxiliary structure for dynamic programming gapped extension */
typedef struct {
  Int4 best;            /**< score of best path that ends in a match
                             at this position */
  Int4 best_gap;        /**< score of best path that ends in a gap
                             at this position */
} BlastGapDP;


/** Structure supporting the gapped alignment */
typedef struct BlastGapAlignStruct {
   Boolean positionBased; /**< Is this PSI-BLAST? */
   GapStateArrayStruct* state_struct; /**< Structure to keep extension 
                                                state information */
   GapEditScript* edit_script; /**< The traceback (gap) information */
   GapPrelimEditBlock *fwd_prelim_tback; /**< traceback from right extensions */
   GapPrelimEditBlock *rev_prelim_tback; /**< traceback from left extensions */
   SGreedyAlignMem* greedy_align_mem;/**< Preallocated memory for the greedy 
                                         gapped extension */
   BlastGapDP* dp_mem; /**< scratch structures for dynamic programming */
   Int4 dp_mem_alloc;  /**< current number of structures allocated */
   BlastScoreBlk* sbp; /**< Pointer to the scoring information block */
   Int4 gap_x_dropoff; /**< X-dropoff parameter to use */
   Int4 query_start; /**< query start offset of current alignment */
   Int4 query_stop; /**< query end offseet of current alignment */
   Int4 subject_start;  /**< subject start offset current alignment */
   Int4 subject_stop; /**< subject end offset of current alignment */
   Int4 greedy_query_seed_start;  /**< for greedy alignments, the query 
                                       offset of the gapped start point */
   Int4 greedy_subject_seed_start;  /**< for greedy alignments, the subject
                                         offset of the gapped start point */
   Int4 score;   /**< Return value: alignment score */
} BlastGapAlignStruct;

/** Initializes the BlastGapAlignStruct structure 
 * @param score_params Parameters related to scoring alignments [in]
 * @param ext_params parameters related to gapped extension [in]
 * @param max_subject_length Maximum length of any subject sequence (needed 
 *        for greedy extension allocation only) [in]
 * @param sbp The scoring information block [in]
 * @param gap_align_ptr The BlastGapAlignStruct structure [out]
*/
NCBI_XBLAST_EXPORT
Int2
BLAST_GapAlignStructNew(const BlastScoringParameters* score_params, 
   const BlastExtensionParameters* ext_params, 
   Uint4 max_subject_length, BlastScoreBlk* sbp, 
   BlastGapAlignStruct** gap_align_ptr);

/** Deallocates memory in the BlastGapAlignStruct structure */
NCBI_XBLAST_EXPORT
BlastGapAlignStruct* 
BLAST_GapAlignStructFree(BlastGapAlignStruct* gap_align);

/** Performs gapped extension for all non-Mega BLAST programs, given
 * that ungapped extension has been done earlier.
 * Sorts initial HSPs by score (from ungapped extension);
 * Deletes HSPs that are included in already extended HSPs;
 * Performs gapped extension;
 * Saves HSPs into an HSP list.
 * @param program_number Type of BLAST program [in]
 * @param query The query sequence block [in]
 * @param query_info Query information structure, containing offsets into 
 *                   the concatenated sequence [in]
 * @param subject The subject sequence block [in]
 * @param gap_align The auxiliary structure for gapped alignment [in]
 * @param score_params Options and parameters related to scoring [in]
 * @param ext_params Options and parameters related to extensions [in]
 * @param hit_params Options related to saving hits [in]
 * @param init_hitlist List of initial HSPs (offset pairs with additional 
 *        information from the ungapped alignment performed earlier) [in]
 * @param hsp_list_ptr Structure containing all saved HSPs [out]
 * @param gapped_stats Return statistics (not filled if NULL) [out]
 * @param fence_hit True is returned here if overrun is detected. [in]
 */
NCBI_XBLAST_EXPORT
Int2 BLAST_GetGappedScore (EBlastProgramType program_number, 
            BLAST_SequenceBlk* query, BlastQueryInfo* query_info, 
              BLAST_SequenceBlk* subject,
              BlastGapAlignStruct* gap_align,
              const BlastScoringParameters* score_params, 
              const BlastExtensionParameters* ext_params,
              const BlastHitSavingParameters* hit_params,
              BlastInitHitList* init_hitlist,
              BlastHSPList** hsp_list_ptr, BlastGappedStats* gapped_stats,
              Boolean * fence_hit);

/** Perform a gapped alignment with traceback
 * @param program Type of BLAST program [in]
 * @param query The query sequence [in]
 * @param subject The subject sequence [in]
 * @param gap_align The gapped alignment structure [in] [out]
 * @param score_params Scoring parameters [in]
 * @param q_start Offset in query where to start alignment [in]
 * @param s_start Offset in subject where to start alignment [in]
 * @param query_length Maximal allowed extension in query [in]
 * @param subject_length Maximal allowed extension in subject [in]
 * @param fence_hit True is returned here if overrun is detected. [in]
 */
NCBI_XBLAST_EXPORT
Int2 BLAST_GappedAlignmentWithTraceback(EBlastProgramType program, 
        const Uint1* query, const Uint1* subject, 
        BlastGapAlignStruct* gap_align, 
        const BlastScoringParameters* score_params,
        Int4 q_start, Int4 s_start, Int4 query_length, Int4 subject_length,
        Boolean * fence_hit);

/** Greedy gapped alignment, with or without traceback.
 * Given two sequences, relevant options and an offset pair, fills the
 * gap_align structure with alignment endpoints and, if traceback is 
 * performed, gap information.
 * @param query The query sequence [in]
 * @param subject The subject sequence [in]
 * @param query_length The query sequence length [in]
 * @param subject_length The subject sequence length [in]
 * @param gap_align The structure holding various information and memory 
 *        needed for gapped alignment [in] [out]
 * @param score_params Parameters related to scoring alignments [in]
 * @param q_off Starting offset in query [in]
 * @param s_off Starting offset in subject [in]
 * @param compressed_subject Is subject sequence compressed? [in]
 * @param do_traceback Should traceback be saved? [in]
 * @param fence_hit True is returned here if overrun is detected. [in]
 */
NCBI_XBLAST_EXPORT
Int2 
BLAST_GreedyGappedAlignment(const Uint1* query, const Uint1* subject, 
   Int4 query_length, Int4 subject_length, BlastGapAlignStruct* gap_align,
   const BlastScoringParameters* score_params, 
   Int4 q_off, Int4 s_off, Boolean compressed_subject, Boolean do_traceback,
   Boolean * fence_hit);

/** Convert initial HSP list to an HSP list: to be used in ungapped search.
 * Ungapped data must be available in the initial HSP list for this function 
 * to work.
 * @param init_hitlist List of initial HSPs with ungapped extension 
 *                     information [in]
 * @param query_info Query information structure, containing offsets into
 *                   the concatenated queries/strands/frames [in]
 * @param subject Subject sequence block containing frame information [in]
 * @param hit_options Hit saving options [in]
 * @param hsp_list_ptr HSPs in the final form [out]
 */
NCBI_XBLAST_EXPORT
Int2 BLAST_GetUngappedHSPList(BlastInitHitList* init_hitlist, 
        BlastQueryInfo* query_info, BLAST_SequenceBlk* subject, 
        const BlastHitSavingOptions* hit_options, 
        BlastHSPList** hsp_list_ptr);

/** Adjusts range of subject sequence to be passed for gapped extension,
 * taking into account the length and starting position of the alignment in
 * query.
 * @param subject_offset_ptr Start of the subject range [out]
 * @param subject_length_ptr Length of the subject range [out]
 * @param query_offset Offset in query from which alignment starts [in]
 * @param query_length Length of the query sequence [in]
 * @param start_shift The offset by which the output range is shifted with
 *                    respect to the full subject sequence [out]
 */
NCBI_XBLAST_EXPORT
void 
AdjustSubjectRange(Int4* subject_offset_ptr, Int4* subject_length_ptr, 
                   Int4 query_offset, Int4 query_length, Int4* start_shift);

/** Function to look for the highest scoring window (of size HSP_MAX_WINDOW)
 * in an HSP and return the middle of this.  Used by the gapped-alignment
 * functions to start the gapped alignments.
 * @param query The query sequence [in]
 * @param subject The subject sequence [in]
 * @param sbp Scoring block, containing matrix [in]
 * @param q_start Starting offset in query [in]
 * @param q_length Length of HSP in query [in]
 * @param s_start Starting offset in subject [in]
 * @param s_length Length of HSP in subject [in]
 * @return The offset at which alignment should be started [out]
*/
NCBI_XBLAST_EXPORT
Int4 
BlastGetStartForGappedAlignment (const Uint1* query, const Uint1* subject,
   const BlastScoreBlk* sbp, Uint4 q_start, Uint4 q_length, 
   Uint4 s_start, Uint4 s_length);

/** Function to look for the highest scoring window (of size HSP_MAX_WINDOW)
 * in an HSP and return the middle of this.  Used by the gapped-alignment
 * functions to start the gapped alignments.
 * Should be used instead of BlastGetStartForGappedAlignment 
 * @param query The query sequence [in]
 * @param subject The subject sequence [in]
 * @param sbp Scoring block, containing matrix [in]
 * @param hsp start and stops of HSP [in]
 * @param q_retval query offset to use [out]
 * @param s_retval subject offset to use [out]
 *
*/
NCBI_XBLAST_EXPORT
Boolean
BlastGetOffsetsForGappedAlignment (const Uint1* query, const Uint1* subject,
   const BlastScoreBlk* sbp, BlastHSP* hsp, Int4* q_retval, Int4* s_retval);

#ifdef __cplusplus
}
#endif
#endif /* !ALGO_BLAST_CORE__BLAST_GAPALIGN__H */
