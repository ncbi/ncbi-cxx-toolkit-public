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
 * Functions to perform greedy affine and non-affine gapped alignment
 *
 */

/** @file greedy_align.c
 * Functions to perform greedy affine and non-affine gapped alignment.
 * Reference:
 * Zhang et. al., "A Greedy Algorithm for Aligning DNA Sequences"
 * Journal of Computational Biology vol 7 pp 203-214
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/greedy_align.h>
#include <algo/blast/core/blast_util.h> /* for NCBI2NA_UNPACK_BASE macros */

/** see greedy_align.h for description */
SMBSpace* 
MBSpaceNew()
{
    SMBSpace* new_space;
    const Int4 kMaxSpace = 1000000; 

    new_space = (SMBSpace*)malloc(sizeof(SMBSpace));
    if (new_space == NULL)
        return NULL;

    new_space->space_array = (SGreedyOffset*)malloc(
                                   kMaxSpace * sizeof(SGreedyOffset));
    if (new_space->space_array == NULL) {
        sfree(new_space);
        return NULL;
    }
    new_space->space_used = 0; 
    new_space->space_allocated = kMaxSpace;
    new_space->next = NULL;

    return new_space;
}

/** Mark the input collection of space structures as unused
   @param space The space to mark
*/
static void 
s_RefreshMBSpace(SMBSpace* space)
{
    while (space != NULL) {
        space->space_used = 0;
        space = space->next;
    }
}

/** see greedy_align.h for description */
void MBSpaceFree(SMBSpace* space)
{
    SMBSpace* next_space;

    while (space != NULL) {
        next_space = space->next;
        sfree(space->space_array);
        sfree(space);
        space = next_space;
    }
}

/** Allocate a specified number of SGreedyOffset structures from
    a memory pool

    @param pool The memory pool [in]
    @param num_alloc The number of structures to allocate [in]
    @return Pointer to the allocated memory, or NULL in case of error
*/
static SGreedyOffset* 
s_GetMBSpace(SMBSpace* pool, Int4 num_alloc)
{
    SGreedyOffset* out_ptr;
    if (num_alloc < 0) 
        return NULL;  
    
    /** @todo FIXME: Calling code must never ask for more
        than kMaxSpace structures (defined in MBSpaceNew()) */

    while (pool->space_used + num_alloc > pool->space_allocated) {
       if (pool->next == NULL) {
          pool->next = MBSpaceNew();
          if (pool->next == NULL) {
#ifdef ERR_POST_EX_DEFINED
             ErrPostEx(SEV_WARNING, 0, 0, "Cannot get new space for greedy extension");
#endif
             return NULL;
          }
       }
       pool = pool->next;
    }

    out_ptr = pool->space_array + pool->space_used;
    pool->space_used += num_alloc;
    
    return out_ptr;
}

/** During the traceback for a greedy alignment with affine
    gap penalties, determine the next state of the traceback after
    moving upwards in the traceback array from a substitution

    @param last_seq2_off Array of offsets into the second sequence;
                        last_seq2_off[d][k] gives the largest offset into
                        the second sequence that lies on diagonal k and
                        has distance d [in]
    @param diag_lower Array of lower bounds on diagonal index [in]
    @param diag_upper Array of upper bounds on diagonal index [in]
    @param d Starting distance [in][out]
    @param diag Starting diagonal [in]
    @param op_cost The sum of the match and mismatch scores [in]
    @param seq2_index The offset into the second sequence after the traceback
                operation has completed [out]
    @return The state for the next traceback operation
*/
static EGapAlignOpType 
s_GetNextAffineTbackFromMatch(SGreedyOffset** last_seq2_off, Int4* diag_lower, 
                           Int4* diag_upper, Int4* d, Int4 diag, Int4 op_cost, 
                           Int4* seq2_index)
{
    Int4 new_seq2_index;
    
    if (diag >= diag_lower[(*d) - op_cost] && 
        diag <= diag_upper[(*d) - op_cost]) {

        new_seq2_index = last_seq2_off[(*d) - op_cost][diag].match_off;
        if (new_seq2_index >= MAX(last_seq2_off[*d][diag].insert_off, 
                                  last_seq2_off[*d][diag].delete_off)) {
            *d -= op_cost;
            *seq2_index = new_seq2_index;
            return eGapAlignSub;
        }
    }
    if (last_seq2_off[*d][diag].insert_off > 
                        last_seq2_off[*d][diag].delete_off) {
        *seq2_index = last_seq2_off[*d][diag].insert_off;
        return eGapAlignIns;
    } 
    else {
        *seq2_index = last_seq2_off[*d][diag].delete_off;
        return eGapAlignDel;
    }
}

