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
 */

/** @file aa_ungapped.h
 * @todo FIXME: Need file description (protein wordfinding & ungapped 
 * extension code?)
 */

#ifndef AA_UNGAPPED__H
#define AA_UNGAPPED__H

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_lookup.h>
#include <algo/blast/core/blast_extend.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Scan a subject sequence for word hits
 *
 * @param subject the subject sequence [in]
 * @param query the query sequence [in]
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
		       LookupTableWrap* lookup,
		       Int4** matrix,
		       const BlastInitialWordParameters* word_params,
		       Blast_ExtendWord* ewp,
                       BlastOffsetPair* NCBI_RESTRICT offset_pairs,
		       Int4 offset_array_size,
		       BlastInitHitList* init_hitlist, 
             BlastUngappedStats* ungapped_stats);

/** Scan a subject sequence for word hits and trigger two-hit extensions.
 *
 * @param subject the subject sequence [in]
 * @param query the query sequence [in]
 * @param lookup_wrap the lookup table [in]
 * @param diag the diagonal array structure [in/out]
 * @param matrix the substitution matrix [in]
 * @param cutoff cutoff score for saving ungapped HSPs [in]
 * @param dropoff x dropoff [in]
 * @param offset_pairs Array for storing query and subject offsets. [in]
 * @param array_size the number of elements in each offset array [in]
 * @param ungapped_hsps hsps resulting from the ungapped extension [out]
 * @param ungapped_stats Various hit counts. Not filled if NULL [out]
 */

Int2 BlastAaWordFinder_TwoHit(const BLAST_SequenceBlk* subject,
			      const BLAST_SequenceBlk* query,
			      const LookupTableWrap* lookup_wrap,
			      BLAST_DiagTable* diag,
			      Int4 ** matrix,
			      Int4 cutoff,
			      Int4 dropoff,
                              BlastOffsetPair* NCBI_RESTRICT offset_pairs,
			      Int4 array_size,
	                      BlastInitHitList* ungapped_hsps, 
                              BlastUngappedStats* ungapped_stats);

/** Scan a subject sequence for word hits and trigger one-hit extensions.
 *
 * @param subject the subject sequence
 * @param query the query sequence
 * @param lookup_wrap the lookup table
 * @param diag the diagonal array structure
 * @param matrix the substitution matrix [in]
 * @param cutoff cutoff score for saving ungapped HSPs [in]
 * @param dropoff x dropoff [in]
 * @param offset_pairs Array for storing query and subject offsets. [in]
 * @param array_size the number of elements in each offset array
 * @param ungapped_hsps hsps resulting from the ungapped extensions [out]
 * @param ungapped_stats Various hit counts. Not filled if NULL [out]
 */
Int2 BlastAaWordFinder_OneHit(const BLAST_SequenceBlk* subject,
			      const BLAST_SequenceBlk* query,
			      const LookupTableWrap* lookup_wrap,
			      BLAST_DiagTable* diag,
			      Int4 ** matrix,
			      Int4 cutoff,
			      Int4 dropoff,
                              BlastOffsetPair* NCBI_RESTRICT offset_pairs,
			      Int4 array_size,
	            BlastInitHitList* ungapped_hsps, 
               BlastUngappedStats* ungapped_stats);

/**
 * Beginning at s_off and q_off in the subject and query, respectively,
 * extend to the right until the cumulative score becomes negative or
 * drops by at least dropoff.
 *
 * @param matrix the substitution matrix [in]
 * @param subject subject sequence [in]
 * @param query query sequence [in]
 * @param s_off subject offset [in]
 * @param q_off query offset [in]
 * @param dropoff the X dropoff parameter [in]
 * @param displacement the length of the extension [out]
 * @param maxscore the score derived from a previous left extension [in]
 * @param s_last_off the rightmost subject offset examined [out]
 * @return The score of the extension
 */

  Int4 BlastAaExtendRight(Int4 ** matrix,
			const BLAST_SequenceBlk* subject,
			const BLAST_SequenceBlk* query,
			Int4 s_off,
			Int4 q_off,
			Int4 dropoff,
			Int4* displacement,
	                Int4 maxscore,
	                Int4* s_last_off);

  Int4 BlastPSSMExtendRight(Int4 ** matrix,
			const BLAST_SequenceBlk* subject,
			Int4 query_size,
			Int4 s_off,
			Int4 q_off,
			Int4 dropoff,
			Int4* displacement,
	                Int4 maxscore,
	                Int4* s_last_off);


