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
 * Author:  Greg Boratyn
 *
 * Implementaion of Jumper alignment
 *
 */

#include "blast_gapalign_priv.h"

#include <algo/blast/core/blast_nalookup.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/hspfilter_mapper.h>

#include "jumper.h"


JUMP jumper_default [] = {
 {1, 1, 9, 0},  /* this was added for illumina */
 {1, 0, 10, 0},    /* insert in 1 */ 
 {0, 1, 10, 0},    /* deletion in 1 */
 {2, 0, 10, 0}, 
 {0, 2, 10, 0}, 
 {3, 0, 13, 0},
 {0, 3, 13, 0},
 {2, 2, 12, 0}, /* try double mismatch */
 {1, 0, 10, 2},
 {0, 1, 10, 2},
 {2, 0, 10, 2}, 
 {0, 2, 10, 2}, 
 {3, 0, 13, 2},
 {0, 3, 13, 2},
/* removed for illumina
 {4, 0, 15, 3},
 {0, 4, 15, 3},
 {5, 0, 15, 3},
 {0, 5, 15, 3},
 {6, 0, 15, 3},
 {0, 6, 15, 3},
 {7, 0, 15, 3},
 {0, 7, 15, 3},
 {8, 0, 15, 3},
 {0, 8, 15, 3},
 {9, 0, 15, 3},
 {0, 9, 15, 3},
 {10, 0, 15, 3},
 {0, 10, 15, 3},
*/
 {1, 1, 0, 0}    /* default is punctual */
} ;

JUMP jumper_edge [] = {
    {1, 0, 6, 0},
    {0, 1, 6, 0},
    {1, 0, 2, 0},
    {0, 1, 2, 0},
    {1, 1, 0, 0}
};


JUMP jumper_ungapped [] = {
 {1, 1, 0, 0}    /* default is punctual */
};


#define GET_BASE(seq, pos, compressed) (compressed ? NCBI2NA_UNPACK_BASE(seq[pos >> 2], 3 - (pos & 3)) : seq[pos])


#define UNPACK_BASE_OLD(seq, pos) ((((seq)[(pos) >> 2] << (2 * ((pos) & 3))) & 0xC0) >> 6)


#define UNPACK_BASE(seq, pos) (NCBI2NA_UNPACK_BASE((seq)[(pos) / 4], 3 - ((pos) & 3)))


#define JUMPER_EDIT_BLOCK_ADD(block, op) ((block)->edit_ops[(block)->num_ops++] = op)


/* convert jumper edit op type to BLAST op type */
#define JOP_TO_OP(op) (op >= 0 ? eGapAlignSub : (op == JUMPER_INSERTION ? eGapAlignIns : eGapAlignDel))

/* get number of ops */
#define JOP_TO_NUM(op) (op > 0 ? op : 1)


static JumperPrelimEditBlock* JumperPrelimEditBlockNew(Int4 size)
{
    JumperPrelimEditBlock* retval = calloc(1, sizeof(JumperPrelimEditBlock));
    if (!retval) {
        return NULL;
    }

    retval->edit_ops = calloc(size, sizeof(JumperOpType));
    if (!retval->edit_ops) {
        free(retval);
        return NULL;
    }

    retval->num_allocated = size;
    return retval;
}

static JumperPrelimEditBlock* JumperPrelimEditBlockFree(
                                                JumperPrelimEditBlock* block)
{
    if (block) {
        if (block->edit_ops) {
            free(block->edit_ops);
        }

        free(block);
    }

    return NULL;
}

Int4 JumperPrelimEditBlockAdd(JumperPrelimEditBlock* block, JumperOpType op)
{
    if (block->num_ops >= block->num_allocated) {
        block->edit_ops = realloc(block->edit_ops,
                              block->num_allocated * sizeof(JumperOpType) * 2);

        if (!block->edit_ops) {
            return -1;
        }
        block->num_allocated *= 2;
    }

    if (block->num_ops > 0 && block->edit_ops[block->num_ops - 1] > 0 &&
        op > 0) {

        block->edit_ops[block->num_ops - 1] += op;
    }
    else {
        block->edit_ops[block->num_ops++] = op;
    }

    return 0;
}


static void s_CreateTable(Uint4* table)
{
    int i, k;
    for (i = 0; i < 256; i++) {
        table[i] = 0;
        for (k = 0;k < 4; k++) {
            Uint4 cell = ((i >> (2 * k)) & 3);
            switch (k) {
            case 0: table[i] += cell << 3 * 8; break;
            case 1: table[i] += cell << 2 * 8; break;
            case 2: table[i] += cell << 1 * 8; break;
            case 3: table[i] += cell; break;
            default: ASSERT(0);
            }
        }
    }
}


JumperGapAlign* JumperGapAlignFree(JumperGapAlign* jgap_align)
{
    if (!jgap_align) {
        return NULL;
    }

    JumperPrelimEditBlockFree(jgap_align->left_prelim_block);
    JumperPrelimEditBlockFree(jgap_align->right_prelim_block);
    if (jgap_align->table) {
        free(jgap_align->table);
    }
    sfree(jgap_align);
    return NULL;
}


JumperGapAlign* JumperGapAlignNew(Int4 size)
{
    JumperGapAlign* retval = (JumperGapAlign*)calloc(1, sizeof(JumperGapAlign));
    if (!retval) {
        return NULL;
    }

    retval->left_prelim_block = JumperPrelimEditBlockNew(size);
    if (!retval->left_prelim_block) {
        JumperGapAlignFree(retval);
        return NULL;
    }

    retval->right_prelim_block = JumperPrelimEditBlockNew(size);
    if (!retval->right_prelim_block) {
        JumperGapAlignFree(retval);
        return NULL;
    }

    retval->table = calloc(256, sizeof(Uint4));
    if (!retval->table) {
        JumperGapAlignFree(retval);
        return NULL;
    }
    s_CreateTable(retval->table);

    return retval;
}

/* Reset prelim edit blocks so that they can be used for a new alignment */
static void s_ResetJumperPrelimEditBlocks(JumperPrelimEditBlock* left,
                                          JumperPrelimEditBlock* right)
{
    if (!left || !right) {
        return;
    }

    left->num_ops = 0;
    right->num_ops = 0;
}


/* Get query and subject position at the edit_index element of the edit script.
   The jumper edit script must be going forwadr (from right extension) and
   *query_pos and *subject_pos must be initialized to start positions of
   the HSP. The resulting positions will be in *query_pos and *subject_pos. */
static int s_GetSeqPositions(JumperPrelimEditBlock* edit_script, 
                             Int4 edit_index,
                             Int4* query_pos, Int4* subject_pos)
{
    Int4 j;

    ASSERT(edit_script);
    ASSERT(query_pos);
    ASSERT(subject_pos);
    ASSERT(edit_index < edit_script->num_ops);

    if (!edit_script || !query_pos || !subject_pos) {
        return -1;
    }

    for (j = 0;j < edit_index;j++) {
        ASSERT(j < edit_script->num_ops);
        
        if (edit_script->edit_ops[j] == JUMPER_MISMATCH) {
            (*query_pos)++;
            (*subject_pos)++;
        }
        else if (edit_script->edit_ops[j] == JUMPER_INSERTION) {
            (*query_pos)++;
        }
        else if (edit_script->edit_ops[j] == JUMPER_DELETION) {
            (*subject_pos)++;
        }
        else {
            *query_pos += edit_script->edit_ops[j];
            *subject_pos += edit_script->edit_ops[j];
        }
    }

    return 0;
}


/* Shift gaps to the right using the forward edit script (right extension).
   Alignment score and number of matches may improve if input the alignment
   is not optimal */
static int s_ShiftGapsRight(JumperPrelimEditBlock* edit_script,
                            const Uint1* query, const Uint1* subject,
                            Int4 query_offset, Int4 subject_offset,
                            Int4 query_length, Int4 subject_length,
                            Int4* score, Int4 err_score, Int4* num_identical)
{
    Int4 i;
    Int4 q_pos;
    Int4 s_pos;

    ASSERT(score && num_identical);

    /* iterate over edit script elements */
    for (i = 0;i < edit_script->num_ops;i++) {

        /* if insertion or deltion */
        if (edit_script->edit_ops[i] < 0) {

            /* find the end of the gap; gap start will be marked with i and
               k will be right after gap end */
            Int4 k = i + 1;
            while (k < edit_script->num_ops &&
                   edit_script->edit_ops[k] == edit_script->edit_ops[i]) {
                k++;
            }

            /* iterate over the next script elements and see if any can switch
               sides with the indel */
            for (;k < edit_script->num_ops;k++) {
                ASSERT(edit_script->edit_ops[i] < 0);

                /* if a deletion follows insertion (or the other way around),
                   change it into a mismatch, it is one error instead of two */
                if (edit_script->edit_ops[k] < 0 &&
                    edit_script->edit_ops[k] != edit_script->edit_ops[i]) {
                    Int4 j;

                    /* remove one indel */
                    for (j = k;j < edit_script->num_ops - 1;j++) {
                        edit_script->edit_ops[j] =
                            edit_script->edit_ops[j + 1];
                    }
                    edit_script->num_ops--;
                    /* change the leftmost indel to a mismatch */
                    edit_script->edit_ops[i] = JUMPER_MISMATCH;
                    /* update poiner to the beginning of the gap */
                    i++;

                    /* update score */
                    (*score) -= err_score;

                    /* check if the new mismatch is a match and update 
                       scores and identity if it is */
                    q_pos = query_offset;
                    s_pos = subject_offset;
                    s_GetSeqPositions(edit_script, i - 1, &q_pos, &s_pos);
                    if (query[q_pos] == UNPACK_BASE(subject, s_pos)) {
                        edit_script->edit_ops[i - 1] = 1;
                        (*score)++;
                        (*num_identical)++;
                    }

                    /* if there a no indels left, search for a new gap */
                    if (i >= 0) {
                        break;
                    }
                }

                /* if a mismatch follows an indel, move the mismatch to the
                   left; it will result in a different mismatch or a macth,
                   so alignment score will not decrease */
                if (edit_script->edit_ops[k] == JUMPER_MISMATCH) {
                    /* shift the mismatch to the left by swapping ops */
                    JumperOpType tmp = edit_script->edit_ops[i];
                    edit_script->edit_ops[i] = edit_script->edit_ops[k];
                    edit_script->edit_ops[k] = tmp;
                    i++;

                    /* check if mismatch did not change to a match and update 
                       scores and identity if it did */
                    q_pos = query_offset;
                    s_pos = subject_offset;
                    s_GetSeqPositions(edit_script, i - 1, &q_pos, &s_pos);
                    if (query[q_pos] == UNPACK_BASE(subject, s_pos)) {
                        edit_script->edit_ops[i - 1] = 1;
                        (*score) -= err_score;
                        (*score)++;
                        (*num_identical)++;
                    }
                }

                /* if matches follow an indel, check if bases to the right
                   of the indel match bases to the left and move the matches
                   if they do */
                if (edit_script->edit_ops[k] > 0) {

                    Int4 num_matches = (Int4)edit_script->edit_ops[k];
                    Int4 /*j, */ b = 0;

                    q_pos = query_offset;
                    s_pos = subject_offset;
                    s_GetSeqPositions(edit_script, i, &q_pos, &s_pos);

                    /* find how many bases match */
                    while (b < edit_script->edit_ops[k] &&
                           query[q_pos] == UNPACK_BASE(subject, s_pos)) {

                        b++;
                        q_pos++;
                        s_pos++;
                        ASSERT(q_pos <= query_length);
                        ASSERT(s_pos <= subject_length);
                    }

                    /* if none, look for a new indel */
                    if (b == 0) {
                        break;
                    }

                    /* otherwise shift the gap to the right */
                    ASSERT(i > 0);
                    /* if there are no matches preceding the indel, add a new
                       script element */
                    if (edit_script->edit_ops[i - 1] <= 0) {
                        Int4 j;
                        /* make space */
                        JumperPrelimEditBlockAdd(edit_script, JUMPER_MISMATCH);
                        for (j = edit_script->num_ops - 1;j > i;j--) {
                            edit_script->edit_ops[j] =
                                edit_script->edit_ops[j - 1];
                        }
                        k++;
                        i++;
                        /* new matches will be here */
                        edit_script->edit_ops[i - 1] = 0;
                    }
                    /* move matching bases to the left */
                    edit_script->edit_ops[i - 1] += b;
                    edit_script->edit_ops[k] -= b;

                    /* if not all matches were moved, search for a new indel */
                    if (b < num_matches) {
                        break;
                    }
                    else {
                        /* otherwise remove the script element that held the
                           moved matches */
                        Int4 j;
                        ASSERT(edit_script->edit_ops[k] == 0);
                        for (j = k;j < edit_script->num_ops - 1;j++) {
                            edit_script->edit_ops[j] =
                                edit_script->edit_ops[j + 1];
                        }
                        edit_script->num_ops--;
                        ASSERT(edit_script->num_ops > 0);
                        k--;
                    }
                }
            }

            /* move indel pointer to the end of the indel, to find a new one */
            i = k - 1;
        } 
    }
    

    return 0;
}


/* Shift gaps to the right and update scores and identities */
static int s_ShiftGaps(BlastGapAlignStruct* gap_align,
                       const Uint1* query, const Uint1* subject,
                       Int4 query_length, Int4 subject_length,
                       Int4 err_score, Int4* num_identical)
{
    Int4 i;
    JumperPrelimEditBlock* left = gap_align->jumper->left_prelim_block;
    JumperPrelimEditBlock* right = gap_align->jumper->right_prelim_block;
    JumperPrelimEditBlock* combined = JumperPrelimEditBlockNew(
                                                        right->num_allocated);
    /* create a single forward edit script from the scripts for the left
       and right extensions */
    for (i = left->num_ops - 1;i >=0;i--) {
        JUMPER_EDIT_BLOCK_ADD(combined, left->edit_ops[i]);
    }

    for (i = 0;i < right->num_ops;i++) {
        JUMPER_EDIT_BLOCK_ADD(combined, right->edit_ops[i]);
    }

    for (i = 1;i < combined->num_ops;i++) {
        if (combined->edit_ops[i - 1] > 0 && combined->edit_ops[i] > 0) {
            Int4 k;
            combined->edit_ops[i - 1] += combined->edit_ops[i];
            for (k = i + 1;k < combined->num_ops;k++) {
                combined->edit_ops[k - 1] = combined->edit_ops[k];
            }
            combined->num_ops--;
        }
    }

    /* shift gaps */
    s_ShiftGapsRight(combined, query, subject, gap_align->query_start,
                     gap_align->subject_start, query_length,
                     subject_length, &gap_align->score,
                     err_score, num_identical);

    /* trim flanking gaps */
    while (combined->num_ops > 0 &&
           combined->edit_ops[combined->num_ops - 1] < 0) {

        if (combined->edit_ops[combined->num_ops - 1] == JUMPER_DELETION) {
            gap_align->subject_stop--;
        }
        else {
            gap_align->query_stop--;
        }
        combined->num_ops--;
        gap_align->score -= err_score;
    }

    /* replace edit scripts with the new ones in gap_align */
    left->num_ops = 0;
    JumperPrelimEditBlockFree(right);
    gap_align->jumper->right_prelim_block = combined;

    return 0;
}


/* trimm the extension so that there is at least -mismatch penalty matches
   after the last mismatch */
static void s_TrimExtension(JumperPrelimEditBlock* jops, int margin,
                            const Uint1** cp, Int4* cq, Int4* num_identical,
                            Boolean is_right_ext)
{

    Int4 num_matches = 0;
    Int4 index = jops->num_ops - 1;

    if (jops->num_ops == 0 || margin == 0) {
        return;
    }

    /* no trimming if mismatches do not cost anything */
    if (margin == 0) {
        return;
    }

    /* find number of local matches
       Consequitive match operations are not guaranteed to be combined in the
       preliminary block. This is to facilitate faster extensions */

    while (index >= 1 && jops->edit_ops[index] > 0) {
        num_matches += jops->edit_ops[index];
        index--;
    }

    /* we do not remove the last op; thanks to word hit there will be enough
       matches, but extension starting at word hit edge may not have enugh
       matches */
    while (jops->num_ops > 1 && num_matches < margin) {

        JumperOpType op = jops->edit_ops[jops->num_ops - 1];
        switch (JOP_TO_OP(op)) {
        case eGapAlignSub:
            if (op > 0) {
                (*cp) += (is_right_ext ? -op : op);
                (*cq) += (is_right_ext ? -op : op);
                *num_identical -= op;
            }
            else if (is_right_ext) {
                (*cp)--;
                (*cq)--;
            }
            else {
                (*cp)++;
                (*cq)++;
            }
            break;

        case eGapAlignIns:
            if (is_right_ext) {
                (*cp)--;
            }
            else {
                (*cp)++;
            }
            break;

        case eGapAlignDel:
            if (is_right_ext) {
                (*cq)--;
            }
            else {
                (*cq)++;
            }
            break;

        default:
            ASSERT(0);
        }
        jops->num_ops--;

        if (index >= jops->num_ops) {
            num_matches = 0;
            index = jops->num_ops - 1;
            while (index >= 1 && jops->edit_ops[index] > 0) {
                num_matches += jops->edit_ops[index];
                index--;
            }
        }
    }
    ASSERT(jops->num_ops > 0);
    /* remove the last op (closest to the word match) only if it is different
       than match */
    if (jops->num_ops == 1 && jops->edit_ops[0] <= 0) {
        jops->num_ops--;
    }
    ASSERT(jops->num_ops == 0 || jops->edit_ops[jops->num_ops - 1] > 0);
}

