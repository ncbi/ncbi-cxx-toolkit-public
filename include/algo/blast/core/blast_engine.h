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

File name: blast_engine.h

Author: Ilya Dondoshansky

Contents: High level BLAST functions

******************************************************************************
 * $Revision$
 * */
#ifndef __BLAST_ENGINE__
#define __BLAST_ENGINE__

#ifdef __cplusplus
extern "C" {
#endif

#include <blast_def.h>
#include <aa_lookup.h>
#include <blast_extend.h>
#include <blast_gapalign.h>
#include <blast_hits.h>
#include <blast_seqsrc.h>

#define OFFSET_ARRAY_SIZE 4096

/** The high level function performing the BLAST search against a BLAST 
 * database after all the setup has been done.
 * @param program_number Type of BLAST program [in]
 * @param query The query sequence [in]
 * @param query_info Additional query information [in]
 * @param rdfp Structure containing BLAST database [in]
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
 * @param return_stats Return statistics containing numbers of hits on 
 *                     different stages of the search [out]
 */
Int4 
BLAST_DatabaseSearchEngine(Uint1 program_number, 
   const BLAST_SequenceBlkPtr query, const BlastQueryInfoPtr query_info,
   const BlastSeqSrcNewInfoPtr bssn_info, BLAST_ScoreBlkPtr sbp, 
   const BlastScoringOptionsPtr score_options, 
   LookupTableWrapPtr lookup_wrap, 
   const BlastInitialWordOptionsPtr word_options, 
   const BlastExtensionOptionsPtr ext_options, 
   const BlastHitSavingOptionsPtr hit_options,
   const BlastEffectiveLengthsOptionsPtr eff_len_options,
   const PSIBlastOptionsPtr psi_options, 
   const BlastDatabaseOptionsPtr db_options,
   BlastResultsPtr results, BlastReturnStatPtr return_stats);

/** The high level function performing BLAST comparison of two sequences,
 * after all the setup has been done.
 * @param program_number Type of BLAST program [in]
 * @param query Query sequence [in]
 * @param query_info Query information structure [in]
 * @param subject Subject sequence [in]
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
 * @param return_stats Return statistics containing numbers of hits on 
 *                     different stages of the search [out]
 */
Int4 
BLAST_TwoSequencesEngine(Uint1 program_number, 
   const BLAST_SequenceBlkPtr query, const BlastQueryInfoPtr query_info, 
   const BLAST_SequenceBlkPtr subject, 
   BLAST_ScoreBlkPtr sbp, const BlastScoringOptionsPtr score_options, 
   LookupTableWrapPtr lookup_wrap, 
   const BlastInitialWordOptionsPtr word_options, 
   const BlastExtensionOptionsPtr ext_options, 
   const BlastHitSavingOptionsPtr hit_options, 
   const BlastEffectiveLengthsOptionsPtr eff_len_options,
   const PSIBlastOptionsPtr psi_options, 
   const BlastDatabaseOptionsPtr db_options,
   BlastResultsPtr results, BlastReturnStatPtr return_stats);

/** Create the lookup table for all query words.
 * @param query The query sequence [in]
 * @param lookup_options What kind of lookup table to build? [in]
 * @param lookup_segments Locations on query to be used for lookup table
 *                        construction [in]
 * @param sbp Scoring block containing matrix [in]
 * @param lookup_wrap_ptr The initialized lookup table [out]
 */
Int2 LookupTableWrapInit(BLAST_SequenceBlkPtr query, 
        const LookupTableOptionsPtr lookup_options,	
        ListNodePtr lookup_segments, BLAST_ScoreBlkPtr sbp, 
        LookupTableWrapPtr PNTR lookup_wrap_ptr);

/** Function to calculate effective query length and db length as well as
 * effective search space. 
 * @param program_number blastn, blastp, blastx, etc. [in]
 * @param scoring_options options for scoring. [in]
 * @param eff_len_options used to calc. effective lengths [in]
 * @param sbp Karlin-Altschul parameters [out]
 * @param query_info The query information block, which stores the effective
 *                   search spaces for all queries [in] [out]
*/
Int2 BLAST_CalcEffLengths (Uint1 program_number, 
   const BlastScoringOptionsPtr scoring_options,
   const BlastEffectiveLengthsOptionsPtr eff_len_options, 
   const BLAST_ScoreBlkPtr sbp, BlastQueryInfoPtr query_info);

/** Gapped extension function pointer type */
typedef Int2 (*BlastGetGappedScoreType) 
     (Uint1, BLAST_SequenceBlkPtr, BLAST_SequenceBlkPtr, 
      BlastGapAlignStructPtr, BlastScoringOptionsPtr,
      BlastExtensionParametersPtr, BlastHitSavingParametersPtr,
      BlastInitHitListPtr, BlastHSPListPtr PNTR);
     
/** Word finder function pointer type */
typedef Int4 (*BlastWordFinderType) 
     (BLAST_SequenceBlkPtr, BLAST_SequenceBlkPtr,
      LookupTableWrapPtr, Int4Ptr PNTR, BlastInitialWordParametersPtr,
      BLAST_ExtendWordPtr, Uint4Ptr, Uint4Ptr, Int4, BlastInitHitListPtr);

/** Structure to be passed to BLAST_SearchEngineCore, containing pointers 
    to various preallocated structures and arrays. */
typedef struct BlastCoreAuxStruct {

   BlastSeqSrcPtr bssp;     /**< Source for subject sequences */
   BLAST_ExtendWordPtr ewp; /**< Structure for keeping track of diagonal
                               information for initial word matches */
   BlastWordFinderType WordFinder; /**< Word finder function pointer */
   BlastGetGappedScoreType GetGappedScore; /**< Gapped extension function
                                              pointer */
   BlastInitHitListPtr init_hitlist; /**< Placeholder for HSPs after 
                                        ungapped extension */
   BlastHSPListPtr hsp_list; /**< Placeholder for HSPs after gapped 
                                extension */
   Uint4Ptr query_offsets; /**< Placeholder for initial word match query 
                              offsets */
   Uint4Ptr subject_offsets; /**< Placeholder for initial word match  
                              subject offsets */
   Uint1Ptr translation_buffer; /**< Placeholder for translated subject
                                   sequences */
   Uint1Ptr translation_table; /**< Translation table for forward strand */
   Uint1Ptr translation_table_rc; /**< Translation table for reverse 
                                     strand */
} BlastCoreAuxStruct, PNTR BlastCoreAuxStructPtr;

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_ENGINE__ */
