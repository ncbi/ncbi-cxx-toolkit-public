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

/** @file phi_extend.h
 * Word finder for PHI-BLAST
 */

#include <algo/blast/core/blast_extend.h>
#include <algo/blast/core/blast_util.h>

#ifndef ALGO_BLAST_CORE__PHI_EXTEND_H
#define ALGO_BLAST_CORE__PHI_EXTEND_H

#ifdef __cplusplus
extern "C" {
#endif

/** WordFinder type function for PHI BLAST.
 * @param subject Subject sequence [in]
 * @param query Query sequence [in]
 * @param lookup_wrap Wrapper for the lookup table with pattern information [in]
 * @param matrix Scoring matrix (not used) 
 * @param word_params Initial word parameters (not used)
 * @param ewp Word extension structure (not used)
 * @param offset_pairs Allocated array of offset pairs for storing subject 
 *                     pattern hits in  [in] [out]
 * @param max_hits Maximal number of hits allowed in the offset_pairs 
 *                 array. Not used - array is always allocated to fit all 
 *                 pattern hits.
 * @param init_hitlist Structure for saving initial hits. [in]
 * @param ungapped_stats Structure for saving hit counts [in] [out]
 * @return always zero
 */
Int2 PHIBlastWordFinder(BLAST_SequenceBlk* subject, 
        BLAST_SequenceBlk* query, LookupTableWrap* lookup_wrap,
        Int4** matrix, const BlastInitialWordParameters* word_params,
        Blast_ExtendWord* ewp, BlastOffsetPair* NCBI_RESTRICT offset_pairs,
        Int4 max_hits, BlastInitHitList* init_hitlist, 
        BlastUngappedStats* ungapped_stats);

#ifdef __cplusplus
}
#endif

#endif /* ALGO_BLAST_CORE__PHI_EXTEND_H */