/* Create BLAST edit script from jumper scripts for left and right extensions */
GapEditScript* JumperPrelimEditBlockToGapEditScript(
                                      JumperPrelimEditBlock* rev_prelim_block,
                                      JumperPrelimEditBlock* fwd_prelim_block)
{
    Int4 size = 0;
    Int4 i;
    Int4 index;
    GapEditScript* retval;
    EGapAlignOpType last_op;

    if (rev_prelim_block->num_ops == 0 && fwd_prelim_block->num_ops == 0) {
        return NULL;
    }

    /* find the size of the resulting edit scrtipt */
    size = 1;
    last_op = rev_prelim_block->num_ops > 0 ? 
        JOP_TO_OP(rev_prelim_block->edit_ops[rev_prelim_block->num_ops - 1])
        : JOP_TO_OP(fwd_prelim_block->edit_ops[0]);

    for (i = rev_prelim_block->num_ops - 2;i >= 0;i--) {
        if (last_op != JOP_TO_OP(rev_prelim_block->edit_ops[i])) {
            size++;
            last_op = JOP_TO_OP(rev_prelim_block->edit_ops[i]);
        }
    }
    for (i = 0;i < fwd_prelim_block->num_ops;i++) {
        if (last_op != JOP_TO_OP(fwd_prelim_block->edit_ops[i])) {
            size++;
            last_op = JOP_TO_OP(fwd_prelim_block->edit_ops[i]);
        }
    }

    retval = GapEditScriptNew(size);

    /* rev_prelim_block needs to be reveresed, because left extension edit
       operations are collected backwards */
    index = 0;
    if (rev_prelim_block->num_ops > 0) {
        i = rev_prelim_block->num_ops - 1;
        retval->op_type[index] = JOP_TO_OP(rev_prelim_block->edit_ops[i]);
        retval->num[index] = JOP_TO_NUM(rev_prelim_block->edit_ops[i]);
        last_op = retval->op_type[index];
        i--;
        for (;i >= 0;i--) {
            EGapAlignOpType current_op = JOP_TO_OP(rev_prelim_block->edit_ops[i]);
            if (current_op == last_op) {
                retval->num[index] += JOP_TO_NUM(rev_prelim_block->edit_ops[i]);
            }
            else {
                index++;
                retval->op_type[index] = JOP_TO_OP(rev_prelim_block->edit_ops[i]);
                retval->num[index] = JOP_TO_NUM(rev_prelim_block->edit_ops[i]);
                last_op = current_op;
            }
        }
    }

    i = 0;
    if (index == 0 && retval->num[index] == 0) {
        retval->op_type[index] = JOP_TO_OP(fwd_prelim_block->edit_ops[i]);
        retval->num[index] = JOP_TO_NUM(fwd_prelim_block->edit_ops[i]);
        last_op = retval->op_type[index];
        i++;
    }
    for (;i < fwd_prelim_block->num_ops;i++) {
        EGapAlignOpType current_op = JOP_TO_OP(fwd_prelim_block->edit_ops[i]);
        if (current_op == last_op) {
            retval->num[index] += JOP_TO_NUM(fwd_prelim_block->edit_ops[i]);
        }
        else {
            index++;
            retval->op_type[index] = JOP_TO_OP(fwd_prelim_block->edit_ops[i]);
            retval->num[index] = JOP_TO_NUM(fwd_prelim_block->edit_ops[i]);
            last_op = current_op;
        }
    }

    return retval;
}


/* Compute a score for an extension */
static Int4 s_ComputeExtensionScore(JumperPrelimEditBlock* edit_script,
                                    Int4 match_score, Int4 mismatch_score,
                                    Int4 gap_open_score, Int4 gap_extend_score)
{
    Int4 score = 0;
    Int4 i;

    for (i = 0;i < edit_script->num_ops;i++) {
        Int4 op = edit_script->edit_ops[i];

/* indel penalty is the same as mismatch penalty
        if (op < 0) {
            continue;
        }
*/

        if (op > 0) {
            score += op * match_score;
        }
        else if (op == 0) {
            score += mismatch_score;
        }
        else {
            score += gap_open_score;
            while (i < edit_script->num_ops && edit_script->edit_ops[i] == op) {
                score += gap_extend_score;
                i++;
            }
            i--;
        }
    }

    return score;
}



/* query is one base per byte, subject is 2NA, we assume that initial seed is 
   at byte edge */


Int4 JumperExtendRightCompressed(const Uint1* query, const Uint1* subject, 
                                 int query_length, int subject_length,
                                 Int4 match_score, Int4 mismatch_score,
                                 Int4 gap_open_score, Int4 gap_extend_score,
                                 int max_mismatches, int window, Uint4* table,
                                 Int4* query_ext_len, Int4* subject_ext_len,
                                 Int4* num_identical,
                                 Int4* ungapped_ext_len)
{ 
    const Uint1 *cp, *cp1, *cpmax, *cpmax4, *cpstop = NULL;
    Int4 cq, cq1, cqmax, cqmax4, cqstop = 0;
    int i, n;
    JUMP *jp;
    int num_mismatches = 0;
    int new_matches = 0;
    Uint4 trace = 0;
    Uint4 trace_mask = (1 << max_mismatches) - 1;
    Int4 score = 0, best_score = 0;
    Boolean is_ungapped = TRUE;
    JUMP* jumper = jumper_default;

    if (!query || ! subject) {
        return -1;
    }

    cp = query;
    cpmax = cp + query_length;

    /* or assume matches up to byte edge */
    cq = 0;
    cqmax = subject_length;

    cpmax4 = cpmax - 4;
    cqmax4 = cqmax - 4;

    /* skip the first pair as it is compared by the left extension */
    cp++;
    cq++;

    while (cp < cpmax && cq < cqmax && num_mismatches < max_mismatches) {

        if (!(cq & 3) && cp < cpmax4 && cq < cqmax4) {
            if (table[subject[cq / 4]] == *(Uint4*)(cp)) {
                cp += 4;
                cq += 4;
                new_matches += 4;
                continue;
            }
        }

        if (*cp == UNPACK_BASE(subject, cq)) {
            cp++;
            cq++;
            new_matches++;
            continue;
        }

        jp = jumper; 
        jp-- ;
        while (jp++) { /* 1, 1, 0, 0 = last always accepted */
         
            if (!jp->lng) {
                break;
            }

            n = 0;
            i = jp->ok;
            cp1 = cp + jp->dcp;
            cq1 = cq + jp->dcq;
            while (i--) {
                if (cp1 >= cpmax || cq1 >= cqmax
                    || *cp1++ != UNPACK_BASE(subject, cq1)) {

                    goto next_jp;
                }
                cq1++;
            }

            n = 0;
            i = jp->lng;
            cp1 = cp + jp->dcp;
            cq1 = cq + jp->dcq;
            if (i + cp1 >= cpmax || i + cq1 >= cqmax) {
                continue;
            }
            while (i--) {
                if (cp1 >= cpmax || cq1 >= cqmax) {
                    goto next_jp;
                }
                if (*cp1++ != UNPACK_BASE(subject, cq1)) {
                    if (++n > jp->ok) {
                        goto next_jp;
                    }
                }
                cq1++;
            }
            /* current jumper row works */
            break;

next_jp:
            continue;
        }

        if (new_matches) {
            if (trace) {
                if (new_matches < window) {
                    trace <<= new_matches;
                }
                else {
                    trace = 0;
                }
            }
            *num_identical += new_matches;
            score += new_matches * match_score;
            new_matches = 0;
        }

        /* update recent errors and score */
        if (jp->dcp == jp->dcq) {
            score += mismatch_score * jp->dcp;
            if (trace & trace_mask) {
                num_mismatches += jp->dcp;
                trace <<= jp->dcp;
                trace |= (1 << jp->dcp) - 1;
            }
            else {
                num_mismatches = jp->dcp;
                trace = (1 << jp->dcp) - 1;
            }
        }
        /* note that we are not penalizing gaps */
        
        /* save the length of the ungapped extension */
        if (is_ungapped && jp->dcp != jp->dcq) {
            *ungapped_ext_len = cp - query - 1;
            is_ungapped = FALSE;
        }

        /* update cp and cq */
        cp += jp->dcp;
        cq += jp->dcq;
        
        /* we have already checked that these are matches */
        if (jp->ok == 0 && jp->lng) {
            cp += jp->lng;
            cq += jp->lng;
            trace <<= jp->lng;
            *num_identical += jp->lng;
            score += jp->lng * match_score;
        }

        /* update optimal alignment extent */
        if (score >= best_score) {
            cpstop = cp;
            cqstop = cq;
            best_score = score;
        }
    }

    /* if matches all the way to the end of a sequence */
    if (new_matches) {
        *num_identical += new_matches;
        score += new_matches * match_score;
        if (score >= best_score) {
            cpstop = cp;
            cqstop = cq;
        }        
    }

    *query_ext_len = cpstop - query;
    *subject_ext_len = cqstop;

    if (is_ungapped) {
        *ungapped_ext_len = *query_ext_len;
    }

    return best_score;
} /* JumperExtendRightCompressed */



Int4 JumperExtendRightCompressedWithTraceback(
                                 const Uint1* query, const Uint1* subject, 
                                 int query_length, int subject_length,
                                 Int4 match_score, Int4 mismatch_score,
                                 Int4 gap_open_score, Int4 gap_extend_score,
                                 int max_mismatches, int window, Uint4* table,
                                 Int4* query_ext_len, Int4* subject_ext_len,
                                 JumperPrelimEditBlock* edit_script,
                                 Int4* num_identical,
                                 Boolean left_extension,
                                 Int4* ungapped_ext_len,
                                 JUMP* jumper)
{ 
    const Uint1 *cp, *cp1, *cpmax, *cpmax4;
    Int4 cq, cq1, cqmax, cqmax4;
    int i, n;
    JUMP *jp;
    int num_mismatches = 0;
    int new_matches = 0;
    Uint4 trace = 0;
    Uint4 trace_mask = (1 << max_mismatches) - 1;
    Boolean is_ungapped = TRUE;

    if (!query || ! subject) {
        return -1;
    }

    cp = query;
    cpmax = cp + query_length;

    /* or assume matches up to byte edge */
    cq = 0;
    cqmax = subject_length;

    cpmax4 = cpmax - 4;
    cqmax4 = cqmax - 4;

    /* skip the first pair as it is compared by the left extension */
    cp++;
    cq++;
    if (!left_extension) {
        new_matches++;
    }

    while (cp < cpmax && cq < cqmax && num_mismatches < max_mismatches) {

        if (!(cq & 3) && cp < cpmax4 && cq < cqmax4) {
            if (table[subject[cq / 4]] == *(Uint4*)(cp)) {
                cp += 4;
                cq += 4;
                new_matches += 4;
                continue;
            }
        }

        if (*cp == UNPACK_BASE(subject, cq)) {
            cp++;
            cq++;
            new_matches++;
            continue;
        }

        jp = jumper; 
        jp-- ;
        while (jp++) { /* 1, 1, 0, 0 = last always accepted */
         
            if (!jp->lng) {
                break;
            }

            n = 0;
            i = jp->ok;
            cp1 = cp + jp->dcp;
            cq1 = cq + jp->dcq;
            while (i--) {
                if (cp1 >= cpmax || cq1 >= cqmax
                    || *cp1++ != UNPACK_BASE(subject, cq1)) {

                    goto next_jp;
                }
                cq1++;
            }

            n = 0;
            i = jp->lng;
            cp1 = cp + jp->dcp;
            cq1 = cq + jp->dcq;
            if (i + cp1 >= cpmax || i + cq1 >= cqmax) {
                continue;
            }
            while (i--) {
                if (cp1 >= cpmax || cq1 >= cqmax) {
                    goto next_jp;
                }
                if (*cp1++ != UNPACK_BASE(subject, cq1)) {
                    if (++n > jp->ok) {
                        goto next_jp;
                    }
                }
                cq1++;
            }
            /* current jumper row works */
            break;

next_jp:
            continue;
        }

        if (new_matches) {
            ASSERT(edit_script->num_ops < edit_script->num_allocated);
            JUMPER_EDIT_BLOCK_ADD(edit_script, new_matches);
            if (trace) {
                if (new_matches < window) {
                    trace <<= new_matches;
                }
                else {
                    trace = 0;
                }
            }
            *num_identical += new_matches;
            new_matches = 0;
        }

        /* update recent errors */
        if (jp->dcp == jp->dcq) {
            if (trace & trace_mask) {
                num_mismatches += jp->dcp;
                trace <<= jp->dcp;
                trace |= (1 << jp->dcp) - 1;
            }
            else {
                num_mismatches = jp->dcp;
                trace = (1 << jp->dcp) - 1;
            }
            for (i = 0;i < jp->dcp;i++) {
                ASSERT(edit_script->num_ops < edit_script->num_allocated);
                JUMPER_EDIT_BLOCK_ADD(edit_script, JUMPER_MISMATCH);
            }
        } else if (jp->dcp > jp->dcq) {
            for (i = 0;i < jp->dcp - jp->dcq;i++) {
                ASSERT(edit_script->num_ops < edit_script->num_allocated);
                JUMPER_EDIT_BLOCK_ADD(edit_script, JUMPER_INSERTION);
            }
            if (jp->dcq) {
                /* either jp->dcp r jp->dcq must be zero or jp->dcp == jp->dcq
                   otherwise number of mismatches must be updated here */
                ASSERT(0);
            }
        }
        else {
            for (i = 0;i < jp->dcq - jp->dcp;i++) {
                ASSERT(edit_script->num_ops < edit_script->num_allocated);
                JUMPER_EDIT_BLOCK_ADD(edit_script, JUMPER_DELETION);
            }
            if (jp->dcp) {
                /* either jp->dcp r jp->dcq must be zero or jp->dcp == jp->dcq
                   otherwise number of mismatches must be updated here */
                ASSERT(0);
            }
        }
        
        /* save the length of the ungapped extension */
        if (is_ungapped && jp->dcp != jp->dcq) {
            *ungapped_ext_len = cp - query - 1;
            is_ungapped = FALSE;
        }

        /* update cp and cq */
        cp += jp->dcp;
        cq += jp->dcq;
        
        /* we have already checked that these are matches */
        if (jp->ok == 0 && jp->lng) {
            cp += jp->lng;
            cq += jp->lng;
            ASSERT(edit_script->num_ops < edit_script->num_allocated);
            JUMPER_EDIT_BLOCK_ADD(edit_script, jp->lng);
            trace <<= jp->lng;
            *num_identical += jp->lng;
            /* reset number of mismatches (we assume that error window is
               smaller than jp->lng) */
            num_mismatches = 0;
        }
    }

    /* if matches all the way to the end of a sequence */
    if (new_matches) {
        ASSERT(edit_script->num_ops < edit_script->num_allocated);
        JUMPER_EDIT_BLOCK_ADD(edit_script, new_matches);
        *num_identical += new_matches;
    }

    /* find optimal length of the extension */
    s_TrimExtension(edit_script, -mismatch_score, &cp, &cq, num_identical,
                    TRUE);

    *query_ext_len = cp - query;
    *subject_ext_len = cq;

    if (is_ungapped) {
        *ungapped_ext_len = *query_ext_len;
    }

    /* return extension score */
    return s_ComputeExtensionScore(edit_script, match_score, mismatch_score,
                                   gap_open_score, gap_extend_score);
} /* JumperExtendRightCompressedWithTraceback */