/**
 * Beginning at s_off and q_off in the subject and query, respectively,
 * extend to the left until the cumulative score becomes negative or
 * drops by at least dropoff.
 *
 * @param matrix the substitution matrix [in]
 * @param subject subject sequence [in]
 * @param query query sequence [in]
 * @param s_off subject offset [in]
 * @param q_off query offset [in]
 * @param dropoff the X dropoff parameter [in]
 * @param displacement the length of the extension [out]
 * @param score the score so far (probably from initial word hit) [in]
 * @return The score of the extension
 */

Int4 BlastAaExtendLeft(Int4 ** matrix,
		       const BLAST_SequenceBlk* subject,
		       const BLAST_SequenceBlk* query,
		       Int4 s_off,
		       Int4 q_off,
		       Int4 dropoff,
		       Int4* displacement,
                       Int4 score);

Int4 BlastPSSMExtendLeft(Int4 ** matrix,
		       const BLAST_SequenceBlk* subject,
		       Int4 s_off,
		       Int4 q_off,
		       Int4 dropoff,
		       Int4* displacement,
                       Int4 score);


/** Perform a one-hit extension. Beginning at the specified hit,
 * extend to the left, then extend to the right. 
 *
 * @param matrix the substitution matrix [in]
 * @param subject subject sequence [in]
 * @param query query sequence [in]
 * @param s_off subject offset [in]
 * @param q_off query offset [in]
 * @param dropoff X dropoff parameter [in]
 * @param hsp_q the offset in the query where the HSP begins [out]
 * @param hsp_s the offset in the subject where the HSP begins [out]
 * @param hsp_len the length of the HSP [out]
 * @param word_size number of letters in the initial word hit [in]
 * @param use_pssm TRUE if the scoring matrix is position-specific [in]
 * @param s_last_off the rightmost subject offset examined [out]
 * @return the score of the hsp.
 */

Int4 BlastAaExtendOneHit(Int4 ** matrix,
	                 const BLAST_SequenceBlk* subject,
	                 const BLAST_SequenceBlk* query,
	                 Int4 s_off,
	                 Int4 q_off,
	                 Int4 dropoff,
			 Int4* hsp_q,
			 Int4* hsp_s,
			 Int4* hsp_len,
                         Int4 word_size,
	                 Boolean use_pssm,
	                 Int4* s_last_off);
	                 
/** Perform a two-hit extension. Given two hits L and R, begin
 * at R and extend to the left. If we do not reach L, abort the extension.
 * Otherwise, begin at R and extend to the right.
 *
 * @param matrix the substitution matrix [in]
 * @param subject subject sequence [in]
 * @param query query sequence [in]
 * @param s_left_off left subject offset [in]
 * @param s_right_off right subject offset [in]
 * @param q_right_off right query offset [in]
 * @param dropoff X dropoff parameter [in]
 * @param hsp_q the offset in the query where the HSP begins [out]
 * @param hsp_s the offset in the subject where the HSP begins [out]
 * @param hsp_len the length of the HSP [out]
 * @param use_pssm TRUE if the scoring matrix is position-specific [in]
 * @param word_size number of letters in one word [in]
 * @param right_extend set to TRUE if an extension to the right happened [out]
 * @param s_last_off the rightmost subject offset examined [out]
 * @return the score of the hsp.
 */

Int4 BlastAaExtendTwoHit(Int4 ** matrix,
	                 const BLAST_SequenceBlk* subject,
	                 const BLAST_SequenceBlk* query,
	                 Int4 s_left_off,
	                 Int4 s_right_off,
	                 Int4 q_right_off,
	                 Int4 dropoff,
			 Int4* hsp_q,
			 Int4* hsp_s,
			 Int4* hsp_len,
	                 Boolean use_pssm,
	                 Int4 word_size,
	                 Boolean *right_extend,
	                 Int4* s_last_off);

#ifdef __cplusplus
}
#endif

#endif /* AA_UNGAPPED__H */
