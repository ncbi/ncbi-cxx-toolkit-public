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

#ifndef __BLAST_GAPALIGN__
#define __BLAST_GAPALIGN__

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_parameters.h>
#include <algo/blast/core/blast_extend.h>
#include <algo/blast/core/gapinfo.h>
#include <algo/blast/core/greedy_align.h>
#include <algo/blast/core/blast_hits.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Diagonal distance between HSPs for which one can be considered included in 
   the other in Mega BLAST */
#define MB_DIAG_CLOSE 6 

/** Split subject sequences if longer than this */
#define MAX_DBSEQ_LEN 5000000 

/** Structure supporting the gapped alignment */
typedef struct BlastGapAlignStruct {
   Boolean positionBased; /**< Is this PSI-BLAST? */
   GapStateArrayStruct* state_struct; /**< Structure to keep extension 
                                                state information */
   GapEditScript* edit_script; /**< The traceback (gap) information */
   GapPrelimEditBlock *fwd_prelim_tback; /**< traceback from left extensions */
   GapPrelimEditBlock *rev_prelim_tback; /**< traceback from right extensions */
   SGreedyAlignMem* greedy_align_mem;/**< Preallocated memory for the greedy 
                                         gapped extension */
   BlastScoreBlk* sbp; /**< Pointer to the scoring information block */
   Int4 gap_x_dropoff; /**< X-dropoff parameter to use */
   Int4 query_start; /**< query start offset of current alignment */
   Int4 query_stop; /**< query end offseet of current alignment */
   Int4 subject_start;  /**< subject start offset current alignment */
   Int4 subject_stop; /**< subject end offset of current alignment */
   Int4 score;   /**< Return value: alignment score */
   double percent_identity;/**< Return value: percent identity - filled only 
                               by the greedy non-affine alignment algorithm */
} BlastGapAlignStruct;

/** Initializes the BlastGapAlignStruct structure 
 * @param score_params Parameters related to scoring alignments [in]
 * @param ext_params parameters related to gapped extension [in]
 * @param max_subject_length Maximum length of any subject sequence (needed 
 *        for greedy extension allocation only) [in]
 * @param sbp The scoring information block [in]
 * @param gap_align_ptr The BlastGapAlignStruct structure [out]
*/
Int2
BLAST_GapAlignStructNew(const BlastScoringParameters* score_params, 
   const BlastExtensionParameters* ext_params, 
   Uint4 max_subject_length, BlastScoreBlk* sbp, 
   BlastGapAlignStruct** gap_align_ptr);

/** Deallocates memory in the BlastGapAlignStruct structure */
NCBI_XBLAST_EXPORT
BlastGapAlignStruct* 
BLAST_GapAlignStructFree(BlastGapAlignStruct* gap_align);

/** Mega BLAST function performing gapped alignment: 
 *  Sorts initial HSPs by diagonal; 
 *  For each HSP:
 *    - Removes HSP if it is included in another already extended HSP;
 *    - If required, performs ungapped extension;
 *    - Performs greedy gapped extension;
 *    - Saves qualifying HSPs with gapped information into an HSP list 
 *      structure.
 * @param program_number Not needed: added for prototype consistency.
 * @param query The query sequence [in]
 * @param query_info Query information structure, containing offsets into 
 *                   the concatenated sequence [in]
 * @param subject The subject sequence [in]
 * @param gap_align A placeholder for gapped alignment information and 
 *        score block. [in] [out]
 * @param score_params Options related to scoring alignments [in]
 * @param ext_params Options related to alignment extension [in]
 * @param hit_params Options related to saving HSPs [in]
 * @param init_hitlist Contains all the initial hits [in]
 * @param hsp_list_ptr List of HSPs with full extension information [out]
 * @param gapped_stats Return statistics (not filled if NULL) [out]
*/
Int2 BLAST_MbGetGappedScore(EBlastProgramType program_number, 
             BLAST_SequenceBlk* query, BlastQueryInfo* query_info, 
                BLAST_SequenceBlk* subject,
                BlastGapAlignStruct* gap_align,
                const BlastScoringParameters* score_params, 
                const BlastExtensionParameters* ext_params,
                const BlastHitSavingParameters* hit_params,
                BlastInitHitList* init_hitlist,
                BlastHSPList** hsp_list_ptr, BlastGappedStats* gapped_stats);



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
 */