/** During the traceback for a greedy alignment with affine
    gap penalties, determine the next state of the traceback after
    moving upwards in the traceback array from an insertion or deletion

    @param last_seq2_off Array of offsets into the second sequence;
                        last_seq2_off[d][k] gives the largest offset into
                        the second sequence that lies on diagonal k and
                        has distance d [in]
    @param diag_lower Array of lower bounds on diagonal index [in]
    @param diag_upper Array of upper bounds on diagonal index [in]
    @param d Starting distance [in][out]
    @param diag Starting diagonal [in]
    @param gap_open open a gap [in]
    @param gap_extend (Modified) cost to extend a gap [in]
    @param IorD The state of the traceback at present [in]
    @return The state for the next traceback operation
*/
static EGapAlignOpType 
s_GetNextAffineTbackFromIndel(SGreedyOffset** last_seq2_off, Int4* diag_lower, 
                    Int4* diag_upper, Int4* d, Int4 diag, Int4 gap_open, 
                    Int4 gap_extend, EGapAlignOpType IorD)
{
    Int4 new_diag; 
    Int4 new_seq2_index;
    Int4 gap_open_extend = gap_open + gap_extend;

    if (IorD == eGapAlignIns)
        new_diag = diag - 1;
    else 
        new_diag = diag + 1;

    if (new_diag >= diag_lower[(*d) - gap_extend] && 
        new_diag <= diag_upper[(*d) - gap_extend]) {

        if (IorD == eGapAlignIns)
            new_seq2_index = 
                    last_seq2_off[(*d) - gap_extend][new_diag].insert_off;
        else
            new_seq2_index = 
                    last_seq2_off[(*d) - gap_extend][new_diag].delete_off;
    }
    else {
        new_seq2_index = -100;
    }

    if (new_diag >= diag_lower[(*d) - gap_open_extend] && 
        new_diag <= diag_upper[(*d) - gap_open_extend] && 
        new_seq2_index < 
                last_seq2_off[(*d) - gap_open_extend][new_diag].match_off) {

        *d -= gap_open_extend;
        return eGapAlignSub;
    }

    *d -= gap_extend;
    return IorD;
}

/** During the traceback for a non-affine greedy alignment,
    compute the distance that will result from the next 
    traceback operation

    @param last_seq2_off Array of offsets into the second sequence;
                        last_seq2_off[d][k] gives the largest offset into
                        the second sequence that lies on diagonal k and
                        has distance d [in]
    @param d Starting distance [in]
    @param diag Index of diagonal that produced the starting distance [in]
    @param seq2_index The offset into the second sequence after the traceback
                operation has completed [out]
    @return The next distance remaining after the traceback operation
*/
static Int4 
s_GetNextNonAffineTback(Int4 **last_seq2_off, Int4 d, 
                        Int4 diag, Int4 *seq2_index)
{
    if (last_seq2_off[d-1][diag-1] > 
                MAX(last_seq2_off[d-1][diag], last_seq2_off[d-1][diag+1])) {
        *seq2_index = last_seq2_off[d-1][diag-1];
        return diag - 1;
    } 
    if (last_seq2_off[d-1][diag] > last_seq2_off[d-1][diag+1]) {
        *seq2_index = last_seq2_off[d-1][diag];
        return diag;
    }
    *seq2_index = last_seq2_off[d-1][diag+1];
    return diag + 1;
}