Int4 JumperExtendRightCompressedWithTracebackOptimal(
                                 const Uint1* query, const Uint1* subject, 
                                 int query_length, int subject_length,
                                 Int4 match_score, Int4 mismatch_score,
                                 Int4 gap_open_score, Int4 gap_extend_score,
                                 int max_mismatches, int window, 
                                 Int4 x_drop,  Uint4* table,
                                 Int4* query_ext_len, Int4* subject_ext_len,
                                 JumperPrelimEditBlock* edit_script,
                                 Int4* best_num_identical,
                                 Boolean left_extension,
                                 Int4* ungapped_ext_len)
{ 
    const Uint1 *cp, *cp1, *cpmax, *cpmax4, *cpstop = NULL;
    Int4 cq, cq1, cqmax, cqmax4, cqstop = 0;
    int i, n;
    JUMP *jp;
    int num_mismatches = 0;
    int new_matches = 0;
    Uint4 trace = 0;
    Uint4 trace_mask = (1 << max_mismatches) - 1;
    Boolean is_ungapped = TRUE;
    Int4 score = 0, best_score = 0;
    Int4 num_ops = 0, num_identical = *best_num_identical;
    /* 0 - no gap, JUMPER_INSERTION, JUMPER_DELETION for open insertion  or
       deletion */
    Int4 last_gap_open = 0;
    JUMP* jumper = jumper_default;

    if (!query || ! subject) {
        return -1;
    }

    cp = query;
    cpmax = cp + query_length;
    cp1 = cpmax;

    /* or assume matches up to byte edge */
    cq = 0;
    cqmax = subject_length;

    cpmax4 = cpmax - 4;
    cqmax4 = cqmax - 4;

    /* skip the first pair as it is compared by the left extension */
    cp++;
    cq++;
    if (!left_extension) {
        new_matches++;
    }

    while (cp < cpmax && cq < cqmax && num_mismatches < max_mismatches) {

        if (!(cq & 3) && cp < cpmax4 && cq < cqmax4) {
            if (table[subject[cq / 4]] == *(Uint4*)(cp)) {
                cp += 4;
                cq += 4;
                new_matches += 4;
                continue;
            }
        }

        if (*cp == UNPACK_BASE(subject, cq)) {
            cp++;
            cq++;
            new_matches++;
            continue;
        }

        jp = jumper; 
        jp-- ;
        while (jp++) { /* 1, 1, 0, 0 = last always accepted */
         
            if (!jp->lng) {
                break;
            }

            n = 0;
            i = jp->ok;
            cp1 = cp + jp->dcp;
            cq1 = cq + jp->dcq;
            while (i--) {
                if (cp1 >= cpmax || cq1 >= cqmax
                    || *cp1++ != UNPACK_BASE(subject, cq1)) {

                    goto next_jp;
                }
                cq1++;
            }

            n = 0;
            i = jp->lng;
            cp1 = cp + jp->dcp;
            cq1 = cq + jp->dcq;

            if (cp1 >= cpmax || i + cq1 >= cqmax) {
                continue;
            }
            while (i--) {
                if (cp1 >= cpmax) {
                    break;
                }
                if (/*cp1 >= cpmax ||*/ cq1 >= cqmax) {
                    goto next_jp;
                }
                if (*cp1++ != UNPACK_BASE(subject, cq1)) {
                    if (++n > jp->ok) {
                        goto next_jp;
                    }
                }
                cq1++;
            }
            /* current jumper row works */
            break;

next_jp:
            continue;
        }

        if (new_matches) {
            ASSERT(edit_script->num_ops < edit_script->num_allocated);
            JUMPER_EDIT_BLOCK_ADD(edit_script, new_matches);
            if (trace) {
                if (new_matches < window) {
                    trace <<= new_matches;
                }
                else {
                    trace = 0;
                }
            }
            num_identical += new_matches;
            score += new_matches * match_score;
            new_matches = 0;
            last_gap_open = 0;
        }

        /* update optimal alignment extent */
        if (score >= best_score) {
            cpstop = cp;
            cqstop = cq;
            num_ops = edit_script->num_ops;
            best_score = score;
            *best_num_identical = num_identical;
        }

        if (best_score - score > x_drop) {
            break;
        }

        /* update recent errors */
        if (jp->dcp == jp->dcq) {
            score += jp->dcp * mismatch_score;
            if (trace & trace_mask) {
                num_mismatches += jp->dcp;
                trace <<= jp->dcp;
                trace |= (1 << jp->dcp) - 1;
            }
            else {
                num_mismatches = jp->dcp;
                trace = (1 << jp->dcp) - 1;
            }
            for (i = 0;i < jp->dcp;i++) {
                ASSERT(edit_script->num_ops < edit_script->num_allocated);
                JUMPER_EDIT_BLOCK_ADD(edit_script, JUMPER_MISMATCH);
            }
        } else if (jp->dcp > jp->dcq) {
            for (i = 0;i < jp->dcp - jp->dcq;i++) {
                ASSERT(edit_script->num_ops < edit_script->num_allocated);
                JUMPER_EDIT_BLOCK_ADD(edit_script, JUMPER_INSERTION);
                score += gap_extend_score;
            }
            if (last_gap_open != JUMPER_INSERTION) {
                score += gap_open_score;
            }
            last_gap_open = JUMPER_INSERTION;
            if (jp->dcq) {
                /* either jp->dcp r jp->dcq must be zero or jp->dcp == jp->dcq
                   otherwise number of mismatches must be updated here */
                ASSERT(0);
            }
        }
        else {
            for (i = 0;i < jp->dcq - jp->dcp;i++) {
                ASSERT(edit_script->num_ops < edit_script->num_allocated);
                JUMPER_EDIT_BLOCK_ADD(edit_script, JUMPER_DELETION);
                score += gap_extend_score;
            }
            if (last_gap_open != JUMPER_DELETION) {
                score += gap_open_score;
            }
            last_gap_open = JUMPER_DELETION;
            if (jp->dcp) {
                /* either jp->dcp r jp->dcq must be zero or jp->dcp == jp->dcq
                   otherwise number of mismatches must be updated here */
                ASSERT(0);
            }
        }
        
        /* save the length of the ungapped extension */
        if (is_ungapped && jp->dcp != jp->dcq) {
            *ungapped_ext_len = cp - query - 1;
            is_ungapped = FALSE;
        }

        /* update cp and cq */
        cp += jp->dcp;
        cq += jp->dcq;
        
        /* we have already checked that these are matches */
        if (cp1 < cpmax && jp->ok == 0 && jp->lng) {
            cp += jp->lng;
            cq += jp->lng;
            ASSERT(edit_script->num_ops < edit_script->num_allocated);
            JUMPER_EDIT_BLOCK_ADD(edit_script, jp->lng);
            trace <<= jp->lng;
            num_identical += jp->lng;
            score += jp->lng * match_score;
            last_gap_open = 0;
        }

        /* update optimal alignment extent */
        if (score >= best_score) {
            cpstop = cp;
            cqstop = cq;
            num_ops = edit_script->num_ops;
            best_score = score;
            *best_num_identical = num_identical;
        }
    }

    /* if matches all the way to the end of a sequence */
    if (new_matches) {
        ASSERT(edit_script->num_ops < edit_script->num_allocated);
        JUMPER_EDIT_BLOCK_ADD(edit_script, new_matches);
        num_identical += new_matches;
        score += new_matches;
    }

    /* update optimal alignment extent */
    if (score >= best_score) {
        cpstop = cp;
        cqstop = cq;
        num_ops = edit_script->num_ops;
        best_score = score;
        *best_num_identical = num_identical;
    }

    *query_ext_len = cpstop - query;
    *subject_ext_len = cqstop;
    edit_script->num_ops = num_ops;

    if (is_ungapped) {
        *ungapped_ext_len = *query_ext_len;
    }

    return best_score;
} /* JumperExtendRightCompressedWithTracebackOptimal */



Int4 JumperExtendRight(const Uint1* query, const Uint1* subject, 
                       int query_length, int subject_length,
                       Int4 match_score, Int4 mismatch_score,
                       Int4 gap_open_score, Int4 gap_extend_score,
                       int max_mismatches, int window,
                       int* query_ext_len, int* subject_ext_len,
                       GapPrelimEditBlock* edit_script,
                       Boolean left_extension)
{ 
    const Uint1 *cp, *cp1, *cpmax;
    Int4 cq, cq1, cqmax;
    int i, n;
    JUMP *jp;
    int score = 0, num_mismatches = 0;
    int new_matches = 0;
    Uint4 trace = 0;
    Uint4 trace_mask = (1 << max_mismatches) - 1;
    JUMP* jumper = jumper_default;

    if (!query || ! subject || !edit_script) {
        return -1;
    }

    cp = query;
    cpmax = cp + query_length;

    /* or assume matches up to byte edge */
    cq = 0;
    cqmax = subject_length;

    /* skip the first pair as it is compared by the left extension */
    cp++;
    cq++;
    if (!left_extension) {
        new_matches++;
    }

    while (cp < cpmax && cq < cqmax && num_mismatches < max_mismatches) {

        if (*cp == subject[cq]) {
            score += match_score;
            cp++;
            cq++;
            new_matches++;
            continue;
        }

        /* handle ambiguous bases */

/* this is only for 454
        jp = (cqmax - cq > 80) ? jumper : jumper_edge;
*/
        jp = jumper;
        jp-- ;
        while (jp++) { /* 1, 1, 0, 0 = last always accepted */
         
            if (!jp->lng) {
                break;
            }

            n = 0;
            i = jp->ok;
            cp1 = cp + jp->dcp;
            cq1 = cq + jp->dcq;
            while (i--) {
                if (cp1 >= cpmax || cq1 >= cqmax
                    || *cp1++ != subject[cq1]) {

                    goto next_jp;
                }
                cq1++;
            }

            n = 0;
            i = jp->lng;
            cp1 = cp + jp->dcp;
            cq1 = cq + jp->dcq;
            if (i + cp1 >= cpmax || i + cq1 >= cqmax) {
                continue;
            }
            while (i--) {
                if (cp1 >= cpmax || cq1 >= cqmax) {
                    goto next_jp;
                }
                if (*cp1++ != subject[cq1]) {
                    if (++n > jp->ok) {
                        goto next_jp;
                    }
                }
                cq1++;
            }
            /* current jumper row works */
            break;

next_jp:
            continue;
        }

        if (new_matches) {
            GapPrelimEditBlockAdd(edit_script, eGapAlignSub, new_matches);
            if (trace) {
                if (new_matches < window) {
                    trace <<= new_matches;
                }
                else {
                    trace = 0;
                }
            }
            new_matches = 0;
        }

/* another stop condition, ensures a positive score *--/
        if (!jp->lng && num_mismatches >= score + mismatch_score) {
            break;
        }
*/

        /* update score */
        if (jp->dcp == jp->dcq) {
            score += mismatch_score * jp->dcp;
            if (trace & trace_mask) {
                num_mismatches += jp->dcp;
                trace <<= jp->dcp;
                trace |= (1 << jp->dcp) - 1;
            }
            else {
                num_mismatches = jp->dcp;
                trace = (1 << jp->dcp) - 1;
            }
            GapPrelimEditBlockAdd(edit_script, eGapAlignSub, jp->dcp);
        } else if (jp->dcp > jp->dcq) {
            score += gap_open_score + gap_extend_score * (jp->dcp - jp->dcq);
            GapPrelimEditBlockAdd(edit_script, eGapAlignIns, jp->dcp - jp->dcq);
            ASSERT(!jp->dcq);
        }
        else {
            score += gap_open_score + gap_extend_score * (jp->dcq - jp->dcp);
            GapPrelimEditBlockAdd(edit_script, eGapAlignDel, jp->dcq - jp->dcp);
            ASSERT(!jp->dcp);
        }

        /* update cp and cq */
        cp += jp->dcp;
        cq += jp->dcq;
        
        /* we have already checked that these are matches */
        if (jp->ok == 0 && jp->lng) {
            score += match_score * jp->lng;
            cp += jp->lng;
            cq += jp->lng;

            GapPrelimEditBlockAdd(edit_script, eGapAlignSub, jp->lng);
            trace <<= jp->lng;
        }
    }

    /* if matches all the way to the end of a sequence */
    if (new_matches) {
        GapPrelimEditBlockAdd(edit_script, eGapAlignSub, new_matches);
    }

    *query_ext_len = cp - query;
    *subject_ext_len = cq;

    return score;
} /* JumperExtendRight */


Int4 JumperExtendRightWithTraceback(
                                 const Uint1* query, const Uint1* subject, 
                                 int query_length, int subject_length,
                                 Int4 match_score, Int4 mismatch_score,
                                 Int4 gap_open_score, Int4 gap_extend_score,
                                 int max_mismatches, int window,
                                 Int4* query_ext_len, Int4* subject_ext_len,
                                 JumperPrelimEditBlock* edit_script,
                                 Int4* num_identical,
                                 Boolean left_extension,
                                 Int4* ungapped_ext_len,
                                 JUMP* jumper)
{ 
    const Uint1 *cp, *cp1, *cpmax;
    Int4 cq, cq1, cqmax;
    int i, n;
    JUMP *jp;
    int num_mismatches = 0;
    int new_matches = 0;
    Uint4 trace = 0;
    Uint4 trace_mask = (1 << max_mismatches) - 1;
    Boolean is_ungapped = TRUE;

    if (!query || ! subject) {
        return -1;
    }

    cp = query;
    cpmax = cp + query_length;

    /* or assume matches up to byte edge */
    cq = 0;
    cqmax = subject_length;

    /* skip the first pair as it is compared by the left extension */
    if (left_extension) {
        cp++;
        cq++;
    }

    while (cp < cpmax && cq < cqmax && num_mismatches < max_mismatches) {

        if (*cp == subject[cq]) {
            cp++;
            cq++;
            new_matches++;
            continue;
        }

        jp = jumper; 
        jp-- ;
        while (jp++) { /* 1, 1, 0, 0 = last always accepted */
         
            if (!jp->lng) {
                break;
            }

            n = 0;
            i = jp->ok;
            cp1 = cp + jp->dcp;
            cq1 = cq + jp->dcq;
            while (i--) {
                if (cp1 >= cpmax || cq1 >= cqmax
                    || *cp1++ != subject[cq1]) {

                    goto next_jp;
                }
                cq1++;
            }

            n = 0;
            i = jp->lng;
            cp1 = cp + jp->dcp;
            cq1 = cq + jp->dcq;
            if (i + cp1 >= cpmax || i + cq1 >= cqmax) {
                continue;
            }
            while (i--) {
                if (cp1 >= cpmax || cq1 >= cqmax) {
                    goto next_jp;
                }
                if (*cp1++ != subject[cq1]) {
                    if (++n > jp->ok) {
                        goto next_jp;
                    }
                }
                cq1++;
            }
            /* current jumper row works */
            break;

next_jp:
            continue;
        }

        if (new_matches) {
            ASSERT(edit_script->num_ops < edit_script->num_allocated);
            JUMPER_EDIT_BLOCK_ADD(edit_script, new_matches);
            if (trace) {
                if (new_matches < window) {
                    trace <<= new_matches;
                }
                else {
                    trace = 0;
                }
            }
            *num_identical += new_matches;
            new_matches = 0;
        }

        /* update recent errors */
        if (jp->dcp == jp->dcq) {
            if (trace & trace_mask) {
                num_mismatches += jp->dcp;
                trace <<= jp->dcp;
                trace |= (1 << jp->dcp) - 1;
            }
            else {
                num_mismatches = jp->dcp;
                trace = (1 << jp->dcp) - 1;
            }
            for (i = 0;i < jp->dcp;i++) {
                ASSERT(edit_script->num_ops < edit_script->num_allocated);
                JUMPER_EDIT_BLOCK_ADD(edit_script, JUMPER_MISMATCH);
            }
        } else if (jp->dcp > jp->dcq) {
            for (i = 0;i < jp->dcp - jp->dcq;i++) {
                ASSERT(edit_script->num_ops < edit_script->num_allocated);
                JUMPER_EDIT_BLOCK_ADD(edit_script, JUMPER_INSERTION);
            }
            if (jp->dcq) {
                /* either jp->dcp r jp->dcq must be zero or jp->dcp == jp->dcq
                   otherwise number of mismatches must be updated here */
                ASSERT(0);
            }
        }
        else {
            for (i = 0;i < jp->dcq - jp->dcp;i++) {
                ASSERT(edit_script->num_ops < edit_script->num_allocated);
                JUMPER_EDIT_BLOCK_ADD(edit_script, JUMPER_DELETION);
            }
            if (jp->dcp) {
                /* either jp->dcp r jp->dcq must be zero or jp->dcp == jp->dcq
                   otherwise number of mismatches must be updated here */
                ASSERT(0);
            }
        }
        
        /* save the length of the ungapped extension */
        if (is_ungapped && jp->dcp != jp->dcq) {
            *ungapped_ext_len = cp - query - 1;
            is_ungapped = FALSE;
        }

        /* update cp and cq */
        cp += jp->dcp;
        cq += jp->dcq;
        
        /* we have already checked that these are matches */
        if (jp->ok == 0 && jp->lng) {
            cp += jp->lng;
            cq += jp->lng;
            ASSERT(edit_script->num_ops < edit_script->num_allocated);
            JUMPER_EDIT_BLOCK_ADD(edit_script, jp->lng);
            trace <<= jp->lng;
            *num_identical += jp->lng;
            /* reset number of mismatches (we assume that error window is
               smaller than jp->lng) */
            num_mismatches = 0;
        }
    }

    /* if matches all the way to the end of a sequence */
    if (new_matches) {
        ASSERT(edit_script->num_ops < edit_script->num_allocated);
        JUMPER_EDIT_BLOCK_ADD(edit_script, new_matches);
        *num_identical += new_matches;
    }

    /* find optimal length of the extension */
    s_TrimExtension(edit_script, -mismatch_score, &cp, &cq, num_identical,
                    TRUE);

    *query_ext_len = cp - query;
    *subject_ext_len = cq;

    if (is_ungapped) {
        *ungapped_ext_len = *query_ext_len;
    }

    /* return extension score */
    return s_ComputeExtensionScore(edit_script, match_score, mismatch_score,
                                   gap_open_score, gap_extend_score);
} /* JumperExtendRightWithTraceback */