Int2 BLAST_GetGappedScore (EBlastProgramType program_number, 
            BLAST_SequenceBlk* query, BlastQueryInfo* query_info, 
              BLAST_SequenceBlk* subject,
              BlastGapAlignStruct* gap_align,
              const BlastScoringParameters* score_params, 
              const BlastExtensionParameters* ext_params,
              const BlastHitSavingParameters* hit_params,
              BlastInitHitList* init_hitlist,
              BlastHSPList** hsp_list_ptr, BlastGappedStats* gapped_stats);

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
 */
Int2 BLAST_GappedAlignmentWithTraceback(EBlastProgramType program, 
        Uint1* query, Uint1* subject, 
        BlastGapAlignStruct* gap_align, 
        const BlastScoringParameters* score_params,
        Int4 q_start, Int4 s_start, Int4 query_length, Int4 subject_length);

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
 */
Int2 
BLAST_GreedyGappedAlignment(Uint1* query, Uint1* subject, 
   Int4 query_length, Int4 subject_length, BlastGapAlignStruct* gap_align,
   const BlastScoringParameters* score_params, 
   Int4 q_off, Int4 s_off, Boolean compressed_subject, Boolean do_traceback);

/** Perform a gapped alignment with traceback for PHI BLAST
 * @param query The query sequence [in]
 * @param subject The subject sequence [in]
 * @param gap_align The gapped alignment structure [in] [out]
 * @param score_params Scoring parameters [in]
 * @param q_start Offset in query where to start alignment [in]
 * @param s_start Offset in subject where to start alignment [in]
 * @param query_length Maximal allowed extension in query [in]
 * @param subject_length Maximal allowed extension in subject [in]
 */
Int2 PHIGappedAlignmentWithTraceback(Uint1* query, Uint1* subject, 
        BlastGapAlignStruct* gap_align, 
        const BlastScoringParameters* score_params,
        Int4 q_start, Int4 s_start, Int4 query_length, Int4 subject_length);

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
Int2 BLAST_GetUngappedHSPList(BlastInitHitList* init_hitlist, 
        BlastQueryInfo* query_info, BLAST_SequenceBlk* subject, 
        const BlastHitSavingOptions* hit_options, 
        BlastHSPList** hsp_list_ptr);

/** Preliminary gapped alignment for PHI BLAST.
 * @param program_number Type of BLAST program [in]
 * @param query The query sequence block [in]
 * @param query_info Query information structure, containing offsets into 
 *                   the concatenated sequence [in]
 * @param subject The subject sequence block [in]
 * @param gap_align The auxiliary structure for gapped alignment [in]
 * @param score_params Options related to scoring [in]
 * @param ext_params Options and parameters related to extensions [in]
 * @param hit_params Options related to saving hits [in]
 * @param init_hitlist List of initial HSPs, including offset pairs and
 *                     pattern match lengths [in]
 * @param hsp_list_ptr Structure containing all saved HSPs [out]
 * @param gapped_stats Return statistics (not filled if NULL) [out]
 */
Int2 PHIGetGappedScore (EBlastProgramType program_number, 
        BLAST_SequenceBlk* query, BlastQueryInfo* query_info, 
        BLAST_SequenceBlk* subject, 
        BlastGapAlignStruct* gap_align,
        const BlastScoringParameters* score_params,
        const BlastExtensionParameters* ext_params,
        const BlastHitSavingParameters* hit_params,
        BlastInitHitList* init_hitlist,
        BlastHSPList** hsp_list_ptr, BlastGappedStats* gapped_stats);

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
Int4 
BlastGetStartForGappedAlignment (Uint1* query, Uint1* subject,
   const BlastScoreBlk* sbp, Uint4 q_start, Uint4 q_length, 
   Uint4 s_start, Uint4 s_length);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_GAPALIGN__ */
