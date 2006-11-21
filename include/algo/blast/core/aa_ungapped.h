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
 */

/** @file aa_ungapped.h
 * Protein ungapped extension code.
 */

#ifndef ALGO_BLAST_CORE__AA_UNGAPPED_H
#define ALGO_BLAST_CORE__AA_UNGAPPED_H

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/core/blast_lookup.h>
#include <algo/blast/core/blast_parameters.h>
#include <algo/blast/core/blast_extend.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/lookup_wrap.h>
#include <algo/blast/core/blast_diagnostics.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Scan a subject sequence for word hits
 *
 * @param subject the subject sequence [in]
 * @param query the query sequence [in]
 * @param query_info concatenated query information [in]
 * @param lookup the lookup table [in]
 * @param matrix the substitution matrix [in]
 * @param word_params word parameters, needed for cutoff and dropoff [in]
 * @param ewp extend parameters, needed for diagonal tracking [in]
 * @param offset_pairs Array for storing query and subject offsets. [in]
 * @param offset_array_size the number of elements in each offset array [in]
 * @param init_hitlist hsps resulting from the ungapped extension [out]
 * @param ungapped_stats Various hit counts. Not filled if NULL [out]
 */
Int2 BlastAaWordFinder(BLAST_SequenceBlk* subject,
                       BLAST_SequenceBlk* query,
                       BlastQueryInfo* query_info,
                       LookupTableWrap* lookup,
                       Int4** matrix,
                       const BlastInitialWordParameters* word_params,
                       Blast_ExtendWord* ewp,
                       BlastOffsetPair* NCBI_RESTRICT offset_pairs,
                       Int4 offset_array_size,
                       BlastInitHitList* init_hitlist, 
                       BlastUngappedStats* ungapped_stats);

#ifdef __cplusplus
}
#endif

#endif /* !ALGO_BLAST_CORE__AA_UNGAPPED_H */
