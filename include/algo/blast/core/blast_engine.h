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

/** @file blast_engine.h
* Function calls to actually perform a BLAST search (high level).
 */

#ifndef ALGO_BLAST_CORE__BLAST_ENGINE__H
#define ALGO_BLAST_CORE__BLAST_ENGINE__H

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/core/blast_export.h>
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_program.h>
#include <algo/blast/core/blast_extend.h>
#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_parameters.h>
#include <algo/blast/core/blast_seqsrc.h>
#include <algo/blast/core/blast_diagnostics.h>   
#include <algo/blast/core/blast_hspstream.h>

#ifdef __cplusplus
extern "C" {
#endif

NCBI_XBLAST_EXPORT 
extern const int kBlastMajorVersion; /**< Major version */
NCBI_XBLAST_EXPORT 
extern const int kBlastMinorVersion; /**< Minor version */
NCBI_XBLAST_EXPORT 
extern const int kBlastPatchVersion; /**< Patch version */
/** Date of the most recent BLAST release (kept for historical reasons) */
NCBI_XBLAST_EXPORT 
extern const char* kBlastReleaseDate;


/** The high level function performing the BLAST search against a BLAST 
 * database after all the setup has been done.
 * @param program_number Type of BLAST program [in]
 * @param query The query sequence [in]
 * @param query_info Additional query information [in]
 * @param seq_src Structure containing BLAST database [in]
 * @param sbp Scoring and statistical parameters [in]
 * @param score_options Hit scoring options [in]
 * @param lookup_wrap The lookup table, constructed earlier [in] 
 * @param word_options Options for processing initial word hits [in]
 * @param ext_options Options and parameters for the gapped extension [in]
 * @param hit_options Options for saving the HSPs [in]
 * @param eff_len_options Options for setting effective lengths [in]
 * @param psi_options Options specific to PSI-BLAST [in]
 * @param db_options Options for handling BLAST database [in]
 * @param hsp_stream Structure for streaming results [in] [out]
 * @param rps_info RPS BLAST auxiliary data structure [in]
 * @param diagnostics Return statistics containing numbers of hits on 
 *                    different stages of the search [out]
 * @param results Results of the BLAST search [out]
 * @param interrupt_search function callback to allow interruption of BLAST
 * search [in, optional]
 * @param progress_info contains information about the progress of the current
 * BLAST search [in|out]
 */
Int4 
Blast_RunFullSearch(EBlastProgramType program_number, 
   BLAST_SequenceBlk* query, BlastQueryInfo* query_info,
   const BlastSeqSrc* seq_src, BlastScoreBlk* sbp, 
   const BlastScoringOptions* score_options, 
   LookupTableWrap* lookup_wrap, 
   const BlastInitialWordOptions* word_options, 
   const BlastExtensionOptions* ext_options, 
   const BlastHitSavingOptions* hit_options,
   const BlastEffectiveLengthsOptions* eff_len_options,
   const PSIBlastOptions* psi_options, 
   const BlastDatabaseOptions* db_options,
   BlastHSPStream* hsp_stream, const BlastRPSInfo* rps_info,
   BlastDiagnostics* diagnostics, BlastHSPResults** results,
   TInterruptFnPtr interrupt_search,
   SBlastProgress* progress_info);

/** Perform the preliminary stage of the BLAST search.
 * @param  program_number Type of BLAST program [in]
 * @param query The query sequence [in]
 * @param query_info Additional query information [in]
 * @param seq_src Structure containing BLAST database [in]
 * @param gap_align Structure containing scoring block and memory allocated
 *                  for gapped alignment. [in]
 * @param score_params Hit scoring parameters [in]
 * @param lookup_wrap The lookup table, constructed earlier [in] 
 * @param word_options Options for processing initial word hits [in]
 * @param ext_params Parameters for the gapped extension [in]
 * @param hit_params Parameters for saving the HSPs [in]
 * @param eff_len_params Parameters for setting effective lengths [in]
 * @param psi_options Options specific to PSI-BLAST [in]
 * @param db_options Options for handling BLAST database [in]
 * @param hsp_stream Placeholder for saving HSP lists [in]
 * @param diagnostics Return statistics containing numbers of hits on 
 *                    different stages of the search. Statistics saved only 
 *                    for the allocated parts of the structure. [in] [out]
 * @param interrupt_search function callback to allow interruption of BLAST
 * search [in, optional]
 * @param progress_info contains information about the progress of the current
 * BLAST search [in|out]
 */
Int4 
BLAST_PreliminarySearchEngine(EBlastProgramType program_number, 
   BLAST_SequenceBlk* query, BlastQueryInfo* query_info,
   const BlastSeqSrc* seq_src, BlastGapAlignStruct* gap_align,
   BlastScoringParameters* score_params, 
   LookupTableWrap* lookup_wrap,
   const BlastInitialWordOptions* word_options, 
   BlastExtensionParameters* ext_params, 
   BlastHitSavingParameters* hit_params,
   BlastEffectiveLengthsParameters* eff_len_params,
   const PSIBlastOptions* psi_options, 
   const BlastDatabaseOptions* db_options,
   BlastHSPStream* hsp_stream, BlastDiagnostics* diagnostics,
   TInterruptFnPtr interrupt_search, SBlastProgress* progress_info);

/** The high level function performing the BLAST search against a BLAST 
 * database after all the setup has been done.
 * @param program_number Type of BLAST program [in]
 * @param query The query sequence [in]
 * @param query_info Additional query information [in]
 * @param seq_src Structure containing BLAST database [in]
 * @param score_options Hit scoring options [in]
 * @param sbp Scoring and statistical parameters [in]
 * @param lookup_wrap The lookup table, constructed earlier [in] 
 * @param word_options Options for processing initial word hits [in]
 * @param ext_options Options and parameters for the gapped extension [in]
 * @param hit_options Options for saving the HSPs [in]
 * @param eff_len_options Options for setting effective lengths [in]
 * @param psi_options Options specific to PSI-BLAST [in]
 * @param db_options Options for handling BLAST database [in]
 * @param hsp_stream Structure for streaming results [in] [out]
 * @param diagnostics Return statistics containing numbers of hits on 
 *                    different stages of the search [out]
 */
Int2 
Blast_RunPreliminarySearch(EBlastProgramType program, 
   BLAST_SequenceBlk* query, BlastQueryInfo* query_info, 
   const BlastSeqSrc* seq_src, const BlastScoringOptions* score_options,
   BlastScoreBlk* sbp, LookupTableWrap* lookup_wrap,
   const BlastInitialWordOptions* word_options, 
   const BlastExtensionOptions* ext_options,
   const BlastHitSavingOptions* hit_options,
   const BlastEffectiveLengthsOptions* eff_len_options,
   const PSIBlastOptions* psi_options, const BlastDatabaseOptions* db_options, 
   BlastHSPStream* hsp_stream, BlastDiagnostics* diagnostics);


/** Same as above, with support for user interrupt function
 * @param program_number Type of BLAST program [in]
 * @param query The query sequence [in]
 * @param query_info Additional query information [in]
 * @param seq_src Structure containing BLAST database [in]
 * @param score_options Hit scoring options [in]
 * @param sbp Scoring and statistical parameters [in]
 * @param lookup_wrap The lookup table, constructed earlier [in] 
 * @param word_options Options for processing initial word hits [in]
 * @param ext_options Options and parameters for the gapped extension [in]
 * @param hit_options Options for saving the HSPs [in]
 * @param eff_len_options Options for setting effective lengths [in]
 * @param psi_options Options specific to PSI-BLAST [in]
 * @param db_options Options for handling BLAST database [in]
 * @param hsp_stream Structure for streaming results [in] [out]
 * @param diagnostics Return statistics containing numbers of hits on 
 *                    different stages of the search [out]
 * @param interrupt_search User defined function to interrupt search [in]
 * @param progress_info User supplied data structure to aid interrupt [in]
 */
Int2 
Blast_RunPreliminarySearchWithInterrupt(EBlastProgramType program, 
   BLAST_SequenceBlk* query, BlastQueryInfo* query_info, 
   const BlastSeqSrc* seq_src, const BlastScoringOptions* score_options,
   BlastScoreBlk* sbp, LookupTableWrap* lookup_wrap,
   const BlastInitialWordOptions* word_options, 
   const BlastExtensionOptions* ext_options,
   const BlastHitSavingOptions* hit_options,
   const BlastEffectiveLengthsOptions* eff_len_options,
   const PSIBlastOptions* psi_options, const BlastDatabaseOptions* db_options, 
   BlastHSPStream* hsp_stream, BlastDiagnostics* diagnostics,
   TInterruptFnPtr interrupt_search, SBlastProgress* progress_info);

/** Gapped extension function pointer type */
typedef Int2 (*BlastGetGappedScoreType) 
     (EBlastProgramType, /**< @todo comment function pointer types */
      BLAST_SequenceBlk*, 
      BlastQueryInfo*,
      BLAST_SequenceBlk*, 
      BlastGapAlignStruct*, 
      const BlastScoringParameters*,
      const BlastExtensionParameters*, 
      const BlastHitSavingParameters*,
      BlastInitHitList*, 
      BlastHSPList**, 
      BlastGappedStats*,
      Boolean * fence_hit);

/** Word finder function pointer type */
typedef Int2 (*BlastWordFinderType) 
     (BLAST_SequenceBlk*, /**< @todo comment function pointer types */
      BLAST_SequenceBlk*,
      BlastQueryInfo*,
      LookupTableWrap*, 
      Int4**, 
      const BlastInitialWordParameters*,
      Blast_ExtendWord*, 
      BlastOffsetPair*,
      Int4, 
      BlastInitHitList*,
      BlastUngappedStats*);

#ifdef __cplusplus
}
#endif
#endif /* !ALGO_BLAST_CORE__BLAST_ENGINE__H */
