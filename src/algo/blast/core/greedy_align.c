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
 * Author: Webb Miller and Co. Adopted for NCBI libraries by Sergey Shavirin
 *
 * Initial Creation Date: 10/27/1999
 *
 */

/** @file greedy_align.c
 * Greedy gapped alignment functions
 */

static char const rcsid[] = 
    "$Id$";

#include <algo/blast/core/greedy_align.h>
#include <algo/blast/core/blast_util.h> /* for NCBI2NA_UNPACK_BASE macros */

/** Constants used during greedy alignment */
enum {
    MAX_SPACE = 1000000, /**< The number of SThreeVal structures available
                              in a single link of the greedy memory pool */
    LARGE = 100000000,   /**< used to represent infinity */
};

/** Ensures that an edit script has enough memory allocated
    to hold a given number of edit operations

    @param script The script to examine [in/modified]
    @param total_ops Number of operations the script must support [in]
    @return 0 if successful, nonzero otherwise
*/
static Int2 
edit_script_realloc(MBGapEditScript *script, Uint4 total_ops)
{
    if (script->num_ops_allocated <= total_ops) {
        Uint4 new_size = total_ops * 3 / 2;
        MBEditOp *new_ops;
    
        new_ops = realloc(script->edit_ops, new_size * 
                                sizeof(MBEditOp));
        if (new_ops == NULL)
            return -1;

        script->edit_ops = new_ops;
        script->num_ops_allocated = new_size;
    }
    return 0;
}

/** Add an edit operation to an edit script

  @param script The edit script to update [in/modified]
  @param op_type The edit operation to add [in]
  @param num_ops The number of operations of the specified type [in]
  @return 0 on success, nonzero otherwise
*/
static Int2 
edit_script_add_new(MBGapEditScript *script, 
                enum EOpType op_type, Uint4 num_ops)
{
    if (edit_script_realloc(script, script->num_ops + 2) != 0)
        return -1;

    ASSERT(op_type != eEditOpError);

    script->last_op = op_type;
    script->edit_ops[script->num_ops].op_type = op_type;
    script->edit_ops[script->num_ops].num_ops = num_ops;
    script->num_ops++;

    return 0;
}

/** Initialize an edit script structure

  @param script Pointer to an uninitialized edit script
  @return The initialized edit script
*/
static MBGapEditScript *
edit_script_init(MBGapEditScript *script)
{
        script->edit_ops = NULL;
        script->num_ops_allocated = 0;
        script->num_ops = 0;
        script->last_op = eEditOpError;
        edit_script_realloc(script, 8);
        return script;
}

/** Add a new operation to an edit script, possibly combining
    it with the last operation if the two operations are identical

    @param script The edit script to update [in/modified]
    @param op_type The operation type to add [in]
    @param num_ops The number of the specified type of operation to add [in]
    @return 0 on success, nonzero otherwise
*/
static Int2 
edit_script_add(MBGapEditScript *script, 
                 enum EOpType op_type, Uint4 num_ops)
{
    if (op_type == eEditOpError) {
#ifdef ERR_POST_EX_DEFINED
        ErrPostEx(SEV_FATAL, 1, 0, 
                  "edit_script_more: bad opcode %d:%d", op_type, num_ops);
#endif
        return -1;
    }
    
    if (script->last_op == op_type)
        script->edit_ops[script->num_ops-1].num_ops += num_ops;
    else
        edit_script_add_new(script, op_type, num_ops);

    return 0;
}

/** See greedy_align.h for description */
MBGapEditScript *
MBGapEditScriptAppend(MBGapEditScript *dest_script, 
                      MBGapEditScript *src_script)
{
    Int4 i;

    for (i = 0; i < src_script->num_ops; i++) {
        MBEditOp *edit_op = src_script->edit_ops + i;

        edit_script_add(dest_script, edit_op->op_type, edit_op->num_ops);
    }

    return dest_script;
}

