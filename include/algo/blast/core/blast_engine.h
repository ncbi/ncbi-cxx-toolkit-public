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

#define OFFSET_ARRAY_SIZE 4096

/** The high level function performing the BLAST search against a BLAST 
 * database after all the setup has been done.
 * @param program_number Type of BLAST program [in]
 * @param query The query sequence [in]
 * @param query_info Additional query information [in]
 * @param rdfp Structure containing BLAST database [in]
 * @param sbp Scoring and statistical parameters [in]
 * @param score_options Hit scoring options [in]
 * @param lookup_options Options for creation of lookup table [in]
 * @param lookup_segments Query segments for which lookup table has to be 
 *                        built [in]
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
   BLAST_SequenceBlkPtr query, BlastQueryInfoPtr query_info,
   ReadDBFILEPtr rdfp, BLAST_ScoreBlkPtr sbp, 
   const BlastScoringOptionsPtr score_options, 
   const LookupTableOptionsPtr lookup_options,
   ValNodePtr lookup_segments, 
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
 * @param lookup_options Options for creation of lookup table [in]
 * @param lookup_segments Query segments for which lookup table has to be 
 *                        built [in]
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
BLAST_TwoSequencesEngine(Uint1 program_number, BLAST_SequenceBlkPtr query, 
   BlastQueryInfoPtr query_info, BLAST_SequenceBlkPtr subject, 
   BLAST_ScoreBlkPtr sbp, const BlastScoringOptionsPtr score_options, 
   const LookupTableOptionsPtr lookup_options,
   ValNodePtr lookup_segments, 
   const BlastInitialWordOptionsPtr word_options, 
   const BlastExtensionOptionsPtr ext_options, 
   const BlastHitSavingOptionsPtr hit_options, 
   const BlastEffectiveLengthsOptionsPtr eff_len_options,
   const PSIBlastOptionsPtr psi_options, 
   const BlastDatabaseOptionsPtr db_options,
   BlastResultsPtr results, BlastReturnStatPtr return_stats);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_ENGINE__ */
