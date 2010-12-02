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
 *
 */

/** @file blast_traceback.h
 * Functions to do gapped alignment with traceback
 */

#ifndef ALGO_BLAST_CORE__BLAST_TRACEBACK__H
#define ALGO_BLAST_CORE__BLAST_TRACEBACK__H

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/core/blast_export.h>
#include <algo/blast/core/blast_program.h>
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_parameters.h>
#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/blast_encoding.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_seqsrc.h>
#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/blast_hspstream.h>
#include <algo/blast/core/pattern.h>

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
 * @param fence_hit True is returned here if overrun is detected. [in]
 */
NCBI_XBLAST_EXPORT
Int2
Blast_TracebackFromHSPList(EBlastProgramType program_number, 
   BlastHSPList* hsp_list, BLAST_SequenceBlk* query_blk, 
   BLAST_SequenceBlk* subject_blk, BlastQueryInfo* query_info,
   BlastGapAlignStruct* gap_align, BlastScoreBlk* sbp,
   const BlastScoringParameters* score_params,
   const BlastExtensionOptions* ext_options,
   const BlastHitSavingParameters* hit_params,
   const Uint1* gen_code_string,
   Boolean * fence_hit);

/** Get the subject sequence encoding type for the traceback,
 * given a program number.
 */
NCBI_XBLAST_EXPORT
EBlastEncoding Blast_TracebackGetEncoding(EBlastProgramType program_number);

/** Modifies the HSP data after the final gapped alignment.
 * Input includes only data that likely needs modification. This function
 * could be static in blast_traceback.c, but for a unit test which checks its
 * functionality.
 * @param gap_align Structure containing gapped alignment information [in]
 * @param hsp Original HSP from the preliminary stage [in] [out]
 */
NCBI_XBLAST_EXPORT
Int2
Blast_HSPUpdateWithTraceback(BlastGapAlignStruct* gap_align, BlastHSP* hsp);


/** Given the preliminary alignment results from a database search, redo 
 * the gapped alignment with traceback, if it has not yet been done.
 * @param program_number Type of the BLAST program [in]
 * @param hsp_stream A stream for reading HSP lists [in]
 * @param query The query sequence [in]
 * @param query_info Information about the query [in]
 * @param seq_src Source of subject sequences [in]
 * @param gap_align The auxiliary structure for gapped alignment [in]
 * @param score_params Scoring parameters (esp. scale factor) [in]
 * @param ext_params Gapped extension parameters [in]
 * @param hit_params Parameters for saving hits. Can change if not a 
                     database search [in]
 * @param eff_len_params Parameters for recalculating effective search 
 *                       space. Can change if not a database search. [in]
 * @param db_options Options containing database genetic code string [in]
 * @param psi_options Options for iterative searches [in]
 * @param rps_info RPS BLAST auxiliary data structure [in]
 * @param pattern_blk PHI BLAST auxiliary data structure [in]
 * @param results All results from the BLAST search [out]
 * @param interrupt_search function callback to allow interruption of BLAST
 * search [in, optional]
 * @param progress_info contains information about the progress of the current
 * BLAST search [in|out]
 * @return nonzero indicates failure, otherwise zero
 */
NCBI_XBLAST_EXPORT
Int2 
BLAST_ComputeTraceback(EBlastProgramType program_number, 
   BlastHSPStream* hsp_stream, BLAST_SequenceBlk* query, 
   BlastQueryInfo* query_info, const BlastSeqSrc* seq_src, 
   BlastGapAlignStruct* gap_align, BlastScoringParameters* score_params,
   const BlastExtensionParameters* ext_params,
   BlastHitSavingParameters* hit_params,
   BlastEffectiveLengthsParameters* eff_len_params,
   const BlastDatabaseOptions* db_options,
   const PSIBlastOptions* psi_options, const BlastRPSInfo* rps_info, 
   SPHIPatternSearchBlk* pattern_blk, BlastHSPResults** results,
   TInterruptFnPtr interrupt_search, SBlastProgress* progress_info);

