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

/** The high level function performing the BLAST search after all the setup
 * has been done.
 * @param query The query sequence [in]
 * @param lookup The lookup table [in]
 * @param query_info Additional query information [in]
 * @param db Structure containing BLAST database [in]
 * @param subject_blk Subject sequence in two sequences case [in]
 * @param ewp Structure for tracking the initial hits [in]
 * @param gap_align Structure for gapped alignment [in]
 * @param score_options Hit scoring options [in]
 * @param word_params Options for processing initial word hits [in]
 * @param ext_params Options and parameters for the gapped extension [in]
 * @param hit_params Options for saving the HSPs [in]
 * @param results_ptr Structure holding all saved results [out]
 */
Int4 
BLAST_SearchEngineCore(BLAST_SequenceBlkPtr query, 
        LookupTableWrapPtr lookup, BlastQueryInfoPtr query_info,
        ReadDBFILEPtr db, BLAST_SequenceBlkPtr subject_blk, 
        BLAST_ExtendWordPtr ewp, BlastGapAlignStructPtr gap_align, 
        BlastScoringOptionsPtr score_options, 
        BlastInitialWordParametersPtr word_params, 
        BlastExtensionParametersPtr ext_params, 
        BlastHitSavingParametersPtr hit_params, 
        BlastResultsPtr PNTR results_ptr,
	BlastReturnStatPtr return_stats);


/** The high level function performing the BLAST search after all the setup
 * has been done.
 * @param blast_program Type of BLAST program (blastn, blastp, etc.) [in]
 * @param query The query sequence block [in]
 * @param query_info Additional query information [in]
 * @param rdfp Structure containing BLAST database [in]
 * @param subject_blk Subject sequence in two sequences case [in]
 * @param sbp Scoring statistical information [in]
 * @param score_options Hit scoring options [in]
 * @param lookup_options Lookup table options [in]
 * @param lookup_segments Query segments for which lookup table should be 
 *                        created [in]
 * @param word_options Options for processing initial word hits [in]
 * @param ext_options Options for the gapped extension [in]
 * @param eff_len_options Options for calculation of the effective search 
 *                        space [in]
 * @param hit_options Options for saving the HSPs [in]
 * @param word_params_ptr Parameters for processing initial word hits [out]
 * @param ext_params_ptr Parameters for the gapped extension [out]
 * @param results Structure holding all saved results [out]
 * @param return_stats Return statistics numbers [out]
 */
Int2 BLAST_SearchEngine(const Uint1 blast_program, BLAST_SequenceBlkPtr query, 
        BlastQueryInfoPtr query_info,
        ReadDBFILEPtr rdfp, BLAST_SequenceBlkPtr subject_blk, 
        BLAST_ScoreBlkPtr sbp, BlastScoringOptionsPtr score_options, 
        LookupTableOptionsPtr lookup_options, ValNodePtr lookup_segments,
        BlastInitialWordOptionsPtr word_options, 
        BlastExtensionOptionsPtr ext_options, 
        BlastEffectiveLengthsOptionsPtr eff_len_options,
        BlastHitSavingOptionsPtr hit_options, 
        BlastInitialWordParametersPtr PNTR word_params_ptr, 
        BlastExtensionParametersPtr PNTR ext_params_ptr, 
        BlastResultsPtr PNTR results, BlastReturnStatPtr return_stats);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_ENGINE__ */