/** see greedy_align.h for description */
Int4 BLAST_GreedyAlign(const Uint1* seq1, Int4 len1,
                       const Uint1* seq2, Int4 len2,
                       Boolean reverse, Int4 xdrop_threshold, 
                       Int4 match_cost, Int4 mismatch_cost,
                       Int4* seq1_align_len, Int4* seq2_align_len, 
                       SGreedyAlignMem* aux_data, 
                       GapPrelimEditBlock *edit_block, Uint1 rem)
{
    Int4 seq1_index;
    Int4 seq2_index;
    Int4 index;
    Int4 d;
    Int4 k;
    Int4 diag_lower, diag_upper;
    Int4 max_dist;
    Int4 diag_origin;
    Int4 best_dist;
    Int4 best_diag;
    Int4** last_seq2_off;
    Int4* max_score;
    Int4 xdrop_offset;
    Boolean end1_reached, end2_reached;
    SMBSpace* mem_pool;
    const Int4 kInvalidOffset = -1;
 
    /* ordinary dynamic programming alignment, for each offset
       in seq1, walks through offsets in seq2 until an X-dropoff
       test fails, saving the best score encountered along 
       the way. Instead of score, this code tracks the 'distance'
       (number of mismatches plus number of gaps) between seq1
       and seq2. Instead of walking through sequence offsets, it
       walks through diagonals that can achieve a given distance */

    best_dist = 0;
    best_diag = 0;

    /* set the number of distinct distances the algorithm will
       examine in the search for an optimal alignment. The 
       heuristic worst-case running time of the algorithm is 
       O(max_dist**2 + (len1+len2)); for sequences which are
       very similar, the average running time will be sig-
       nificantly better than this */

    max_dist = len2 / GREEDY_MAX_COST_FRACTION + 1;

    /* the main loop assumes that the index of all diagonals is
       biased to lie in the middle of allocated bookkeeping 
       structures */

    diag_origin = max_dist + 2;

    /* last_seq2_off[d][k] is the largest offset into seq2 that
       lies on diagonal k and has distance d */

    last_seq2_off = aux_data->last_seq2_off;

    /* Instead of tracking the best alignment score and using
       xdrop_theshold directly, track the best score for each
       unique distance and use the best score for some previously
       computed distance to implement the X-dropoff test.

       xdrop_offset gives the distance backwards in the score
       array to look */

    xdrop_offset = (xdrop_threshold + match_cost / 2) / 
                           (match_cost + mismatch_cost) + 1;
    
    /* find the offset of the first mismatch between seq1 and seq2 */

    index = 0;
    if (reverse) {
        if (rem == 4) {
            while (index < len1 && index < len2) {
                if (seq1[len1-1 - index] != seq2[len2-1 - index])
                    break;
                index++;
            }
        } 
        else {
            while (index < len1 && index < len2) {
                if (seq1[len1-1 - index] != 
                          NCBI2NA_UNPACK_BASE(seq2[(len2-1 - index) / 4], 
                                              3 - (len2-1 - index) % 4)) 
                    break;
                index++;
            }
        }
    } 
    else {
        if (rem == 4) {
            while (index < len1 && index < len2) {
                if (seq1[index] != seq2[index])
                    break; 
                index++;
            }
        } 
        else {
            while (index < len1 && index < len2) {
                if (seq1[index] != 
                          NCBI2NA_UNPACK_BASE(seq2[(index + rem) / 4], 
                                              3 - (index + rem) % 4))
                    break;
                index++;
            }
        }
    }

    /* update the extents of the alignment, and bail out
       early if no further work is needed */

    *seq1_align_len = index;
    *seq2_align_len = index;
    seq1_index = index;

    if (index == len1 || index == len2) {
        if (edit_block != NULL)
            GapPrelimEditBlockAdd(edit_block, eGapAlignSub, index);
        return best_dist;
    }

    /* set up the memory pool */

    mem_pool = aux_data->space;
    if (edit_block == NULL) {
       mem_pool = NULL;
    } 
    else if (mem_pool == NULL) {
       aux_data->space = mem_pool = MBSpaceNew();
    } 
    else { 
        s_RefreshMBSpace(mem_pool);
    }
    
    /* set up the array of per-distance maximum scores. There
       are max_diags + xdrop_offset distances to track, the first
       xdrop_offset of which are 0 */

    max_score = aux_data->max_score + xdrop_offset;
    for (index = 0; index < xdrop_offset; index++)
        aux_data->max_score[index] = 0;
    
    /* fill in the initial offsets of the distance matrix */

    last_seq2_off[0][diag_origin] = seq1_index;
    max_score[0] = seq1_index * match_cost;
    diag_lower = diag_origin - 1;
    diag_upper = diag_origin + 1;
    end1_reached = end2_reached = FALSE;

    /* for each distance */

    for (d = 1; d <= max_dist; d++) {
        Int4 xdrop_score;
        Int4 curr_extent;
        Int4 curr_score;
        Int4 curr_diag;
        Int4 orig_diag_lower;
        Int4 orig_diag_upper;

        /* assign impossible seq2 offsets to any diagonals that
           are not in the range (diag_lower,diag_upper).
           These will serve as sentinel values for the
           inner loop */

        last_seq2_off[d - 1][diag_lower-1] = kInvalidOffset;
        last_seq2_off[d - 1][diag_lower] = kInvalidOffset;
        last_seq2_off[d - 1][diag_upper] = kInvalidOffset;
        last_seq2_off[d - 1][diag_upper+1] = kInvalidOffset;

        /* compute the score for distance d that corresponds to
           the X-dropoff criterion */

        xdrop_score = max_score[d - xdrop_offset] + 
                      (match_cost + mismatch_cost) * d - xdrop_threshold;
        xdrop_score = (Int4)ceil((double)xdrop_score / (match_cost / 2)); 
        curr_extent = 0;
        orig_diag_lower = diag_lower;
        orig_diag_upper = diag_upper;

        /* for each diagonal of interest */

        for (k = orig_diag_lower; k <= orig_diag_upper; k++) {

            /* find the largest offset into seq2 that increases
               the distance from d-1 to d (i.e. keeps the alignment
               from getting worse for as long as possible), then 
               choose the offset into seq1 that will keep the
               resulting diagonal fixed at k 
             
               Note that this requires kInvalidOffset to be smaller
               than any valid offset into seq2, i.e. to be negative */

            seq2_index = MAX(last_seq2_off[d - 1][k + 1], 
                             last_seq2_off[d - 1][k    ]) + 1;
            seq2_index = MAX(seq2_index, last_seq2_off[d - 1][k - 1]);
            seq1_index = seq2_index + k - diag_origin;

            if (seq1_index + seq2_index >= xdrop_score) {

                /* passed X-dropoff test; set the new current
                   upper bound on diagonals to test */

                diag_upper = k;
            }
            else {

                /* failed the X-dropoff test; remove the current
                   diagonal from consideration, possibly narrowing
                   the range of diagonals to test */

                if (k == diag_lower)
                    diag_lower++;
                else
                    last_seq2_off[d][k] = kInvalidOffset;
                continue;
            }
            
            /* make sure the chosen index has not walked off seq2 */

            if (seq2_index > len2 || seq2_index < 0) {
                diag_lower = k + 1; 
                end2_reached = TRUE;
            } 
            else {

                /* slide down diagonal k until a mismatch 
                   occurs. As long as only matches are encountered,
                   the current distance d will not change */

                if (reverse) {
                    if (rem == 4) {
                        while (seq1_index < len1 && seq2_index < len2 && 
                                        seq1[len1-1 - seq1_index] == 
                                        seq2[len2-1 - seq2_index]) {
                            ++seq1_index;
                            ++seq2_index;
                        }
                    } 
                    else {
                        while (seq1_index < len1 && seq2_index < len2 && 
                            seq1[len1-1 - seq1_index] == 
                            NCBI2NA_UNPACK_BASE(seq2[(len2-1-seq2_index) / 4],
                                                 3 - (len2-1-seq2_index) % 4)) {
                            ++seq1_index;
                            ++seq2_index;
                        }
                    }
                } 
                else {
                    if (rem == 4) {
                        while (seq1_index < len1 && seq2_index < len2 && 
                               seq1[seq1_index] == seq2[seq2_index]) {
                            ++seq1_index;
                            ++seq2_index;
                        }
                    } 
                    else {
                        while (seq1_index < len1 && seq2_index < len2 && 
                            seq1[seq1_index] == 
                            NCBI2NA_UNPACK_BASE(seq2[(seq2_index + rem) / 4],
                                                 3 - (seq2_index + rem) % 4)) {
                            ++seq1_index;
                            ++seq2_index;
                        }
                    }
                }
            }

            /* set the new largest seq2 offset that achieves
               distance d on diagonal k */

            last_seq2_off[d][k] = seq2_index;

            /* since all values of k are constrained to have the
               same distance d, the value of k which maximizes the
               alignment score is the one that covers the most
               of seq1 and seq2 */

            if (seq1_index + seq2_index > curr_extent) {
                curr_extent = seq1_index + seq2_index;
                curr_diag = k;
            }

            /* clamp the bounds on diagonals to avoid walking off
               either sequence */

            if (seq2_index == len2) {
                diag_lower = k + 1; 
                end2_reached = TRUE;
            }
            if (seq1_index == len1) {
                diag_upper = k - 1; 
                end1_reached = TRUE;
            }
        }   /* end loop over diagonals */

        /* compute the maximum score possible for distance d */

        curr_score = curr_extent * (match_cost / 2) - 
                        d * (match_cost + mismatch_cost);

        /* if this is the best score seen so far, update the
           statistics of the best alignment */

        if (curr_score > max_score[d - 1]) {
            max_score[d] = curr_score;
            best_dist = d;
            best_diag = curr_diag;
            *seq2_align_len = last_seq2_off[d][best_diag];
            *seq1_align_len = (*seq2_align_len) + best_diag - diag_origin;
        } 
        else {
            max_score[d] = max_score[d - 1];
        }

        /* alignment has finished if the lower and upper bounds
           on diagonals to check have converged to each other */

        if (diag_lower > diag_upper)
            break;

        /* set up for the next distance to examine */

        if (!end2_reached)
            diag_lower--; 
        if (!end1_reached)
            diag_upper++;

        /* if no traceback is specified, the next row of
           last_seq2_off can reuse previously allocated memory */

        if (edit_block == NULL) {

            /** @todo FIXME The following assumes two arrays of
                at least max_dist+4 Int4's have already been allocated */

            last_seq2_off[d + 1] = last_seq2_off[d - 1];
        }
        else {

            /* traceback requires all rows of last_seq2_off to be saved,
               so a new row must be allocated. The allocator provides 
               SThreeVal structures which are 3 times larger than Int4, 
               so divide requested amount by 3 */

            /** @todo FIXME Should make allocator more general */

            last_seq2_off[d + 1] = (Int4*) s_GetMBSpace(mem_pool, 
                                     (diag_upper - diag_lower + 7) / 3);

            /* move the origin for this row backwards */

            last_seq2_off[d + 1] = last_seq2_off[d + 1] - diag_lower + 2;
        }
    }   /* end loop over distinct distances */

    
    if (edit_block == NULL)
        return best_dist;

    /* perform traceback */

    d = best_dist; 
    seq1_index = *seq1_align_len;
    seq2_index = *seq2_align_len; 

    /* for all positive distances */

    while (d > 0) {
        Int4 new_diag;
        Int4 new_seq1_index;
        Int4 new_seq2_index;

        /* retrieve the value of the diagonal after the next
           traceback operation. best_diag starts off with the
           value computed during the alignment process */

        new_diag = s_GetNextNonAffineTback(last_seq2_off, d, 
                                           best_diag, &new_seq2_index);
        new_seq1_index = new_seq2_index + new_diag - diag_origin;

        if (new_diag == best_diag) {

            /* same diagonal: issue a group of substitutions */

            if (seq2_index - new_seq2_index > 0) {
                GapPrelimEditBlockAdd(edit_block, eGapAlignSub, 
                                seq2_index - new_seq2_index);
            }
        } 
        else if (new_diag < best_diag) {

            /* smaller diagonal: issue a group of substitutions
               and then a gap in seq2 */

            if (seq2_index - new_seq2_index > 0) {
                GapPrelimEditBlockAdd(edit_block, eGapAlignSub, 
                                seq2_index - new_seq2_index);
            }
            GapPrelimEditBlockAdd(edit_block, eGapAlignIns, 1);
        } 
        else {
            /* larger diagonal: issue a group of substitutions
               and then a gap in seq1 */

            if (seq2_index - new_seq2_index - 1 > 0) {
                GapPrelimEditBlockAdd(edit_block, eGapAlignSub,
                                seq2_index - new_seq2_index -1);
            }
            GapPrelimEditBlockAdd(edit_block, eGapAlignDel, 1);
        }
        d--; 
        best_diag = new_diag; 
        seq1_index = new_seq1_index;
        seq2_index = new_seq2_index; 
    }

    /* handle the final group of substitutions back to distance zero,
       i.e. back to offset zero of seq1 and seq2 */

    GapPrelimEditBlockAdd(edit_block, eGapAlignSub, 
                          last_seq2_off[0][diag_origin]);

    return best_dist;
}

