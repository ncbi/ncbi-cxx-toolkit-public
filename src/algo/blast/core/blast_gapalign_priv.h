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

/** @file blast_gapalign_priv.h
 *  Private interface for blast_gapalign.c
 */

#ifndef ALGO_BLAST_CORE___BLAST_GAPALIGN_PRIV__H
#define ALGO_BLAST_CORE___BLAST_GAPALIGN_PRIV__H

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/core/gapinfo.h>
#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/blast_stat.h>
#include <algo/blast/core/blast_parameters.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Low level function to perform dynamic programming gapped extension 
 * with traceback.
 * @param A The query sequence [in]
 * @param B The subject sequence [in]
 * @param M Maximal extension length in query [in]
 * @param N Maximal extension length in subject [in]
 * @param a_offset Resulting starting offset in query [out]
 * @param b_offset Resulting starting offset in subject [out]
 * @param edit_block Structure to hold traceback generated [out]
 * @param gap_align Structure holding various information and allocated 
 *        memory for the gapped alignment [in]
 * @param scoringParams Parameters related to scoring [in]
 * @param query_offset The starting offset in query [in]
 * @param reversed Has the sequence been reversed? Used for psi-blast [in]
 * @param reverse_sequence Do reverse the sequence [in]
 * @param hit_fence If NULL, set to TRUE if the extension encountered
 *                  sequence letters that indicate finding a region
 *                  of B that is uninitialized [out]
 * @return The best alignment score found.
*/
Int4
ALIGN_EX(const Uint1* A, const Uint1* B, Int4 M, Int4 N, Int4* a_offset,
        Int4* b_offset, GapPrelimEditBlock *edit_block, 
        BlastGapAlignStruct* gap_align, 
        const BlastScoringParameters* scoringParams, Int4 query_offset,
        Boolean reversed, Boolean reverse_sequence,
        Boolean * hit_fence);

/** Low level function to perform gapped extension in one direction with 
 * or without traceback.
 * @param A The query sequence [in]
 * @param B The subject sequence [in]
 * @param M Maximal extension length in query [in]
 * @param N Maximal extension length in subject [in]
 * @param a_offset Resulting starting offset in query [out]
 * @param b_offset Resulting starting offset in subject [out]
 * @param score_only Only find the score, without saving traceback [in]
 * @param edit_block Structure to hold generated traceback [out]
 * @param gap_align Structure holding various information and allocated 
 *        memory for the gapped alignment [in]
 * @param score_params Parameters related to scoring [in]
 * @param query_offset The starting offset in query [in]
 * @param reversed Has the sequence been reversed? Used for psi-blast [in]
 * @param reverse_sequence Do reverse the sequence [in]
 * @param fence_hit If NULL, set to TRUE if the extension encountered
 *                  sequence letters that indicate finding a region
 *                  of B that is uninitialized [out]
 * @return The best alignment score found.
 */
Int4 
Blast_SemiGappedAlign(const Uint1* A, const Uint1* B, Int4 M, Int4 N,
                  Int4* a_offset, Int4* b_offset, Boolean score_only, 
                  GapPrelimEditBlock *edit_block, BlastGapAlignStruct* gap_align, 
                  const BlastScoringParameters* score_params, 
                  Int4 query_offset, Boolean reversed, Boolean reverse_sequence,
                  Boolean * fence_hit);

/** Convert the initial list of traceback actions from a non-OOF
 *  gapped alignment into a blast edit script. Note that this routine
 *  assumes the input edit blocks have not been reversed or rearranged
 *  by calling code
 *  @param rev_prelim_tback Traceback from extension to the left [in]
 *  @param fwd_prelim_tback Traceback from extension to the right [in]
 *  @return Pointer to the resulting edit script, or NULL if there
 *          are no traceback actions specified
 */
GapEditScript*
Blast_PrelimEditBlockToGapEditScript (GapPrelimEditBlock* rev_prelim_tback,
                                      GapPrelimEditBlock* fwd_prelim_tback);

/** Window size used to scan HSP for highest score region, where gapped
 * extension starts. 
 */
#define HSP_MAX_WINDOW 11

/** Function to check that the highest scoring region in an HSP still gives a 
 * positive score. This value was originally calcualted by 
 * BlastGetStartForGappedAlignment but it may have changed due to the 
 * introduction of ambiguity characters. Such a change can lead to 'strange' 
 * results from ALIGN. 
 * @param hsp An HSP structure [in]
 * @param query Query sequence buffer [in]
 * @param subject Subject sequence buffer [in]
 * @param sbp Scoring block containing matrix [in]
 * @return TRUE if region aroung starting offsets gives a positive score
*/
Boolean
BLAST_CheckStartForGappedAlignment(const BlastHSP* hsp, 
                                   const Uint1* query, 
                                   const Uint1* subject, 
                                   const BlastScoreBlk* sbp);

/** Are the two HSPs within a given number of diagonals from each other? */
#define MB_HSP_CLOSE(q1, s1, q2, s2, c) \
(ABS(((q1)-(s1)) - ((q2)-(s2))) < c)

/** Modify a BlastScoreBlk structure so that it can be used in RPS-BLAST. This
 * involves allocating a SPsiBlastScoreMatrix structure so that the PSSMs 
 * memory mapped from the RPS-BLAST database files can be assigned to that
 * structure.
 * @param sbp BlastScoreBlk structure to modify [in|out]
 * @param rps_pssm PSSMs in RPS-BLAST database to use [in]
 * @param rps_freq frequency counts in RPS-BLAST database to use [in]
 * @param alphabet_size Elements in one pssm row [in]
 */
void RPSPsiMatrixAttach(BlastScoreBlk* sbp, Int4** rps_pssm, double** rps_freq,
                        Int4 alphabet_size);

/** Remove the artificially built SPsiBlastScoreMatrix structure allocated by
 * RPSPsiMatrixAttach
 * @param sbp BlastScoreBlk structure to modify [in|out]
 */
void RPSPsiMatrixDetach(BlastScoreBlk* sbp);

#ifdef __cplusplus
}
#endif

#endif /* !ALGO_BLAST_CORE__BLAST_GAPALIGN_PRIV__H */
