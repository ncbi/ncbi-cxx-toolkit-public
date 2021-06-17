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
 * Author:  Tom Madden
 *
 */

/** @file blast_setup.h
 * Utilities initialize/setup BLAST.
 */

#ifndef __BLAST_SETUP__
#define __BLAST_SETUP__

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/core/blast_export.h>
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_query_info.h>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_parameters.h>
#include <algo/blast/core/blast_message.h>
#include <algo/blast/core/blast_stat.h>
#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/pattern.h>

#ifdef __cplusplus
extern "C" {
#endif

/** "Main" setup routine for BLAST. Calculates all information for BLAST search
 * that is dependent on the ASN.1 structures.
 * @todo FIXME: this function only filters query and sets up score block structure
 * @param program_number Type of BLAST program (0=blastn, ...). [in]
 * @param qsup_options options for query setup. [in]
 * @param scoring_options options for scoring. [in]
 * @param query_blk BLAST_SequenceBlk* for the query. [in]
 * @param query_info The query information block [in]
 * @param scale_factor Multiplier for cutoff and dropoff scores [in]
 * @param lookup_segments Start/stop locations for non-masked query 
 *                        segments [out]
 * @param mask masking locations. [out]
 * @param sbpp Contains scoring information. [out]
 * @param blast_message error or warning [out] 
 * @param get_path callback function to get matrix path [in]
 */
NCBI_XBLAST_EXPORT
Int2 BLAST_MainSetUp(EBlastProgramType program_number,
        const QuerySetUpOptions* qsup_options,
        const BlastScoringOptions* scoring_options,
        BLAST_SequenceBlk* query_blk,
        const BlastQueryInfo* query_info, 
        double scale_factor,
        BlastSeqLoc* *lookup_segments,
        BlastMaskLoc* *mask,
        BlastScoreBlk* *sbpp, 
        Blast_Message* *blast_message,
        GET_MATRIX_PATH get_path);

/** Blast_ScoreBlkKbpGappedCalc, fills the ScoreBlkPtr for a gapped search.  
 *      Should be moved to blast_stat.c in the future.
 * @param sbp Contains fields to be set, should not be NULL. [out]
 * @param scoring_options Scoring_options [in]
 * @param program Used to set fields on sbp [in]
 * @param query_info Query information containing context information [in]
 * @param error_return Pointer to structure for returning errors. [in][out]
 * @return Status.
 */
NCBI_XBLAST_EXPORT
Int2 Blast_ScoreBlkKbpGappedCalc(BlastScoreBlk * sbp,
                                 const BlastScoringOptions * scoring_options,
                                 EBlastProgramType program, 
                                 const BlastQueryInfo * query_info,
                                 Blast_Message** error_return);

/** Function to calculate effective query length and db length as well as
 * effective search space. 
 * @param program_number blastn, blastp, blastx, etc. [in]
 * @param scoring_options options for scoring. [in]
 * @param eff_len_params Used to calculate effective lengths [in]
 * @param sbp Karlin-Altschul parameters [out]
 * @param query_info The query information block, which stores the effective
 *                   search spaces for all queries [in] [out]
 * @param blast_message Error message [out]
*/
NCBI_XBLAST_EXPORT
Int2 BLAST_CalcEffLengths (EBlastProgramType program_number, 
   const BlastScoringOptions* scoring_options,
   const BlastEffectiveLengthsParameters* eff_len_params, 
   const BlastScoreBlk* sbp, BlastQueryInfo* query_info,
   Blast_Message **blast_message);

/** Set up the auxiliary structures for gapped alignment / traceback only 
 * @param program_number blastn, blastp, blastx, etc. [in]
 * @param seq_src Sequence source information, with callbacks to get 
 *                sequences, their lengths, etc. [in]
 * @param scoring_options options for scoring. [in]
 * @param eff_len_options Options overriding real database sizes for
 *                        calculating effective lengths [in]
 * @param ext_options options for gapped extension. [in]
 * @param hit_options options for saving hits. [in]
 * @param query_info The query information block [in]
 * @param sbp Contains scoring information. [in]
 * @param score_params Parameters for scoring [out]
 * @param ext_params Parameters for gapped extension [out]
 * @param hit_params Parameters for saving hits [out]
 * @param eff_len_params Parameters for search space calculations [out]
 * @param gap_align Gapped alignment information and allocated memory [out]
 */
NCBI_XBLAST_EXPORT
Int2 BLAST_GapAlignSetUp(EBlastProgramType program_number,
   const BlastSeqSrc* seq_src,
   const BlastScoringOptions* scoring_options,
   const BlastEffectiveLengthsOptions* eff_len_options,
   const BlastExtensionOptions* ext_options,
   const BlastHitSavingOptions* hit_options,
   BlastQueryInfo* query_info, 
   BlastScoreBlk* sbp, 
   BlastScoringParameters** score_params,
   BlastExtensionParameters** ext_params,
   BlastHitSavingParameters** hit_params,
   BlastEffectiveLengthsParameters** eff_len_params,
   BlastGapAlignStruct** gap_align);

/** Recalculates the parameters that depend on an individual sequence, if
 * this is not a database search.
 * @param program_number BLAST program [in]
 * @param subject_length Length of the current subject sequence [in]
 * @param scoring_options Scoring options [in]
 * @param query_info The query information structure. Effective lengths
 *                   are recalculated here. [in] [out]
 * @param sbp Scoring statistical parameters [in]
 * @param hit_params Parameters for saving hits. Score cutoffs are recalculated
 *                   here [in] [out]
 * @param word_params Parameters for ungapped extension. Score cutoffs are
 *                    recalculated here [in] [out]
 * @param eff_len_params Parameters for effective lengths calculation. Reset
 *                       with the current sequence data [in] [out]
 */
NCBI_XBLAST_EXPORT
Int2 BLAST_OneSubjectUpdateParameters(EBlastProgramType program_number,
    Uint4 subject_length,
    const BlastScoringOptions* scoring_options,
    BlastQueryInfo* query_info, 
    const BlastScoreBlk* sbp, 
    BlastHitSavingParameters* hit_params,
    BlastInitialWordParameters* word_params,
    BlastEffectiveLengthsParameters* eff_len_params);

/** Initializes the substitution matrix in the BlastScoreBlk according to the
 * scoring options specified.
 * @todo Should be moved to blast_stat.c in the future.
 * @param program_number Used to set fields on sbp [in]
 * @param scoring_options Scoring_options [in]
 * @param sbp Contains fields to be set, should not be NULL. [out]
 * @param get_path callback function to get matrix path [in]
 *
*/
NCBI_XBLAST_EXPORT
Int2 Blast_ScoreBlkMatrixInit(EBlastProgramType program_number, 
    const BlastScoringOptions* scoring_options,
    BlastScoreBlk* sbp,
    GET_MATRIX_PATH get_path);

/** Initializes the score block structure.
 * @param query_blk Query sequence(s) [in]
 * @param query_info Additional query information [in]
 * @param scoring_options Scoring options [in]
 * @param program_number BLAST program type [in]
 * @param sbpp Initialized score block [out]
 * @param scale_factor Matrix scaling factor for this search [in]
 * @param blast_message Error message [out]
 * @param get_path callback function to get matrix path [in]
 */
NCBI_XBLAST_EXPORT
Int2 BlastSetup_ScoreBlkInit(BLAST_SequenceBlk* query_blk, 
    const BlastQueryInfo* query_info, 
    const BlastScoringOptions* scoring_options, 
    EBlastProgramType program_number, 
    BlastScoreBlk* *sbpp, 
    double scale_factor, 
    Blast_Message* *blast_message,
    GET_MATRIX_PATH get_path);


/** Adjusts the mask locations coordinates to a sequence interval. Removes those
 * mask locations that do not intersect the interval. Can do this either for all 
 * queries or only for the first one.
 * @param mask Structure containing a mask location. [in] [out]
 * @param from Starting offset of a sequence interval [in]
 * @param to Ending offset of a sequence interval [in]
 */
NCBI_XBLAST_EXPORT
void
BlastSeqLoc_RestrictToInterval(BlastSeqLoc* *mask, Int4 from, Int4 to);


/** In a PHI BLAST search, adds pattern information to the BlastQueryInfo 
 * structure.
 * @param program Type of PHI BLAST program [in]
 * @param pattern_blk Auxiliary pattern items structure [in]
 * @param query Query sequence [in]
 * @param lookup_segments Locations on query sequence to find pattern on [in]
 * @param query_info Query information structure, where pattern occurrences
 *                   will be saved. [in][out]
 * @param blast_message will be filled in if pattern not found on query [in][out]
 * @return Status, 0 on success, -1 on error.
 */
NCBI_XBLAST_EXPORT
Int2 
Blast_SetPHIPatternInfo(EBlastProgramType            program,
                        const SPHIPatternSearchBlk * pattern_blk,
                        const BLAST_SequenceBlk    * query,
                        const BlastSeqLoc          * lookup_segments,
                        BlastQueryInfo             * query_info,
                        Blast_Message** blast_message);

/** Auxiliary function to retrieve the subject's number of sequences and total
 * length. 
 * @note In the case of a Blast2Sequences search, this function assumes a
 * single sequence and returns the length of the first sequence only
 */
NCBI_XBLAST_EXPORT
void
BLAST_GetSubjectTotals(const BlastSeqSrc* seqsrc,
                       Int8* total_length,
                       Int4* num_seqs);

/** Validation function for the setup of queries for the BLAST search.
 * @param query_info properly set up BlastQueryInfo structure [in]
 * @param score_blk optional properly set up BlastScoreBlk structure (may be
 * NULL)[in]
 * @return If no valid queries are found, 1 is returned, otherwise 0.
 */
NCBI_XBLAST_EXPORT
Int2
BlastSetup_Validate(const BlastQueryInfo* query_info, 
                    const BlastScoreBlk* score_blk);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_SETUP__ */