Int4 JumperExtendLeftCompressed(const Uint1* query, const Uint1* subject,
                                Int4 query_offset, Int4 subject_offset,
                                Int4 match_score, Int4 mismatch_score,
                                Int4 gap_open_score, Int4 gap_extend_score,
                                int max_mismatches, int window, Uint4* table,
                                Int4* query_ext_len, Int4* subject_ext_len,
                                Int4* num_identical)
{ 
    const Uint1 *cp, *cp1, *cpmin, *cpmin4, *cpstop = NULL;
    Int4 cq, cq1, cqmin, cqmin4, cqstop = 0;
    int i, n;
    JUMP *jp;
    int num_mismatches = 0;
    int new_matches = 0;
    Uint4 trace = 0;
    Uint4 trace_mask = (1 << max_mismatches) - 1;
    Int4 score = 0, best_score = 0;
    JUMP* jumper = jumper_default;

    if (!query || ! subject) {
        return -1;
    }

    cp = query + query_offset;
    cpmin = query;

    /* or assume matches up to byte edge */
    cq = subject_offset;
    cqmin = 0;

    cpmin4 = query + 4;
    cqmin4 = 4;


    while (cp >= cpmin && cq >= cqmin && num_mismatches < max_mismatches) {

        if ((cq & 3) == 3 && cp >= cpmin4 && cq >= cqmin4) {
            if (table[subject[cq / 4]] == *(Uint4*)(cp - 3)) {
                cp -= 4;
                cq -= 4;
                new_matches += 4;
                continue;
            }
        }

        if (*cp == UNPACK_BASE(subject, cq)) {
            cp--;
            cq--;
            new_matches++;
            continue;
        }

        /* handle ambiguous bases */


        jp = jumper; 
        jp-- ;
        while (jp++) { /* 1, 1, 0, 0 = last always accepted */
         
            if (!jp->lng) {
                break;
            }

            n = 0;
            i = jp->ok;
            cp1 = cp - jp->dcp;
            cq1 = cq - jp->dcq;
            while (i--) {
                if (cp1 < cpmin || cq1 < cqmin
                    || *cp1-- != UNPACK_BASE(subject, cq1)) {

                    goto next_jp;
                }
                cq1--;
            }

            n = 0;
            i = jp->lng;
            cp1 = cp - jp->dcp;
            cq1 = cq - jp->dcq;
            if (cp1 - i < cpmin || cq1 - i < cqmin) {
                continue;
            }
            while (i--) {
                if (cp1 < cpmin || cq1 < cqmin) {
                    goto next_jp;
                }
                if (*cp1-- != UNPACK_BASE(subject, cq1)) {
                    if (++n > jp->ok) {
                        goto next_jp;
                    }
                }
                cq1--;
            }
            /* current jumper row works */
            break;

next_jp:
            continue;
        }

        if (new_matches) {
            if (trace) {
                if (new_matches < window) {
                    trace <<= new_matches;
                }
                else {
                    trace = 0;
                }
            }
            *num_identical += new_matches;
            score = new_matches * match_score;
            new_matches = 0;
        }

        /* update recent errors and score */
        if (jp->dcp == jp->dcq) {
            score += mismatch_score * jp->dcp;
            if (trace & trace_mask) {
                num_mismatches += jp->dcp;
                trace <<= jp->dcp;
                trace |= (1 << jp->dcp) - 1;
            }
            else {
                num_mismatches = jp->dcp;
                trace = (1 << jp->dcp) - 1;
            }
        }
        /* note that we are not penalizing gaps */

        /* update cp and cq */
        cp -= jp->dcp;
        cq -= jp->dcq;
        
        /* we have already checked that these are matches */
        if (!jp->ok && jp->lng) {
            cp -= jp->lng;
            cq -= jp->lng;
            trace <<= jp->lng;
            *num_identical += jp->lng;
            score += jp->lng * match_score;
        }

        /* update optimal alignment extent */
        if (score >= best_score) {
            cpstop = cp;
            cqstop = cq;
            best_score = score;
        }
    }

    /* if matches all the way to the end of a sequence */
    if (new_matches) {
        *num_identical += new_matches;
        score += new_matches * match_score;
        if (score >= best_score) {
            cpstop = cp;
            cqstop = cq;
        }
    }

    *query_ext_len = query + query_offset - cpstop;
    *subject_ext_len = subject_offset - cqstop;

    return best_score;
} /* JumperExtendLeftCompressed */


Int4 JumperExtendLeftCompressedWithTraceback(
                                const Uint1* query, const Uint1* subject,
                                Int4 query_offset, Int4 subject_offset,
                                Int4 match_score, Int4 mismatch_score,
                                Int4 gap_open_score, Int4 gap_extend_score,
                                int max_mismatches, int window, Uint4* table,
                                Int4* query_ext_len, Int4* subject_ext_len,
                                JumperPrelimEditBlock* edit_script,
                                Int4* num_identical,
                                JUMP* jumper)
{ 
    const Uint1 *cp, *cp1, *cpmin, *cpmin4;
    Int4 cq, cq1, cqmin, cqmin4;
    int i, n;
    JUMP *jp;
    int num_mismatches = 0;
    int new_matches = 0;
    Uint4 trace = 0;
    Uint4 trace_mask = (1 << max_mismatches) - 1;

    if (!query || ! subject) {
        return -1;
    }

    cp = query + query_offset;
    cpmin = query;

    /* or assume matches up to byte edge */
    cq = subject_offset;
    cqmin = 0;

    cpmin4 = query + 4;
    cqmin4 = 4;


    while (cp >= cpmin && cq >= cqmin && num_mismatches < max_mismatches) {

        if ((cq & 3) == 3 && cp >= cpmin4 && cq >= cqmin4) {
            if (table[subject[cq / 4]] == *(Uint4*)(cp - 3)) {
                cp -= 4;
                cq -= 4;
                new_matches += 4;
                continue;
            }
        }

        if (*cp == UNPACK_BASE(subject, cq)) {
            cp--;
            cq--;
            new_matches++;
            continue;
        }

        /* handle ambiguous bases */


        jp = jumper; 
        jp-- ;
        while (jp++) { /* 1, 1, 0, 0 = last always accepted */
         
            if (!jp->lng) {
                break;
            }

            n = 0;
            i = jp->ok;
            cp1 = cp - jp->dcp;
            cq1 = cq - jp->dcq;
            while (i--) {
                if (cp1 < cpmin || cq1 < cqmin
                    || *cp1-- != UNPACK_BASE(subject, cq1)) {

                    goto next_jp;
                }
                cq1--;
            }

            n = 0;
            i = jp->lng;
            cp1 = cp - jp->dcp;
            cq1 = cq - jp->dcq;
            if (cp1 - i < cpmin || cq1 - i < cqmin) {
                continue;
            }
            while (i--) {
                if (cp1 < cpmin || cq1 < cqmin) {
                    goto next_jp;
                }
                if (*cp1-- != UNPACK_BASE(subject, cq1)) {
                    if (++n > jp->ok) {
                        goto next_jp;
                    }
                }
                cq1--;
            }
            /* current jumper row works */
            break;

next_jp:
            continue;
        }

        if (new_matches) {
            ASSERT(edit_script->num_ops < edit_script->num_allocated);
            JUMPER_EDIT_BLOCK_ADD(edit_script, new_matches);
            if (trace) {
                if (new_matches < window) {
                    trace <<= new_matches;
                }
                else {
                    trace = 0;
                }
            }
            *num_identical += new_matches;
            new_matches = 0;
        }

        /* update recent errors */
        if (jp->dcp == jp->dcq) {
            if (trace & trace_mask) {
                num_mismatches += jp->dcp;
                trace <<= jp->dcp;
                trace |= (1 << jp->dcp) - 1;
            }
            else {
                num_mismatches = jp->dcp;
                trace = (1 << jp->dcp) - 1;
            }
            for (i = 0;i < jp->dcp;i++) {
                ASSERT(edit_script->num_ops < edit_script->num_allocated);
                JUMPER_EDIT_BLOCK_ADD(edit_script, JUMPER_MISMATCH);
            }
        } else if (jp->dcp > jp->dcq) {
            for (i = 0;i < jp->dcp - jp->dcq;i++) {
                ASSERT(edit_script->num_ops < edit_script->num_allocated);
                JUMPER_EDIT_BLOCK_ADD(edit_script, JUMPER_INSERTION);
            }
            if (jp->dcq) {
                /* either jp->dcp r jp->dcq must be zero or jp->dcp == jp->dcq
                   otherwise number of mismatches must be updated here */
                ASSERT(0);
            }
        }
        else {
            for (i = 0;i < jp->dcq - jp->dcp;i++) {
                ASSERT(edit_script->num_ops < edit_script->num_allocated);
                JUMPER_EDIT_BLOCK_ADD(edit_script, JUMPER_DELETION);
            }
            if (jp->dcp) {
                /* either jp->dcp r jp->dcq must be zero or jp->dcp == jp->dcq
                   otherwise number of mismatches must be updated here */
                ASSERT(0);
            }
        }

        /* update cp and cq */
        cp -= jp->dcp;
        cq -= jp->dcq;
        
        /* we have already checked that these are matches */
        if (!jp->ok && jp->lng) {
            cp -= jp->lng;
            cq -= jp->lng;
            ASSERT(edit_script->num_ops < edit_script->num_allocated);
            JUMPER_EDIT_BLOCK_ADD(edit_script, jp->lng);
            trace <<= jp->lng;
            *num_identical += jp->lng;
            /* reset number of mismatches (we assume that error window is
               smaller than jp->lng) */
            num_mismatches = 0;
        }
    }

    /* if matches all the way to the end of a sequence */
    if (new_matches) {
        ASSERT(edit_script->num_ops < edit_script->num_allocated);
        JUMPER_EDIT_BLOCK_ADD(edit_script, new_matches);
        *num_identical += new_matches;
    }

    /* find optimal extension length */
    s_TrimExtension(edit_script, -mismatch_score, &cp, &cq, num_identical,
                    FALSE); 

    *query_ext_len = query + query_offset - cp;
    *subject_ext_len = subject_offset - cq;

    /* return extension score */
    return s_ComputeExtensionScore(edit_script, match_score, mismatch_score,
                                   gap_open_score, gap_extend_score);
} /* JumperExtendLeftCompressedWithTraceback */


Int4 JumperExtendLeftCompressedWithTracebackOptimal(
                                const Uint1* query, const Uint1* subject,
                                Int4 query_offset, Int4 subject_offset,
                                Int4 match_score, Int4 mismatch_score,
                                Int4 gap_open_score, Int4 gap_extend_score,
                                int max_mismatches, int window, 
                                Int4 x_drop, Uint4* table,
                                Int4* query_ext_len, Int4* subject_ext_len,
                                JumperPrelimEditBlock* edit_script,
                                Int4* best_num_identical)
{ 
    const Uint1 *cp, *cp1, *cpmin, *cpmin4, *cpstop = NULL;
    Int4 cq, cq1, cqmin, cqmin4, cqstop = 0;
    int i, n;
    JUMP *jp;
    int num_mismatches = 0;
    int new_matches = 0;
    Uint4 trace = 0;
    Uint4 trace_mask = (1 << max_mismatches) - 1;
    Int4 score = 0, best_score = 0;
    Int4 num_ops = 0, num_identical = *best_num_identical;
    /* 0 - no gap, JUMPER_INSERTION, JUMPER_DELETION for open insertion  or
       deletion */
    Int4 last_gap_open = 0;
    JUMP* jumper = jumper_default;

    if (!query || ! subject) {
        return -1;
    }

    cp = query + query_offset;
    cpmin = query;
    cp1 = cpmin;

    /* or assume matches up to byte edge */
    cq = subject_offset;
    cqmin = 0;

    cpmin4 = query + 4;
    cqmin4 = 4;


    while (cp >= cpmin && cq >= cqmin && num_mismatches < max_mismatches) {

        if ((cq & 3) == 3 && cp >= cpmin4 && cq >= cqmin4) {
            if (table[subject[cq / 4]] == *(Uint4*)(cp - 3)) {
                cp -= 4;
                cq -= 4;
                new_matches += 4;
                continue;
            }
        }

        if (*cp == UNPACK_BASE(subject, cq)) {
            cp--;
            cq--;
            new_matches++;
            continue;
        }

        /* handle ambiguous bases */


        jp = jumper; 
        jp-- ;
        while (jp++) { /* 1, 1, 0, 0 = last always accepted */

            if (!jp->lng) {
                break;
            }

            n = 0;
            i = jp->ok;
            cp1 = cp - jp->dcp;
            cq1 = cq - jp->dcq;
            while (i--) {
                if (cp1 < cpmin || cq1 < cqmin
                    || *cp1-- != UNPACK_BASE(subject, cq1)) {

                    goto next_jp;
                }
                cq1--;
            }

            n = 0;
            i = jp->lng;
            cp1 = cp - jp->dcp;
            cq1 = cq - jp->dcq;

            if (cp1 <= cpmin || cq1 - i < cqmin) {
                continue;
            }
            while (i--) {
                if (cp1 < cpmin) {
                    break;
                }
                if (/*cp1 < cpmin ||*/ cq1 < cqmin) {
                    goto next_jp;
                }
                if (*cp1-- != UNPACK_BASE(subject, cq1)) {
                    if (++n > jp->ok) {
                        goto next_jp;
                    }
                }
                cq1--;
            }
            /* current jumper row works */
            break;

next_jp:
            continue;
        }

        if (new_matches) {
            ASSERT(edit_script->num_ops < edit_script->num_allocated);
            JUMPER_EDIT_BLOCK_ADD(edit_script, new_matches);
            if (trace) {
                if (new_matches < window) {
                    trace <<= new_matches;
                }
                else {
                    trace = 0;
                }
            }
            num_identical += new_matches;
            score += new_matches * match_score;
            new_matches = 0;
            last_gap_open = 0;
        }

        /* update optimal alignment extent */
        if (score >= best_score) {
            cpstop = cp;
            cqstop = cq;
            best_score = score;
            num_ops = edit_script->num_ops;
            *best_num_identical = num_identical;
        }

        if (best_score - score > x_drop) {
            break;
        }

        /* update recent errors */
        if (jp->dcp == jp->dcq) {
            score += jp->dcp * mismatch_score;
            if (trace & trace_mask) {
                num_mismatches += jp->dcp;
                trace <<= jp->dcp;
                trace |= (1 << jp->dcp) - 1;
            }
            else {
                num_mismatches = jp->dcp;
                trace = (1 << jp->dcp) - 1;
            }
            for (i = 0;i < jp->dcp;i++) {
                ASSERT(edit_script->num_ops < edit_script->num_allocated);
                JUMPER_EDIT_BLOCK_ADD(edit_script, JUMPER_MISMATCH);
            }
        } else if (jp->dcp > jp->dcq) {
            for (i = 0;i < jp->dcp - jp->dcq;i++) {
                ASSERT(edit_script->num_ops < edit_script->num_allocated);
                JUMPER_EDIT_BLOCK_ADD(edit_script, JUMPER_INSERTION);
                score += gap_extend_score;
            }
            if (last_gap_open != JUMPER_INSERTION) {
                score += gap_open_score;
            }
            last_gap_open = JUMPER_INSERTION;
            if (jp->dcq) {
                /* either jp->dcp r jp->dcq must be zero or jp->dcp == jp->dcq
                   otherwise number of mismatches must be updated here */
                ASSERT(0);
            }
        }
        else {
            for (i = 0;i < jp->dcq - jp->dcp;i++) {
                ASSERT(edit_script->num_ops < edit_script->num_allocated);
                JUMPER_EDIT_BLOCK_ADD(edit_script, JUMPER_DELETION);
                score += gap_extend_score;
            }
            if (last_gap_open != JUMPER_DELETION) {
                score += gap_open_score;
            }
            last_gap_open  = JUMPER_DELETION;
            if (jp->dcp) {
                /* either jp->dcp r jp->dcq must be zero or jp->dcp == jp->dcq
                   otherwise number of mismatches must be updated here */
                ASSERT(0);
            }
        }

        /* update cp and cq */
        cp -= jp->dcp;
        cq -= jp->dcq;
        
        /* we have already checked that these are matches */
        if (cp1 > cpmin && !jp->ok && jp->lng) {
            cp -= jp->lng;
            cq -= jp->lng;
            ASSERT(edit_script->num_ops < edit_script->num_allocated);
            JUMPER_EDIT_BLOCK_ADD(edit_script, jp->lng);
            trace <<= jp->lng;
            num_identical += jp->lng;
            score += jp->lng * match_score;
            last_gap_open = 0;
        }

        /* update optimal alignment extent */
        if (score >= best_score) {
            cpstop = cp;
            cqstop = cq;
            best_score = score;
            num_ops = edit_script->num_ops;
            *best_num_identical = num_identical;
        }
    }

    /* if matches all the way to the end of a sequence */
    if (new_matches) {
        ASSERT(edit_script->num_ops < edit_script->num_allocated);
        JUMPER_EDIT_BLOCK_ADD(edit_script, new_matches);
        num_identical += new_matches;
        score += new_matches * match_score;
    }
    
    /* update optimal alignment extent */
    if (score >= best_score) {
        cpstop = cp;
        cqstop = cq;
        best_score = score;
        num_ops = edit_script->num_ops;
        *best_num_identical = num_identical;
    }

    *query_ext_len = query + query_offset - cpstop;
    *subject_ext_len = subject_offset - cqstop;
    edit_script->num_ops = num_ops;

    /* return extension score */
    return best_score;
} /* JumperExtendLeftCompressedWithTracebackOptimal */


