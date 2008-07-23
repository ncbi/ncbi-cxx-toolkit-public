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

/** @file na_ungapped.h
 * Nucleotide ungapped extension code.
 */

#ifndef ALGO_BLAST_CORE__NA_UNGAPPED__H
#define ALGO_BLAST_CORE__NA_UNGAPPED__H

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_extend.h>
#include <algo/blast/core/blast_parameters.h>
#include <algo/blast/core/blast_query_info.h>
#include <algo/blast/core/lookup_wrap.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_diagnostics.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Signature of function used to compute ungapped alignments */
typedef Int4 (*TNaExtendFunction)(const BlastOffsetPair* offset_pairs, 
                    Int4 num_hits, 
                    const BlastInitialWordParameters* word_params,
                    LookupTableWrap* lookup_wrap,
                    BLAST_SequenceBlk* query, BLAST_SequenceBlk* subject,
                    Int4** matrix, BlastQueryInfo* query_info,
                    Blast_ExtendWord* ewp, 
                    BlastInitHitList* init_hitlist);

/** Find all words for a given subject sequence and perform 
 * ungapped extensions, assuming ordinary blastn.
 * @param subject The subject sequence [in]
 * @param query The query sequence (needed only for the discontiguous word 
 *        case) [in]
 * @param query_info concatenated query information [in]
 * @param lookup_wrap Pointer to the (wrapper) lookup table structure. Only
 *        traditional BLASTn lookup table supported. [in]
 * @param matrix The scoring matrix [in]
 * @param word_params Parameters for the initial word extension [in]
 * @param ewp Structure needed for initial word information maintenance [in]
 * @param offset_pairs Array for storing query and subject offsets. [in]
 * @param max_hits size of offset arrays [in]
 * @param init_hitlist Structure to hold all hits information. Has to be 
 *        allocated up front [out]
 * @param ungapped_stats Various hit counts. Not filled if NULL [out]
 */
NCBI_XBLAST_EXPORT
Int2 BlastNaWordFinder(BLAST_SequenceBlk* subject, 
                       BLAST_SequenceBlk* query,
                       BlastQueryInfo* query_info,
                       LookupTableWrap* lookup_wrap,
                       Int4** matrix,
                       const BlastInitialWordParameters* word_params, 
                       Blast_ExtendWord* ewp,
                       BlastOffsetPair* offset_pairs,
                       Int4 max_hits,
                       BlastInitHitList* init_hitlist, 
                       BlastUngappedStats* ungapped_stats);


/** Choose the best routine to use for creating ungapped alignments
 * @param lookup_wrap Lookup table that influences routine choice [in][out]
 */
NCBI_XBLAST_EXPORT
void BlastChooseNaExtend(LookupTableWrap *lookup_wrap);

#ifdef __cplusplus
}
#endif
#endif /* !ALGO_BLAST_CORE__NA_UNGAPPED__H */
