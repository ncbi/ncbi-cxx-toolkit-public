/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: blast_traceback.h

Author: Ilya Dondoshansky

Contents: Functions to do gapped alignment with traceback

******************************************************************************
 * $Revision$
 * */
#ifndef __BLAST_TRACEBACK__
#define __BLAST_TRACEBACK__

#ifdef __cplusplus
extern "C" {
#endif

#include <algo/blast/core/blast_seqsrc.h>
#include <algo/blast/core/blast_gapalign.h>

/** Compute gapped alignment with traceback for all HSPs from a single
 * query/subject sequence pair. 
 * Final e-values are calculated here, except when sum statistics is used,
 * in which case this is done in file link_hsps.c:
 * @sa { BLAST_LinkHsps }
 * @param program_number Type of BLAST program [in]
 * @param hsp_list List of HSPs [in]
 * @param query_blk The query sequence [in]
 * @param subject_blk The subject sequence [in]
 * @param query_info Query information, needed to get pointer to the
 *        start of this query within the concatenated sequence [in]
 * @param gap_align Auxiliary structure used for gapped alignment [in]
 * @param sbp Statistical parameters [in]
 * @param score_options Scoring parameters [in]
 * @param ext_options Gapped extension options [in]
 * @param hit_params Hit saving parameters [in]
 * @param gen_code_string specifies genetic code [in]
 * @param psi_options Options specific to PSI BLAST [in]
 */
Int2
Blast_TracebackFromHSPList(Uint1 program_number, BlastHSPList* hsp_list,
   BLAST_SequenceBlk* query_blk, BLAST_SequenceBlk* subject_blk,
   BlastQueryInfo* query_info,
   BlastGapAlignStruct* gap_align, BlastScoreBlk* sbp,
   const BlastScoringOptions* score_options,
   const BlastExtensionOptions* ext_options,
   const BlastHitSavingParameters* hit_params,
   const Uint1* gen_code_string, 
   const PSIBlastOptions* psi_options);

/** Given the preliminary alignment results from a database search, redo 
 * the gapped alignment with traceback, if it has not yet been done.
 * @param program_number Type of the BLAST program [in]
 * @param results Results of this BLAST search [in] [out]
 * @param query The query sequence [in]
 * @param query_info Information about the query [in]
 * @param bssp BLAST database structure [in]
 * @param gap_align The auxiliary structure for gapped alignment [in]
 * @param score_options The scoring related options [in]
 * @param ext_params Gapped extension parameters [in]
 * @param hit_params Parameters for saving hits. Can change if not a 
                     database search [in]
 * @param eff_len_params Parameters for recalculating effective search 
 *                       space. Can change if not a database search. [in]
 * @param db_options Options containing database genetic code string [in]
 * @param psi_options Options specific to PSI BLAST [in]
 * @return nonzero indicates failure, otherwise zero
 */
Int2 BLAST_ComputeTraceback(Uint1 program_number, BlastHSPResults* results, 
        BLAST_SequenceBlk* query, BlastQueryInfo* query_info, 
        const BlastSeqSrc* bssp, BlastGapAlignStruct* gap_align,
        const BlastScoringOptions* score_options,
        const BlastExtensionParameters* ext_params,
        BlastHitSavingParameters* hit_params,
        BlastEffectiveLengthsParameters* eff_len_params,
        const BlastDatabaseOptions* db_options,
        const PSIBlastOptions* psi_options);

/** Compute traceback information for alignments found by an
 *  RPS blast search. This function performs two major tasks:
 *  - Computes a composition-specific PSSM to be used during the
 *    traceback computation (non-translated searches only)
 *  - After traceback is computed, switch query offsets with 
 *    subject offsets and switch the edit blocks that describe
 *    the alignment. This is required because the entire RPS search
 *    was performed with these quatities reversed.
 * This call is also the first time that enough information 
 * exists to compute E-values for alignments that are found.
 *
 * @param program_number Type of the BLAST program [in]
 * @param results Structure containing the single HSPList 
 *                that is the result of a call to RPSUpdateResults.
 *                Traceback information is added to HSPs in list [in] [out]
 * @param concat_db The concatentation of all RPS DB sequences. 
 *                  The sequence data itself is not needed, 
 *                  only its size [in]
 * @param concat_db_info Used for the list of context offsets 
 *                       for concat_db [in]
 * @param query The original query sequence [in]
 * @param query_info Information associated with the original query. 
 *                   Only used for the search space [in]
 * @param gap_align The auxiliary structure for gapped alignment [in]
 * @param score_options The scoring related options [in]
 * @param ext_params Gapped extension parameters [in]
 * @param hit_params Parameters for saving hits. Can change if not a 
                     database search [in]
 * @param db_options Options containing database genetic code string [in]
 * @param psi_options Options specific to PSI BLAST. Only used for
 *                    the scaling factor at present [in]
 * @param karlin_k Array of Karlin values, one for each database 
 *                 sequence. Used for E-value calculation [in]
 * @return nonzero indicates failure, otherwise zero
 */
Int2 BLAST_RPSTraceback(Uint1 program_number,
        BlastHSPResults* results,
        BLAST_SequenceBlk* concat_db,
        BlastQueryInfo* concat_db_info,
        BLAST_SequenceBlk* query,
        BlastQueryInfo* query_info,
        BlastGapAlignStruct* gap_align, 
        const BlastScoringOptions* score_options,
        const BlastExtensionParameters* ext_params,
        BlastHitSavingParameters* hit_params,
        const BlastDatabaseOptions* db_options,
        const PSIBlastOptions* psi_options,
        const double* karlin_k);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_TRACEBACK__ */