Int4 JumperExtendLeft(const Uint1* query, const Uint1* subject,
                      Int4 query_offset, Int4 subject_offset,
                      Int4 match_score, Int4 mismatch_score,
                      Int4 gap_open_score, Int4 gap_extend_score,
                      int max_mismatches, int window,
                      int* query_ext_len, int* subject_ext_len,
                      GapPrelimEditBlock* edit_script)
{ 
    const Uint1 *cp, *cp1, *cpmin;
    Int4 cq, cq1, cqmin;
    int i, n;
    JUMP *jp;
    int score = 0, num_mismatches = 0;
    int new_matches = 0;
    Uint4 trace = 0;
    Uint4 trace_mask = (1 << max_mismatches) - 1;
    JUMP* jumper = jumper_default;

    if (!query || ! subject || !edit_script) {
        return -1;
    }

    cp = query + query_offset;
    cpmin = query;

    /* or assume matches up to byte edge */
    cq = subject_offset;
    cqmin = 0;

    while (cp >= cpmin && cq >= cqmin && num_mismatches < max_mismatches) {

        if (*cp == subject[cq]) {
            score += match_score;
            cp--;
            cq--;
            new_matches++;
            continue;
        }

        /* handle ambiguous bases */


        jp = jumper; 
        jp-- ;
        while (jp++) { /* 1, 1, 0, 0 = last always accepted */
         
            if (!jp->lng) {
                break;
            }

            n = 0;
            i = jp->ok;
            cp1 = cp - jp->dcp;
            cq1 = cq - jp->dcq;
            while (i--) {
                if (cp1 < cpmin || cq1 < cqmin
                    || *cp1-- != subject[cq1]) {

                    goto next_jp;
                }
                cq1--;
            }

            n = 0;
            i = jp->lng;
            cp1 = cp - jp->dcp;
            cq1 = cq - jp->dcq;
            if (cp1 - i < cpmin || cq1 - i < cqmin) {
                continue;
            }
            while (i--) {
                if (cp1 < cpmin || cq1 < cqmin) {
                    goto next_jp;
                }
                if (*cp1-- != subject[cq1]) {
                    if (++n > jp->ok) {
                        goto next_jp;
                    }
                }
                cq1--;
            }
            /* current jumper row works */
            break;

next_jp:
            continue;
        }

        if (new_matches) {
            GapPrelimEditBlockAdd(edit_script, eGapAlignSub, new_matches);
            if (trace) {
                if (new_matches < window) {
                    trace <<= new_matches;
                }
                else {
                    trace = 0;
                }
            }
            new_matches = 0;
        }

/* another stop condition, ensures a positive score *--/
        if (!jp->lng && num_mismatches >= score + mismatch_score) {
            break;
        }
*/

        /* update score */
        if (jp->dcp == jp->dcq) {
            score += mismatch_score * jp->dcp;
            if  (trace & trace_mask) {
                num_mismatches += jp->dcp;
                trace <<= jp->dcp;
                trace |= (1 << jp->dcp) - 1;
            }
            else {
                num_mismatches = jp->dcp;
                trace = (1 << jp->dcp) - 1;
            }
            GapPrelimEditBlockAdd(edit_script, eGapAlignSub, jp->dcp);
        } else if (jp->dcp > jp->dcq) {
            score += gap_open_score + gap_extend_score * (jp->dcp - jp->dcq);
            GapPrelimEditBlockAdd(edit_script, eGapAlignIns, jp->dcp - jp->dcq);
            ASSERT(!jp->dcq);
        }
        else {
            score += gap_open_score + gap_extend_score * (jp->dcq - jp->dcp);
            GapPrelimEditBlockAdd(edit_script, eGapAlignDel, jp->dcq - jp->dcp);
            ASSERT(!jp->dcp);
        }

        /* update cp and cq */
        cp -= jp->dcp;
        cq -= jp->dcq;
        
        /* we have already checked that these are matches */
        if (!jp->ok && jp->lng) {
            score += match_score * jp->lng;
            cp -= jp->lng;
            cq -= jp->lng;

            GapPrelimEditBlockAdd(edit_script, eGapAlignSub, jp->lng);
            trace <<= jp->lng;
        }
    }

    /* if matches all the way to the end of a sequence */
    if (new_matches) {
        GapPrelimEditBlockAdd(edit_script, eGapAlignSub, new_matches);
    }

    *query_ext_len = query + query_offset - cp;
    *subject_ext_len = subject_offset - cq;

    return score;
} /* JumperExtendLeft */


int JumperGappedAlignmentCompressedWithTraceback(const Uint1* query,
                               const Uint1* subject,
                               Int4 query_length, Int4 subject_length,
                               Int4 query_start, Int4 subject_start,
                               BlastGapAlignStruct* gap_align,
                               const BlastScoringParameters* score_params,
                               Int4* num_identical,
                               Int4* right_ungapped_ext_len)
{
    Int4 score_left = 0, score_right = 0;
    Int4 q_ext_len, s_ext_len;
    Int4 q_length, s_length;
    Int4 offset_adjustment;
    Boolean left_ext_done = FALSE;
    const Uint1 kBaseN = 14;
    Int4 i;


    ASSERT(gap_align->jumper);

    *num_identical = 0;

    JumperPrelimEditBlock** rev_prelim_block =
        &gap_align->jumper->left_prelim_block;

    JumperPrelimEditBlock** fwd_prelim_block =
        &gap_align->jumper->right_prelim_block;

    /* reallocate gapped preliminary edit blocks if necessary;
       edit scripts should not be longer than 2 times the length of the shorter
       sequence, so memory reallocation will not be necessary */
    if (!*rev_prelim_block || !*fwd_prelim_block ||
        (*rev_prelim_block)->num_allocated <
        2 * MIN(query_length, subject_length)) {

        JumperPrelimEditBlockFree(*rev_prelim_block);
        *rev_prelim_block =
            JumperPrelimEditBlockNew(2 * MIN(query_length, subject_length));

        JumperPrelimEditBlockFree(*fwd_prelim_block);
        *fwd_prelim_block =
            JumperPrelimEditBlockNew(2 * MIN(query_length, subject_length));
    }
    s_ResetJumperPrelimEditBlocks(*rev_prelim_block, *fwd_prelim_block);

    /* this is currently needed for right extension */
    offset_adjustment = COMPRESSION_RATIO - (subject_start % COMPRESSION_RATIO);

    q_length = query_start + offset_adjustment;
    s_length = subject_start + offset_adjustment;


    if (query_start > 0 && subject_start > 0) {

        score_left = JumperExtendLeftCompressedWithTracebackOptimal(
                                      query, subject,
                                      q_length, s_length,
                                      score_params->reward,
                                      score_params->penalty,
                                      -score_params->gap_open,
                                      -score_params->gap_extend,
                                      gap_align->max_mismatches,
                                      gap_align->mismatch_window,
                                      gap_align->gap_x_dropoff,
                                      gap_align->jumper->table,
                                      &q_ext_len, &s_ext_len,
                                      *rev_prelim_block,
                                      num_identical);


        gap_align->query_start = q_length - q_ext_len + 1;
        gap_align->subject_start = s_length - s_ext_len + 1;
        left_ext_done = TRUE;
        ASSERT(gap_align->query_start >= 0);
        ASSERT(gap_align->subject_start >= 0);
    }
    else {
        gap_align->query_start = query_start;
        gap_align->subject_start = subject_start;
    }
                                  
    if (query_start < query_length - 1 && subject_start < subject_length - 1) {

        score_right = JumperExtendRightCompressedWithTracebackOptimal(
                                        query + q_length,
                                        subject + (s_length + 3) / 4,
                                        query_length - q_length,
                                        subject_length - s_length,
                                        score_params->reward,
                                        score_params->penalty,
                                        -score_params->gap_open,
                                        -score_params->gap_extend,
                                        gap_align->max_mismatches,
                                        gap_align->mismatch_window,
                                        gap_align->gap_x_dropoff,
                                        gap_align->jumper->table,
                                        &q_ext_len, &s_ext_len,
                                        *fwd_prelim_block,
                                        num_identical,
                                        left_ext_done,
                                        right_ungapped_ext_len);

        gap_align->query_stop = q_length + q_ext_len;
        gap_align->subject_stop = s_length + s_ext_len;
    }
    else {
        gap_align->query_stop = query_start;
        gap_align->subject_stop = subject_start;
    }

    gap_align->score = score_left + score_right;

    if (offset_adjustment && !left_ext_done) {
        ASSERT((*rev_prelim_block)->num_ops < 
               (*rev_prelim_block)->num_allocated);
        JUMPER_EDIT_BLOCK_ADD(*rev_prelim_block, offset_adjustment);

        *num_identical += offset_adjustment;
        gap_align->score += offset_adjustment * score_params->reward;
    }
    if (offset_adjustment && *right_ungapped_ext_len) {
        *right_ungapped_ext_len += offset_adjustment;
    }

    /* Remove mismatch penalty for Ns. We assume that there are no gaps
       corresponfding to Ns (gaps may happen, but are unlikely). This to
       ensure that an alignment spanning the whole read containing a few Ns
       has a larger score than one covering a part. */
    for (i = gap_align->query_start;i < gap_align->query_stop;i++) {
        if (query[i] == kBaseN) {
            gap_align->score -= score_params->penalty;
        }
    }

    return 0;
}


Boolean JumperGoodAlign(const BlastGapAlignStruct* gap_align,
                        const BlastHitSavingParameters* hit_params,
                        Int4 num_identical,
                        BlastContextInfo* context_info)
{
    Int4 align_len;
    Int4 cutoff_score;
    Int4 edit_dist;
    Int4 score = gap_align->score;

    align_len = MAX(gap_align->query_stop - gap_align->query_start,
                    gap_align->subject_stop - gap_align->subject_start);

    /* check percent identity */
    if (100.0 * (double)num_identical / (double)align_len
        < hit_params->options->percent_identity) {

        return FALSE;
    }

    /* for spliced alignments thresholds apply to the final spliced
       alignment */
    if (hit_params->options->splice) {
        return TRUE;
    }

    cutoff_score = 0;
    if (hit_params->options->cutoff_score_fun[1] != 0) {
        cutoff_score = (hit_params->options->cutoff_score_fun[0] + 
                        context_info->query_length *
                        hit_params->options->cutoff_score_fun[1]) / 100;
    }
    else {
        cutoff_score = hit_params->options->cutoff_score;
    }

    /* for continuous alignments check score threshold here */
    if (score < cutoff_score) {
        return FALSE;
    }

    edit_dist = align_len - num_identical;
    if (edit_dist > hit_params->options->max_edit_distance) {
        return FALSE;
    }

    return TRUE;
}



JumperEditsBlock* JumperEditsBlockFree(JumperEditsBlock* block)
{
    if (!block) {
        return NULL;
    }

    if (block->edits) {
        sfree(block->edits);
    }
    sfree(block);

    return NULL;
}

JumperEditsBlock* JumperEditsBlockNew(Int4 num)
{
    JumperEditsBlock* retval =
        (JumperEditsBlock*)calloc(1, sizeof(JumperEditsBlock));

    if (!retval) {
        return NULL;
    }

    retval->edits = (JumperEdit*)calloc(num, sizeof(JumperEdit));
    if (!retval->edits) {
        JumperEditsBlockFree(retval);
        return NULL;
    }

    retval->num_edits = 0;
    return retval;
}

JumperEditsBlock* JumperEditsBlockDup(const JumperEditsBlock* block)
{
    JumperEditsBlock* retval = NULL;

    if (!block) {
        return NULL;
    }

    retval = JumperEditsBlockNew(block->num_edits);
    if (retval) {
        memcpy(retval->edits, block->edits,
               block->num_edits * sizeof(JumperEdit));
        retval->num_edits = block->num_edits;
    }

    return retval;
}

JumperEditsBlock* JumperFindEdits(const Uint1* query, const Uint1* subject,
                                  BlastGapAlignStruct* gap_align)
{
    const Uint1 kGap = 15;
    Int4 q_pos = gap_align->query_start;
    Int4 s_pos = gap_align->subject_start;

    JumperPrelimEditBlock* left_ext = gap_align->jumper->left_prelim_block;
    JumperPrelimEditBlock* right_ext = gap_align->jumper->right_prelim_block;
    
    JumperEditsBlock* retval = NULL;
    int len = left_ext->num_ops + right_ext->num_ops;
    int i, k;

    retval = JumperEditsBlockNew(len);
    if (!retval) {
        return NULL;
    }
    k = 0;
    for (i=left_ext->num_ops - 1;i >= 0;i--) {

        ASSERT(k < len);

        JumperEdit* edit = &retval->edits[k];
        JumperOpType* edit_op = &left_ext->edit_ops[i];
        switch(*edit_op) {
        case JUMPER_MISMATCH:
            edit->query_pos = q_pos;
            edit->query_base = query[q_pos];
            edit->subject_base = UNPACK_BASE(subject, s_pos);
            q_pos++;
            s_pos++;
            k++;
            break;

        case JUMPER_DELETION:
            edit->query_pos = q_pos;
            edit->query_base = kGap;
            edit->subject_base = UNPACK_BASE(subject, s_pos);
            s_pos++;
            k++;
            break;

        case JUMPER_INSERTION:
            edit->query_pos = q_pos;
            edit->query_base = query[q_pos];
            edit->subject_base = kGap;
            q_pos++;
            k++;
            break;

        default:
            q_pos += *edit_op;
            s_pos += *edit_op;
        }
    }

    for (i=0;i < right_ext->num_ops;i++) {

        ASSERT(k < len);

        JumperEdit* edit = &retval->edits[k];
        JumperOpType* edit_op = &right_ext->edit_ops[i];
        switch(*edit_op) {
        case JUMPER_MISMATCH:
            edit->query_pos = q_pos;
            edit->query_base = query[q_pos];
            edit->subject_base = UNPACK_BASE(subject, s_pos);
            q_pos++;
            s_pos++;
            k++;
            break;

        case JUMPER_DELETION:
            edit->query_pos = q_pos;
            edit->query_base = kGap;
            edit->subject_base = UNPACK_BASE(subject, s_pos);
            s_pos++;
            k++;
            break;

        case JUMPER_INSERTION:
            edit->query_pos = q_pos;
            edit->query_base = query[q_pos];
            edit->subject_base = kGap;
            q_pos++;
            k++;
            break;

        default:
            q_pos += *edit_op;
            s_pos += *edit_op;
        }
    }

    retval->num_edits = k;

    ASSERT(k <= len);
    ASSERT(q_pos == gap_align->query_stop);
    ASSERT(s_pos == gap_align->subject_stop);

    return retval;
}


