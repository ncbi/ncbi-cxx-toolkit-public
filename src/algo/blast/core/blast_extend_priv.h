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

/** @file blast_extend_priv.h
 * Prototypes for functions from blast_extend.c and aa_ungapped.c that are considered
 * low-level so they should not be called outside of the "core" directory except in the
 * context of a unit test.  
 * 
 * Two different types of function prototypes are included:
 * 1.) functions that perform an extension of an initial hit.
 * 2.) functions that manipulate BLAST_DiagTable.
 *
 * 
 */

#ifndef __BLAST_EXTEND_PRI__
#define __BLAST_EXTEND_PRI__

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/lookup_wrap.h>
#include <algo/blast/core/blast_diagnostics.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Extend the lookup table exact match hit in one direction and 
 * update the diagonal structure.
 * @param q_offsets Array of query offsets [in]
 * @param s_offsets Array of subject offsets [in]
 * @param num_hits Size of the above arrays [in]
 * @param word_params Parameters for word extension [in]
 * @param lookup_wrap Lookup table wrapper structure [in]
 * @param query Query sequence data [in]
 * @param subject Subject sequence data [in]
 * @param matrix Scoring matrix for ungapped extension [in]
 * @param ewp Word extension structure containing information about the 
 *            extent of already processed hits on each diagonal [in]
 * @param init_hitlist Structure to keep the extended hits. 
 *                     Must be allocated outside of this function [in] [out]
 * @return Number of hits extended. 
 */
Int4
BlastNaExtendRight(Uint4* q_offsets, Uint4* s_offsets, Int4 num_hits, 
                   const BlastInitialWordParameters* word_params,
                   LookupTableWrap* lookup_wrap,
                   BLAST_SequenceBlk* query, BLAST_SequenceBlk* subject,
                   Int4** matrix, Blast_ExtendWord* ewp, 
                   BlastInitHitList* init_hitlist);

/** Extend the lookup table exact match hit in both directions and 
 * update the diagonal structure.
 * @param q_offsets Array of query offsets [in]
 * @param s_offsets Array of subject offsets [in]
 * @param num_hits Size of the above arrays [in]
 * @param word_params Parameters for word extension [in]
 * @param lookup_wrap Lookup table wrapper structure [in]
 * @param query Query sequence data [in]
 * @param subject Subject sequence data [in]
 * @param matrix Scoring matrix for ungapped extension [in]
 * @param ewp Word extension structure containing information about the 
 *            extent of already processed hits on each diagonal [in]
 * @param init_hitlist Structure to keep the extended hits. 
 *                     Must be allocated outside of this function [in] [out]
 * @return Number of hits extended. 
 */
Int4 
BlastNaExtendRightAndLeft(Uint4* q_offsets, Uint4* s_offsets, Int4 num_hits, 
                          const BlastInitialWordParameters* word_params,
                          LookupTableWrap* lookup_wrap,
                          BLAST_SequenceBlk* query, BLAST_SequenceBlk* subject,
                          Int4** matrix, Blast_ExtendWord* ewp, 
                          BlastInitHitList* init_hitlist);

/** Traditional Mega BLAST initial word extension
 * @param query The query sequence [in]
 * @param subject The subject sequence [in]
 * @param lookup Lookup table structure [in]
 * @param word_params The parameters related to initial word extension [in]
 * @param matrix the substitution matrix for ungapped extension [in]
 * @param ewp The structure containing word extension information [in]
 * @param q_off The offset in the query sequence [in]
 * @param s_off The offset in the subject sequence [in]
 * @param init_hitlist The structure containing information about all 
 *                     initial hits [in] [out]
 * @return Has this hit been extended? 
 */
Boolean
MB_ExtendInitialHits(const Uint4* query_offsets, const Uint4* subject_offsets,
                     Int4 num_hits, BLAST_SequenceBlk* query, 
                     BLAST_SequenceBlk* subject, LookupTableWrap* lookup,
                     const BlastInitialWordParameters* word_params, 
                     Int4** matrix, Blast_ExtendWord* ewp, 
                     BlastInitHitList* init_hitlist);


/** Allocates memory for the BLAST_DiagTable*. This function also 
 * sets many of the parametes such as diag_array_length etc.
 * @param qlen Length of the query [in]
 * @param multiple_hits Specifies whether multiple hits method is used [in]
 * @param window_size The max. distance between two hits that are extended [in]
 * @return The allocated BLAST_DiagTable structure
*/
BLAST_DiagTable*
BlastDiagTableNew (Int4 qlen, Boolean multiple_hits, Int4 window_size);

/** Deallocate memory for the diagonal table structure 
 * @param diag_table the object to be freed [in]
 * @return NULL
*/
BLAST_DiagTable*
BlastDiagTableFree(BLAST_DiagTable* diag_table);

/** Update the offset for use with a new sequence.
 * @param diag pointer to the diagonal array structure [in]
 * @param length length of the last db sequence searched [in]
 * @return allocated ptr to BLAST_DiagTable.
 */
Int4 BlastDiagUpdate(BLAST_DiagTable* diag, Int4 length);

/** Reset the diagonal array structure. Used when offset has wrapped around.
 * @param diag pointer to the diagonal array structure [in]
 */
Int4 BlastDiagClear(BLAST_DiagTable* diag);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_EXTEND_PRI__ */