/** See greedy_align.h for description */
Int4 BLAST_AffineGreedyAlign (const Uint1* seq1, Int4 len1, 
                              const Uint1* seq2, Int4 len2,
                              Boolean reverse, Int4 xdrop_threshold, 
                              Int4 match_score, Int4 mismatch_score, 
                              Int4 in_gap_open, Int4 in_gap_extend,
                              Int4* seq1_align_len, Int4* seq2_align_len, 
                              SGreedyAlignMem* aux_data, 
                              GapPrelimEditBlock *edit_block, Uint1 rem)
{
    Int4 seq1_index;
    Int4 seq2_index;
    Int4 index;
    Int4 d;
    Int4 k;
    Int4 max_dist;
    Int4 scaled_max_dist;
    Int4 diag_origin;
    Int4 best_dist;
    Int4 best_diag;
    SGreedyOffset** last_seq2_off;
    Int4* max_score;
    Int4 xdrop_offset;
    Int4 end1_diag, end2_diag;
    SMBSpace* mem_pool;

    Int4 op_cost;
    Int4 gap_open;
    Int4 gap_extend;
    Int4 gap_open_extend;
    Int4 max_penalty;
    Int4 score_common_factor;
    Int4 match_score_half;

    Int4 *diag_lower; 
    Int4 *diag_upper;
    Int4 curr_diag_lower; 
    Int4 curr_diag_upper;

    Int4 num_nonempty_dist;
    const Int4 kInvalidDiag = 100000000; /* larger than any valid diag. index */
 
    /* make sure bits of match_score don't disappear if it
       is divided by 2 */

    if (match_score % 2 == 1) {
        match_score *= 2;
        mismatch_score *= 2;
        xdrop_threshold *= 2;
        in_gap_open *= 2;
        in_gap_extend *= 2;
    }

    if (in_gap_open == 0 && in_gap_extend == 0) {
       return BLAST_GreedyAlign(seq1, len1, seq2, len2, reverse, 
                                xdrop_threshold, match_score, 
                                mismatch_score, seq1_align_len, 
                                seq2_align_len, aux_data, edit_block, 
                                rem);
    }
    
    /* ordinary dynamic programming alignment, for each offset
       in seq1, walks through offsets in seq2 until an X-dropoff
       test fails, saving the best score encountered along 
       the way. Instead of score, this code tracks the 'distance'
       (number of mismatches plus number of gaps) between seq1
       and seq2. Instead of walking through sequence offsets, it
       walks through diagonals that can achieve a given distance */

    best_dist = 0;
    best_diag = 0;

    /* fill in derived scores and penalties */

    match_score_half = match_score / 2;
    op_cost = match_score + mismatch_score;
    gap_open = in_gap_open;
    gap_extend = in_gap_extend + match_score_half;
    gap_open_extend = gap_open + gap_extend;
    score_common_factor = BLAST_Gdb3(&op_cost, &gap_open, &gap_extend);
    max_penalty = MAX(op_cost, gap_open_extend);
    
    /* set the number of distinct distances the algorithm will
       examine in the search for an optimal alignment */

    max_dist = len2 / GREEDY_MAX_COST_FRACTION + 1;
    scaled_max_dist = max_dist * gap_extend;
    
    /* the main loop assumes that the index of all diagonals is
       biased to lie in the middle of allocated bookkeeping structures */

    diag_origin = max_dist + 2;

    /* last_seq2_off[d][k] is the largest offset into seq2 that
       lies on diagonal k and has distance d. Unlike the non-affine
       case, the largest offset for paths ending in an insertion,
       deletion, and match must all be separately saved for
       each d and k */

    last_seq2_off = aux_data->last_seq2_off_affine;

    /* Instead of tracking the best alignment score and using
       xdrop_theshold directly, track the best score for each
       unique distance and use the best score for some previously
       computed distance to implement the X-dropoff test.

       xdrop_offset gives the distance backwards in the score
       array to look */

    xdrop_offset = (xdrop_threshold + match_score_half) / 
                                      score_common_factor + 1;

    /* find the offset of the first mismatch between seq1 and seq2 */

    index = 0;
    if (reverse) {
        if (rem == 4) {
            while (index < len1 && index < len2) {
                if (seq1[len1-1 - index] != seq2[len2-1 - index])
                    break;
                index++;
            }
        } 
        else {
            while (index < len1 && index < len2) {
                if (seq1[len1-1 - index] != 
                          NCBI2NA_UNPACK_BASE(seq2[(len2-1 - index) / 4], 
                                              3 - (len2-1 - index) % 4)) 
                    break;
                index++;
            }
        }
    } 
    else {
        if (rem == 4) {
            while (index < len1 && index < len2) {
                if (seq1[index] != seq2[index])
                    break; 
                index++;
            }
        } 
        else {
            while (index < len1 && index < len2) {
                if (seq1[index] != 
                          NCBI2NA_UNPACK_BASE(seq2[(index + rem) / 4], 
                                              3 - (index + rem) % 4))
                    break;
                index++;
            }
        }
    }

    /* update the extents of the alignment, and bail out
       early if no further work is needed */

    *seq1_align_len = index;
    *seq2_align_len = index;
    seq1_index = index;

    if (index == len1 || index == len2) {
        if (edit_block != NULL)
            GapPrelimEditBlockAdd(edit_block, eGapAlignSub, index);
        return best_dist;
    }

    /* set up the memory pool */

    mem_pool = aux_data->space;
    if (edit_block == NULL) {
        mem_pool = NULL;
    } 
    else if (!mem_pool) {
       aux_data->space = mem_pool = MBSpaceNew();
    } 
    else { 
        s_RefreshMBSpace(mem_pool);
    }

    /* set up the array of per-distance maximum scores. There
       are scaled_max_diags + xdrop_offset distances to track, 
       the first xdrop_offset of which are 0 */

    max_score = aux_data->max_score + xdrop_offset;
    for (index = 0; index < xdrop_offset; index++)
        aux_data->max_score[index] = 0;

    /* For affine greedy alignment, contributions to distance d
       can come from distances further back than d-1 (which is
       sufficient for non-affine alignment). Where non-affine
       alignment only needs to track the current bounds on diagonals
       to test, the present code must also track bounds for 
       max_penalty previous distances. These share the same
       preallocated array */

    diag_lower = aux_data->diag_bounds;
    diag_upper = aux_data->diag_bounds + 
                 scaled_max_dist + 1 + max_penalty;

    /* the first max_penalty elements correspond to negative
       distances; initialize with an empty range of diagonals */

    for (index = 0; index < max_penalty; index++) {
        diag_lower[index] = kInvalidDiag;  
        diag_upper[index] = -kInvalidDiag;
    }
    diag_lower += max_penalty;
    diag_upper += max_penalty; 
    
    /* fill in the initial offsets of the distance matrix */

    last_seq2_off[0][diag_origin].match_off = seq1_index;
    last_seq2_off[0][diag_origin].insert_off = -2;
    last_seq2_off[0][diag_origin].delete_off = -2;
    max_score[0] = seq1_index * match_score;
    diag_lower[0] = diag_origin;
    diag_upper[0] = diag_origin;
    curr_diag_lower = diag_origin - 1;
    curr_diag_upper = diag_origin + 1;
    end1_diag = 0;
    end2_diag = 0;
    num_nonempty_dist = 1;
    d = 1;

    /* for each distance */

    while (d <= scaled_max_dist) {
        Int4 xdrop_score;
        Int4 curr_score;
        Int4 curr_extent;
        Int4 curr_diag;
        Int4 tmp_diag_lower;
        Int4 tmp_diag_upper;

        /* compute the score for distance d that corresponds to
           the X-dropoff criterion */

        xdrop_score = max_score[d - xdrop_offset] + 
                      score_common_factor * d - xdrop_threshold;
        xdrop_score = (Int4)ceil((double)xdrop_score / match_score_half);
        if (xdrop_score < 0) 
            xdrop_score = 0;

        /* for each diagonal of interest */

        curr_extent = 0;
        tmp_diag_lower = curr_diag_lower;
        tmp_diag_upper = curr_diag_upper;

        for (k = tmp_diag_lower; k <= tmp_diag_upper; k++) {

            seq2_index = -2;

            /* As with the non-affine algorithm, the object is
               to find the largest offset into seq2 that can
               achieve distance d from diagonal k. Here, however,
               contributions are possible from distances < d-1 */

            /* begin by assuming the best offset comes from opening
               a gap in seq1. Since opening a gap costs gap_open_extend,
               use the offset associated with a match from that
               far back in the table. Do not use diagonal k+1 if
               it was not valid back then */

            if (k + 1 <= diag_upper[d - gap_open_extend] && 
                k + 1 >= diag_lower[d - gap_open_extend]) {
                seq2_index = last_seq2_off[d - gap_open_extend][k+1].match_off;
            }

            /* Replace with the offset derived from extending a gap
               in seq1, if that is larger */

            if (k + 1 <= diag_upper[d - gap_extend] && 
                k + 1 >= diag_lower[d - gap_extend] &&
                seq2_index < last_seq2_off[d - gap_extend][k+1].delete_off) {
                seq2_index = last_seq2_off[d - gap_extend][k+1].delete_off;
            }
            seq2_index++;

            /* Whether or not this offset will be used, save it
               if it passes the X-dropoff test */

            if (2 * seq2_index + k - diag_origin >= xdrop_score) {
                last_seq2_off[d][k].delete_off = seq2_index;
            }
            else {
                last_seq2_off[d][k].delete_off = -2;
            }


            seq2_index = -1; 

            /* repeat the process assuming a gap is opened or
               extended in seq2 */

            if (k - 1 <= diag_upper[d - gap_open_extend] && 
                k - 1 >= diag_lower[d - gap_open_extend]) {
                seq2_index = last_seq2_off[d - gap_open_extend][k-1].match_off;
            }
            if (k - 1 <= diag_upper[d - gap_extend] && 
                k - 1 >= diag_lower[d - gap_extend] &&
                seq2_index < last_seq2_off[d - gap_extend][k-1].insert_off) {
                seq2_index = last_seq2_off[d - gap_extend][k-1].insert_off;
            }
            if (2 * seq2_index + k - diag_origin >= xdrop_score) {
                last_seq2_off[d][k].insert_off = seq2_index;
            }
            else {
                last_seq2_off[d][k].insert_off = -2;
            }
            
            /* Compare the greater of the two previous answers with
               the offset associated with a match on diagonal k. */

            seq2_index = MAX(last_seq2_off[d][k].insert_off, 
                             last_seq2_off[d][k].delete_off);
            if (k <= diag_upper[d - op_cost] && 
                k >= diag_lower[d - op_cost]) {
                seq2_index = MAX(seq2_index, 
                                 last_seq2_off[d - op_cost][k].match_off + 1);
            }
            
            /* choose the offset into seq1 so as to remain on diagonal k */

            seq1_index = seq2_index + k - diag_origin;

            /* perform the X-dropoff test, adjusting the current
               bounds on diagonals to check */

            if (seq1_index + seq2_index >= xdrop_score) {
                curr_diag_upper = k;
            }
            else {
                if (k == curr_diag_lower)
                    curr_diag_lower++;
                else
                    last_seq2_off[d][k].match_off = -2;
                continue;
            }

            if (seq2_index > len2 || seq2_index < -2) {
                curr_diag_lower = k; 
                end2_diag = k + 1; 
            } 
            else {

                /* slide down diagonal k until a mismatch 
                   occurs. As long as only matches are encountered,
                   the current distance d will not change */

                if (reverse) {
                    if (rem == 4) {
                        while (seq1_index < len1 && seq2_index < len2 && 
                                        seq1[len1-1 - seq1_index] == 
                                        seq2[len2-1 - seq2_index]) {
                            ++seq1_index;
                            ++seq2_index;
                        }
                    } 
                    else {
                        while (seq1_index < len1 && seq2_index < len2 && 
                            seq1[len1-1 - seq1_index] == 
                            NCBI2NA_UNPACK_BASE(seq2[(len2-1-seq2_index) / 4],
                                                 3 - (len2-1-seq2_index) % 4)) {
                            ++seq1_index;
                            ++seq2_index;
                        }
                    }
                } 
                else {
                    if (rem == 4) {
                        while (seq1_index < len1 && seq2_index < len2 && 
                               seq1[seq1_index] == seq2[seq2_index]) {
                            ++seq1_index;
                            ++seq2_index;
                        }
                    } 
                    else {
                        while (seq1_index < len1 && seq2_index < len2 && 
                            seq1[seq1_index] == 
                            NCBI2NA_UNPACK_BASE(seq2[(seq2_index + rem) / 4],
                                                 3 - (seq2_index + rem) % 4)) {
                            ++seq1_index;
                            ++seq2_index;
                        }
                    }
                }
            }

            /* since all values of k are constrained to have the
               same distance d, the value of k which maximizes the
               alignment score is the one that covers the most
               of seq1 and seq2 */

            last_seq2_off[d][k].match_off = seq2_index;
            if (seq1_index + seq2_index > curr_extent) {
                curr_extent = seq1_index + seq2_index;
                curr_diag = k;
            }

            /* clamp the bounds on diagonals to avoid walking off
               either sequence */

            if (seq1_index == len1) {
                curr_diag_upper = k; 
                end1_diag = k - 1;
            }
            if (seq2_index == len2) {
                curr_diag_lower = k; 
                end2_diag = k + 1;
            }
        }  /* end loop over diagonals */

        /* compute the maximum score possible for distance d */

        curr_score = curr_extent * match_score_half - d * score_common_factor;

        /* if this is the best score seen so far, update the
           statistics of the best alignment */

        if (curr_score > max_score[d - 1]) {
            max_score[d] = curr_score;
            best_dist = d;
            best_diag = curr_diag;
            *seq2_align_len = last_seq2_off[d][best_diag].match_off;
            *seq1_align_len = (*seq2_align_len) + best_diag - diag_origin;
        } 
        else {
            max_score[d] = max_score[d - 1];
        }

        /* save the bounds on diagonals to examine for distance d.
           Note that in the non-affine case the alignment could stop
           if these bounds converged to each other. Here, however,
           it's possible for distances less than d to continue the
           alignment even if no diagonals are available at distance d.
           Hence we can only stop if max_penalty consecutive ranges
           of diagonals are empty */

        if (curr_diag_lower <= curr_diag_upper) {
            num_nonempty_dist++;
            diag_lower[d] = curr_diag_lower; 
            diag_upper[d] = curr_diag_upper;
        } 
        else {
            diag_lower[d] = kInvalidDiag; 
            diag_upper[d] = -kInvalidDiag;
        }

        if (diag_lower[d - max_penalty] <= diag_upper[d - max_penalty]) 
            num_nonempty_dist--;

        if (num_nonempty_dist == 0) 
            break;
        
        /* compute the range of diagonals to test for the next
           value of d. These must be conservative, in that any
           diagonal that could possibly contribute must be allowed */

        d++;
        curr_diag_lower = MIN(diag_lower[d - gap_open_extend], 
                              diag_lower[d - gap_extend]) - 1;
        curr_diag_lower = MIN(curr_diag_lower, diag_lower[d - op_cost]);

        if (end2_diag > 0) 
            curr_diag_lower = MAX(curr_diag_lower, end2_diag);

        curr_diag_upper = MAX(diag_upper[d - gap_open_extend], 
                              diag_upper[d - gap_extend]) + 1;
        curr_diag_upper = MAX(curr_diag_upper,
                              diag_upper[d - op_cost]);

        if (end1_diag > 0) 
            curr_diag_upper = MIN(curr_diag_upper, end1_diag);

        if (d > max_penalty) {
            if (edit_block == NULL) {

                /* if no traceback is specified, the next row of
                   last_seq2_off can reuse previously allocated memory */

                last_seq2_off[d] = last_seq2_off[d - max_penalty - 1];
            } 
            else {

                /* traceback requires all rows of last_seq2_off to be saved,
                   so a new row must be allocated */

                last_seq2_off[d] = s_GetMBSpace(mem_pool, 
                                   curr_diag_upper - curr_diag_lower + 1) - 
                                   curr_diag_lower;
            }
        }
    }  /* end loop over distances */
    
    /* compute the traceback if desired */

    if (edit_block != NULL) { 
        Int4 new_seq2_index;
        EGapAlignOpType state;

        d = best_dist; 
        seq2_index = *seq2_align_len; 
        state = eGapAlignSub;

        while (d > 0) {
            if (state == eGapAlignSub) {
                /* substitution */
                state = s_GetNextAffineTbackFromMatch(last_seq2_off, 
                                       diag_lower, diag_upper, &d, best_diag, 
                                       op_cost, &new_seq2_index);

                if (seq2_index - new_seq2_index > 0) 
                    GapPrelimEditBlockAdd(edit_block, eGapAlignSub, 
                                    seq2_index - new_seq2_index);

                seq2_index = new_seq2_index;
            } 
            else if (state == eGapAlignIns) {
                /* gap in seq1 */
                state = s_GetNextAffineTbackFromIndel(last_seq2_off, 
                                     diag_lower, diag_upper, &d, best_diag, 
                                     gap_open, gap_extend, eGapAlignIns);
                best_diag--;
                GapPrelimEditBlockAdd(edit_block, eGapAlignIns, 1);
            } 
            else {
                /* gap in seq2 */
                GapPrelimEditBlockAdd(edit_block, eGapAlignDel, 1);
                state = s_GetNextAffineTbackFromIndel(last_seq2_off, 
                                     diag_lower, diag_upper, &d, best_diag, 
                                     gap_open, gap_extend, eGapAlignDel);
                best_diag++;
                seq2_index--;
            }
        }

        GapPrelimEditBlockAdd(edit_block, eGapAlignSub, 
                        last_seq2_off[0][diag_origin].match_off);
    }

    return max_score[best_dist];
}