JumperEditsBlock* JumperEditsBlockCombine(JumperEditsBlock** block_ptr,
                                          JumperEditsBlock** append_ptr)
{
    Int4 i;
    JumperEditsBlock* block = NULL;
    JumperEditsBlock* append = NULL;

    if (!block_ptr || !*block_ptr || !append_ptr) {
        return NULL;
    }

    block = *block_ptr;
    append = *append_ptr;

    if (!append || append->num_edits == 0) {
        *append_ptr = JumperEditsBlockFree(*append_ptr);
        return block;
    }

    block->edits = realloc(block->edits,
                           (block->num_edits + append->num_edits) *
                           sizeof(JumperEdit));

    if (!block->edits) {
        return NULL;
    }
    for (i = 0;i < append->num_edits;i++) {
        block->edits[block->num_edits++] = append->edits[i];
    }

    *append_ptr = JumperEditsBlockFree(*append_ptr);

    return block;
}

GapEditScript* GapEditScriptCombine(GapEditScript** edit_script_ptr,
                                    GapEditScript** append_ptr)
{
    Int4 i = 0;
    GapEditScript* edit_script = NULL;
    GapEditScript* append = NULL;

    if (!edit_script_ptr || !*edit_script_ptr || !append_ptr) {
        return NULL;
    }

    edit_script = *edit_script_ptr;
    append = *append_ptr;

    if (!append || append->size == 0) {
        *append_ptr = GapEditScriptDelete(*append_ptr);
        return edit_script;
    }

    edit_script->op_type = realloc(edit_script->op_type,
                                   (edit_script->size + append->size) *
                                   sizeof(EGapAlignOpType));
    if (!edit_script->op_type) {
        return NULL;
    }
    edit_script->num = realloc(edit_script->num,
                               (edit_script->size + append->size) *
                               sizeof(Int4));
    if (!edit_script->num) {
        return NULL;
    }

    if (edit_script->op_type[edit_script->size - 1] == append->op_type[0]) {
        edit_script->num[edit_script->size - 1] += append->num[0];
        i = 1;
    }
    for (;i < append->size;i++) {
        edit_script->op_type[edit_script->size] = append->op_type[i];
        edit_script->num[edit_script->size] = append->num[i];
        edit_script->size++;
    }

    *append_ptr = GapEditScriptDelete(*append_ptr);

    return edit_script;
}


#define NUM_SIGNALS 8

/* Record subject bases pre and post alignment in HSP as possible splice
   signals and set a flag for recognized signals */
int JumperFindSpliceSignals(BlastHSP* hsp, Int4 query_len,
                            const Uint1* subject, Int4 subject_len)
{
    Uint1 signals[NUM_SIGNALS] = {1, /* AC */
                                  2, /* AG */
                                  4, /* CA */
                                  7, /* CT */
                                  8, /* GA */
                                  11, /* GT */
                                  13, /* TC */
                                  14 /* TG */ };


    if (!hsp || !subject) {
        return -1;
    }

    if (hsp->query.offset == 0 || hsp->subject.offset < 2) {
        hsp->map_info->left_edge = MAPPER_EXON;
    }
    else {
        int k;
        hsp->map_info->left_edge =
            (UNPACK_BASE(subject, hsp->subject.offset - 2) << 2) |
            UNPACK_BASE(subject, hsp->subject.offset - 1);

        for (k = 0;k < NUM_SIGNALS;k++) {
            if (hsp->map_info->left_edge == signals[k]) {
                hsp->map_info->left_edge |= MAPPER_SPLICE_SIGNAL;
                break;
            }
        }
    }

    if (hsp->query.end == query_len || hsp->subject.end == subject_len) {
        hsp->map_info->right_edge = MAPPER_EXON;
    }
    else {
        int k;
        hsp->map_info->right_edge =
            (UNPACK_BASE(subject, hsp->subject.end) << 2) |
            UNPACK_BASE(subject, hsp->subject.end + 1);

        for (k = 0;k < NUM_SIGNALS;k++) {
            if (hsp->map_info->right_edge == signals[k]) {
                hsp->map_info->right_edge |= MAPPER_SPLICE_SIGNAL;
                break;
            }
        }
    }

    return 0;
}


#define MAX_SUBJECT_OVERHANG 30

SequenceOverhangs* SequenceOverhangsFree(SequenceOverhangs* overhangs)
{
    if (!overhangs) {
        return NULL;
    }

    if (overhangs->left) {
        sfree(overhangs->left);
    }

    if (overhangs->right) {
        sfree(overhangs->right);
    }

    sfree(overhangs);
    return NULL;
}


static Int4 s_SaveSubjectOverhangs(BlastHSP* hsp, Uint1* subject,
                                   Int4 query_len)
{
    SequenceOverhangs* overhangs = NULL;
    const Int4 kMinOverhangLength = 0;

    if (hsp->query.offset < kMinOverhangLength &&
        query_len - hsp->query.end < kMinOverhangLength) {

        return 0;
    }

    overhangs = calloc(1, sizeof(SequenceOverhangs));
    if (!overhangs) {
        return -1;
    }

    if (hsp->query.offset >= kMinOverhangLength) {
        Int4 i;
        /* at least two subject bases are needed for the search for splice
           signals */
        Int4 len = MIN(MAX(hsp->query.offset, 2), MAX_SUBJECT_OVERHANG);
        Uint1* overhang = calloc(len, sizeof(Uint1));
        if (!overhang) {
            SequenceOverhangsFree(overhangs);
            return -1;
        }
        if (hsp->subject.offset < len) {
            len = hsp->subject.offset;
        }
        for (i = 0;i < len; i++) {
            overhang[i] = UNPACK_BASE(subject, hsp->subject.offset - len + i);
        }
        overhangs->left = overhang;
        overhangs->left_len = len;
    }

    if (hsp->query.end <= query_len - kMinOverhangLength) {
        Int4 i;
        Int4 len =
            MIN(MAX(query_len - hsp->query.end + 1, 2), MAX_SUBJECT_OVERHANG);
        Uint1* overhang = calloc(len, sizeof(Uint1));
        if (!overhang) {
            SequenceOverhangsFree(overhangs);
            return -1;
        }
        for (i = 0;i < len;i++) {
            /* hsp->subject.end is an open limit */
            overhang[i] = UNPACK_BASE(subject, hsp->subject.end + i);
        }
        overhangs->right = overhang;
        overhangs->right_len = len;
    }

    hsp->map_info->subject_overhangs = overhangs;

    return 0;
}


static int s_CompareOffsetPairsByDiagQuery(const void* a, const void* b)
{
    BlastOffsetPair* first = (BlastOffsetPair*)a;
    BlastOffsetPair* second = (BlastOffsetPair*)b;

    Int4 first_diag = first->qs_offsets.s_off - first->qs_offsets.q_off;
    Int4 second_diag = second->qs_offsets.s_off - second->qs_offsets.q_off;

    if (first_diag < second_diag) {
        return -1;
    }
    if (first_diag > second_diag) {
        return 1;
    }

    if (first->qs_offsets.q_off < second->qs_offsets.q_off) {
        return -1;
    }
    if (first->qs_offsets.q_off > second->qs_offsets.q_off) {
        return 1;
    }

    if (first->qs_offsets.s_off < second->qs_offsets.s_off) {
        return -1;
    }
    if (first->qs_offsets.s_off > second->qs_offsets.s_off) {
        return 1;
    }

    return 0;
}


/* Create an HSP from a word hit */
static BlastHSP* s_CreateHSPForWordHit(Int4 q_offset, Int4 s_offset,
                                       Int4 length, Int4 context,
                                       const Uint1* query,
                                       const BlastQueryInfo* query_info,
                                       const BLAST_SequenceBlk* subject,
                                       Int4 query_len)
{
    BlastHSP* retval = NULL;
    GapEditScript* edit_script = NULL;
    Int2 status;
    Int4 i;
    Int4 num_Ns = 0;

    edit_script = GapEditScriptNew(1);
    if (!edit_script) {
        return NULL;
    }

    edit_script->num[0] = length;
    edit_script->op_type[0] = eGapAlignSub;
    

    status = Blast_HSPInit(q_offset,
                           q_offset + length,
                           s_offset,
                           s_offset + length,
                           q_offset, s_offset,
                           context,
                           query_info->contexts[context].frame,
                           subject->frame,
                           length,
                           &edit_script,
                           &retval);
    if (status) {
        if (retval) {
            Blast_HSPFree(retval);
        }
        else {
            GapEditScriptDelete(edit_script);
        }
        return NULL;
    }
    
    retval->map_info = BlastHSPMappingInfoNew();
    if (!retval->map_info) {
        Blast_HSPFree(retval);
        return NULL;
    }

    /* ambiguous bases in query are allowed for extending small word matches,
       so there can be Ns within the word hit */
    for (i = 0;i < length;i++) {
        if ((query[q_offset + i] & 0xfc) != 0) {
            num_Ns++;
        }
    }

    retval->num_ident = length - num_Ns;
    retval->evalue = 0.0;
    retval->map_info->edits = JumperEditsBlockNew(num_Ns);
    if (!retval->map_info->edits) {
        Blast_HSPFree(retval);
        return NULL;
    }

    /* add edits for ambiguous bases */
    for (i = 0;i < length;i++) {
        if ((query[q_offset + i] & 0xfc) != 0) {
            JumperEdit* edit = retval->map_info->edits->edits +
                retval->map_info->edits->num_edits;

            ASSERT(retval->map_info->edits->num_edits < num_Ns);
            edit->query_pos = q_offset + i;
            edit->query_base = query[q_offset + i];
            edit->subject_base = UNPACK_BASE(subject->sequence, s_offset + i);
            retval->map_info->edits->num_edits++;
        }
    }

    /* FIXME: This is currently needed because these splice sites are used
       in finding splice signals for overlapping HSPs */
    JumperFindSpliceSignals(retval, query_len, subject->sequence,
                            subject->length);

    s_SaveSubjectOverhangs(retval, subject->sequence, query_len);

    return retval;
}


static BlastHSP* s_CreateHSP(Uint1* query_seq,
                             Int4 query_len,
                             Int4 context,
                             BlastQueryInfo* query_info,
                             BlastGapAlignStruct* gap_align,
                             BLAST_SequenceBlk* subject,
                             const BlastScoringParameters* score_params,
                             const BlastHitSavingParameters* hit_params)
{
    Int4 num_identical = 0;
    Int4 status;
    BlastHSP* new_hsp = NULL;

    if (!getenv("MAPPER_NO_GAP_SHIFT")) {
        s_ShiftGaps(gap_align, query_seq, subject->sequence,
                    query_len, subject->length,
                    score_params->penalty, &num_identical);
    }

    gap_align->edit_script = JumperPrelimEditBlockToGapEditScript(
                                      gap_align->jumper->left_prelim_block,
                                      gap_align->jumper->right_prelim_block);


    status = Blast_HSPInit(gap_align->query_start,
                           gap_align->query_stop,
                           gap_align->subject_start,
                           gap_align->subject_stop,
                           gap_align->query_start,
                           gap_align->subject_start,
                           context,
                           query_info->contexts[context].frame,
                           subject->frame,
                           gap_align->score,
                           &(gap_align->edit_script),
                           &new_hsp);

    if (!new_hsp || status) {
        return NULL;
    }
    new_hsp->map_info = BlastHSPMappingInfoNew();
    if (!new_hsp->map_info) {
        return NULL;
    }

    new_hsp->num_ident = num_identical;
    new_hsp->evalue = 0.0;
    new_hsp->map_info->edits =
        JumperFindEdits(query_seq, subject->sequence, gap_align);

    if (hit_params->options->splice) {
        /* FIXME: This is currently needed because these splice 
           sites are used in finding splice signals for overlapping
           HSPs */
        JumperFindSpliceSignals(new_hsp, query_len, subject->sequence,
                                subject->length);

        s_SaveSubjectOverhangs(new_hsp, subject->sequence, query_len);
    }
                
    return new_hsp;
}

