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

#ifdef GAPPED_LOG
Int4 
BLAST_SearchEngineCore(BLAST_SequenceBlkPtr query, 
        LookupTableWrapPtr lookup, BlastQueryInfoPtr query_info,
        ReadDBFILEPtr db, Int4 numseqs, 
        BLAST_ExtendWordPtr ewp, BlastGapAlignStructPtr gap_align, 
        BlastScoringOptionsPtr score_options, 
        BlastInitialWordParametersPtr word_params, 
        BlastExtensionParametersPtr ext_params, 
        BlastHitSavingParametersPtr hit_params, 
        BlastResultsPtr PNTR results_ptr, CharPtr logname);
#else
/** The high level function performing the BLAST search after all the setup
 * has been done.
 * @param query The query sequence [in]
 * @param lookup The lookup table [in]
 * @param query_info Additional query information [in]
 * @param db Structure containing BLAST database [in]
 * @param numseqs Number of sequences in the database [in]
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
        ReadDBFILEPtr db, Int4 numseqs, 
        BLAST_ExtendWordPtr ewp, BlastGapAlignStructPtr gap_align, 
        BlastScoringOptionsPtr score_options, 
        BlastInitialWordParametersPtr word_params, 
        BlastExtensionParametersPtr ext_params, 
        BlastHitSavingParametersPtr hit_params, 
        BlastResultsPtr PNTR results_ptr);
#endif

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_ENGINE__ */
