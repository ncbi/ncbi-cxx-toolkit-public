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
 * High level BLAST functions
 */

#ifndef __BLAST_ENGINE__
#define __BLAST_ENGINE__

#ifdef __cplusplus
extern "C" {
#endif

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_extend.h>
#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_seqsrc.h>
#include <algo/blast/core/blast_diagnostics.h>   

/** How many subject sequences to process in one database chunk. */
#define BLAST_DB_CHUNK_SIZE 1024

/** The high level function performing the BLAST search against a BLAST 
 * database after all the setup has been done.
 * @param program_number Type of BLAST program [in]
 * @param query The query sequence [in]
 * @param query_info Additional query information [in]
 * @param bssp Structure containing BLAST database [in]
 * @param sbp Scoring and statistical parameters [in]
 * @param score_options Hit scoring options [in]
 * @param lookup_wrap The lookup table, constructed earlier [in] 
 * @param word_options Options for processing initial word hits [in]
 * @param ext_options Options and parameters for the gapped extension [in]
 * @param hit_options Options for saving the HSPs [in]
 * @param eff_len_options Options for setting effective lengths [in]
 * @param psi_options Options specific to PSI-BLAST [in]
 * @param db_options Options for handling BLAST database [in]
 * @param results Structure holding all saved results [in] [out]
 * @param diagnostics Return statistics containing numbers of hits on 
 *                    different stages of the search [out]
 */
Int4 
BLAST_SearchEngine(Uint1 program_number, 
   BLAST_SequenceBlk* query, BlastQueryInfo* query_info,
   const BlastSeqSrc* bssp, BlastScoreBlk* sbp, 
   const BlastScoringOptions* score_options, 
   LookupTableWrap* lookup_wrap, 
   const BlastInitialWordOptions* word_options, 
   const BlastExtensionOptions* ext_options, 
   const BlastHitSavingOptions* hit_options,
   const BlastEffectiveLengthsOptions* eff_len_options,
   const PSIBlastOptions* psi_options, 
   const BlastDatabaseOptions* db_options,
   BlastHSPResults* results, BlastDiagnostics* diagnostics);

/** The high level function performing an RPS BLAST search 
 * @param program_number Type of BLAST program [in]
 * @param query The query sequence [in]
 * @param query_info Additional query information [in]
 * @param bssp Structure containing BLAST database [in]
 * @param sbp Scoring and statistical parameters [in]
 * @param score_options Hit scoring options [in]
 * @param lookup_wrap The lookup table, constructed earlier [in] 
 * @param word_options Options for processing initial word hits [in]
 * @param ext_options Options and parameters for the gapped extension [in]
 * @param hit_options Options for saving the HSPs [in]
 * @param eff_len_options Options for setting effective lengths [in]
 * @param psi_options Options specific to PSI-BLAST [in]
 * @param db_options Options for handling BLAST database [in]
 * @param results Structure holding all saved results [in] [out]
 * @param diagnostics Return statistics containing numbers of hits on 
 *                    different stages of the search [out]
 */
Int4 
BLAST_RPSSearchEngine(Uint1 program_number, 
   BLAST_SequenceBlk* query, BlastQueryInfo* query_info,
   const BlastSeqSrc* bssp, BlastScoreBlk* sbp, 
   const BlastScoringOptions* score_options, 
   LookupTableWrap* lookup_wrap, 
   const BlastInitialWordOptions* word_options, 
   const BlastExtensionOptions* ext_options, 
   const BlastHitSavingOptions* hit_options,
   const BlastEffectiveLengthsOptions* eff_len_options,
   const PSIBlastOptions* psi_options, 
   const BlastDatabaseOptions* db_options,
   BlastHSPResults* results, BlastDiagnostics* diagnostics);

/** Gapped extension function pointer type */
typedef Int2 (*BlastGetGappedScoreType) 
     (Uint1, BLAST_SequenceBlk*, BlastQueryInfo* query_info,
      BLAST_SequenceBlk*, BlastGapAlignStruct*, const BlastScoringParameters*,
      const BlastExtensionParameters*, const BlastHitSavingParameters*,
      BlastInitHitList*, BlastHSPList**, BlastGappedStats*);
     
/** Word finder function pointer type */
typedef Int2 (*BlastWordFinderType) 
     (BLAST_SequenceBlk*, BLAST_SequenceBlk*,
      LookupTableWrap*, Int4**, const BlastInitialWordParameters*,
      Blast_ExtendWord*, Uint4*, Uint4*, Int4, BlastInitHitList*,
      BlastUngappedStats*);

/** Structure to be passed to BLAST_SearchEngineCore, containing pointers 
    to various preallocated structures and arrays. */
typedef struct BlastCoreAuxStruct {

   Blast_ExtendWord* ewp; /**< Structure for keeping track of diagonal
                               information for initial word matches */
   BlastWordFinderType WordFinder; /**< Word finder function pointer */
   BlastGetGappedScoreType GetGappedScore; /**< Gapped extension function
                                              pointer */
   BlastInitHitList* init_hitlist; /**< Placeholder for HSPs after 
                                        ungapped extension */
   BlastHSPList* hsp_list; /**< Placeholder for HSPs after gapped 
                                extension */
   Uint4* query_offsets; /**< Placeholder for initial word match query 
                              offsets */
   Uint4* subject_offsets; /**< Placeholder for initial word match  
                              subject offsets */
   Uint1* translation_buffer; /**< Placeholder for translated subject
                                   sequences */
   Uint1* translation_table; /**< Translation table for forward strand */
   Uint1* translation_table_rc; /**< Translation table for reverse 
                                     strand */
} BlastCoreAuxStruct;

/** Deallocates all memory in BlastCoreAuxStruct */
BlastCoreAuxStruct* 
BlastCoreAuxStructFree(BlastCoreAuxStruct* aux_struct);

/** Setup of the auxiliary BLAST structures; 
 * also calculates internally used parameters from options. 
 * @param program_number blastn, blastp, blastx, etc. [in]
 * @param seq_src Sequence source information, with callbacks to get 
 *             sequences, their lengths, etc. [in]
 * @param scoring_options options for scoring. [in]
 * @param eff_len_options  used to calculate effective lengths. [in]
 * @param lookup_wrap Lookup table, already constructed. [in]
 * @param word_options options for initial word finding. [in]
 * @param ext_options options for gapped extension. [in]
 * @param hit_options options for saving hits. [in]
 * @param query The query sequence block [in]
 * @param query_info The query information block [in]
 * @param sbp Contains scoring information. [in]
 * @param gap_align Gapped alignment information and allocated memory [out]
 * @param score_params Parameters for scoring [out]
 * @param word_params Parameters for initial word processing [out]
 * @param ext_params Parameters for gapped extension [out]
 * @param hit_params Parameters for saving hits [out]
 * @param eff_len_params Parameters for calculating effective lengths [out]
 * @param aux_struct_ptr Placeholder joining various auxiliary memory 
 *                       structures [out]
 */
Int2 
BLAST_SetUpAuxStructures(Uint1 program_number,
   const BlastSeqSrc* seq_src,
   const BlastScoringOptions* scoring_options,
   const BlastEffectiveLengthsOptions* eff_len_options,
   LookupTableWrap* lookup_wrap,	
   const BlastInitialWordOptions* word_options,
   const BlastExtensionOptions* ext_options,
   const BlastHitSavingOptions* hit_options,
   BLAST_SequenceBlk* query, BlastQueryInfo* query_info, 
   BlastScoreBlk* sbp, 
   BlastGapAlignStruct** gap_align, 
   BlastScoringParameters** score_params,
   BlastInitialWordParameters** word_params,
   BlastExtensionParameters** ext_params,
   BlastHitSavingParameters** hit_params,
   BlastEffectiveLengthsParameters** eff_len_params,                         
   BlastCoreAuxStruct** aux_struct_ptr);

/** The core of the BLAST search: comparison between the (concatenated)
 * query against one subject sequence. Translation of the subject sequence
 * into 6 frames is done inside, if necessary. If subject sequence is 
 * too long, it can be split into several chunks. 
 */
Int2
BLAST_SearchEngineCore(Uint1 program_number, BLAST_SequenceBlk* query, 
   BlastQueryInfo* query_info, BLAST_SequenceBlk* subject, 
   LookupTableWrap* lookup, BlastGapAlignStruct* gap_align, 
   BlastScoringParameters* score_params, 
   BlastInitialWordParameters* word_params, 
   BlastExtensionParameters* ext_params, 
   BlastHitSavingParameters* hit_params, 
   const BlastDatabaseOptions* db_options,
   BlastDiagnostics* diagnostics,
   BlastCoreAuxStruct* aux_struct,
   BlastHSPList** hsp_list_out);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_ENGINE__ */