/* for mapping this may only work if we hash genome and scan reads */
Int4
BlastNaExtendJumper(BlastOffsetPair* offset_pairs, Int4 num_hits,
                    const BlastInitialWordParameters* word_params,
                    const BlastScoringParameters* score_params,
                    const BlastHitSavingParameters* hit_params,
                    LookupTableWrap* lookup_wrap,
                    BLAST_SequenceBlk* query,
                    BLAST_SequenceBlk* subject,
                    BlastQueryInfo* query_info,
                    BlastGapAlignStruct* gap_align,
                    BlastHSPList* hsp_list,
                    Uint4 s_range,
                    SubjectIndex* s_index)
{
    Int4 index = 0;
    Int4 hits_extended = 0;
    Int4 word_length, lut_word_length, ext_to;
    Int4 context;
    int status;
    Int4 num_identical = 0;
    /* word matches until this query position will be skipped */
    Int4 skip_until = 0;
    Int4 last_diag = 0;

    if (lookup_wrap->lut_type == eMBLookupTable) {
        BlastMBLookupTable *lut = (BlastMBLookupTable *) lookup_wrap->lut;
        word_length = lut->word_length;
        lut_word_length = lut->lut_word_length;
    } 
    else {
        BlastNaLookupTable *lut = (BlastNaLookupTable *) lookup_wrap->lut;
        word_length = lut->word_length;
        lut_word_length = lut->lut_word_length;
    }
    ext_to = word_length - lut_word_length;

    /* We trust that the bases of the hit itself are exact matches, 
       and look only for exact matches before and after the hit.

       Most of the time, the lookup table width is close to the word size 
       so only a few bases need examining. Also, most of the time (for
       random sequences) extensions will fail almost immediately (the
       first base examined will not match about 3/4 of the time). Thus it 
       is critical to reduce as much as possible all work that is not the 
       actual examination of sequence data */

    /* Sort word hits by diagnal, query, and subject position */
    qsort(offset_pairs, num_hits, sizeof(BlastOffsetPair),
          s_CompareOffsetPairsByDiagQuery);

    for (; index < num_hits; ++index) {
        Int4 s_offset = offset_pairs[index].qs_offsets.s_off;
        Int4 q_offset = offset_pairs[index].qs_offsets.q_off;
        Int4 query_start;
        /* FIXME: this may need to be Int8 for 10M queries or long reads */
        Int4 diag = s_offset - q_offset;

        /* skip word hits from already explored part of the diagonal (word
           hits must be sorted by diagonal and query position) */
        if (diag == last_diag && q_offset < skip_until) {
            continue;
        }

        /* begin with the left extension; the initialization is slightly
           faster. Point to the first base of the lookup table hit and
           work backwards */

        Int4 ext_left = 0;
        Int4 s_off = s_offset;
        Uint1 *q = query->sequence + q_offset;
        Uint1 *s = subject->sequence + s_off / COMPRESSION_RATIO;

        for (; ext_left < MIN(ext_to, s_offset); ++ext_left) {
            s_off--;
            q--;
            if (s_off % COMPRESSION_RATIO == 3)
                s--;
            if (((Uint1) (*s << (2 * (s_off % COMPRESSION_RATIO))) >> 6)
                != *q)
                break;
        }

        /* do the right extension if the left extension did not find all
           the bases required. Begin at the first base beyond the lookup
           table hit and move forwards */

        if (ext_left < ext_to) {
            Int4 ext_right = 0;
            s_off = s_offset + lut_word_length;
            if (s_off + ext_to - ext_left > s_range) 
                continue;
            q = query->sequence + q_offset + lut_word_length;
            s = subject->sequence + s_off / COMPRESSION_RATIO;

            for (; ext_right < ext_to - ext_left; ++ext_right) {
                if (((Uint1) (*s << (2 * (s_off % COMPRESSION_RATIO))) >>
                     6) != *q)
                    break;
                s_off++;
                q++;
                if (s_off % COMPRESSION_RATIO == 0)
                    s++;
            }

            /* check if enough extra matches were found */
            if (ext_left + ext_right < ext_to)
                continue;
        }
        
        q_offset -= ext_left;
        s_offset -= ext_left;

        /* adjust query offset */
        context = BSearchContextInfo(q_offset, query_info);
        query_start = query_info->contexts[context].query_offset;
        q_offset -= query_start;
        ASSERT(q_offset >= 0);

        Int4 query_len = query_info->contexts[context].query_length;
        Uint1* query_seq = query->sequence + query_start;
        Int4 right_ungapped_ext_len = 0;

        JumperGappedAlignmentCompressedWithTraceback(query_seq,
                                                     subject->sequence,
                                                     query_len,
                                                     subject->length,
                                                     q_offset,
                                                     s_offset,
                                                     gap_align,
                                                     score_params, 
                                                     &num_identical,
                                                     &right_ungapped_ext_len);

        hits_extended++;
        /* Word hits on the same diagonal up to this query position will be
           skipped. right_ungapped_ext_len gives the length of the first
           ungapped segment of the right extension, hence word hits on this
           part of the diagonal will not generate better alignment than the
           one just found. This is assuming that word hits are sorted by
           diagonal and query position. */
        skip_until = q_offset + query_start + right_ungapped_ext_len;
        last_diag = diag;

        if (JumperGoodAlign(gap_align, hit_params, num_identical,
                            &query_info->contexts[context])) {

            BlastHSP* new_hsp;
            Uint1* query_seq = query->sequence + query_start;
            /* minimum length of the unaligned part of the subject to search
               for matching small words */
            const Int4 kMinSubjectOverhang = 100;
            /* maximum intron length, i.e. gap between two hsps that can be
               combined */
            const Int4 kMaxIntronLength = hit_params->options->longest_intron;

            /* shift gaps the right */
            /* When several gap positions give the same alignment score,
               gap positions in the alignment are arbitrary. Here we ensure
               that gaps will be at the same postitions for all reads
               that map to the same place in the genome. */
            if (!getenv("MAPPER_NO_GAP_SHIFT")) {
                s_ShiftGaps(gap_align, query_seq, subject->sequence,
                            query_len, subject->length,
                            score_params->penalty, &num_identical);
            }

            gap_align->edit_script = JumperPrelimEditBlockToGapEditScript(
                                        gap_align->jumper->left_prelim_block,
                                        gap_align->jumper->right_prelim_block);


            status = Blast_HSPInit(gap_align->query_start,
                                   gap_align->query_stop,
                                   gap_align->subject_start,
                                   gap_align->subject_stop,
                                   q_offset, s_offset,
                                   context,
                                   query_info->contexts[context].frame,
                                   subject->frame,
                                   gap_align->score,
                                   &(gap_align->edit_script),
                                   &new_hsp);
            if (!new_hsp) {
                return -1;
            }
            new_hsp->map_info = BlastHSPMappingInfoNew();
            if (!new_hsp->map_info) {
                return -1;
            }
            new_hsp->num_ident = num_identical;
            new_hsp->evalue = 0.0;
            new_hsp->map_info->edits =
                    JumperFindEdits(query_seq, subject->sequence, gap_align);

            if (hit_params->options->splice) {
                /* FIXME: This is currently needed because these splice 
                   sites are used in finding splice signals for overlapping
                   HSPs */
                JumperFindSpliceSignals(new_hsp, query_len, subject->sequence,
                                        subject->length);

                s_SaveSubjectOverhangs(new_hsp, subject->sequence, query_len);
            }
                
            ASSERT(new_hsp);
            status = Blast_HSPListSaveHSP(hsp_list, new_hsp);
            if (status) {
                break;
            }


            /* If left of right query overhang is shorter than word size, then
               if overhang length is smaller than mismatch penalty,
               first assume the alignment ended with a mismatch and try to
               extend it with perfect matches to the end of the query. If this
               does not succeed, then look for 4-base word matches from
               query overhangs. */

            /* first for the right end of the query */
            if (getenv("MAPPER_USE_SMALL_WORDS") &&
                query_len - new_hsp->query.end < 16 &&
                query_len - new_hsp->query.end >= SUBJECT_INDEX_WORD_LENGTH &&
                subject->length - new_hsp->subject.end >= kMinSubjectOverhang) {

                Int4 q = new_hsp->query.end;
                Int4 round = 0;
                Boolean found = FALSE;
                ASSERT(s_index);

                /* if number of unaligned query bases is smaller than
                   mismatch penalty, then try extending the alignment past the
                   mismatch to the end of the query */
                if (query_len - new_hsp->query.end < -score_params->penalty) {
                    Int4 i;
                    for (i = 1;i < query_len - new_hsp->query.end;i++) {
                        if (query_seq[new_hsp->query.end + i] !=
                            UNPACK_BASE(subject->sequence,
                                        new_hsp->subject.end + i)) {
                            break;
                        }
                    }

                    /* if extension succeeded through 4 bases or till the end
                       of thequery, create and save an HSP */
                    if (i > 4 || i == query_len - new_hsp->query.end) {

                        BlastHSP* w_hsp = s_CreateHSPForWordHit(
                                               new_hsp->query.end + 1,
                                               new_hsp->subject.end + 1,
                                               i - 1,
                                               context, query_seq,
                                               query_info, subject,
                                               query_len);

                        ASSERT(w_hsp->query.offset >= 0 &&
                               w_hsp->query.end <= query_len);
                        status = Blast_HSPListSaveHSP(hsp_list, w_hsp);
                        if (status) {
                            return -1;
                        }
                    }
                }

                for (; !found && q + SUBJECT_INDEX_WORD_LENGTH <= query_len &&
                         round < 2;q++, round++) {
                    Int4 word;
                    Int4 from;
                    Int4 s_pos;
                    SubjectIndexIterator* it = NULL;
                    Int4 i;

                    /* skip over ambiguous bases */
                    while (q + SUBJECT_INDEX_WORD_LENGTH <= query_len) {
                        for (i = 0;i < SUBJECT_INDEX_WORD_LENGTH;i++) {
                            if ((query_seq[q + i] & 0xfc) != 0) {
                                q = q + i + 1;
                                break;
                            }
                        }

                        if (i == SUBJECT_INDEX_WORD_LENGTH) {
                            break;
                        }
                    }

                    if (q + SUBJECT_INDEX_WORD_LENGTH - 1 >= query_len) {
                        break;
                    }

                    /* this is query word */
                    word = (query_seq[q] << 6) | (query_seq[q + 1] << 4) |
                        (query_seq[q + 2] << 2) | query_seq[q + 3];
                    for (i = 4; i < SUBJECT_INDEX_WORD_LENGTH; i++) {
                        word = (word << 2) | query_seq[q + i];
                    }

                    /* search subject for matching words from this position */
                    from = new_hsp->subject.end;

                    /* create subject word iterator and iterate from the end
                       of current HSP through the max intron length or up to
                       the end of subject less query overhang bases (so that
                       we can extend to the end of the query) */
                    it = SubjectIndexIteratorNew(s_index, word, from,
                                MIN((from + kMaxIntronLength),
                                    (subject->length - (query_len - q + 1))));

                    /* for each word match position in the subject */
                    for (s_pos = SubjectIndexIteratorNext(it); s_pos >= 0;
                         s_pos = SubjectIndexIteratorNext(it)) {
                        Int4 qt;
                        Int4 st;

                        /* try to extend the hit right to the end of the
                           query accepting only matches */
                        qt = q;
                        st = s_pos;
                        while (qt < query_len && st < subject->length &&
                               (query_seq[qt] ==
                                UNPACK_BASE(subject->sequence, st) ||
                                /* skip over ambiguous bases */
                                (query_seq[qt] & 0xfc) != 0)) {
                            qt++;
                            st++;
                        }

                        /* proceed only if query matches to the end and try
                           extending left as much as possible */
                        if (qt == query_len) {
                            Int4 qf = q;
                            Int4 sf = s_pos;
                            BlastHSP* w_hsp = NULL;

                            while (qf >= 0 && sf >= 0 &&
                                   (query_seq[qf] ==
                                    UNPACK_BASE(subject->sequence, sf) ||
                                    (query_seq[qf] & 0xfc) != 0)) {

                                qf--;
                                sf--;
                            }
                            qf++;
                            sf++;
                            ASSERT(qt - qf == st - sf);

                            if (qt - qf < 5 ||
                                qf > new_hsp->query.end + 1 ||
                                qf <= new_hsp->query.offset) {

                                continue;
                            }


                            /* trim ambiguous bases at the ends of the word
                               match */
                            while (qf < qt && (query_seq[qf] & 0xfc) != 0) {
                                qf++;
                                sf++;
                            }

                            while (qt > qf && (query_seq[qt - 1] & 0xfc) != 0) {
                                qt--;
                                st--;
                            }

                            if (qf >= qt) {
                                continue;
                            }

                            /* create HSP */
                            w_hsp = s_CreateHSPForWordHit(qf, sf, qt - qf,
                                                          context, query_seq,
                                                          query_info, subject,
                                                          query_len);

                            ASSERT(w_hsp->query.offset >= 0 &&
                                   w_hsp->query.end <= query_len);

                            ASSERT(w_hsp);
                            if (!w_hsp) {
                                return -1;
                            }

                            /* add HSP to the list */
                            status = Blast_HSPListSaveHSP(hsp_list, w_hsp);
                            if (status) {
                                return -1;
                            }
                        }
                    }
                    it = SubjectIndexIteratorFree(it);
                }
            }


            if (getenv("MAPPER_USE_SMALL_WORDS") &&
                new_hsp->query.offset < 16 &&
                new_hsp->query.offset >= SUBJECT_INDEX_WORD_LENGTH &&
                new_hsp->subject.offset >= kMinSubjectOverhang) {

                Int4 q = MAX(new_hsp->query.offset - SUBJECT_INDEX_WORD_LENGTH,
                             0);
                Int4 round = 0;
                Boolean found = FALSE;
                ASSERT(s_index);

                /* if number of unaligned query bases is smaller than
                   mismatch penalty, then try extending the alignment past the
                   mismatch to beginning of the query */
                if (new_hsp->query.offset < -score_params->penalty) {
                    Int4 i;
                    for (i = 2;i < new_hsp->query.offset - 1;i++) {
                        if (query_seq[new_hsp->query.offset - i] !=
                            UNPACK_BASE(subject->sequence,
                                        new_hsp->subject.offset - i)) {
                            break;
                        }
                    }

                    /* if the extension suceeded thrught at least 4 bases or
                       to the beginning of the query, create and save the HSP */
                    if (i > 4 || i == new_hsp->query.offset - 1) {

                        BlastHSP* w_hsp = s_CreateHSPForWordHit(
                                               new_hsp->query.offset - 1 - i,
                                               new_hsp->subject.offset - 1- i,
                                               i,
                                               context, query_seq,
                                               query_info, subject,
                                               query_len);

                        ASSERT(w_hsp->query.offset >= 0 &&
                               w_hsp->query.end <= query_len);
                        status = Blast_HSPListSaveHSP(hsp_list, w_hsp);
                        if (status) {
                            return -1;
                        }
                    }
                }


                for (; !found && q >= 0 && round < 2;q--, round++) {

                    Int4 word;
                    Int4 from;
                    Int4 s_pos;
                    SubjectIndexIterator* it = NULL;
                    Int4 i;

                    /* skip over ambiguous bases */
                    while (q >= 0) {
                        for (i = 0;i < SUBJECT_INDEX_WORD_LENGTH;i++) {
                            if ((query_seq[q + i] & 0xfc) != 0) {
                                q = q + i - SUBJECT_INDEX_WORD_LENGTH;
                                break;
                            }
                        }

                        if (i == SUBJECT_INDEX_WORD_LENGTH) {
                            break;
                        }
                    }

                    if (q < 0) {
                        break;
                    }


                    /* this is query word */
                    word = (query_seq[q] << 6) | (query_seq[q + 1] << 4) |
                        (query_seq[q + 2] << 2) | query_seq[q + 3];
                    for (i = 4; i < SUBJECT_INDEX_WORD_LENGTH; i++) {
                        word = (word << 2) | query_seq[q + i];
                    }


                    /* search subject for matching words from this position */
                    from = new_hsp->subject.offset - SUBJECT_INDEX_WORD_LENGTH;

                    it = SubjectIndexIteratorNew(s_index, word, from,
                                        MAX(from - kMaxIntronLength, q + 1));

                    /* for each word match position in the subject */
                    for (s_pos = SubjectIndexIteratorPrev(it); s_pos >= 0;
                         s_pos = SubjectIndexIteratorPrev(it)) {

                        /* try extending left */
                        Int4 k = q;
                        Int4 s = s_pos;
                        while (k >= 0 && 
                               (query_seq[k] ==
                                UNPACK_BASE(subject->sequence, s) ||
                                (query_seq[k] & 0xfc) != 0)) {
                            k--;
                            s--;
                        }
                        /* if query matches all the way, then extend right as
                           far as there are matches */
                        if (k == -1) {
                            Int4 qt = q;
                            Int4 st = s_pos;
                            BlastHSP* w_hsp = NULL;

                            k++;
                            s++;
                            while (qt < query_len && st < subject->length && 
                                   (query_seq[qt] ==
                                    UNPACK_BASE(subject->sequence, st) ||
                                    /* skip over ambiguous bases */
                                    (query_seq[qt] & 0xfc) != 0)) {
                                qt++;
                                st++;
                            }
                            ASSERT(k - qt == s - st);


                            if (qt - k < 5 || 
                                s >= new_hsp->subject.offset ||
                                new_hsp->query.offset > qt + 1) {
                                continue;
                            }


                            /* trim ambiguous bases at the ends of the word
                               match */
                            while (k < qt && (query_seq[k] & 0xfc) != 0) {
                                k++;
                                s++;
                            }

                            while (qt > k && (query_seq[qt] & 0xfc) != 0) {
                                qt--;
                                st--;
                            }

                            if (k >= qt) {
                                continue;
                            }

                            /* create HSP */
                            w_hsp = s_CreateHSPForWordHit(k, s, qt - k,
                                                          context, query_seq,
                                                          query_info, subject,
                                                          query_len);

                            ASSERT(w_hsp->query.offset >= 0 &&
                                   w_hsp->query.end <= query_len);

                            ASSERT(w_hsp);
                            if (!w_hsp) {
                                return -1;
                            }

                            status = Blast_HSPListSaveHSP(hsp_list, w_hsp);
                            if (status) {
                                return -1;
                            }
                        }
                    }
                    it = SubjectIndexIteratorFree(it);
                }
            }

        }
    }

    return hits_extended;
}


SubjectIndex* SubjectIndexFree(SubjectIndex* sindex)
{
    if (!sindex) {
        return NULL;
    }

    if (sindex->lookups) {
        Int4 i;
        for (i = 0;i < sindex->num_lookups;i++) {
            if (sindex->lookups[i]) {
                BlastNaLookupTableDestruct(sindex->lookups[i]);
            }
        }
        free(sindex->lookups);
    }

    free(sindex);
    return NULL;
}

static void 
s_SubjectIndexNewCleanup(BLAST_SequenceBlk* sequence, BlastSeqLoc* seqloc,
                         LookupTableOptions* opt, QuerySetUpOptions* query_opt,
                         SubjectIndex* sindex)
{
    if (sequence) {
        if (sequence->sequence) {
            free(sequence->sequence);
        }
        free(sequence);
    }

    while (seqloc) {
        BlastSeqLoc* s = seqloc->next;
        if (seqloc->ssr) {
            free(seqloc->ssr);
        }
        free(seqloc);
        seqloc = s;
    }

    if (opt) {
        free(opt);
    }

    if (query_opt) {
        free(query_opt);
    }

    SubjectIndexFree(sindex);
}

SubjectIndex* SubjectIndexNew(BLAST_SequenceBlk* subject, Int4 width,
                              Int4 word_size)
{
    Int4 i;
    BLAST_SequenceBlk* sequence = NULL;
    BlastSeqLoc* seqloc = NULL;
    SSeqRange* ssr = NULL;
    LookupTableOptions* opt = NULL;
    QuerySetUpOptions* query_opt = NULL;
    SubjectIndex* retval = NULL;
    Int4 num_lookups = subject->length / width + 1;
    
    sequence = calloc(1, sizeof(BLAST_SequenceBlk));
    if (!sequence) {
        return NULL;
    }

    /* convert subject sequence to blastna */
    sequence->sequence = calloc(subject->length, sizeof(Uint1));
    if (!sequence->sequence) {
        free(sequence);
        return NULL;
    }

    for (i = 0;i < subject->length / 4;i++) {
        Int4 k;
        for (k = 0;k < 4;k++) {
            sequence->sequence[4 * i + k] =
                (subject->sequence[i] >> (2 * (3 - k))) & 3;
        }
    }

    retval = calloc(1, sizeof(SubjectIndex));
    if (!retval) {
        s_SubjectIndexNewCleanup(sequence, seqloc, opt, query_opt, retval);
        return NULL;
    }

    retval->lookups = calloc(num_lookups, sizeof(BlastNaLookupTable*));
    if (!retval->lookups) {
        s_SubjectIndexNewCleanup(sequence, seqloc, opt, query_opt, retval);
        return NULL;
    }

    ssr = malloc(sizeof(SSeqRange));
    if (!ssr) {
        s_SubjectIndexNewCleanup(sequence, seqloc, opt, query_opt, retval);
        return NULL;
    }

    seqloc = calloc(1, sizeof(BlastSeqLoc));
    if (!seqloc) {
        if (ssr) {
            free(ssr);
        }
        s_SubjectIndexNewCleanup(sequence, seqloc, opt, query_opt, retval);
        return NULL;
    }

    opt = calloc(1, sizeof(LookupTableOptions));
    if (!opt) {
        s_SubjectIndexNewCleanup(sequence, seqloc, opt, query_opt, retval);
        return NULL;
    }
    opt->word_size = 4;

    query_opt = calloc(1, sizeof(QuerySetUpOptions));
    if (!query_opt) {
        s_SubjectIndexNewCleanup(sequence, seqloc, opt, query_opt, retval);
        return NULL;
    }


    for (i = 0;i < num_lookups;i++) {

        ssr->left = width * i;
        ssr->right = MIN(ssr->left + width, subject->length - 1);

        seqloc->ssr = ssr;

        BlastNaLookupTableNew(sequence, seqloc, &(retval->lookups[i]), opt,
                              query_opt, word_size);

        if (!retval->lookups[i]) {
            s_SubjectIndexNewCleanup(sequence, seqloc, opt, query_opt, retval);
        }

        ASSERT(i < num_lookups);
    }
    retval->num_lookups = i;
    retval->width = width;

    s_SubjectIndexNewCleanup(sequence, seqloc, opt, query_opt, NULL);

    return retval;
}