/** Entry point from the API level to perform the traceback stage of a BLAST 
 * search, given the source of HSP lists, obtained from the preliminary stage. 
 * The parameters internal to the engine are calculated here independently of 
 * the similar calculation in the preliminary stage, effectively making the two 
 * stages independent of each other.
 * @param program BLAST program type [in]
 * @param query Query sequence(s) structure [in]
 * @param query_info Additional query information [in]
 * @param seq_src Source of subject sequences [in]
 * @param score_options Scoring options [in]
 * @param ext_options Word extension options, needed for cutoff scores 
 *                    calculation only [in]
 * @param hit_options Hit saving options [in]
 * @param eff_len_options Options for calculating effective lengths [in]
 * @param db_options Database options (database genetic code) [in]
 * @param psi_options PSI BLAST options [in]
 * @param sbp Scoring block with statistical parameters and matrix [in]
 * @param hsp_stream Source of HSP lists. [in]
 * @param rps_info RPS database information structure [in]
 * @param pattern_blk PHI BLAST auxiliary data structure [in]
 * @param results Where to save the results after traceback. [out]
 */
NCBI_XBLAST_EXPORT
Int2 
Blast_RunTracebackSearch(EBlastProgramType program, 
   BLAST_SequenceBlk* query, BlastQueryInfo* query_info, 
   const BlastSeqSrc* seq_src, const BlastScoringOptions* score_options,
   const BlastExtensionOptions* ext_options,
   const BlastHitSavingOptions* hit_options,
   const BlastEffectiveLengthsOptions* eff_len_options,
   const BlastDatabaseOptions* db_options, 
   const PSIBlastOptions* psi_options, BlastScoreBlk* sbp,
   BlastHSPStream* hsp_stream, const BlastRPSInfo* rps_info, 
   SPHIPatternSearchBlk* pattern_blk, BlastHSPResults** results);


/** Entry point from the API level to perform the traceback stage of a BLAST 
 * search, given the source of HSP lists, obtained from the preliminary stage. 
 * The parameters internal to the engine are calculated here independently of 
 * the similar calculation in the preliminary stage, effectively making the two 
 * stages independent of each other.
 * @param program BLAST program type [in]
 * @param query Query sequence(s) structure [in]
 * @param query_info Additional query information [in]
 * @param seq_src Source of subject sequences [in]
 * @param score_options Scoring options [in]
 * @param ext_options Word extension options, needed for cutoff scores 
 *                    calculation only [in]
 * @param hit_options Hit saving options [in]
 * @param eff_len_options Options for calculating effective lengths [in]
 * @param db_options Database options (database genetic code) [in]
 * @param psi_options PSI BLAST options [in]
 * @param sbp Scoring block with statistical parameters and matrix [in]
 * @param hsp_stream Source of HSP lists. [in]
 * @param rps_info RPS database information structure [in]
 * @param pattern_blk PHI BLAST auxiliary data structure [in]
 * @param results Where to save the results after traceback. [out]
 * @param interrupt_search User specified function to interrupt search [in]
 * @param progress_info User supplied data structure to aid interrupt [in]
 */
NCBI_XBLAST_EXPORT
Int2 
Blast_RunTracebackSearchWithInterrupt(EBlastProgramType program, 
   BLAST_SequenceBlk* query, BlastQueryInfo* query_info, 
   const BlastSeqSrc* seq_src, const BlastScoringOptions* score_options,
   const BlastExtensionOptions* ext_options,
   const BlastHitSavingOptions* hit_options,
   const BlastEffectiveLengthsOptions* eff_len_options,
   const BlastDatabaseOptions* db_options, 
   const PSIBlastOptions* psi_options, BlastScoreBlk* sbp,
   BlastHSPStream* hsp_stream, const BlastRPSInfo* rps_info, 
   SPHIPatternSearchBlk* pattern_blk, BlastHSPResults** results,
   TInterruptFnPtr interrupt_search, SBlastProgress* progress_info);

#ifdef __cplusplus
}
#endif
#endif /* !ALGO_BLAST_CORE__BLAST_TRACEBACK__H */