/** See greedy_align.h for description */
MBGapEditScript *
MBGapEditScriptNew(void)
{
    MBGapEditScript *script = calloc(1, sizeof(MBGapEditScript));
    if (script != NULL)
        return edit_script_init(script);
    return NULL;
}

/** See greedy_align.h for description */
MBGapEditScript *
MBGapEditScriptFree(MBGapEditScript *script)
{
    if (script == NULL)
        return NULL;

    if (script->edit_ops)
        sfree(script->edit_ops);

    sfree(script);
    return NULL;
}

/** Reverse the order of the operations in an edit script

    @param script The script to be reversed [in/modified]
    @return Pointer to the updated edit script
*/
static MBGapEditScript *
edit_script_reverse_inplace(MBGapEditScript *script)
{
    Uint4 i;
    const Uint4 num_ops = script->num_ops;
    const Uint4 mid = num_ops / 2;
    const Uint4 end = num_ops - 1;
    
    for (i = 0; i < mid; ++i) {
        const MBEditOp t = script->edit_ops[i];
        script->edit_ops[i] = script->edit_ops[end - i];
        script->edit_ops[end - i] = t;
    }
    return script;
}

/** see greedy_align.h for description */
SMBSpace* 
MBSpaceNew()
{
    SMBSpace* new_space;
    
    /** @todo FIXME: Later code assumes that a request will
       never be made for more than MAX_SPACE structures at once */

    new_space = (SMBSpace*)malloc(sizeof(SMBSpace));
    if (new_space == NULL)
        return NULL;

    new_space->space_array = (SThreeVal*)malloc(
                                   MAX_SPACE * sizeof(SThreeVal));
    if (new_space->space_array == NULL) {
        sfree(new_space);
        return NULL;
    }
    new_space->space_used = 0; 
    new_space->space_allocated = MAX_SPACE;
    new_space->next = NULL;

    return new_space;
}

/** Mark the input collection of space structures as unused
   @param space The space to mark
*/
static void 
refresh_mb_space(SMBSpace* space)
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

