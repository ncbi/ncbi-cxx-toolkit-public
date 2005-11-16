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
 * Author: Ilya Dondoshansky
 *
 */

/** @file greedy_align.h
 * Prototypes and structures for greedy gapped alignment
 */

#ifndef _GREEDY_H_
#define _GREEDY_H_

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/gapinfo.h>

#ifdef __cplusplus
extern "C" {
#endif

/** sequence_length / (this number) is a measure of how hard the 
    alignment code will work to find the optimal alignment; in fact
    this gives a worst case bound on the number of loop iterations */
#define GREEDY_MAX_COST_FRACTION 2

/** The largest distance to be examined for an optimal alignment */
#define GREEDY_MAX_COST 1000

/* ----- pool allocator ----- */

/** Bookkeeping structure for greedy alignment. When aligning
    two sequences, the members of this structure store the
    largest offset into the second sequence that leads to a
    high-scoring alignment for a given start point */
typedef struct SGreedyOffset {
    Int4 insert_off;    /**< Best offset for a path ending in an insertion */
    Int4 match_off;     /**< Best offset for a path ending in a match */
    Int4 delete_off;    /**< Best offset for a path ending in a deletion */
} SGreedyOffset;

/** Space structure for greedy alignment algorithm */
typedef struct SMBSpace {
    SGreedyOffset* space_array; /**< array of bookkeeping structures */
    Int4 space_allocated;       /**< number of structures allocated */
    Int4 space_used;            /**< number of structures actually in use */
    struct SMBSpace *next;      /**< pointer to next structure in list */
} SMBSpace;

/** Allocate a space structure for greedy alignment
 *  At least num_space_arrays will be allocated, possibly more if the
 *  number is low.
 *
 *  @param num_space_arrays number of array elements to allocated [in]
 *  @return Pointer to allocated structure, or NULL upon failure
 */
SMBSpace* MBSpaceNew(int num_space_arrays);

/** Free the space structure 
    @param sp Linked list of structures to free
*/
void MBSpaceFree(SMBSpace* sp);

/** All auxiliary memory needed for the greedy extension algorithm. */
typedef struct SGreedyAlignMem {
   Int4** last_seq2_off;              /**< 2-D array of distances */
   Int4* max_score;                   /**< array of maximum scores */
   SGreedyOffset** last_seq2_off_affine;  /**< Like last_seq2_off but for 
                                               affine searches */
   Int4* diag_bounds;                 /**< bounds on ranges of diagonals */
   SMBSpace* space;                   /**< local memory pool for 
                                           SGreedyOffset structs */
} SGreedyAlignMem;

/** Perform the greedy extension algorithm with non-affine gap penalties.
 * @param seq1 First sequence (always uncompressed) [in]
 * @param len1 Maximal extension length in first sequence [in]
 * @param seq2 Second sequence (may be compressed) [in]
 * @param len2 Maximal extension length in second sequence [in]
 * @param reverse Is extension performed in backwards direction? [in]
 * @param xdrop_threshold X-dropoff value to use in extension [in]
 * @param match_cost Match score to use in extension [in]
 * @param mismatch_cost Mismatch score to use in extension [in]
 * @param seq1_align_len Length of extension on sequence 1 [out]
 * @param seq2_align_len Length of extension on sequence 2 [out]
 * @param aux_data Structure containing all preallocated memory [in]
 * @param edit_block Edit script structure for saving traceback. 
 *          Traceback is not saved if NULL is passed. [in] [out]
 * @param rem Offset within a byte of the compressed second sequence. 
 *          Set to 4 if sequence is uncompressed. [in]
 * @return The minimum distance between the two sequences, i.e.
 *          the number of mismatches plus gaps in the resulting alignment
 */
Int4 
BLAST_GreedyAlign (const Uint1* seq1, Int4 len1,
                   const Uint1* seq2, Int4 len2,
                   Boolean reverse, Int4 xdrop_threshold, 
                   Int4 match_cost, Int4 mismatch_cost,
                   Int4* seq1_align_len, Int4* seq2_align_len, 
                   SGreedyAlignMem* aux_data, 
                   GapPrelimEditBlock *edit_block, Uint1 rem);

/** Perform the greedy extension algorithm with affine gap penalties.
 * @param seq1 First sequence (always uncompressed) [in]
 * @param len1 Maximal extension length in first sequence [in]
 * @param seq2 Second sequence (may be compressed) [in]
 * @param len2 Maximal extension length in second sequence [in]
 * @param reverse Is extension performed in backwards direction? [in]
 * @param xdrop_threshold X-dropoff value to use in extension [in]
 * @param match_cost Match score to use in extension [in]
 * @param mismatch_cost Mismatch score to use in extension [in]
 * @param in_gap_open Gap opening penalty [in]
 * @param in_gap_extend Gap extension penalty [in]
 * @param seq1_align_len Length of extension on sequence 1 [out]
 * @param seq2_align_len Length of extension on sequence 2 [out]
 * @param aux_data Structure containing all preallocated memory [in]
 * @param edit_block Edit script structure for saving traceback. 
 *          Traceback is not saved if NULL is passed. [in] [out]
 * @param rem Offset within a byte of the compressed second sequence.
 *          Set to 4 if sequence is uncompressed. [in]
 * @return The score of the alignment
 */
Int4 
BLAST_AffineGreedyAlign (const Uint1* seq1, Int4 len1,
                         const Uint1* seq2, Int4 len2,
                         Boolean reverse, Int4 xdrop_threshold, 
                         Int4 match_cost, Int4 mismatch_cost,
                         Int4 in_gap_open, Int4 in_gap_extend,
                         Int4* seq1_align_len, Int4* seq2_align_len, 
                         SGreedyAlignMem* aux_data, 
                         GapPrelimEditBlock *edit_block, Uint1 rem);

#ifdef __cplusplus
}
#endif
#endif /* _GREEDY_H_ */
