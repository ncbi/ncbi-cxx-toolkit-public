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
 * ===========================================================================
 *
 * Author: Ilya Dondoshansky
 *
 */

/** @file blast_traceback.h
 * Functions to do gapped alignment with traceback
 */

#ifndef __BLAST_TRACEBACK__
#define __BLAST_TRACEBACK__

#include <algo/blast/core/blast_seqsrc.h>
#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/blast_hspstream.h>

#ifdef __cplusplus
extern "C" {
#endif

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
 * @param score_params Scoring parameters (esp. scale factor) [in]
 * @param ext_options Gapped extension options [in]
 * @param hit_params Hit saving parameters [in]
 * @param gen_code_string specifies genetic code [in]
 */
NCBI_XBLAST_EXPORT
Int2
Blast_TracebackFromHSPList(EBlastProgramType program_number, BlastHSPList* hsp_list,
   BLAST_SequenceBlk* query_blk, BLAST_SequenceBlk* subject_blk,
   BlastQueryInfo* query_info,
   BlastGapAlignStruct* gap_align, BlastScoreBlk* sbp,
   const BlastScoringParameters* score_params,
   const BlastExtensionOptions* ext_options,
   const BlastHitSavingParameters* hit_params,
   const Uint1* gen_code_string);

/** Given the preliminary alignment results from a database search, redo 
 * the gapped alignment with traceback, if it has not yet been done.
 * @param program_number Type of the BLAST program [in]
 * @param hsp_stream A stream for reading HSP lists [in]
 * @param query The query sequence [in]
 * @param query_info Information about the query [in]
 * @param bssp BLAST database structure [in]
 * @param gap_align The auxiliary structure for gapped alignment [in]
 * @param score_params Scoring parameters (esp. scale factor) [in]
 * @param ext_params Gapped extension parameters [in]
 * @param hit_params Parameters for saving hits. Can change if not a 
                     database search [in]
 * @param eff_len_params Parameters for recalculating effective search 
 *                       space. Can change if not a database search. [in]
 * @param db_options Options containing database genetic code string [in]
 * @param psi_options Options for iterative searches [in]
 * @param results All results from the BLAST search [out]
 * @return nonzero indicates failure, otherwise zero
 */
NCBI_XBLAST_EXPORT
Int2 BLAST_ComputeTraceback(EBlastProgramType program_number, BlastHSPStream* hsp_stream, 
        BLAST_SequenceBlk* query, BlastQueryInfo* query_info, 
        const BlastSeqSrc* bssp, BlastGapAlignStruct* gap_align,
        BlastScoringParameters* score_params,
        const BlastExtensionParameters* ext_params,
        BlastHitSavingParameters* hit_params,
        BlastEffectiveLengthsParameters* eff_len_params,
        const BlastDatabaseOptions* db_options,
        const PSIBlastOptions* psi_options, BlastHSPResults** results);

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
 * @param hsp_stream A stream for reading HSP lists [in]
 * @param concat_db The concatentation of all RPS DB sequences. 
 *                  The sequence data itself is not needed, 
 *                  only its size [in]
 * @param concat_db_info Used for the list of context offsets 
 *                       for concat_db [in]
 * @param query The original query sequence [in]
 * @param query_info Information associated with the original query. 
 *                   Only used for the search space [in]
 * @param gap_align The auxiliary structure for gapped alignment [in]
 * @param score_params Scoring parameters (esp. scale factor) [in]
 * @param ext_params Gapped extension parameters [in]
 * @param hit_params Parameters for saving hits. Can change if not a 
                     database search [in]
 * @param db_options Options containing database genetic code string [in]
 * @param karlin_k Array of Karlin values, one for each database 
 *                 sequence. Used for E-value calculation [in]
 * @param results Results structure containing all HSPs, with added
 *                traceback information. [out]
 * @return nonzero indicates failure, otherwise zero
 */
NCBI_XBLAST_EXPORT
Int2 BLAST_RPSTraceback(EBlastProgramType program_number,
        BlastHSPStream* hsp_stream, 
        BLAST_SequenceBlk* concat_db,
        BlastQueryInfo* concat_db_info,
        BLAST_SequenceBlk* query,
        BlastQueryInfo* query_info,
        BlastGapAlignStruct* gap_align, 
        const BlastScoringParameters* score_params,
        const BlastExtensionParameters* ext_params,
        BlastHitSavingParameters* hit_params,
        const BlastDatabaseOptions* db_options,
        const double* karlin_k,
        BlastHSPResults** results);

/** Get the subject sequence encoding type for the traceback,
 * given a program number.
 */
NCBI_XBLAST_EXPORT
Uint1 Blast_TracebackGetEncoding(EBlastProgramType program_number);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_TRACEBACK__ */