/** Allocate a specified number of SThreeVal structures from
    a memory pool

    @param pool The memory pool [in]
    @param amount The number of structures to allocate [in]
    @return Pointer to the allocated memory, or NULL in case of error
*/
static SThreeVal* 
get_mb_space(SMBSpace* pool, Int4 num_alloc)
{
    SThreeVal* out_ptr;
    if (num_alloc < 0) 
        return NULL;  
    
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

/** Compute the greatest common divisor of two numbers
    @param a First number [in]
    @param b Second number [in]
    @return The computed gcd
*/
static Int4 
gcd(Int4 a, Int4 b)
{
    Int4 c;
    if (a < b) {
        c = a;
        a = b; b = c;
    }
    while ((c = a % b) != 0) {
        a = b; b = c;
    }

    return b;
}

/** Compute the greatest common divisor of three numbers,
    divide that quantity out of each number, and return the computed GCD
    @param a Pointer to first number [in/modified]
    @param b Pointer to second number [in/modified]
    @param c Pointer to third number [in/modified]
    @return The computed gcd
*/
static Int4 
gdb3(Int4* a, Int4* b, Int4* c)
{
    Int4 g;
    if (*b == 0) 
        g = gcd(*a, *c);
    else 
        g = gcd(*a, gcd(*b, *c));
    if (g > 1) {
        *a /= g;
        *b /= g;
        *c /= g;
    }
    return g;
}

/** During the traceback for a greedy affine alignment,
    determine the state of the traceback that results
    from moving a specified number of diagonals upwards
    in the traceback array from a match

    @param flast_d 2-D Array of traceback scores [in]
    @param lower Array of per-diagonal lower bounds [in]
    @param upper Array of per-diagonal upper bounds [in]
    @param d Starting score / ending score(?) [in/modified]
    @param diag Starting diagonal(?) [in]
    @param Mis_cost Cost of a mismatch [in]
    @param row1 The row of the traceback array where
                the next score occurs(?) [out]
    @return The state for the next traceback operation
*/
static enum EOpType 
get_lastC(SThreeVal** flast_d, Int4* lower, Int4* upper, 
          Int4* d, Int4 diag, Int4 Mis_cost, Int4* row1)
{
    Int4 row;
    
    if (diag >= lower[(*d)-Mis_cost] && 
        diag <= upper[(*d)-Mis_cost]) {

        row = flast_d[(*d)-Mis_cost][diag].C;
        if (row >= MAX(flast_d[*d][diag].I, flast_d[*d][diag].D)) {
            *d = *d-Mis_cost;
            *row1 = row;
            return eEditOpReplace;
        }
    }
    if (flast_d[*d][diag].I > flast_d[*d][diag].D) {
        *row1 = flast_d[*d][diag].I;
        return eEditOpInsert;
    } 
    else {
        *row1 = flast_d[*d][diag].D;
        return eEditOpDelete;
    }
}

/** During the traceback for a greedy affine alignment,
    determine the state of the traceback that results
    from moving a specified number of diagonals upwards
    in the traceback array from an insertion or deletion

    @param flast_d 2-D Array of scores [in]
    @param lower Array of per-diagonal lower bounds [in]
    @param upper Array of per-diagonal upper bounds [in]
    @param d Starting score / ending score(?) [in/modified]
    @param diag Starting diagonal(?) [in]
    @param GO_cost Cost to open a gap [in]
    @param GE_cost Cost to extend a gap [in]
    @param IorD The state of the traceback at present [in]
    @return The state for the next traceback operation
*/
static enum EOpType 
get_last_ID(SThreeVal** flast_d, Int4* lower, Int4* upper, 
            Int4* d, Int4 diag, Int4 GO_cost, 
            Int4 GE_cost, enum EOpType IorD)
{
    Int4 ndiag; 
    Int4 row;

    if (IorD == eEditOpInsert)
        ndiag = diag - 1;
    else 
        ndiag = diag + 1;

    if (ndiag >= lower[(*d)-GE_cost] && 
        ndiag <= upper[(*d)-GE_cost]) {

        if (IorD == eEditOpInsert)
            row = flast_d[(*d)-GE_cost][ndiag].I;
        else
            row = flast_d[(*d)-GE_cost][ndiag].D;
    }
    else {
        row = -100;
    }

    if (ndiag >= lower[(*d)-GO_cost-GE_cost] && 
        ndiag <= upper[(*d)-GO_cost-GE_cost] && 
        row < flast_d[(*d)-GO_cost-GE_cost][ndiag].C) {

        *d = (*d) - GO_cost - GE_cost;
        return eEditOpReplace;
    }
    *d = (*d) - GE_cost;
    return IorD;
}

/** During the traceback for a non-affine greedy alignment,
    compute the diagonal that the next traceback operation
    will use

    @param flast_d 2-D Array of scores [in]
    @param d Starting score(?) [in]
    @param diag Starting diagonal(?) [in]
    @param row1 The next traceback row to examine(?) [out]
    @return The next diagonal in the traceback
*/
static Int4 
get_last(Int4 **flast_d, Int4 d, Int4 diag, Int4 *row1)
{
    if (flast_d[d-1][diag-1] > MAX(flast_d[d-1][diag], flast_d[d-1][diag+1])) {
        *row1 = flast_d[d-1][diag-1];
        return diag - 1;
    } 
    if (flast_d[d-1][diag] > flast_d[d-1][diag+1]) {
        *row1 = flast_d[d-1][diag];
        return diag;
    }
    *row1 = flast_d[d-1][diag+1];
    return diag + 1;
}

/** see greedy_align.h for description */
Int4 BLAST_GreedyAlign(const Uint1* seq1, Int4 len1,
                       const Uint1* seq2, Int4 len2,
                       Boolean reverse, Int4 xdrop_threshold, 
                       Int4 match_cost, Int4 mismatch_cost,
                       Int4* extent1, Int4* extent2, 
                       SGreedyAlignMem* aux_data, 
                       MBGapEditScript *script, Uint1 rem)
{
    Int4 col;               /* column number */
    Int4 row;               /* row number */
    Int4 d;                 /* current distance */
    Int4 k;                 /* current diagonal */
    Int4 flower, fupper;    /* boundaries for searching diagonals */
    Int4 MAX_D;             /* maximum cost */
    Int4 ORIGIN;
    Int4 final_dist = 0;
    Int4** flast_d = aux_data->flast_d; /* rows containing the last d */
    Int4* max_row;                      /* reached for cost d=0, ... len1.  */
    
    Int4 match_cost_half = match_cost / 2;
    Int4 op_cost = mismatch_cost + match_cost;
    Int4 d_dropoff = ICEIL(xdrop_threshold + match_cost_half, op_cost);
    
    Int4 cur_max; 
    Int4 b_diag = 0; 
    Int4 best_diag = INT4_MAX / 2;
    Int4* max_row_free = aux_data->max_row_free;
    char nlower, nupper;
    SMBSpace* mem_pool = aux_data->space;
    Int4 max_len = len2;
 
    MAX_D = (Int4) (len1 / GREEDY_MAX_COST_FRACTION + 1);
    ORIGIN = MAX_D + 2;
    *extent1 = 0;
    *extent2 = 0;
    
    /* find the offset of the first mismatch between
       seq1 and seq2 */

    row = 0;
    if (reverse) {
        if (rem == 4) {
            while (row < len1 && row < len2) {
                if (seq2[len2-1-row] != seq1[len1-1-row])
                    break;
                row++;
            }
        } 
        else {
            while (row < len1 && row < len2) {
                if (seq2[len2-1-row] != 
                                NCBI2NA_UNPACK_BASE(seq1[(len1-1-row) / 4], 
                                                    3 - (len1-1-row) % 4)) 
                    break;
                row++;
            }
        }
    } 
    else {
        if (rem == 4) {
            while (row < len1 && row < len2) {
                if (seq2[row] != seq1[row])
                    break; 
                row++;
            }
        } 
        else {
            while (row < len1 && row < len2) {
                if (seq2[row] != NCBI2NA_UNPACK_BASE(seq1[(row+rem) / 4], 
                                                     3 - (row+rem) % 4))
                    break;
                row++;
            }
        }
    }

    /* update the extents of the alignment, and bail out
       early if no further work is needed. */

    *extent1 = row;
    *extent2 = row;
    if (row == len1 || row == len2) {
        if (script != NULL)
            edit_script_add(script, eEditOpReplace, row);
        return final_dist;
    }

    /* set up the memory pool */

    if (script == NULL) {
       mem_pool = NULL;
    } 
    else if (mem_pool == NULL) {
       aux_data->space = mem_pool = MBSpaceNew();
    } 
    else { 
        refresh_mb_space(mem_pool);
    }
    
    max_row = max_row_free + d_dropoff;
    for (k = 0; k < d_dropoff; k++)
        max_row_free[k] = 0;
    
    flast_d[0][ORIGIN] = row;
    max_row[0] = row * match_cost;
    flower = ORIGIN - 1;
    fupper = ORIGIN + 1;
    d = 1;
    nupper = nlower = 0;

    while (d <= MAX_D) {
        Int4 x, flower0, fupper0;

        flast_d[d - 1][flower-1] = -1;
        flast_d[d - 1][flower] = -1;
        flast_d[d - 1][fupper] = -1;
        flast_d[d - 1][fupper+1] = -1;

        x = max_row[d - d_dropoff] + op_cost * d - xdrop_threshold;
        x = ICEIL(x, match_cost_half);        
        cur_max = 0;
        flower0 = flower;
        fupper0 = fupper;

        for (k = flower0; k <= fupper0; k++) {
            row = MAX(flast_d[d - 1][k + 1], flast_d[d - 1][k]) + 1;
            row = MAX(row, flast_d[d - 1][k - 1]);
            col = row + k - ORIGIN;
            if (row + col >= x) {
                fupper = k;
            }
            else {
                if (k == flower)
                    flower++;
                else
                    flast_d[d][k] = -1;
                continue;
            }
            
            if (row > max_len || row < 0) {
                flower = k + 1; 
                nlower = 1;
            } 
            else {
                /* Slide down the diagonal. */
                if (reverse) {
                    if (rem == 4) {
                        while (row < len2 && col < len1 && 
                               seq2[len2-1-row] == seq1[len1-1-col]) {
                            ++row; ++col;
                        }
                    } 
                    else {
                        while (row < len2 && col < len1 && seq2[len2-1-row] == 
                               NCBI2NA_UNPACK_BASE(seq1[(len1-1-col) / 4],
                                                    3 - (len1-1-col) % 4)) {
                            ++row; ++col;
                        }
                    }
                } 
                else {
                    if (rem == 4) {
                        while (row < len2 && col < len1 && 
                               seq2[row] == seq1[col]) {
                            ++row; ++col;
                        }
                    } 
                    else {
                        while (row < len2 && col < len1 && seq2[row] == 
                               NCBI2NA_UNPACK_BASE(seq1[(col+rem) / 4],
                                                    3 - (col+rem) % 4)) {
                            ++row; ++col;
                        }
                    }
                }
            }
            flast_d[d][k] = row;
            if (row + col > cur_max) {
                cur_max = row + col;
                b_diag = k;
            }
            if (row == len2) {
                flower = k + 1; 
                nlower = 1;
            }
            if (col == len1) {
                fupper = k - 1; 
                nupper = 1;
            }
        }

        k = cur_max * match_cost_half - d * op_cost;
        if (max_row[d - 1] < k) {
            max_row[d] = k;
            final_dist = d;
            best_diag = b_diag;
            *extent2 = flast_d[d][b_diag];
            *extent1 = (*extent2) + b_diag - ORIGIN;
        } 
        else {
            max_row[d] = max_row[d - 1];
        }
        if (flower > fupper)
            break;

        d++;
        if (nlower == 0) 
            flower--; 
        if (nupper == 0) 
            fupper++;
        if (script == NULL) {
           flast_d[d] = flast_d[d - 2];
        }
        else {
           /* space array consists of SThreeVal structures which are 
              3 times larger than Int4, so divide requested amount by 3
           */
           flast_d[d] = (Int4*) get_mb_space(mem_pool, 
                                             (fupper - flower + 7) / 3);
           if (flast_d[d] != NULL)
              flast_d[d] = flast_d[d] - flower + 2;
           else
              return final_dist;
        }
    }
    
    /* perform traceback if desired */

    if (script != NULL) {
        Int4 diag;

        d = final_dist; 
        diag = best_diag;
        row = *extent2; 
        col = *extent1;

        while (d > 0) {
            Int4 row1, col1, diag1;
            diag1 = get_last(flast_d, d, diag, &row1);
            col1 = row1 + diag1 - ORIGIN;
            if (diag1 == diag) {
                if (row - row1 > 0) {
                    edit_script_add(script, eEditOpReplace, row - row1);
                }
            } 
            else if (diag1 < diag) {
                if (row - row1 > 0) {
                    edit_script_add(script, eEditOpReplace, row - row1);
                }
                edit_script_add(script, eEditOpInsert, 1);
            } 
            else {
                if (row - row1 - 1 > 0) {
                    edit_script_add(script, eEditOpReplace, row - row1 - 1);
                }
                edit_script_add(script, eEditOpDelete, 1);
            }
            d--; 
            diag = diag1; 
            col = col1; 
            row = row1;
        }
        edit_script_add(script, eEditOpReplace, flast_d[0][ORIGIN]);
        if (!reverse) 
            edit_script_reverse_inplace(script);
    }
    return final_dist;
}

/** See greedy_align.h for description */
Int4 BLAST_AffineGreedyAlign (const Uint1* seq1, Int4 len1, 
                              const Uint1* seq2, Int4 len2,
                              Boolean reverse, Int4 xdrop_threshold, 
                              Int4 match_score, Int4 mismatch_score, 
                              Int4 gap_open, Int4 gap_extend,
                              Int4* extent1, Int4* extent2, 
                              SGreedyAlignMem* aux_data, 
                              MBGapEditScript *script, Uint1 rem)
{
    Int4 col;                        /* column number */
    Int4 row;                        /* row number */
    Int4 d;                        /* current distance */
    Int4 k;                        /* current diagonal */
    Int4 flower, fupper;         /* boundaries for searching diagonals */
    Int4 MAX_D;                         /* maximum cost */
    Int4 ORIGIN;
    Int4 final_score = 0;
    SThreeVal** flast_d;        /* rows containing the last d */
    Int4* max_row_free;
    Int4* max_row;                /* reached for cost d=0, ... len1.  */
    Int4 Mis_cost, GO_cost, GE_cost;
    Int4 D_diff, gd;
    Int4 match_score_half;
    Int4 max_cost;
    Int4 *lower, *upper;
    
    Int4 cur_max; 
    Int4 b_diag = 0; 
    Int4 best_diag = INT4_MAX/2;
    char nlower = 0, nupper = 0;
    SMBSpace* mem_pool = aux_data->space;
    Int4 stop_condition;
    Int4 max_d;
    Int4* uplow_free;
    Int4 max_len = len2;
 
    if (match_score % 2 == 1) {
        match_score *= 2;
        mismatch_score *= 2;
        xdrop_threshold *= 2;
        gap_open *= 2;
        gap_extend *= 2;
    }

    if (gap_open == 0 && gap_extend == 0) {
       return BLAST_GreedyAlign(seq1, len1, seq2, len2, reverse, 
                                   xdrop_threshold, match_score, 
                                   mismatch_score, extent1, extent2, 
                                   aux_data, script, rem);
    }
    
    match_score_half = match_score / 2;
    Mis_cost = mismatch_score + match_score;
    GO_cost = gap_open;
    GE_cost = gap_extend + match_score_half;
    gd = gdb3(&Mis_cost, &GO_cost, &GE_cost);
    D_diff = ICEIL(xdrop_threshold + match_score_half, gd);
    
    MAX_D = (Int4) (len1/GREEDY_MAX_COST_FRACTION + 1);
    max_d = MAX_D * GE_cost;
    ORIGIN = MAX_D + 2;
    max_cost = MAX(Mis_cost, GO_cost + GE_cost);
    *extent1 = 0;
    *extent2 = 0;
    
    /* find the offset of the first mismatch between
       seq1 and seq2 */

    row = 0;
    if (reverse) {
        if (rem == 4) {
            while (row < len1 && row < len2) {
                if (seq2[len2-1-row] != seq1[len1-1-row])
                    break;
                row++;
            }
        } 
        else {
            while (row < len1 && row < len2) {
                if (seq2[len2-1-row] != 
                                NCBI2NA_UNPACK_BASE(seq1[(len1-1-row) / 4], 
                                                    3 - (len1-1-row) % 4)) 
                    break;
                row++;
            }
        }
    } 
    else {
        if (rem == 4) {
            while (row < len1 && row < len2) {
                if (seq2[row] != seq1[row])
                    break; 
                row++;
            }
        } 
        else {
            while (row < len1 && row < len2) {
                if (seq2[row] != NCBI2NA_UNPACK_BASE(seq1[(row+rem) / 4], 
                                                     3 - (row+rem) % 4))
                    break;
                row++;
            }
        }
    }

    /* update the extents of the alignment, and bail out
       early if no further work is needed */

    *extent1 = row;
    *extent2 = row;
    if (row == len1 || row == len2) {
        if (script != NULL)
            edit_script_add(script, eEditOpReplace, row);
        return row * match_score;
    }

    /* set up the memory pool */

    if (script == NULL) {
        mem_pool = NULL;
    } 
    else if (!mem_pool) {
       aux_data->space = mem_pool = MBSpaceNew();
    } 
    else { 
        refresh_mb_space(mem_pool);
    }

    flast_d = aux_data->flast_d_affine;
    max_row_free = aux_data->max_row_free;
    max_row = max_row_free + D_diff;
    for (k = 0; k < D_diff; k++)
        max_row_free[k] = 0;

    uplow_free = aux_data->uplow_free;
    lower = uplow_free;
    upper = uplow_free + max_d + 1 + max_cost;

    /* set boundary for -1,-2,...,-max_cost+1*/
    for (k = 0; k < max_cost; k++) {
        lower[k] = LARGE;  
        upper[k] = -LARGE;
    }
    lower += max_cost;
    upper += max_cost; 
    
    flast_d[0][ORIGIN].C = row;
    flast_d[0][ORIGIN].I = -2;
    flast_d[0][ORIGIN].D = -2;
    max_row[0] = row * match_score;
    lower[0] = ORIGIN;
    upper[0] = ORIGIN;
    flower = ORIGIN - 1;
    fupper = ORIGIN + 1;
    
    d = 1;
    stop_condition = 1;
    while (d <= max_d) {
        Int4 x, flower0, fupper0;

        x = max_row[d - D_diff] + gd * d - xdrop_threshold;
        x = ICEIL(x, match_score_half);
        if (x < 0) 
            x = 0;
        cur_max = 0;
        flower0 = flower;
        fupper0 = fupper;

        for (k = flower0; k <= fupper0; k++) {
            row = -2;
            if (k+1 <= upper[d-GO_cost-GE_cost] && 
                k+1 >= lower[d-GO_cost-GE_cost]) {
                row = flast_d[d-GO_cost-GE_cost][k+1].C;
            }
            if (k+1  <= upper[d-GE_cost] && k+1 >= lower[d-GE_cost] &&
                row < flast_d[d-GE_cost][k+1].D) {
                row = flast_d[d-GE_cost][k+1].D;
            }
            row++;

            if (2 * row + k - ORIGIN >= x) {
                flast_d[d][k].D = row;
            }
            else {
                flast_d[d][k].D = -2;
            }
            row = -1; 

            if (k-1 <= upper[d-GO_cost-GE_cost] && 
                k-1 >= lower[d-GO_cost-GE_cost]) {
                row = flast_d[d-GO_cost-GE_cost][k-1].C;
            }
            if (k-1 <= upper[d-GE_cost] && 
                k-1 >= lower[d-GE_cost] &&
                row < flast_d[d-GE_cost][k-1].I) {
                row = flast_d[d-GE_cost][k-1].I;
            }
            if (2 * row + k - ORIGIN >= x) {
                flast_d[d][k].I = row;
            }
            else {
                flast_d[d][k].I = -2;
            }
            
            row = MAX(flast_d[d][k].I, flast_d[d][k].D);
            if (k <= upper[d-Mis_cost] && 
                k >= lower[d-Mis_cost]) {
                row = MAX(flast_d[d-Mis_cost][k].C+1,row);
            }
            
            col = row + k - ORIGIN;
            if (row + col >= x) {
                fupper = k;
            }
            else {
                if (k == flower)
                    flower++;
                else
                    flast_d[d][k].C = -2;
                continue;
            }

            if (row > max_len || row < -2) {
                flower = k; nlower = k+1; 
            } 
            else {
                /* slide down the diagonal */
                if (reverse) {
                    if (rem == 4) {
                        while (row < len2 && col < len1 && 
                               seq2[len2-1-row] == seq1[len1-1-col]) {
                            ++row; ++col;
                        }
                    } 
                    else {
                        while (row < len2 && col < len1 && seq2[len2-1-row] == 
                               NCBI2NA_UNPACK_BASE(seq1[(len1-1-col) / 4],
                                                    3 - (len1-1-col) % 4)) {
                            ++row; ++col;
                        }
                    }
                } 
                else {
                    if (rem == 4) {
                        while (row < len2 && col < len1 && 
                               seq2[row] == seq1[col]) {
                            ++row; ++col;
                        }
                    } 
                    else {
                        while (row < len2 && col < len1 && seq2[row] == 
                               NCBI2NA_UNPACK_BASE(seq1[(col+rem) / 4],
                                                    3 - (col+rem) % 4)) {
                            ++row; ++col;
                        }
                    }
                }
            }
            flast_d[d][k].C = row;
            if (row + col > cur_max) {
                cur_max = row + col;
                b_diag = k;
            }
            if (row == len2) {
                flower = k; 
                nlower = k+1;
            }
            if (col == len1) {
                fupper = k; 
                nupper = k-1;
            }
        }

        k = cur_max * match_score_half - d * gd;
        if (max_row[d - 1] < k) {
            max_row[d] = k;
            final_score = d;
            best_diag = b_diag;
            *extent2 = flast_d[d][b_diag].C;
            *extent1 = (*extent2) + b_diag - ORIGIN;
        } 
        else {
            max_row[d] = max_row[d - 1];
        }

        if (flower <= fupper) {
            stop_condition++;
            lower[d] = flower; 
            upper[d] = fupper;
        } 
        else {
            lower[d] = LARGE; 
            upper[d] = -LARGE;
        }

        if (lower[d-max_cost] <= upper[d-max_cost]) 
            stop_condition--;
        if (stop_condition == 0) 
            break;
        
        d++;
        flower = MIN(lower[d-Mis_cost], 
                     MIN(lower[d-GO_cost-GE_cost], lower[d-GE_cost])-1);
        if (nlower > 0) 
            flower = MAX(flower, nlower);

        fupper = MAX(upper[d-Mis_cost], 
                     MAX(upper[d-GO_cost-GE_cost], upper[d-GE_cost])+1);
        if (nupper > 0) 
            fupper = MIN(fupper, nupper);

        if (d > max_cost) {
           if (script == NULL) {
               flast_d[d] = flast_d[d - max_cost-1];
           } 
           else {
               flast_d[d] = get_mb_space(mem_pool, fupper-flower+1)-flower;
               if (flast_d[d] == NULL)
                   return final_score;
           }
        }
    }
    
    /* compute the traceback if desired */

    if (script != NULL) { 
        Int4 row1, diag; 
        enum EOpType state;

        d = final_score; 
        diag = best_diag;
        row = *extent2; 
        state = eEditOpReplace;
        while (d > 0) {
            if (state == eEditOpReplace) {
                /* diag unchanged */
                state = get_lastC(flast_d, lower, upper, 
                                  &d, diag, Mis_cost, &row1);
                if (row - row1 > 0) 
                    edit_script_add(script, eEditOpReplace, row-row1);
                row = row1;
            } 
            else {
                if (state == eEditOpInsert) {
                    /*row unchanged */
                    state = get_last_ID(flast_d, lower, upper, &d, 
                                  diag, GO_cost, GE_cost, eEditOpInsert);
                    diag--;
                    edit_script_add(script, eEditOpInsert, 1);
                } 
                else {
                    edit_script_add(script, eEditOpDelete, 1);
                    state = get_last_ID(flast_d, lower, upper, &d, 
                                  diag, GO_cost, GE_cost, eEditOpDelete);
                    diag++;
                    row--;
                }
            }
        }
        edit_script_add(script, eEditOpReplace, flast_d[0][ORIGIN].C);
        if (!reverse) 
            edit_script_reverse_inplace(script);
    }
    final_score = max_row[final_score];
    return final_score;
}