SubjectIndexIterator* SubjectIndexIteratorFree(SubjectIndexIterator* it)
{
    if (it) {
        free(it);
    }

    return NULL;
}

/* Create an iterator for positions of a given word in subject sequence between
   positions from and to */
SubjectIndexIterator* SubjectIndexIteratorNew(SubjectIndex* s_index, Int4 word,
                                              Int4 from, Int4 to)
{
    SubjectIndexIterator* retval = NULL;

    if (!s_index || !s_index->lookups[0]) {
        return NULL;
    }

    retval = calloc(1, sizeof(SubjectIndexIterator));
    if (!retval) {
        return NULL;
    }

    retval->subject_index = s_index;
    retval->to = to;
    retval->lookup_index = from / s_index->width;
    ASSERT(retval->lookup_index < s_index->num_lookups);
    if (retval->lookup_index >= s_index->num_lookups) {
        SubjectIndexIteratorFree(retval);
        return NULL;
    }

    /* find the first word with position at least from */
    while (retval->lookup_index < retval->subject_index->num_lookups) {
        BlastNaLookupTable* lookup = s_index->lookups[retval->lookup_index];
        if (!lookup) {
            SubjectIndexIteratorFree(retval);
            return NULL;
        }

        word = word & lookup->mask;
        retval->num_words = lookup->thick_backbone[word].num_used;
        if (retval->num_words <= NA_HITS_PER_CELL) {
            retval->lookup_pos = lookup->thick_backbone[word].payload.entries;
        }
        else {
            retval->lookup_pos = lookup->overflow +
                lookup->thick_backbone[word].payload.overflow_cursor;
        }

        retval->word = word;
        retval->word_index = 0;

        while (retval->word_index < retval->num_words &&
               retval->lookup_pos[retval->word_index] < from) {
        
            retval->word_index++;
        }

        if (retval->word_index >= retval->num_words) {
            retval->lookup_index++;
        }
        else {
            break;
        }
    }

    return retval;
}

/* Return current position of a word and move iterator to the next position */
Int4 SubjectIndexIteratorNext(SubjectIndexIterator* it)
{
    Int4 pos;

    if (!it) {
        return -1;
    }

    /* if all words in the current lookup table were processed, move to the
       next lookup table */
    if (it->word_index >= it->num_words) {
        BlastNaLookupTable* lookup = NULL;
        it->lookup_index++;

        /* if no more lookup table return no positon found */
        if (it->lookup_index >= it->subject_index->num_lookups) {
            return -1;
        }
        lookup = it->subject_index->lookups[it->lookup_index];
        ASSERT(lookup);
        it->num_words = lookup->thick_backbone[it->word].num_used;
        if (it->num_words <= NA_HITS_PER_CELL) {
            it->lookup_pos = lookup->thick_backbone[it->word].payload.entries;
        }
        else {
            it->lookup_pos = lookup->overflow +
                lookup->thick_backbone[it->word].payload.overflow_cursor;
        }
        ASSERT(it->lookup_pos);
        it->word_index = 0;
    }

    if (!it->lookup_pos) {
        return -1;
    }

    pos = it->lookup_pos[it->word_index];
    if (pos > it->to) {
        return -1;
    }

    /* move iterator to the next position */
    it->word_index++;

    return pos;
}

/* Return current position of a word and move iterator to the previous
   position */
Int4 SubjectIndexIteratorPrev(SubjectIndexIterator* it)
{
    Int4 pos;

    if (!it) {
        return -1;
    }

    if (it->word_index < 0) {
        BlastNaLookupTable* lookup = NULL;
        it->lookup_index--;
        if (it->lookup_index < 0) {
            return -1;
        }
        lookup = it->subject_index->lookups[it->lookup_index];
        ASSERT(lookup);
        it->num_words = lookup->thick_backbone[it->word].num_used;
        if (it->num_words <= NA_HITS_PER_CELL) {
            it->lookup_pos = lookup->thick_backbone[it->word].payload.entries;
        }
        else {
            it->lookup_pos = lookup->overflow +
                lookup->thick_backbone[it->word].payload.overflow_cursor;
        }
        ASSERT(it->lookup_pos);
        it->word_index = it->num_words - 1;
    }

    if (!it->lookup_pos) {
        return -1;
    }

    pos = it->lookup_pos[it->word_index];
    if (pos < it->to) {
        return -1;
    }

    it->word_index--;

    return pos;
}


#define MAX_NUM_MATCHES 10

static Int4 DoAnchoredScan(Uint1* query_seq, Int4 query_len,
                           Int4 query_from, Int4 context,
                           BLAST_SequenceBlk* subject,
                           Int4 subject_from, Int4 subject_to,
                           BlastQueryInfo* query_info,
                           BlastGapAlignStruct* gap_align,
                           const BlastScoringParameters* score_params,
                           const BlastHitSavingParameters* hit_params,
                           BlastHSPList* hsp_list)
                     
{
    Int4 q = query_from;
    BlastHSP* hsp = NULL;
    Int4 num = 0;
    const int kMaxNumMatches = MAX_NUM_MATCHES;
    Int4 num_extensions = 0;
    Int4 word_size = 12;
    Int4 big_word_size = 0;
    Int4 scan_step;

    Uint4 word[MAX_NUM_MATCHES];
    Int4 query_pos[MAX_NUM_MATCHES];
    Uint4 w;
    Int4 i;

    Int4 scan_from = subject_from;
    Int4 scan_to = MIN(subject_to, subject->length - 1);
    
    Uint4 mask = (1U << (2 * word_size)) - 1;

    Int4 best_score = 0;
    Int4 num_matches = 0;
    Uint1* s = NULL;
    Int4 last_idx = 0;
    Int4 num_bytes = 0;
    Int4 num_words = 0;

    Boolean is_right = subject_from < subject_to;
    if (is_right) {
        big_word_size = MIN(MAX(query_len - query_from - 5, word_size), 24);
        scan_step = big_word_size - word_size + 1;

    }
    else {
        big_word_size = MIN(MAX(query_from - 5, word_size), 24);
        scan_step = -(big_word_size - word_size + 1);
    }
    
    if ((is_right && (query_len - query_from + 1 < big_word_size ||
                      scan_to - subject_from < big_word_size)) ||
        (!is_right && (query_from < big_word_size ||
                       subject_from - scan_to < big_word_size))) {

        return 0;
    }

    

    if (is_right) {
        for (; q + big_word_size < query_len && num_words < MAX_NUM_MATCHES; q++) {

            /* skip over ambiguous bases */
            while (q + big_word_size <= query_len) {
                for (i = 0;i < big_word_size;i++) {
                    if ((query_seq[q + i] & 0xfc) != 0) {
                        q = q + i + 1;
                        break;
                    }
                }

                /* success */
                if (i == big_word_size) {
                    break;
                }

                q++;
            }

            /* not enough query left */
            if (q + big_word_size - 1 >= query_len) {
                break;
            }

            /* this is query word */
            word[num_words] = (query_seq[q] << 6) | (query_seq[q + 1] << 4) |
                (query_seq[q + 2] << 2) | query_seq[q + 3];
            for (i = 4; i < word_size; i++) {
                word[num_words] = (word[num_words] << 2) | query_seq[q + i];
            }

            /* do not search for PolyA words */
            if (word[num_words] == 0 || word[num_words] == 0xffffff) {
                continue;
            }

            query_pos[num_words] = q;
            num_words++;
        }
    }
    else {
        q -= big_word_size;
        for (; q >= 0 && num_words < MAX_NUM_MATCHES; q--) {

            /* skip over ambiguous bases */
            while (q >= 0) {
                for (i = 0;i < big_word_size;i++) {
                    if ((query_seq[q + i] & 0xfc) != 0) {
                        q = q - big_word_size + i;
                        break;
                    }
                }

                /* success */
                if (i == big_word_size) {
                    break;
                }

                q--;
            }

            /* not enough query left */
            if (q < 0) {
                break;
            }

            /* this is query word */
            word[num_words] = (query_seq[q] << 6) | (query_seq[q + 1] << 4) |
                (query_seq[q + 2] << 2) | query_seq[q + 3];
            for (i = 4; i < word_size; i++) {
                word[num_words] = (word[num_words] << 2) | query_seq[q + i];
            }

            /* do not search for PolyA words */
            if (word[num_words] == 0 || word[num_words] == 0xffffff) {
                continue;
            }

            query_pos[num_words] = q;
            num_words++;
        }

    }

    if (num_words == 0) {
        return 0;
    }

    for (i = scan_from; (scan_from < scan_to && i < scan_to) ||
             (scan_from > scan_to && i > scan_to); i += scan_step) {

        Int4 local_ungapped_ext;
        Int4 shift;
        /* subject word */
        Uint4 index;

        Int4 q_offset;
        Int4 s_offset;
        Int4 num_identical;
        Int4 k;

        if (num_matches > kMaxNumMatches) {
            break;
        }

        s = subject->sequence + i / COMPRESSION_RATIO;

        w = (Int4)s[0] << 16 | s[1] << 8 | s[2];
        last_idx = 3;
        num_bytes = word_size / COMPRESSION_RATIO;
        ASSERT(num_bytes < 9);
        for (; last_idx < num_bytes; last_idx++) {
            w = w << 8 | s[last_idx];
        }

        if (i % COMPRESSION_RATIO != 0) {
            w = (w << 8) | s[last_idx];
            shift = 2 * (COMPRESSION_RATIO - (i % COMPRESSION_RATIO));
            index = (w >> shift) & mask;
        }
        else {
            index = w & mask;
        }


        for (k = 0;k < num_words; k++) {
            if (index == word[k]) {
                break;
            }
        }

        if (k >= num_words) {
            continue;
        }

        q_offset = query_pos[k];
        s_offset = i;

        for (k = word_size;k < big_word_size;k++) {
            if (query_seq[q_offset + k] != UNPACK_BASE(subject->sequence, s_offset + k)) {
                break;
            }
        }
        if (k < big_word_size) {
            continue;
        }


        num_matches++;

        num_extensions++;

        num_identical = 0;
        JumperGappedAlignmentCompressedWithTraceback(query_seq,
                                                     subject->sequence,
                                                     query_len,
                                                     subject->length,
                                                     q_offset,
                                                     s_offset,
                                                     gap_align,
                                                     score_params, 
                                                     &num_identical,
                                                     &local_ungapped_ext);


        if (gap_align->score <= best_score) {
            continue;
        }

        best_score = gap_align->score;

        if (hsp) {
            hsp = Blast_HSPFree(hsp);
        }

        hsp = s_CreateHSP(query_seq, query_len, context,
                          query_info, gap_align, subject,
                          score_params, hit_params);


        if (hsp->score >= query_len - query_from) {
            break;
        }


    }

    if (hsp) {
        Blast_HSPListSaveHSP(hsp_list, hsp);
        num++;
    }

    return num;
}


Int2 DoAnchoredSearch(BLAST_SequenceBlk* query,
                      BLAST_SequenceBlk* subject,
                      Int4 word_size,
                      BlastQueryInfo* query_info,
                      BlastGapAlignStruct* gap_align,
                      const BlastScoringParameters* score_params,
                      const BlastHitSavingParameters* hit_params, 
                      BlastHSPStream* hsp_stream)
{
    HSPChain* chains = NULL;
    HSPChain* ch = NULL;
    BlastHSPList* hsp_list = NULL;

    if (!query || !subject || !query_info || !gap_align || !score_params ||
        !hit_params || !hsp_stream) {

        return -1;
    }

    hsp_list = Blast_HSPListNew(MAX(query_info->num_queries, 100));
    if (!hsp_list) {
        return BLASTERR_MEMORY;
    }
    hsp_list->oid = subject->oid;

    /* Collect HSPs for HSP chains with that cover queries partially */
    MT_LOCK_Do(hsp_stream->x_lock, eMT_Lock);
    chains = FindPartialyCoveredQueries(hsp_stream->writer->data,
                                        hsp_list->oid, word_size);
    MT_LOCK_Do(hsp_stream->x_lock, eMT_Unlock);


    /* Search uncovered parts of the queries */ 
    for (ch = chains; ch; ch = ch->next) {
        HSPContainer* h = ch->hsps;
        Int4 context = h->hsp->context;
        Uint1* query_seq =
            query->sequence + query_info->contexts[context].query_offset;
        Int4 query_len = query_info->contexts[context].query_length;
        Int4 num = 0;


        if (h->hsp->query.offset >= 12) {

            num = DoAnchoredScan(query_seq, query_len,
                                 h->hsp->query.offset - 1,
                                 context, subject,
                                 h->hsp->subject.offset - 1,
                                 h->hsp->subject.offset - 1 - 
                                     hit_params->options->longest_intron,
                                 query_info, gap_align, score_params,
                                 hit_params, hsp_list);
        }

        while (h->next) {
            h = h->next;
        }


        if (query_len - h->hsp->query.end > 12) {

            num += DoAnchoredScan(query_seq, query_len, h->hsp->query.end,
                                  context, subject,
                                  h->hsp->subject.end,
                                  h->hsp->subject.end + 
                                      hit_params->options->longest_intron,
                                  query_info, gap_align, score_params,
                                  hit_params, hsp_list);
        }

        if (num) {
            for (h = ch->hsps; h; h = h->next) {
                Blast_HSPListSaveHSP(hsp_list, h->hsp);
                h->hsp = NULL;
            }
        }
    }

    BlastHSPStreamWrite(hsp_stream, &hsp_list);
    HSPChainFree(chains);
    Blast_HSPListFree(hsp_list);

    return 0;
}

#define NUM_DIMERS (1 << 4)

/* Compute enrtopy of 2-mers in a BLASTNA sequence */
static Int4 s_FindDimerEntropy(Uint1* sequence, Int4 length)
{
    Int4 counts[NUM_DIMERS];
    Int4 num = 0;
    Int4 i;
    double sum = 0.0;

    memset(counts, 0, NUM_DIMERS * sizeof(Int4));
    // count dimers
    for (i=0;i < length - 1;i++) {
        Uint1 base_1 = sequence[i];
        Uint1 base_2 = sequence[i + 1];

        if ((base_1 & 0xfc) == 0 && (base_2 & 0xfc) == 0) {
            Int4 dimer = (base_1 << 2) | base_2;
            counts[dimer]++;
            num++;
        }
    }

    // compute amount of information in the sequence
    for (i=0;i < NUM_DIMERS;i++) {
        if (counts[i]) {
            sum += (double)counts[i] * log((double)counts[i] / num);
        }
    }

    return -sum * (1.0 /(log(16.0))) + 0.5;
}


static Int2 s_MaskSequence(Int4 offset, Int4 length, BlastSeqLoc** seq_locs)
{
    BlastSeqLoc* loc = calloc(1, sizeof(BlastSeqLoc));
    if (!loc) {
        return BLASTERR_MEMORY;
    }
    loc->ssr = calloc(1, sizeof(SSeqRange));
    if (!loc->ssr) {
        free(loc);
        return BLASTERR_MEMORY;
    }
    loc->ssr->left = offset;
    loc->ssr->right = offset + length - 1;
    *seq_locs = loc;

    return 0;
}

Int2 FilterQueriesForMapping(Uint1* sequence, Int4 length, Int4 offset,
                             const SReadQualityOptions* options,
                             BlastSeqLoc** seq_loc)
{
    const double kMaxFractionOfAmbiguousBases = options->frac_ambig;
    Int4 i;
    Int4 num = 0;
    Int4 entropy;
    Int2 status = 0;

    for (i = 0;i < length;i++) {
        if (sequence[i] & 0xfc) {
            num++;
        }
    }

    if ((double)num / length > kMaxFractionOfAmbiguousBases) {
        status =  s_MaskSequence(offset, length, seq_loc);
        return status;
    }

    entropy = s_FindDimerEntropy(sequence, length);
    if (entropy <= options->entropy) {
        status = s_MaskSequence(offset, length, seq_loc);
    }

    return status;
}


