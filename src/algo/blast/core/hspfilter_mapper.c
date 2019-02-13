/*  $Id$
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
 */

/** @file hspfilter_mapper.c
 * Implementation of the BlastHSPWriter interface to save only the best scoring * chains of HSPs for aligning RNA-Seq sequences to a genome.
 */

#include <algo/blast/core/hspfilter_mapper.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/spliced_hits.h>
#include "jumper.h"

/* Pair configurations, in the order of prefernece */
#define PAIR_CONVERGENT 0
#define PAIR_DIVERGENT 1
#define PAIR_PARALLEL 2
#define PAIR_NONE 3

/* Insert a single chain into the list so that the list is sorted in decending
   order of chain scores. */
static Int4 s_HSPChainListInsertOne(HSPChain** list, HSPChain* chain,
                                    Boolean check_for_duplicates)
{
    HSPChain* ch = NULL;

    if (!list || !chain) {
        return -1;
    }
    ASSERT(!chain->next);

    if (!*list) {
        *list = chain;
        return 0;
    }

    ch = *list;
    if (ch->score < chain->score) {
        chain->next = ch;
        *list = chain;
        return 0;
    }

    /* check for duplicate chain: the new chains may be the same as already
       saved; this may come from writing HSPs to HSP stream for each chunk */
    if (check_for_duplicates &&
        ch->oid == chain->oid && ch->score == chain->score &&
        ch->hsps->hsp->query.frame == chain->hsps->hsp->query.frame &&
        ch->hsps->hsp->subject.offset ==
        chain->hsps->hsp->subject.offset) {

        chain->next = NULL;
        chain = HSPChainFree(chain);
        return 0;
    }

    while (ch->next && ch->next->score >= chain->score){

        /* check for duplicate chain: the new chains may be the same as already
           saved; this may come from writing HSPs to HSP stream for each chunk */
        if (check_for_duplicates &&
            ch->next->oid == chain->oid && ch->next->score == chain->score &&
            ch->next->hsps->hsp->query.frame == chain->hsps->hsp->query.frame &&
            ch->next->hsps->hsp->subject.offset ==
            chain->hsps->hsp->subject.offset) {

            chain->next = NULL;
            chain = HSPChainFree(chain);
            return 0;
        }

        ch = ch->next;
    }
    ASSERT(ch);
    chain->next = ch->next;
    ch->next = chain;

    return 0;
}


static Boolean s_TestCutoffs(HSPChain* chain, Int4 cutoff_score,
                             Int4 cutoff_edit_dist)
{
    if (!chain) {
        return FALSE;
    }

    if (chain->score >= cutoff_score) {

        Int4 align_len = 0;
        Int4 num_identical = 0;
        HSPContainer* h = chain->hsps;

        if (cutoff_edit_dist < 0) {
            return TRUE;
        }

        for (; h; h = h->next) {
            align_len += MAX(h->hsp->query.end - h->hsp->query.offset,
                             h->hsp->subject.end - h->hsp->subject.offset);

            num_identical += h->hsp->num_ident;

            ASSERT(h->hsp->num_ident <=
                   MAX(h->hsp->query.end - h->hsp->query.offset,
                       h->hsp->subject.end - h->hsp->subject.offset));
        }

        if (align_len - num_identical <= cutoff_edit_dist) {
            return TRUE;
        }
    }

    return FALSE;
}

/* Insert chains into the list so that the list is sorted in descending order
   of chain scores. If chain is a list, each element is added separately. The
   list must be sorted before adding chain */
static Int4 HSPChainListInsert(HSPChain** list, HSPChain** chain,
                               Int4 cutoff_score, Int4 cutoff_edit_dist,
                               Boolean check_for_duplicates)
{
    HSPChain* ch = NULL;
    Int4 status = 0;

    if (!list || !chain) {
        return -1;
    }

    ch = *chain;
    while (ch) {
        HSPChain* next = ch->next;
        ch->next = NULL;
        if (ch->score >= cutoff_score) {
            status = s_HSPChainListInsertOne(list, ch, check_for_duplicates);
        }
        else {
            HSPChainFree(ch);
        }
        if (status) {
            return status;
        }
        ch = next;

    }

    *chain = NULL;
    return 0;
}

/* Remove from the list chains with scores lower than the best one by at least
   the given margin. The list must be sorted in descending order of scores. */
static Int4 HSPChainListTrim(HSPChain* list, Int4 margin)
{
    HSPChain* ch = NULL;
    Int4 best_score;

    if (!list) {
        return -1;
    }

    best_score = list->score;
    ch = list;
    while (ch->next && best_score - ch->next->score <= margin) {
        ASSERT(best_score - ch->next->score >= 0);
        ch = ch->next;
    }
    ASSERT(ch);

    ch->next = HSPChainFree(ch->next);

    return 0;
}


/* Test that all pointers in the chain are set */
static Boolean s_TestChains(HSPChain* chain)
{
    HSPContainer* hc;

    ASSERT(chain);
    hc = chain->hsps;
    ASSERT(hc);
    ASSERT(hc->hsp);
    ASSERT(hc->hsp->context == chain->context);

    ASSERT(hc->hsp->gap_info->size > 1 ||
           hc->hsp->query.end - hc->hsp->query.offset ==
           hc->hsp->subject.end - hc->hsp->subject.offset);

    hc = hc->next;
    while (hc) {
        ASSERT(hc->hsp);
        ASSERT(hc->hsp->context == chain->context);

        ASSERT(hc->hsp->gap_info->size > 1 ||
               hc->hsp->query.end - hc->hsp->query.offset ==
               hc->hsp->subject.end - hc->hsp->subject.offset);

        if (hc->next) {
            ASSERT(hc->hsp->query.offset < hc->next->hsp->query.offset);
            ASSERT(hc->hsp->subject.offset < hc->next->hsp->subject.offset);
        }
        hc = hc->next;
    }

    chain = chain->next;
    while (chain) {
        hc = chain->hsps;
        ASSERT(hc);
        ASSERT(hc->hsp);
        ASSERT(hc->hsp->context == chain->context);

        ASSERT(hc->hsp->gap_info->size > 1 ||
               hc->hsp->query.end - hc->hsp->query.offset ==
               hc->hsp->subject.end - hc->hsp->subject.offset);

        hc = hc->next;
        while (hc) {
            ASSERT(hc);
            ASSERT(hc->hsp);
            ASSERT(hc->hsp->context == chain->context);

            ASSERT(hc->hsp->gap_info->size > 1 ||
                   hc->hsp->query.end - hc->hsp->query.offset ==
                   hc->hsp->subject.end - hc->hsp->subject.offset);

            if (hc->next) {
                ASSERT(hc->hsp->query.offset < hc->next->hsp->query.offset);
                ASSERT(hc->hsp->subject.offset < hc->next->hsp->subject.offset);
            }

            hc = hc->next;
        }
        chain = chain->next;
    }

    return TRUE;
}

#if _DEBUG
static Boolean s_TestChainsSorted(HSPChain* chain)
{
    HSPChain* prev;

    s_TestChains(chain);

    prev = chain;
    chain = chain->next;
    for (; chain; chain = chain->next, prev = prev->next) {
        ASSERT(prev->score >= chain->score);
    }

    return TRUE;
}
#endif

/** Data structure used by the writer */
typedef struct BlastHSPMapperData {
   BlastHSPMapperParams* params;         /**< how many hits to save */
   BLAST_SequenceBlk* query;              /**< query sequence */
   BlastQueryInfo* query_info;            /**< information about queries */
   HSPChain** saved_chains;               /**< HSP chains are stored here */
} BlastHSPMapperData;

/*************************************************************/
/** The following are implementations for BlastHSPWriter ADT */

/** Perform pre-run stage-specific initialization
 * @param data The internal data structure [in][out]
 * @param results The HSP results to operate on  [in]
 */
static int
s_BlastHSPMapperPairedInit(void* data, void* results)
{
   BlastHSPMapperData * spl_data = data;
   BlastHSPResults* r = (BlastHSPResults*)results;
   spl_data->saved_chains = calloc(r->num_queries, sizeof(HSPChain*));

   return 0;
}


/* Get subject start position */
static Int4 s_FindFragmentStart(HSPChain* chain)
{
    Int2 frame;
    HSPContainer* hc = NULL;
    ASSERT(chain);
    ASSERT(chain->hsps);
    ASSERT(chain->hsps->hsp);

    frame = chain->hsps->hsp->query.frame;
    if (frame > 0) {
        return chain->hsps->hsp->subject.offset;
    }

    hc = chain->hsps;
    while (hc->next) {
        hc = hc->next;
    }
    ASSERT(hc);

    return hc->hsp->subject.end - 1;
}


/* Get subject end position */
/* Not used
static Int4 s_FindFragmentEnd(HSPChain* chain)
{
    Int2 frame;
    HSPContainer* hc = NULL;
    ASSERT(chain);
    ASSERT(chain->hsps);
    ASSERT(chain->hsps->hsp);

    frame = chain->hsps->hsp->query.frame;
    if (frame < 0) {
        return chain->hsps->hsp->subject.offset;
    }

    hc = chain->hsps;
    while (hc->next) {
        hc = hc->next;
    }
    ASSERT(hc);

    return hc->hsp->subject.end - 1;
}
*/

static Int4 s_ComputeGapScore(Int4 length, Int4 open_score, Int4 extend_score, 
                              Int4 seq_error)
{
    if (length < 4) {
        return length * seq_error;
    }

    return open_score + MIN(length, 4) * extend_score;
}

/* Compute HSP alignment score from Jumper edit script */
static Int4 s_ComputeAlignmentScore(BlastHSP* hsp, Int4 mismatch_score,
                                    Int4 gap_open_score, Int4 gap_extend_score)
{
    Int4 i;
    Int4 last_pos = hsp->query.offset;
    Int4 score = 0;
    const Int4 kGap = 15;
    Int4 num_identical = 0;
    Int4 query_gap = 0;
    Int4 subject_gap = 0;

    for (i = 0;i < hsp->map_info->edits->num_edits;i++) {
        JumperEdit* e = &(hsp->map_info->edits->edits[i]);
        Int4 num_matches = e->query_pos - last_pos;
        ASSERT(num_matches >= 0);
        last_pos = e->query_pos;
        score += num_matches;
        num_identical += num_matches;

        if (e->query_base == kGap) {
            ASSERT(e->subject_base != kGap);
            query_gap++;

            if (subject_gap > 0) {
                score += s_ComputeGapScore(subject_gap, -12, -1, -4);
                subject_gap = 0;
            }
        }
        else if (e->subject_base == kGap) {
            subject_gap++;
            last_pos++;

            if (query_gap > 0) {
                score += s_ComputeGapScore(query_gap, -12, -1, -4);
                query_gap = 0;
            }
        }
        else {
            score += mismatch_score;
            last_pos++;

            if (subject_gap > 0) {
                score += s_ComputeGapScore(subject_gap, -12, -1, -4);
                subject_gap = 0;
            }
            if (query_gap > 0) {
                score += s_ComputeGapScore(query_gap, -12, -1, -4);
                query_gap = 0;
            }
        }
    }

    if (subject_gap > 0) {
        score += s_ComputeGapScore(subject_gap, -12, -1, -4);
        subject_gap = 0;
    }
    if (query_gap > 0) {
        score += s_ComputeGapScore(query_gap, -12, -1, -4);
        query_gap = 0;
    }

    score += hsp->query.end - last_pos;
    num_identical += hsp->query.end - last_pos;
    hsp->num_ident = num_identical;
    return score;
}


/* Find the cost of chain HSPs overlapping on the query, as the smaller score
   of the overlapping region */
static Int4 s_GetOverlapCost(const BlastHSP* a, const BlastHSP* b,
                             Int4 edit_penalty)
{
    Int4 i;
    Int4 overlap_f, overlap_s;
    const BlastHSP* f;
    const BlastHSP* s;

    /* if one HSP in contained within the other on the query, return the
       smaller score */
    if ((a->query.offset <= b->query.offset && a->query.end >= b->query.end) ||
        (a->query.offset >= b->query.offset && a->query.end <= b->query.end)) {

        return MIN(a->score, b->score);
    }

    /* if the two HSPs are mutually exclusive on the query, there is no cost */
    if ((a->query.end < b->query.offset && a->query.offset < b->query.end) ||
        (b->query.end < a->query.offset && b->query.offset < a->query.end)) {

        return 0;
    }

    /* otherwise the HSPs partially overlap; reurn the the smaller score for
       the overlap region */

    /* find which HSP precedes which on the query */
    if (a->query.offset <= b->query.offset) {
        f = a;
        s = b;
    }
    else {
        f = b;
        s = a;
    }

    /* this is the overlap score assuming perfect matches in the overlap */
    overlap_f = overlap_s = f->query.end - s->query.offset;
    ASSERT(overlap_f >= 0 && overlap_s >= 0);
    /* subtract penalties for mismatches and gaps in the overlap region */
    for (i = 0;i < f->map_info->edits->num_edits;i++) {
        if (f->map_info->edits->edits[i].query_pos >= s->query.offset) {
            overlap_f -= edit_penalty;
        }
    }
    for (i = 0;i < s->map_info->edits->num_edits;i++) {
        if (s->map_info->edits->edits[i].query_pos < f->query.end) {
            overlap_s -= edit_penalty;
        }
    }

    return MIN(overlap_f, overlap_s);
}


/* Compute chain score */
static Int4 s_ComputeChainScore(HSPChain* chain,
                                const ScoringOptions* score_options,
                                Int4 query_len,
                                Boolean comp_hsp_score)
{
    Int4 retval;
    HSPContainer* h = NULL;
    HSPContainer* prev = NULL;

    if (!chain || !score_options) {
        return -1;
    }

    h = chain->hsps;
    if (comp_hsp_score) {
        h->hsp->score = s_ComputeAlignmentScore(h->hsp, score_options->penalty,
                                                score_options->gap_open,
                                                score_options->gap_extend);
    }
    retval = h->hsp->score;

    prev = h;
    h = h->next;

    for (; h; h = h->next, prev = prev->next) {
        /* HSPs must be sorted by query positon */
        ASSERT(h->hsp->query.offset >= prev->hsp->query.offset);

        if (comp_hsp_score) {

            h->hsp->score = s_ComputeAlignmentScore(h->hsp,
                                                    score_options->penalty,
                                                    score_options->gap_open,
                                                    score_options->gap_extend);
        }
        retval += h->hsp->score;

        if ((h->hsp->map_info->left_edge & MAPPER_SPLICE_SIGNAL) == 0 ||
            (prev->hsp->map_info->right_edge & MAPPER_SPLICE_SIGNAL) == 0) {

            Int4 query_gap =
                MAX(h->hsp->query.offset - prev->hsp->query.end, 0);

            Int4 subject_gap =
                MAX(h->hsp->subject.offset - prev->hsp->subject.end, 0);

            retval += s_ComputeGapScore(query_gap, -12, -1, -4);
            retval += s_ComputeGapScore(subject_gap, -12, -1, -4);
        }

    }

    return retval;
}

#if _DEBUG
static Boolean s_TestHSPRanges(const BlastHSP* hsp)
{
    Int4 i;
    Int4 d = 0;
    Int4 q = 0, s = 0;
    Int4 num_matches;
    Int4 last_pos;
    const Int4 kGap = 15;
    for (i=0;i < hsp->gap_info->size;i++) {
        switch (hsp->gap_info->op_type[i]) {
        case eGapAlignIns:
            d -= hsp->gap_info->num[i];
            q += hsp->gap_info->num[i];
            break;

        case eGapAlignDel:
            d += hsp->gap_info->num[i];
            s += hsp->gap_info->num[i];
            break;

        case eGapAlignSub:
            q += hsp->gap_info->num[i];
            s += hsp->gap_info->num[i];

        default:
            break;
        }
    }
    if (hsp->query.end - hsp->query.offset + d !=
        hsp->subject.end - hsp->subject.offset) {

        return FALSE;
    }

    ASSERT(hsp->query.end - hsp->query.offset == q);
    ASSERT(hsp->subject.end - hsp->subject.offset == s);

    d = 0;
    q = 0;
    s = 0;
    last_pos = hsp->query.offset;
    for (i=0;i < hsp->map_info->edits->num_edits;i++) {

        num_matches = hsp->map_info->edits->edits[i].query_pos - last_pos - 1;
        if (i == 0 ||
            (i > 0 && hsp->map_info->edits->edits[i - 1].query_base == kGap)) {
            num_matches++;
        }
        q += num_matches;
        s += num_matches;

        ASSERT(hsp->query.offset + q ==
               hsp->map_info->edits->edits[i].query_pos);

        if (hsp->map_info->edits->edits[i].query_base == kGap) {
            d++;
            s++;
        }
        else if (hsp->map_info->edits->edits[i].subject_base == kGap) {
            d--;
            q++;
        }
        else {
            q++;
            s++;
        }

        last_pos = hsp->map_info->edits->edits[i].query_pos;
    }
    num_matches = hsp->query.end - last_pos - 1;
    if (hsp->map_info->edits->num_edits == 0 ||
        (hsp->map_info->edits->num_edits > 0 &&
         hsp->map_info->edits->edits[hsp->map_info->edits->num_edits - 1].query_base == kGap)) {

        num_matches++;
    }
    q += num_matches;
    s += num_matches;

    ASSERT(hsp->query.end - hsp->query.offset + d ==
           hsp->subject.end - hsp->subject.offset);

    ASSERT(hsp->query.end - hsp->query.offset == q);
    ASSERT(hsp->subject.end - hsp->subject.offset == s);

    return TRUE;
}
#endif

/* Trim HSP by a number of bases on query or subject, either from the start or
   from the end */
static Int4 s_TrimHSP(BlastHSP* hsp, Int4 num, Boolean is_query,
                      Boolean is_start, Int4 mismatch_score,
                      Int4 gap_open_score, Int4 gap_extend_score)
{
    Int4 num_left = num;
    Int4 i = is_start ? 0 : hsp->gap_info->size - 1;
    Int4 d = is_start ? 1 : -1;
    Int4 end = is_start ? hsp->gap_info->size : -1;
    Int4 delta_query = 0, delta_subject = 0;
    Boolean is_subject = !is_query;
    Boolean is_end = !is_start;
    Int4 k;

    ASSERT(hsp /* */ && num > 0 /* */);
    ASSERT((is_query && num <= hsp->query.end - hsp->query.offset) ||
           (!is_query && num <= hsp->subject.end - hsp->subject.offset));

#if _DEBUG
    ASSERT(s_TestHSPRanges(hsp));
#endif

    if (num == 0) {
        return 0;
    }


    /* itereate over the edit script and remove the required number
       of subject bases */
    while (i != end && num_left > 0) {
        switch (hsp->gap_info->op_type[i]) {
        case eGapAlignSub:
            if (hsp->gap_info->num[i] > num_left) {
                hsp->gap_info->num[i] -= num_left;
                delta_query += num_left;
                delta_subject += num_left;
                num_left = 0;
            }
            else {
                Int4 n = hsp->gap_info->num[i];

                delta_query += n;
                delta_subject += n;
                num_left -= n;
                i += d;
            }
            break;

        case eGapAlignDel:
            if (is_subject) {
                if (hsp->gap_info->num[i] > num_left) {
                    hsp->gap_info->num[i] -= num_left;
                    delta_subject += num_left;
                    num_left = 0;
                }
                else {
                    delta_subject += hsp->gap_info->num[i];
                    num_left -= hsp->gap_info->num[i];
                    i += d;
                }
            }
            else {
                delta_subject += hsp->gap_info->num[i];
                i += d;
            }
            break;

        case eGapAlignIns:
            if (is_query) {
                if (hsp->gap_info->num[i] > num_left) {
                    hsp->gap_info->num[i] -= num_left;
                    delta_query += num_left;
                    num_left = 0;
                }
                else {
                    delta_query += hsp->gap_info->num[i];
                    num_left -= hsp->gap_info->num[i];
                    i += d;
                }
            }
            else {
                delta_query += hsp->gap_info->num[i];
                i += d;
            }
            break;

        default:
            ASSERT(0);
            break;
        }
    }

    /* shift edit script elements to fill the removed ones */
    if (is_start && i > 0) {
        for (k = 0;k < hsp->gap_info->size - i;k++) {
            hsp->gap_info->op_type[k] = hsp->gap_info->op_type[k + i];
            hsp->gap_info->num[k] = hsp->gap_info->num[k + i];
        }
        hsp->gap_info->size -= i;
    }

    if (is_end) {
        hsp->gap_info->size = i + 1;
    }

    /* update the Jumper edit script */
    if (is_start) {
        Int4 k = hsp->map_info->edits->num_edits - 1;
        Int4 p = hsp->query.end - 1;
        const Uint1 kGap = 15;
        for (i = hsp->gap_info->size - 1;i >= 0;i--) {
            if (hsp->gap_info->op_type[i] != eGapAlignDel) {

                p -= hsp->gap_info->num[i];
                while (k >= 0 &&
                       hsp->map_info->edits->edits[k].query_pos > p &&
                       hsp->map_info->edits->edits[k].query_base != kGap) {

                    k--;
                }
            }
            else {
                Int4 j;
                for (j = 0;j < hsp->gap_info->num[i];j++) {
                    ASSERT(k >= 0);
                    ASSERT(hsp->map_info->edits->edits[k].query_base == kGap);
                    k--;
                }
            }
        }
        k++;
        if (k > 0) {
            for (i = 0;i < hsp->map_info->edits->num_edits - k;i++) {
                hsp->map_info->edits->edits[i] =
                    hsp->map_info->edits->edits[i + k];
            }
            hsp->map_info->edits->num_edits -= k;
            ASSERT(hsp->map_info->edits->num_edits >= 0);
        }
    }
    else {
        Int4 k = 0;
        Int4 p = hsp->query.offset;
        const Uint1 kGap = 15;

        for (i = 0;i < hsp->gap_info->size;i++) {
            if (hsp->gap_info->op_type[i] != eGapAlignDel) {

                p += hsp->gap_info->num[i];
                while (k < hsp->map_info->edits->num_edits &&
                       hsp->map_info->edits->edits[k].query_pos < p &&
                       hsp->map_info->edits->edits[k].query_base != kGap) {

                    k++;
                }
            }
            else {
                Int4 j;
                for (j = 0;j < hsp->gap_info->num[i];j++) {
                    ASSERT(hsp->map_info->edits->edits[k].query_base == kGap);
                    k++;
                }
            }
        }
        hsp->map_info->edits->num_edits = k;
    }

    /* update HSP start positions */
    if (is_start) {
        hsp->query.offset += delta_query;
        hsp->subject.offset += delta_subject;
    }
    else {
        hsp->query.end -= delta_query;
        hsp->subject.end -= delta_subject;
    }

    /* update HSP score */
    hsp->score = s_ComputeAlignmentScore(hsp, mismatch_score, gap_open_score,
                                         gap_extend_score);

#if _DEBUG
    ASSERT(s_TestHSPRanges(hsp));
#endif

    return 0;
}


#define NUM_ADAPTERS 4
#define MAX_ADAPTER_LEN 20

/* Find adapeter on the 5' end in a single sequence. The search will start
   towards the end of the last HSP (from, to) */
static Int4 s_FindAdapterInSequence(Int4 hsp_from, Int4 hsp_to, Uint1* query,
                                    Int4 query_len)
{
    Uint1 adapters_tab[NUM_ADAPTERS][MAX_ADAPTER_LEN] = {
        /* Contaminants from FastQC config file:
           http://www.bioinformatics.babraham.ac.uk/projects/fastqc/ */
        /* Illumina universal adapter: AGATCGGAAGAG */
        {0, 2, 0, 3, 1, 2, 2, 0, 0, 2, 0, 2},
        /* Illumina small RNA adapter: ATGGAATTCTCG */
        {0, 3, 2, 2, 0, 0, 3, 3, 1, 3, 1, 2},
        /* Nextera transosase sequence: CTGTCTCTTATA */
        {1, 3, 2, 3, 1, 3, 1, 3, 3, 0, 3, 0},
        /* Illumina multiplexing adapter 1: GATCGGAAGAGCACACGTCT */
        {2, 0, 3, 1, 2, 2, 0, 0, 2, 0, 2, 1, 0, 1, 0, 1, 2, 3, 1, 3}};

    int lengths[NUM_ADAPTERS] = {12, 12, 12, 20};
    Boolean found = FALSE;
    Int4 adapter_start = -1;
    int adptr_idx;
    Int4 from = hsp_from, to = hsp_to;
    const Int4 kMaxErrors = 1; /* max number of mismaches allowed for matching
                                  the adapter sequence + 1*/


    if (!query) {
        return -1;
    }

    for (adptr_idx = 0;!found && adptr_idx < NUM_ADAPTERS;adptr_idx++) {
        Uint1* adapter = adapters_tab[adptr_idx];
        Uint4 word = *(Uint4*)adapter;
        Int4 q = MAX(to - lengths[adptr_idx], from);
        int i = 0;
        while (q < query_len - 4 && q + i < query_len && i < lengths[adptr_idx]) {
            while (q < query_len - 4 && *(Uint4*)(query + q) != word) {
                q++;
            }
            if (q < query_len - 4) {
                Int4 errors = kMaxErrors + 1;
                i = 0;
                while (q + i < query_len && i < lengths[adptr_idx] &&
                       errors) {

                    if (query[q + i] != adapter[i]) {
                        errors--;
                    }
                    i++;
                }
                if (q + i == query_len || i == lengths[adptr_idx]) {
                    adapter_start = q;
                    found = TRUE;
                    break;
                }

                q++;
            }
        }
    }

    ASSERT(adapter_start <= query_len);
    return adapter_start;
}


/* Set adapter position in the chain and trim alignments that span beyond
   adapter start position */
static Int2 s_SetAdapter(HSPChain** chains_ptr, Int4 adapter_pos,
                         Int4 query_len, const ScoringOptions* scores)
{
    HSPChain* head = NULL;
    HSPChain* chain = NULL;
    const Int4 kMinAdapterLen = 3;

    if (!chains_ptr || !*chains_ptr || adapter_pos < 0) {
        return -1;
    }

    head = *chains_ptr;
    chain = *chains_ptr;
    /* iterate over chains */
    while (chain) {
        BlastHSP* hsp = chain->hsps->hsp;

        /* check the 3' alignment extent on the query and skip this chain
           if the query is covered almost to the end */
        if (hsp->query.frame > 0) {
            /* for positive strand, check the last query position */
            HSPContainer* h = chain->hsps;
            while (h->next) {
                h = h->next;
            }
            ASSERT(h);
            if (query_len - h->hsp->query.end < kMinAdapterLen) {
                chain = chain->next;
                continue;
            }
        }
        else {
            /* for negative strand, check the first query position */
            ASSERT(hsp);
            if (hsp->query.offset < kMinAdapterLen) {
                chain = chain->next;
                continue;
            }
        }

        /* set adapter start position in the chain */
        chain->adapter = adapter_pos;

        /* Trim alignment that covers part of the adapter */
        /* for query on the positive strand */
        if (hsp->query.frame > 0) {
            HSPContainer* h = chain->hsps;

            /* if all HSPs are behind the adapter, delete this chain;
               we require that HSP without adapter is at least 5 bases long */
            if (h->hsp->query.offset >= adapter_pos - 5) {
                HSPChain* prev = head;
                HSPChain* next = chain->next;
                while (prev && prev->next != chain) {
                    prev =prev->next;
                }

                chain->next = NULL;
                HSPChainFree(chain);
                chain = next;
                if (prev) {
                    prev->next = next;
                }
                else {
                    head = chain;
                }

                continue;
            }

            /* find the first HSP with end position after adapter */
            while (h && h->hsp->query.end <= adapter_pos) {
                h = h->next;
            }

            /* if one is found */
            if (h) {
                HSPContainer* hh = NULL;
                hsp = h->hsp;

                /* if adapter is with in the HSP trim the HSP */
                if (hsp->query.offset < adapter_pos ) {
                    Int4 old_score = hsp->score;
                    ASSERT(hsp->query.end - adapter_pos > 0);
                    s_TrimHSP(hsp, hsp->query.end - adapter_pos, TRUE,
                              FALSE, scores->penalty, scores->gap_open,
                              scores->gap_extend);
                    chain->score += hsp->score - old_score;

                    /* remove HSPs after h as they are past the adapter */
                    if (h->next) {
                        h->next = HSPContainerFree(h->next);
                    }
                }
                else {
                    /* otherwise delete h and everything after it */
                    hh = chain->hsps;
                    while (hh && hh->next != h) {
                        hh = hh->next;
                    }
                    ASSERT(hh);
                    hh->next = HSPContainerFree(hh->next);
                }

                /* compute the new chain score */
                chain->score = s_ComputeChainScore(chain, scores, query_len,
                                                   FALSE);
            }

            /* mark adapter in the HSPs */
            h = chain->hsps;
            while (h->next) {
                h = h->next;
            }
            h->hsp->map_info->right_edge |= MAPPER_ADAPTER;
            h->hsp->map_info->right_edge |= MAPPER_EXON;
        }
        else {
            /* negative strand: adapter is in the beginning of the chain */
            Int4 pos_minus = query_len - adapter_pos - 1;
            HSPContainer* h = chain->hsps;

            /* find the first HSP with end position after adapter;
               we require that HSP without adapter is at least 5 bases long */
            while (h && h->hsp->query.end <= pos_minus + 5) {
                h = h->next;
            }

            /* if all HSPs are before the adapter, delete this chain */
            if (!h) {
                /* remove chain */
                HSPChain* prev = head;
                HSPChain* next = chain->next;
                while (prev && prev->next != chain) {
                    prev =prev->next;
                }

                chain->next = NULL;
                HSPChainFree(chain);
                chain = next;
                if (prev) {
                    prev->next = next;
                }
                else {
                    head = chain;
                }

                continue;
            }

            /* if h is not the first HSP in the chain, remove all HSPs before
               h */
            if (h != chain->hsps) {
                HSPContainer* hh = chain->hsps;
                while (hh && hh->next != h) {
                    hh = hh->next;
                }
                ASSERT(hh);
                hh->next = NULL;
                HSPContainerFree(chain->hsps);
                chain->hsps = h;

                /* compute new chain score */
                chain->score = s_ComputeChainScore(chain, scores, query_len,
                                                   FALSE);
            }
            ASSERT(h);

            /* set adapter information */
            hsp = chain->hsps->hsp;
            hsp->map_info->left_edge |= MAPPER_ADAPTER;
            hsp->map_info->left_edge |= MAPPER_EXON;

            /* trim HSP if necessary */
            if (pos_minus >= hsp->query.offset) {
                Int4 old_score = hsp->score;
                ASSERT(pos_minus - hsp->query.offset + 1 > 0);
                s_TrimHSP(hsp, pos_minus - hsp->query.offset + 1,
                          TRUE, TRUE, scores->penalty, scores->gap_open,
                          scores->gap_extend);
                chain->score += hsp->score - old_score;
            }
        }

        chain = chain->next;
    }

    ASSERT(!head || s_TestChains(head));
    *chains_ptr = head;

    return 0;
}


static Int4 s_FindAdapters(HSPChain** saved,
                           const BLAST_SequenceBlk* query_blk,
                           const BlastQueryInfo* query_info,
                           const ScoringOptions* score_opts)
{
    Int4 query_idx;

    for (query_idx = 0;query_idx < query_info->num_queries;query_idx++) {
        HSPChain* chain = NULL;
        HSPChain* ch = NULL;
        Uint1* query = NULL;
        Int4 query_len;
        Int4 from = -1, to = -1;
        BlastHSP* hsp = NULL;
        Int4 overhang = 0;
        Int4 adapter_pos = -1;

        if (!saved[query_idx]) {
            continue;
        }

        /* we search for adapters and  only in the plus strand of the query */
        query = query_blk->sequence +
            query_info->contexts[query_idx * NUM_STRANDS].query_offset;
        ASSERT(query);
        query_len = query_info->contexts[query_idx * NUM_STRANDS].query_length;

        /* find the best scoring chain */
        chain = saved[query_idx];
        ch = chain->next;
        for (; ch; ch = ch->next) {
            if (ch->score > chain->score) {
                chain = ch;
            }
        }
        ASSERT(chain);

        /* find query coverage by the whole chain */
        if (chain->hsps->hsp->query.frame > 0) {
            HSPContainer* h = chain->hsps;
            from = h->hsp->query.offset;
            while (h->next) {
                h = h->next;
            }
            to = h->hsp->query.end;
        }
        else {
            HSPContainer* h = chain->hsps;
            to = query_len - h->hsp->query.offset;
            while (h->next) {
                h = h->next;
            }
            from = query_len - h->hsp->query.end;
        }
        ASSERT(from >= 0 && to >= 0);

        /* do not search for adapters if the query is mostly covered by the
           best scoring chain */
        if (from < 20 && to > query_len - 3) {
            continue;
        }

        /* find the chain with the longest overhang */
        chain = saved[query_idx];
        ch = chain->next;
        for (; ch; ch = ch->next) {
            HSPContainer* h = ch->hsps;
            if (h->hsp->query.frame > 0) {
                while (h->next) {
                    h = h->next;
                }
                if (query_len - h->hsp->query.end > overhang) {
                    overhang = query_len - h->hsp->query.end;
                    chain = ch;
                }
            }
            else {
                if (h->hsp->query.offset + 1 > overhang) {
                    overhang = h->hsp->query.offset + 1;
                    chain = ch;
                }
            }
        }
        ASSERT(chain);

        /* find location from where to search for the adapter */
        hsp = chain->hsps->hsp;
        if (hsp->query.frame > 0) {
            HSPContainer* h = chain->hsps;
            while (h->next) {
                h = h->next;
            }
            hsp = h->hsp;
        }
        ASSERT(hsp);

        if (hsp->query.frame > 0) {
            from = hsp->query.offset;
            to  = hsp->query.end;
        }
        else {
            from = query_len - hsp->query.end;
            to = query_len - hsp->query.offset;
        }

        /* do not search for adapters if the read aligns almost to the end */
        if (to >= query_len - 3) {
            continue;
        }

        /* search for adapters */
        adapter_pos = s_FindAdapterInSequence(from, to, query, query_len);

        /* set adapter information in all chains and trim HSPs that extend
           into the adapter */
        if (adapter_pos >= 0) {
            s_SetAdapter(&(saved[query_idx]), adapter_pos, query_len,
                         score_opts);
        }
    }

    return 0;
}


/* Search for polyA tail at the end of a sequence and return the start position */
static Int4 s_FindPolyAInSequence(Uint1* sequence, Int4 length)
{
    Int4 i;
    const Uint1 kBaseA = 0;
    Int4 num_a = 0;
    Int4 err = 0;
    const Int4 kMaxErrors = 3;

    if (!sequence) {
        return -1;
    }

    /* iterate over positions untile kMaxErrors non A bases are found */
    i = length - 1;
    while (i >= 0 && err < kMaxErrors) {
        if (sequence[i] != kBaseA) {
            err++;
        }

        i--;
    }
    i++;

    /* find the beginnig of the string of As */
    while (i < length - 1 &&
           (sequence[i] != kBaseA || sequence[i + 1] != kBaseA)) {

        if (sequence[i] != kBaseA) {
            err--;
        }
        i++;
    }

    num_a = length - i - err;

    /* short tails must have no errors */
    if (num_a < 3 || (num_a < 5 && err > 0)) {
        return -1;
    }

    return i;
}


/* Set PolyA information in HSP chains */
static Int4 s_SetPolyATail(HSPChain* chains, Int4 positive_start,
                           Int4 negative_start, Int4 query_len)
{
    HSPChain* ch = NULL;

    if (!chains) {
        return -1;
    }

    for (ch = chains; ch; ch = ch->next) {
        HSPContainer* h = ch->hsps;
        while (h->next) {
            h = h->next;
        }

        if (query_len - h->hsp->query.end < 5) {
            continue;
        }

        if ((h->hsp->query.frame < 0 && negative_start >= 0) ||
            (h->hsp->query.frame > 0 && positive_start >= 0)) {

            h->hsp->map_info->right_edge |= MAPPER_POLY_A;
            h->hsp->map_info->right_edge |= MAPPER_EXON;

            if (h->hsp->query.frame > 0) {
                ch->polyA = MAX(positive_start, h->hsp->query.end);
            }
            else {
                ch->polyA = MAX(negative_start, h->hsp->query.end);
            }
        }
    }

    return 0;
}


static Int4 s_FindPolyATails(HSPChain** saved,
                             const BLAST_SequenceBlk* query_blk,
                             const BlastQueryInfo* query_info)
{
    Int4 query_idx;

    /* for each query */
    for (query_idx = 0;query_idx < query_info->num_queries;query_idx++) {
        HSPChain* chain = NULL;
        HSPChain* ch = NULL;
        Uint1* query = NULL;
        Int4 query_len;
        Int4 from = -1, to = -1;
        Int4 positive_start, negative_start;

        /* skip queries with no results and those with adapters */
        if (!saved[query_idx] || saved[query_idx]->adapter >= 0) {
            continue;
        }

        query_len = query_info->contexts[query_idx * NUM_STRANDS].query_length;

        /* find the best scoring chain */
        chain = saved[query_idx];
        ch = chain->next;
        for (; ch; ch = ch->next) {
            if (ch->score > chain->score) {
                chain = ch;
            }
        }
        ASSERT(chain);

        /* find query coverage by the whole chain */
        if (chain->hsps->hsp->query.frame > 0) {
            HSPContainer* h = chain->hsps;
            from = h->hsp->query.offset;
            while (h->next) {
                h = h->next;
            }
            to = h->hsp->query.end;
        }
        else {
            HSPContainer* h = chain->hsps;
            to = query_len - h->hsp->query.offset;
            while (h->next) {
                h = h->next;
            }
            from = query_len - h->hsp->query.end;
        }
        ASSERT(from >= 0 && to >= 0);

        /* do not search for polyA tails if the query is mostly covered by the
           best scoring chain */
        if (from < 4 && to > query_len - 3) {
            continue;
        }

        /* search for polyA tail on the positive strand */
        query = query_blk->sequence +
            query_info->contexts[query_idx * NUM_STRANDS].query_offset;
        positive_start = s_FindPolyAInSequence(query, query_len);

        /* search for the polyA tail on the negative strand */
        query = query_blk->sequence +
            query_info->contexts[query_idx * NUM_STRANDS + 1].query_offset;
        negative_start = s_FindPolyAInSequence(query, query_len);

        /* set polyA start positions and HSP flags in the saved chains */
        if (positive_start >= 0 || negative_start >= 0) {
            s_SetPolyATail(saved[query_idx], positive_start, negative_start,
                           query_len);
        }
    }

    return 0;
}


/* Merge two HSPs into one. The two HSPs may only be separated by at most
   one gap or mismatch. Function returns NULL is the HSPs cannon be merged */
static BlastHSP* s_MergeHSPs(const BlastHSP* first, const BlastHSP* second,
                             const Uint1* query,
                             const ScoringOptions* score_opts)
{
    BlastHSP* merged_hsp = NULL;  /* this will be the result */
    const BlastHSP* hsp = second;
    Int4 query_gap;
    Int4 subject_gap;
    Int4 mismatches = 0;
    Int4 gap_info_size;
    Int4 edits_size;
    Int4 k;
    const Uint1 kGap = 15;

    if (!first || !second || !query || !score_opts) {
        return NULL;
    }

    merged_hsp = Blast_HSPClone(first);
    if (!merged_hsp) {
        return NULL;
    }

    query_gap = second->subject.offset - first->subject.end;
    subject_gap = second->query.offset - first->query.end;

    if (query_gap < 0 || subject_gap < 0) {

        return NULL;
    }

    if (MAX(query_gap, subject_gap) < 4) {
        mismatches = MIN(query_gap, subject_gap);
        query_gap -= mismatches;
        subject_gap -= mismatches;
    }

    gap_info_size = first->gap_info->size + second->gap_info->size + 3;

    edits_size = first->map_info->edits->num_edits +
        second->map_info->edits->num_edits + 
        mismatches + query_gap + subject_gap;

    /* FIXME: should be done through an API */
    /* reallocate memory for edit scripts */
    merged_hsp->gap_info->op_type = realloc(merged_hsp->gap_info->op_type,
                                            gap_info_size *
                                            sizeof(EGapAlignOpType));

    merged_hsp->gap_info->num = realloc(merged_hsp->gap_info->num,
                                        gap_info_size *
                                        sizeof(Int4));

    merged_hsp->map_info->edits->edits = realloc(
                                       merged_hsp->map_info->edits->edits,
                                       edits_size * sizeof(JumperEdit));

    if (!merged_hsp->gap_info->op_type ||
        !merged_hsp->gap_info->num ||
        !merged_hsp->map_info->edits->edits) {

        Blast_HSPFree(merged_hsp);
        return NULL;
    }

    /* add mismatches to gap_info */
    if (mismatches > 0) {
        if (merged_hsp->gap_info->op_type[merged_hsp->gap_info->size - 1]
            == eGapAlignSub) {
            merged_hsp->gap_info->num[merged_hsp->gap_info->size - 1] +=
                mismatches;
        }
        else {
            merged_hsp->gap_info->op_type[merged_hsp->gap_info->size] =
                eGapAlignSub;
            merged_hsp->gap_info->num[merged_hsp->gap_info->size] =
                mismatches;
            merged_hsp->gap_info->size++;
        }
        ASSERT(merged_hsp->gap_info->size <= gap_info_size);
    }


    /* add query gap to gap info */
    if (query_gap > 0) {
        if (merged_hsp->gap_info->op_type[merged_hsp->gap_info->size - 1]
            == eGapAlignDel) {

            merged_hsp->gap_info->num[merged_hsp->gap_info->size - 1] +=
                query_gap;
        }
        else {
            merged_hsp->gap_info->op_type[merged_hsp->gap_info->size] =
                eGapAlignDel;
            merged_hsp->gap_info->num[merged_hsp->gap_info->size] =
                query_gap;
            merged_hsp->gap_info->size++;
        }
        ASSERT(merged_hsp->gap_info->size <= gap_info_size);
    }

    /* add subject gap to gap info */
    if (subject_gap > 0) {
        if (merged_hsp->gap_info->op_type[merged_hsp->gap_info->size - 1]
            == eGapAlignIns) {

            merged_hsp->gap_info->num[merged_hsp->gap_info->size - 1] +=
                subject_gap;
        }
        else {
            merged_hsp->gap_info->op_type[merged_hsp->gap_info->size] =
                eGapAlignIns;
            merged_hsp->gap_info->num[merged_hsp->gap_info->size] =
                subject_gap;
            merged_hsp->gap_info->size++;
        }
        ASSERT(merged_hsp->gap_info->size <= gap_info_size);
    }

    /* merge gap_info */
    for (k = 0;k < hsp->gap_info->size;k++) {

        if (merged_hsp->gap_info->op_type[merged_hsp->gap_info->size - 1]
            == hsp->gap_info->op_type[k]) {

            merged_hsp->gap_info->num[merged_hsp->gap_info->size - 1] +=
                hsp->gap_info->num[k];
        }
        else {
            merged_hsp->gap_info->op_type[merged_hsp->gap_info->size]
                = hsp->gap_info->op_type[k];

            merged_hsp->gap_info->num[merged_hsp->gap_info->size++]
                = hsp->gap_info->num[k];
        }
        ASSERT(merged_hsp->gap_info->size <= gap_info_size);
    }

    /* add mismatches to jumper edits */
    if (mismatches > 0) {
        for (k = 0;k < mismatches;k++) {
            JumperEdit* edit = merged_hsp->map_info->edits->edits +
                merged_hsp->map_info->edits->num_edits++;

            edit->query_pos = merged_hsp->query.end + k;
            /* FIXME: Mismatch bases cannot be currently set because there is
               no access to query or subject sequence in this function. */
            edit->query_base = query[edit->query_pos];
            edit->subject_base = edit->query_base;

            ASSERT(merged_hsp->map_info->edits->num_edits <= edits_size);
        }
    }

    /* add query gap to jumper edits */
    if (query_gap > 0) {
        for (k = 0;k < query_gap;k++) {
            JumperEdit* edit = merged_hsp->map_info->edits->edits +
                merged_hsp->map_info->edits->num_edits++;
            
            edit->query_pos = merged_hsp->query.end + mismatches;
            /* FIXME: Mismatch bases cannot be currently set because there is
               no access to query or subject sequence in this function. */
            edit->query_base = kGap;
            edit->subject_base = 0;

            ASSERT(merged_hsp->map_info->edits->num_edits <= edits_size);
        }
    }

    /* add subject gap to jumper edits */
    if (subject_gap > 0) {
        for (k = 0;k < subject_gap;k++) {
            JumperEdit* edit = merged_hsp->map_info->edits->edits +
                merged_hsp->map_info->edits->num_edits++;
            
            edit->query_pos = merged_hsp->query.end + mismatches + k;
            /* FIXME: Mismatch bases cannot be currently set because there is
               no access to query or subject sequence in this function. */
            edit->query_base = query[edit->query_pos];
            edit->subject_base = kGap;

            ASSERT(merged_hsp->map_info->edits->num_edits <= edits_size);
        }
    }

    /* merge jumper edits */
    if (hsp->map_info->edits->num_edits) {
        JumperEdit* edit = merged_hsp->map_info->edits->edits +
            merged_hsp->map_info->edits->num_edits;

        ASSERT(merged_hsp->map_info->edits->num_edits +
               hsp->map_info->edits->num_edits <= edits_size);

        memcpy(edit, hsp->map_info->edits->edits,
               hsp->map_info->edits->num_edits * sizeof(JumperEdit));

        merged_hsp->map_info->edits->num_edits +=
            hsp->map_info->edits->num_edits;
    }

    /* update alignment extents, scores, etc */
    merged_hsp->query.end = hsp->query.end;
    merged_hsp->subject.end = hsp->subject.end;
    merged_hsp->score = s_ComputeAlignmentScore(merged_hsp,
                                                score_opts->penalty,
                                                score_opts->gap_open,
                                                score_opts->gap_extend);

    merged_hsp->map_info->right_edge = hsp->map_info->right_edge;
    if (!merged_hsp->map_info->subject_overhangs) {
        return merged_hsp;
    }

    if (!hsp->map_info->subject_overhangs ||
        !hsp->map_info->subject_overhangs->right_len ||
        !hsp->map_info->subject_overhangs->right) {

        merged_hsp->map_info->subject_overhangs->right_len = 0;
        if (merged_hsp->map_info->subject_overhangs->right) {
            sfree(merged_hsp->map_info->subject_overhangs->right);
            merged_hsp->map_info->subject_overhangs->right = NULL;
        }
    }
    else {
        Int4 new_right_len = hsp->map_info->subject_overhangs->right_len;
        if (merged_hsp->map_info->subject_overhangs->right_len <
            new_right_len) {

            merged_hsp->map_info->subject_overhangs->right =
                realloc(merged_hsp->map_info->subject_overhangs->right,
                        new_right_len * sizeof(Uint1));
        }
        memcpy(merged_hsp->map_info->subject_overhangs->right,
               hsp->map_info->subject_overhangs->right,
               new_right_len * sizeof(Uint1));
    }

    return merged_hsp;
}


/* Find paired reads mapped to different database sequences */
static int s_FindRearrangedPairs(HSPChain** saved,
                                 const BlastQueryInfo* query_info)
{
    Int4 query_idx;

    /* iterate over single reads from each pair */
    for (query_idx = 0;query_idx + 1 < query_info->num_queries; query_idx++) {
        HSPChain* chain = saved[query_idx];
        HSPChain* thepair = saved[query_idx + 1];

        /* skip queries with no results */
        if (!chain || !thepair) {
            continue;
        }

        /* skip queries that do not have pairs */
        if (query_info->contexts[query_idx * NUM_STRANDS].segment_flags !=
            eFirstSegment) {

            continue;
        }

        /* skip queries with more than one saved chain and ones that already
           have a pair */
        if (chain->next || chain->pair || thepair->next) {
            continue;
        }

        /* skip chains aligned to the same subject; these are taken care of
           elsewhere */
        if (chain->oid == thepair->oid) {
            continue;
        }

        /* skip HSP chains aligned on the same strands */
        if (SIGN(chain->hsps->hsp->query.frame) ==
            SIGN(thepair->hsps->hsp->query.frame)) {
            continue;
        }

        /* mark the pair */
        chain->pair = thepair;
        thepair->pair = chain;

        chain->pair_conf = PAIR_PARALLEL;
        thepair->pair_conf = PAIR_PARALLEL;
    }

    return 0;
}


/* Sort chains by score in descending order */
static int s_CompareChainsByScore(const void* cha, const void* chb)
{
    const HSPChain* a = *((HSPChain**)cha);
    const HSPChain* b = *((HSPChain**)chb);

    if (a->score < b->score) {
        return 1;
    }
    else if (a->score > b->score) {
        return -1;
    }
    else {
        return 0;
    }
}

/* Remove chains with scores below the these of the best ones considering
   score bonus for pairs and count the number of chains with same of better
   score for each chain */
static int s_PruneChains(HSPChain** saved, Int4 num_queries, Int4 pair_bonus)

{
    Int4 query_idx;
    Int4 array_size = 10;
    HSPChain** array = calloc(array_size, sizeof(HSPChain*));

    /* Prunning is done in two passes: first considering single chains,
       then considering pairs. The first pass breaks pairs where there is much
       better chain with no pair. The second pass selects the best scoring
       chains giving preference to pairs. Pairs are preferable, so they get
       additional score. Two passes are requires, because we compare single
       chains with single chains and pairs with pairs. This is to avoid
       breaking pairs in situations with two pairs with one high scoring
       chain. */

    /* first pass: for each query remove poor scoring chains that may split
       pairs */
    for (query_idx = 0; query_idx < num_queries; query_idx++) {
        HSPChain* chain = saved[query_idx];
        HSPChain* prev = NULL;
        Int4 best_score = 0;

        if (!chain || !chain->next) {
            continue;
        }

        /* find the best single chain score */
        for (chain = saved[query_idx]; chain; chain = chain->next) {
            if (chain->score > best_score) {
                best_score = chain->score;
            }
        }

        /* remove chains with poor scores, considering pair bonus */
        chain = saved[query_idx];
        while (chain) {
            Int4 score = (chain->pair ? chain->score + pair_bonus : chain->score);

            if (score < best_score) {
                HSPChain* next = chain->next;

                chain->next = NULL;
                HSPChainFree(chain);
                chain = next;

                if (prev) {
                    prev->next = next;
                }
                else {
                    saved[query_idx] = next;
                }
            }
            else {
                prev = chain;
                chain = chain->next;
            }
        }

    }

    /* second pass: for each query remove chains scoring poorer than the best
       ones considering pair bonus */
    for (query_idx = 0; query_idx < num_queries; query_idx++) {
        HSPChain* chain = saved[query_idx];
        HSPChain* prev = NULL;
        Int4 best_score = 0;
        Int4 best_pair_score = 0;
        Int4 i;
        Int4 count = 0;
        Int4 num_chains = 0;

        if (!chain) {
            continue;
        }

        if (!chain->next) {
            chain->count = 1;
            continue;
        }

        /* find the best scores for single chains and pairs */
        for (chain = saved[query_idx]; chain; chain = chain->next) {
            Int4 score = (chain->pair ? chain->score + pair_bonus : chain->score);
            if (score >= best_score) {
                best_score = score;
            }

            if (chain->pair &&
                chain->score + chain->pair->score >= best_pair_score) {

                best_pair_score = chain->score + chain->pair->score;
            }

            /* collect chains in the array */
            if (num_chains >= array_size) {
                array_size *= 2;
                array = realloc(array, array_size * sizeof(HSPChain*));
                if (!array) {
                    return -1;
                }
            }
            array[num_chains++] = chain;
        }

        /* sort chains by scores in descending order */
        ASSERT(num_chains > 1);
        qsort(array, num_chains, sizeof(HSPChain*), s_CompareChainsByScore);

        /* find multiplicity for each chain: number of chains with same or
           larger score */
        /* Note that multiplicity is currently not reported. Only the number
           of returned hits for each query is reported */
        for (i = 0;i < num_chains; i++) {
            Int4  k;

            count++;
            /* check for chains with the same score */
            k = i + 1;
            while (k < num_chains && array[k]->score == array[i]->score) {
                k++;
                count++;
            }

            /* save multiplicity */
            for (;i < k;i++) {
                array[i]->count = count;
            }
            i--;
        }


        /* remove chains with poor score and count chains with equal or better
           score to the minimum pair one */
        chain = saved[query_idx];
        while (chain) {
            Int4 score = (chain->pair ? chain->score + pair_bonus : chain->score);

            /* We compare single chains with single chains and pairs with
               pairs. There may be pairs with one strong and one weak hit.
               Comparing single chain scores we can be left with single chains
               from different pairs. This comparison aviods that. The first
               pass ensures that pairs with poor scoring chains are removed,
               so we do not have to compare singles with pairs. */
            if ((!chain->pair && score < best_score) ||
                (chain->pair && chain->score + chain->pair->score <
                 best_pair_score)) {

                HSPChain* next = chain->next;

                chain->next = NULL;
                HSPChainFree(chain);
                chain = next;

                if (prev) {
                    prev->next = next;
                }
                else {
                    saved[query_idx] = next;
                }
            }
            else {
                prev = chain;
                chain = chain->next;
            }
        }
    }

    if (array) {
        sfree(array);
    }

    return 0;
}


/* Compare chains by subject oid and offfset */
static int s_CompareChainsByOid(const void* cha, const void* chb)
{
    const HSPChain* a = *((HSPChain**)cha);
    const HSPChain* b = *((HSPChain**)chb);

    if (a->oid > b->oid) {
        return 1;
    }
    else if (a->oid < b->oid) {
        return -1;
    }
    else if (a->hsps->hsp->subject.offset > b->hsps->hsp->subject.offset) {
        return 1;
    }
    else if (a->hsps->hsp->subject.offset < b->hsps->hsp->subject.offset) {
        return -1;
    }

    return 0;
}

/* Sort chains by subject oid and position and HSPs in the chains by query and
   subject positions */
static Int4 s_SortChains(HSPChain** saved, Int4 num_queries,
                         int(*comp)(const void*, const void*))
{
    Int4 i;
    Int4 array_size = 50;
    HSPChain** chain_array = NULL;

    if (!saved || num_queries < 0) {
        return -1;
    }

    chain_array = calloc(array_size, sizeof(HSPChain*));
    if (!chain_array) {
        return -1;
    }

    /* for each query */
    for (i = 0;i < num_queries;i++) {
        HSPChain* chain = saved[i];
        Int4 num_chains = 0;
        Int4 k;

        if (!chain) {
            continue;
        }

        /* create an array of pointer to chains */
        for (; chain; chain = chain->next) {
            if (num_chains >= array_size) {
                array_size *= 2;
                chain_array = realloc(chain_array, array_size *
                                      sizeof(HSPChain*));

                ASSERT(chain_array);
                if (!chain_array) {
                    return -1;
                }
            }
            chain_array[num_chains++] = chain;
        }

        if (num_chains > 1) {
            /* sort chains */
            qsort(chain_array, num_chains, sizeof(HSPChain*), comp);

            /* create the list of chains in the new order */
            for (k = 0;k < num_chains - 1;k++) {
                chain_array[k]->next = chain_array[k + 1];
            }
            chain_array[num_chains - 1]->next = NULL;

            saved[i] = chain_array[0];
            ASSERT(saved[i]);
        }
    }

    if (chain_array) {
        free(chain_array);
    }

    return 0;
}


/* Separate HSPs that overlap on the query to different chains (most output
   formats do not support query overlaps) */
static int s_RemoveOverlaps(HSPChain* chain, const ScoringOptions* score_opts,
                            Int4 query_len)
{
    HSPContainer* h = NULL;
    HSPContainer* prev = NULL;
    HSPChain* tail = chain->next;
    HSPChain* head = chain;
    Boolean rescore = FALSE;

    if (!chain) {
        return -1;
    }

    /* current and previous HSP */
    h = chain->hsps->next;
    prev = chain->hsps;

    for (; h; h = h->next) {

        /* if HSPs overlap on the query */
        if (prev->hsp->query.end > h->hsp->query.offset) {
            /* do a flat copy of the chain */
            HSPChain* new_chain = HSPChainNew(chain->context);
            memcpy(new_chain, chain, sizeof(HSPChain));
            new_chain->pair = NULL;
            new_chain->next = NULL;

            /* new chain starts with h */
            new_chain->hsps = h;

            /* the original chain ends on the previous HSP */
            prev->next = NULL;

            /* add new chain to the list */
            head->next = new_chain;
            head = new_chain;

            rescore = TRUE;
        }
        prev = h;
    }

    /* compute chains scores */
    if (rescore) {
        for (; chain; chain = chain->next) {
            chain->score = s_ComputeChainScore(chain, score_opts, query_len,
                                               FALSE);
        }
    }

    /* re-attach other chains to the list */
    head->next = tail;

    return 0;
}


/* Removes chains with scores below the cutoff */
static int s_FilterChains(HSPChain** chains_ptr, Int4 cutoff_score,
                          Int4 cutoff_edit_distance)
{
    HSPChain* chain = *chains_ptr;

    while (chain && !s_TestCutoffs(chain, cutoff_score, cutoff_edit_distance)
           /*chain->score < cutoff_score*/) {
        HSPChain* next = chain->next;
        chain->next = NULL;
        HSPChainFree(chain);
        chain = next;
    }
    *chains_ptr = chain;

    if (chain) {
        while (chain && chain->next) {
            if (!s_TestCutoffs(chain->next, cutoff_score, cutoff_edit_distance)
                /*chain->next->score < cutoff_score*/) {
                HSPChain* next = chain->next->next;
                chain->next->next = NULL;
                HSPChainFree(chain->next);
                chain->next = next;
            }
            else {
                chain = chain->next;
            }
        }
    }

    return 0;
}


/* Populate HSP data and save final results */
static int s_Finalize(HSPChain** saved, BlastMappingResults* results,
                      const BlastQueryInfo* query_info,
                      const BLAST_SequenceBlk* query_blk,
                      const ScoringOptions* score_opts,
                      Boolean is_paired,
                      Int4 cutoff_score,
                      Int4 cutoff_edit_dist)
{
    Int4 query_idx;
    const Int4 kPairBonus = 21;

    /* remove poor scoring chains and find chain multiplicity */
    if (!getenv("MAPPER_NO_PRUNNING")) {
        s_PruneChains(saved, query_info->num_queries, kPairBonus);
    }

    /* find adapters in query sequences */
    s_FindAdapters(saved, query_blk, query_info, score_opts);

    /* find polyA tails in query sequences */
    s_FindPolyATails(saved, query_blk, query_info);

    /* find pairs of chains aligned to different subjects */
    s_FindRearrangedPairs(saved, query_info);

    /* sort chains by subject oid to ensure stable results for multithreaded
       runs; sorting must be done here so that compartment numbers are stable */
    /* FIXME: this may not be needed */
    s_SortChains(saved, query_info->num_queries, s_CompareChainsByOid);


    /* separate HSPs that overlap on the query into different chains */
    if (getenv("MAPPER_NO_OVERLAPPED_HSP_MERGE")) {
        for (query_idx = 0; query_idx < query_info->num_queries; query_idx++) {
            HSPChain* chain = saved[query_idx];
            Int4 query_len;

            if (!chain) {
                continue;
            }

            query_len = query_info->contexts[chain->context].query_length;
            for (; chain; chain = chain->next) {
                s_RemoveOverlaps(chain, score_opts, query_len);
            }

        }
    }


    /* remove chains with score below cutoff */
    for (query_idx = 0; query_idx < query_info->num_queries; query_idx++) {
        s_FilterChains(&saved[query_idx], cutoff_score, cutoff_edit_dist);
    }

    /* Compute number of results to store, and number of unique mappings for
       each query (some chains may be copied to create pairs) */

    /* count number of unique mappings */
    for (query_idx = 0; query_idx < query_info->num_queries; query_idx++) {
        HSPChain* chain = saved[query_idx];
        HSPChain* prev = chain;
        Int4 num_unique = 1;

        if (!chain) {
            continue;
        }

        chain = chain->next;
        for (; chain; chain = chain->next, prev = prev->next) {
            if (prev->oid != chain->oid ||
                s_FindFragmentStart(prev) != s_FindFragmentStart(chain)) {

                num_unique++;
            }
        }

        /* FIXME: for tabular output check count computed earlier */
        for (chain = saved[query_idx]; chain; chain = chain->next) {
            chain->count = num_unique;
        }
    }

#if _DEBUG
    for (query_idx = 0; query_idx < query_info->num_queries; query_idx++) {
         HSPChain* chain = saved[query_idx];
         for (; chain; chain = chain->next) {
             HSPContainer* h = chain->hsps;
             for (; h; h = h->next) {
                 s_TestHSPRanges(h->hsp);
             }
         }
         
    }
#endif

    results->chain_array = saved;
    results->num_queries = query_info->num_queries;

    return 0;
}

/** Perform post-run clean-ups
 * @param data The buffered data structure [in]
 * @param results The HSP results to propagate [in][out]
 */
static int
s_BlastHSPMapperFinal(void* data, void* mapping_results)
{
    BlastHSPMapperData* spl_data = data;
    BlastHSPMapperParams* params = spl_data->params;
    BlastMappingResults* results = (BlastMappingResults*)mapping_results;

    ASSERT(spl_data->saved_chains);

    if (spl_data->saved_chains != NULL) {
        s_Finalize(spl_data->saved_chains, results, spl_data->query_info,
                   spl_data->query, &params->scoring_options, params->paired,
                   params->cutoff_score, params->cutoff_edit_dist);
    }

    spl_data->saved_chains = NULL;

   return 0;
}


/* A node representing an HSP for finding the best chain of HSPs for RNA-seq */
typedef struct HSPNode {
    BlastHSP** hsp;     /* pointer to the entry in hsp_list */
    Int4 best_score;    /* score for path that starts at this node */
    struct HSPNode* path_next; /* next node in the path */
} HSPNode;


/* Copy a array of HSP nodes */
static int s_HSPNodeArrayCopy(HSPNode* dest, HSPNode* source, Int4 num)
{
    int i;

    if (!dest || !source) {
        return -1;
    }

    for (i = 0;i < num;i++) {
        dest[i] = source[i];
        if (source[i].path_next) {
            dest[i].path_next = dest + (source[i].path_next - source);
        }
    }

    return 0;
}


/* Maximum number of top scoring HSP chains to report for a single read and
   strand */
#define MAX_NUM_HSP_PATHS 40

/* A chain of HSPs aligning a spliced RNA-Seq read to a genome */
typedef struct HSPPath {
    HSPNode** start;  /* array of chain starting nodes */
    int num_paths;    /* number of chains */
    Int4 score;       /* all chains in start have this cumulative score */
} HSPPath;


static HSPPath* HSPPathFree(HSPPath* path)
{
    if (path) {
        if (path->start) {
            free(path->start);
        }
        free(path);
    }

    return NULL;
}

static HSPPath* HSPPathNew(void)
{
    HSPPath* retval = calloc(1, sizeof(HSPPath));
    if (!retval) {
        return NULL;
    }

    retval->start = calloc(MAX_NUM_HSP_PATHS, sizeof(HSPNode*));
    if (!retval->start) {
        free(retval);
        return NULL;
    }

    return retval;
}


#define NUM_SIGNALS 23
#define NUM_SIGNALS_CONSENSUS 2

/* Find a split for HSPs overlapping on the query by finding splice signals in
   the subject sequence. The first HSP must have smaller query offset than the
   second one. Subject is recreated from the query sequence and information
   in an HSP.
   @param first HSP with smaller query offset [in|out]
   @param second HSP with larger query offset [in|out]
   @param query Query sequence [in]
   @return Status
 */
static Int4
s_FindSpliceJunctionsForOverlaps(BlastHSP* first, BlastHSP* second,
                                 Uint1* query, Int4 query_len,
                                 Boolean consensus_only)
{
    Int4 i, k;
    /* splice signal pairs from include/algo/sequence/consensus_splice.hpp */
    Uint1 signals[NUM_SIGNALS] = {0xb2, /* GTAG */
                                  0x71, /* CTAC (reverse complement) */
                                  0x92, /* GCAG */
                                  0x79, /* CTGC */
                                  0x31, /* ATAC */
                                  0xb3, /* GTAT */
                                  0xbe, /* GTTG */
                                  0x41, /* CAAC */
                                  0xba, /* GTGG */
                                  0x51, /* CCAC */
                                  0xb0, /* GTAA */
                                  0xf1, /* TTAC */
                                  0x82, /* GAAG */
                                  0x7d, /* CTTC */
                                  0xf2, /* TTAG */
                                  0x70, /* CTAA */
                                  0x32, /* ATAG */
                                  0x73, /* CTAT */
                                  0xa2, /* GGAG */
                                  0x75, /* CTCC */
                                  0x33, /* ATAT (revese complemented is self) */
                                  0x30, /* ATAA */
                                  0xf3  /* TTAT */};


    Boolean found = FALSE;
    Uint1* subject = NULL;
    Int4 overlap_len;
    const Uint1 kGap = 15;
    Int4 num_signals = consensus_only ? NUM_SIGNALS_CONSENSUS : NUM_SIGNALS;

    /* HSPs must be sorted and must overlap only on the query */
    ASSERT(first->query.offset < second->query.offset);
    ASSERT(second->query.offset <= first->query.end);
    ASSERT(first->subject.offset < second->subject.offset);


    /* if there are edits within the overlap region, then try to maximize
       the chain score by assigning a part of the overlap region to the
       HSP that has fewer edits in the overlap */
    if ((first->map_info->edits->num_edits > 0 &&
         first->map_info->edits->edits[
                        first->map_info->edits->num_edits - 1].query_pos >=
            second->query.offset)
        || (second->map_info->edits->num_edits > 0 &&
            second->map_info->edits->edits[0].query_pos <= first->query.end)) {

        /* find nummer of  edits in both HSPs within the overlap and assign
           overlap to the HSP that has fewer edits */
        Int4 num_first = 0;
        Int4 num_second = 0;

        /* find number of edits */
        for (i = first->map_info->edits->num_edits - 1;i >= 0;i--) {
            JumperEdit* edits = first->map_info->edits->edits;
            if (edits[i].query_pos < second->query.offset) {
                break;
            }
            num_first++;
        }

        for (i = 0;i < second->map_info->edits->num_edits;i++) {
            JumperEdit* edits = second->map_info->edits->edits;
            if (edits[i].query_pos >= first->query.end) {
                break;
            }
            num_second++;
        }

        if (num_first > num_second) {
            while (first->map_info->edits->num_edits > 0 &&
                   first->map_info->edits->edits[
                          first->map_info->edits->num_edits - 1].query_pos >=
                   second->query.offset) {

                JumperEdit* edits = first->map_info->edits->edits;
                Uint1 edge = first->map_info->right_edge;
                Int4 num_edits = first->map_info->edits->num_edits;
                Int4 trim_by;

                if (edits[num_edits - 1].query_pos >= first->query.end - 1) {
                    if (edits[num_edits - 1].subject_base != kGap) {
                        edge >>= 2;
                        edge |= edits[num_edits - 1].subject_base << 2;
                    }
                }
                else if (edits[num_edits - 1].query_pos == query_len - 2 &&
                         edits[num_edits - 1].subject_base == kGap) {

                    edge = (edge << 2) | query[query_len - 1];
                }
                else {
                    if (edits[num_edits - 1].subject_base != kGap &&
                        edits[num_edits - 1].query_base != kGap) {

                        edge = (edits[num_edits - 1].subject_base << 2) |
                            query[edits[num_edits - 1].query_pos + 1];
                    }
                    else if (edits[num_edits - 1].subject_base == kGap) {
                        edge = (query[edits[num_edits - 1].query_pos + 1] << 2 ) |
                            query[edits[num_edits - 1].query_pos + 2];
                    }
                    else {
                        edge = (edits[num_edits - 1].subject_base << 2) |
                            query[edits[num_edits - 1].query_pos];
                    }
                }

                trim_by = first->query.end -  edits[
                            first->map_info->edits->num_edits - 1].query_pos;
                /* if the edit is an insert in the genome, count genome bases
                   when trimming the HSP */
                if (edits[num_edits - 1].query_base == kGap) {
                    trim_by++;
                    /* FIXME: use proper scores here */
                    s_TrimHSP(first, trim_by, FALSE, FALSE, -4, -4, -4);
                }
                else {
                    s_TrimHSP(first, trim_by, TRUE, FALSE, -4, -4, -4);
                }
                first->map_info->right_edge = edge;
            }
        }
        else if (num_second > num_first) {
            while (second->map_info->edits->num_edits > 0 &&
                   second->map_info->edits->edits[0].query_pos <
                   first->query.end) {

                JumperEdit* edits = second->map_info->edits->edits;
                Uint1 edge = second->map_info->left_edge;
                Int4 trim_by;

                if (edits[0].query_pos == 0) {
                    if (edits[0].subject_base != kGap) {
                        edge <<= 2;
                        edge |= edits[0].subject_base;
                    }
                }
                else if (edits[0].query_pos == 1 &&
                         edits[0].subject_base == kGap) {

                    edge = (edge << 2) | query[0];
                }
                else {

                    edge = (query[edits[0].query_pos - 1] << 2) |
                        edits[0].subject_base;

                    if (edits[0].subject_base == kGap) {
                        edge = (query[edits[0].query_pos - 2] << 2) |
                            query[edits[0].query_pos - 1];
                    }
                }

                trim_by = edits[0].query_pos - second->query.offset + 1;
                /* if the edit is an insert in the genome, count genome bases
                   when trimming the HSP */
                if (edits[0].query_base == kGap) {
                    trim_by++;
                    /* FIXME: use proper scores here */
                    s_TrimHSP(second,
                              edits[0].query_pos - second->query.offset + 1,
                              FALSE, TRUE, -4, -4, -4);
                }
                else {
                    s_TrimHSP(second,
                              edits[0].query_pos - second->query.offset + 1,
                              TRUE, TRUE, -4, -4, -4);
                }
                second->map_info->left_edge = edge;
            }
        }
        else {
            /* if the number of edits is the same we do not know how to
               divide the overlap */

            first->map_info->right_edge &= ~MAPPER_SPLICE_SIGNAL;
            second->map_info->left_edge &= ~MAPPER_SPLICE_SIGNAL;
            return 0;
        }
    }

    /* give up if there are gaps in the alignments or mismatches still left in
       the overlapping parts */
    if ((first->map_info->edits->num_edits > 0 &&
         first->map_info->edits->edits[
                        first->map_info->edits->num_edits - 1].query_pos >=
            second->query.offset)
        || (second->map_info->edits->num_edits > 0 &&
            second->map_info->edits->edits[0].query_pos <= first->query.end)) {

        first->map_info->right_edge &= ~MAPPER_SPLICE_SIGNAL;
        second->map_info->left_edge &= ~MAPPER_SPLICE_SIGNAL;
        return 0;
    }

    /* recreate the subject sequence plus two bases on each side */
    overlap_len = first->query.end - second->query.offset;
    subject = calloc(overlap_len + 4, sizeof(Uint1));
    if (!subject) {
        return -1;
    }

    /* FIXME: lef_edge and right_edge should be changed to subject_overhangs */
    subject[0] = (second->map_info->left_edge & 0xf) >> 2;
    subject[1] = second->map_info->left_edge & 3;
    memcpy(subject + 2, query + second->query.offset,
           overlap_len * sizeof(Uint1));

    subject[2 + overlap_len] = (first->map_info->right_edge & 0xf) >> 2;
    subject[2 + overlap_len + 1] = first->map_info->right_edge & 3;

    /* search for matching splice signals in the subject sequence */
    for (k = 0;k < num_signals;k++) {

        for (i = 0;i <= overlap_len && !found;i++) {

            Uint1 seq = (subject[i] << 2) | subject[i + 1] |
                (subject[i + 2] << 6) | (subject[i + 3] << 4);

            /* if splice signals are found, update HSPs */
            if (seq == signals[k]) {
                Int4 d = first->query.end - (second->query.offset + i);
                first->query.end -= d;
                first->subject.end -= d;
                first->gap_info->num[first->gap_info->size - 1] -= d;
                first->score -= d;
                first->num_ident -= d;
                first->map_info->right_edge = (seq & 0xf0) >> 4;
                first->map_info->right_edge |= MAPPER_SPLICE_SIGNAL;

                d = i;
                second->query.offset += d;
                second->subject.offset += d;
                second->gap_info->num[0] -= d;
                second->score -= d;
                second->num_ident -= d;
                second->map_info->left_edge = seq & 0xf;
                second->map_info->left_edge |= MAPPER_SPLICE_SIGNAL;

                ASSERT(first->gap_info->size > 1 ||
                       first->query.end - first->query.offset ==
                       first->subject.end - first->subject.offset);

                ASSERT(second->gap_info->size > 1 ||
                       second->query.end - second->query.offset ==
                       second->subject.end - second->subject.offset);

                found = TRUE;
                break;
            }
        }
    }

    if (!found) {
        first->map_info->right_edge &= ~MAPPER_SPLICE_SIGNAL;
        second->map_info->left_edge &= ~MAPPER_SPLICE_SIGNAL;
    }

    if (subject) {
        sfree(subject);
    }

    ASSERT(first->query.offset < first->query.end);
    ASSERT(second->query.offset < second->query.end);

    return 0;
}

static void s_ExtendAlignmentCleanup(Uint1* subject,
                                     BlastGapAlignStruct* gap_align,
                                     GapEditScript* edit_script,
                                     JumperEditsBlock* edits)
{
    if (subject) {
        free(subject);
    }

    BLAST_GapAlignStructFree(gap_align);
    GapEditScriptDelete(edit_script);
    JumperEditsBlockFree(edits);
}

/* Extend alignment on one side of an HSP up to a given point (a splice site)
*/
static Int4 s_ExtendAlignment(BlastHSP* hsp, const Uint1* query,
                              Int4 query_from, Int4 query_to,
                              Int4 subject_from, Int4 subject_to,
                              const ScoringOptions* score_options,
                              Boolean is_left)
{
    Int4 o_len;
    Uint1* o_seq = NULL;
    Uint1* subject = NULL;
    BlastGapAlignStruct* gap_align = NULL;
    GapEditScript* edit_script = NULL;
    JumperEditsBlock* edits = NULL;
    Int4 i, k, s;
    /* number of query bases to align */
    Int4 query_gap = query_to - query_from + 1;
    /* number of subject bases to align */
    Int4 subject_gap = subject_to - subject_from + 1;
    Int4 num_identical;
    Int4 query_ext_len, subject_ext_len;
    Int4 ungapped_ext_len;

    JUMP* jumper_table = NULL;

    JUMP jumper_mismatch[] = {{1, 1, 0, 0}};

    JUMP jumper_insertion[] = {{1, 1, 2, 0},
                               {1, 0, 1, 0},
                               {1, 1, 0, 0}};

    JUMP jumper_deletion[] = {{1, 1, 2, 0},
                              {0, 1, 1, 0},
                              {1, 1, 0, 0}};

    if (!hsp || !query || !hsp->map_info ||
        !hsp->map_info->subject_overhangs) {
        return -1;
    }

    /* length of the subject overgang sequence saved in the HSP */
    o_len = is_left ? hsp->map_info->subject_overhangs->left_len :
        hsp->map_info->subject_overhangs->right_len;
    /* subject sequence */
    o_seq = is_left ? hsp->map_info->subject_overhangs->left :
        hsp->map_info->subject_overhangs->right;
    ASSERT(o_seq);

    /* compressed subject sequence (needed for computig jumper edits) */
    subject = calloc(o_len / 4 + 1, sizeof(Uint1));
    if (!subject) {
        return -1;
    }

    /* compress subject sequence  */
    k = 6;
    s = 0;
    for (i = 0;i < o_len;i++, k-=2) {
        subject[s] |= o_seq[i] << k;
        if (k == 0) {
            s++;
            k = 8;
        }
    }

    /* gap_align is needed in jumper alignment API calls, we only allocate
       fields that will be needed in these calls */
    gap_align = calloc(1, sizeof(BlastGapAlignStruct));
    if (!gap_align) {
        s_ExtendAlignmentCleanup(subject, NULL, edit_script, edits);
        return -1;
    }
    gap_align->jumper = JumperGapAlignNew(o_len * 2);
    if (!gap_align->jumper) {
        s_ExtendAlignmentCleanup(subject, gap_align, edit_script, edits);
        return -1;
    }

    /* select jumper table based on the number of expected indels; we allow
       only insertions or only deletions */
    switch (SIGN(query_gap - subject_gap)) {
    case 0: jumper_table = jumper_mismatch;
        break;

    case -1: jumper_table = jumper_deletion;
        break;

    case 1: jumper_table = jumper_insertion;
        break;

    default:
        ASSERT(0);
        s_ExtendAlignmentCleanup(subject, gap_align, edit_script, edits);
        return -1;
    };

    /* Compute alignment. We always do right extension as this is typically
       a short alignment. This is global alignment, hence the large max number
       of errors and no penalties for mismatches or gaps. The jumper table
       specifies costs for alignment errors. The penalty socres specify
       required number of macthes at the ends of the alignment. */
    JumperExtendRightWithTraceback(query + query_from, o_seq + subject_from,
                                   query_gap, subject_gap,
                                   1, 0, 0, 0,
                                   20, 20,
                                   &query_ext_len, &subject_ext_len,
                                   gap_align->jumper->right_prelim_block,
                                   &num_identical,
                                   FALSE,
                                   &ungapped_ext_len,
                                   jumper_table);

    ASSERT(query_ext_len <= query_gap);
    ASSERT(subject_ext_len <= subject_gap);

    /* Because this is a global alignment a gap may exist after the last query
       or subject base which causes jumper extension to stop prematurely. If
       this is the case, add the missing indel */
    while (query_ext_len < query_gap) {
        JumperPrelimEditBlockAdd(gap_align->jumper->right_prelim_block,
                                 JUMPER_INSERTION);

        query_ext_len++;
    }

    while (subject_ext_len < subject_gap) {
        JumperPrelimEditBlockAdd(gap_align->jumper->right_prelim_block,
                                 JUMPER_DELETION);

        subject_ext_len++;
    }
    ASSERT(query_ext_len == query_gap);
    ASSERT(subject_ext_len == subject_gap);

    /* update gap info in the HSP */
    edit_script = JumperPrelimEditBlockToGapEditScript(
                                        gap_align->jumper->left_prelim_block,
                                        gap_align->jumper->right_prelim_block);
    if (!edit_script) {
        s_ExtendAlignmentCleanup(subject, gap_align, edit_script, edits);
        return -1;
    }

    if (is_left) {
        hsp->gap_info = GapEditScriptCombine(&edit_script, &hsp->gap_info);
    }
    else {
        hsp->gap_info = GapEditScriptCombine(&hsp->gap_info, &edit_script);
        ASSERT(!edit_script);
    }
    ASSERT(hsp->gap_info);
    if (!hsp->gap_info) {
        s_ExtendAlignmentCleanup(subject, gap_align, edit_script, edits);
        return -1;
    }

    /* update jumper edits */
    gap_align->query_start = query_from;
    gap_align->query_stop = query_from + query_ext_len;
    gap_align->subject_start = subject_from;
    gap_align->subject_stop = subject_from + subject_ext_len;
    edits = JumperFindEdits(query, subject, gap_align);
    ASSERT(edits);
    if (!edits) {
        s_ExtendAlignmentCleanup(subject, gap_align, NULL, edits);
        return -1;
    }

    if (is_left) {
        hsp->map_info->edits = JumperEditsBlockCombine(&edits,
                                                   &hsp->map_info->edits);
    }
    else {
        hsp->map_info->edits = JumperEditsBlockCombine(
                                                    &hsp->map_info->edits,
                                                    &edits);
        ASSERT(!edits);
    }
    ASSERT(hsp->map_info->edits);
    if (!hsp->map_info->edits) {
        s_ExtendAlignmentCleanup(subject, gap_align, NULL, edits);
        return -1;
    }

    if (is_left) {
        hsp->query.offset -= query_ext_len;
        hsp->subject.offset -= subject_ext_len;
    }
    else {
        hsp->query.end += query_ext_len;
        hsp->subject.end += subject_ext_len;
    }

    /* compute new HSP score */
    hsp->score = s_ComputeAlignmentScore(hsp, score_options->penalty,
                                         score_options->gap_open,
                                         score_options->gap_extend);

    ASSERT(hsp->gap_info);
    ASSERT(hsp->map_info->edits);

    ASSERT(subject);


    if (subject) {
        free(subject);
    }
    BLAST_GapAlignStructFree(gap_align);

    return 0;
}


/* Find splcie junctions and extend HSP alignment if there are unaligned query
   bases in the middle of a chain.
*/
static Int4
s_FindSpliceJunctionsForGap(BlastHSP* first, BlastHSP* second,
                            Uint1* query, Int4 query_len,
                            const ScoringOptions* score_options)
{
    Int4 query_gap;
    Int4 first_len, second_len;
    Boolean found = FALSE;
    Int4 k;
    Int4 q;
    Uint1 signal = 0;
    /* use only cannonical splice signals here */
    /* The procedure does not maximize score for unaligned bases of the query.
       It only finds splice signals and then the best alignment it can find
       for the unaligned bases. For limited number of unaligned query bases,
       we can be sure of the splice site vs a continuous alignment, because a
       continuous alignment would score better. But we do not compare
       alignment scores between different splice sites, so we do not know
       which splice site and signal is better; and many splice signals here
       would lead to finding wrong splice sites. */
    Uint1 signals[NUM_SIGNALS_CONSENSUS] = {0xb2, /* GTAG */
                                            0x71  /* CTAC */};

    if (!first || !second) {
        return -1;
    }

    if (!first->map_info->subject_overhangs ||
        !first->map_info->subject_overhangs->right ||
        !second->map_info->subject_overhangs ||
        !second->map_info->subject_overhangs->left) {
        return -1;
    }

    /* number of query bases that fall between the HSPs */
    query_gap = second->query.offset - first->query.end;

    /* we do not have enough subject sequence saved (-1 because we allow up to
       one indel) */
    if (query_gap > first->map_info->subject_overhangs->right_len - 2 ||
        query_gap > second->map_info->subject_overhangs->left_len - 2) {
        return 0;
    }

    first_len = first->map_info->subject_overhangs->right_len;
    second_len = second->map_info->subject_overhangs->left_len;

    /* assume that the begining of intron was found, search for the splice
       signal at the end of the first HSP */
    for (q = 0; !found && q < 4;q++) {

        if (first->query.end - q <= first->query.offset) {
            break;
        }

        for (k = 0;k < NUM_SIGNALS_CONSENSUS && !found;k++) {
            Int4 i;
            Int4 status = 0;
            Int4 start;
            Uint1 seq;

            if (q == 0) {
                seq = (first->map_info->subject_overhangs->right[0] << 6) |
                    (first->map_info->subject_overhangs->right[1] << 4);
            }
            else if (q == 1) {
                seq = (query[first->query.end - 1] << 6) |
                    (first->map_info->subject_overhangs->right[0] << 4);
            }
            else if (q > 1) {
                seq = (query[first->query.end - q] << 6) |
                    (query[first->query.end - q + 1] << 4);
            }
            else {
                return -1;
            }

            if (seq != (signals[k] & 0xf0)) {
                continue;
            }

            /* location of the splice signa in subject overhang if there are no
               indels */
            start = second->map_info->subject_overhangs->left_len - 1 -
                query_gap - 2 - q;

            /* search for the splice signal at the end of intron;
               allow for up to 1 indel */
            for (i = MAX(start - 1, 0);i <= MIN(start + 1, second_len - 2);i++) {
                seq &= 0xf0;
                seq |= (second->map_info->subject_overhangs->left[i] << 2) |
                    second->map_info->subject_overhangs->left[i + 1];

                /* if the splice signal found extend the alignment */
                if (seq == signals[k]) {
                    Int4 subject_gap = second_len - (i + 2) + q;
                    if (query_gap - subject_gap < -1 ||
                        query_gap - subject_gap > 1) {

                        continue;
                    }

                    found = TRUE;
                    if (q > 0) {
                        s_TrimHSP(first, q, TRUE, FALSE,
                                  score_options->penalty,
                                  score_options->gap_open,
                                  score_options->gap_extend);
                    }

                    status = s_ExtendAlignment(second, query,
                             first->query.end,
                             second->query.offset - 1,
                             i + 2,
                             second->map_info->subject_overhangs->left_len - 1,
                             score_options,
                             TRUE);

                    ASSERT(status == 0);
                    if (status) {
                        return status;
                    }

                    signal = seq;
                    break;
                }
            }
        }
    }

    /* assume that end of the intron was found, search for the splice signal
       at the beginning of the intron */
    for (q = 0; !found && q < 4;q++) {

        if (second->query.offset + q >= second->query.end) {
            break;
        }

        for (k = 0;k < NUM_SIGNALS_CONSENSUS && !found;k++) {
            Int4 i;
            Int4 end;
            Uint1 seq;
            if (q == 0) {
                seq =
                    (second->map_info->subject_overhangs->left[second_len - 2] << 2) |
                    second->map_info->subject_overhangs->left[second_len - 1];
            }
            else if (q == 1) {
                seq = (second->map_info->subject_overhangs->left[second_len - 1] << 2) | query[second->query.offset];


            }
            else if (q > 1) {

                if (second->map_info->edits->num_edits > 0 &&
                    second->map_info->edits->edits[0].query_pos < q - 2) {

                    break;
                }

                seq = (query[second->query.offset + q - 2] << 2) |
                    query[second->query.offset + q + 1 - 2];
            }
            else {
                return -1;
            }

            if (seq != (signals[k] & 0xf)) {
                continue;
            }

            /* location of the splice signal in the subject overhang if there are
               no indels */
            end = query_gap + q;

            /* allow for up to 1 indel */
            for (i = MAX(end - 1, 0);i <= MIN(end + 1, first_len - 2);i++) {
                seq &= 0xf;
                seq |= (first->map_info->subject_overhangs->right[i] << 6) |
                    (first->map_info->subject_overhangs->right[i + 1] << 4);

                if (seq == signals[k]) {
                    Int4 status = 0;
                    Int4 subject_gap = i - q;
                    if (query_gap - subject_gap < -1 ||
                        query_gap - subject_gap > 1) {

                        continue;
                    }

                    found = TRUE;
                    if (q > 0) {
                        s_TrimHSP(second, q, TRUE, TRUE,
                                  score_options->penalty,
                                  score_options->gap_open,
                                  score_options->gap_extend);
                    }

                    status = s_ExtendAlignment(first, query,
                                               first->query.end,
                                               second->query.offset - 1,
                                               0, i - 1,
                                               score_options,
                                               FALSE);
                    ASSERT(status == 0);
                    if (status) {
                        return status;
                    }

                    signal = seq;
                    break;
                }
            }
        }
    }

    if (found) {
        first->map_info->right_edge = (signal & 0xf0) >> 4;
        first->map_info->right_edge |= MAPPER_SPLICE_SIGNAL;
        second->map_info->left_edge = signal & 0xf;
        second->map_info->left_edge |= MAPPER_SPLICE_SIGNAL;

        ASSERT(first->gap_info->size > 1 ||
               first->query.end - first->query.offset ==
               first->subject.end - first->subject.offset);

        ASSERT(second->gap_info->size > 1 ||
               second->query.end - second->query.offset ==
               second->subject.end - second->subject.offset);
    }
    else {
        first->map_info->right_edge &= ~MAPPER_SPLICE_SIGNAL;
        second->map_info->left_edge &= ~MAPPER_SPLICE_SIGNAL;
    }

    return 0;
}


#define NUM_SINGLE_SIGNALS 8

/* Record subject bases pre and post alignment in HSP as possible splice
   signals and set a flag for recognized signals */
/* Not used
static Int4 s_FindSpliceSignals(BlastHSP* hsp, Uint1* query, Int4 query_len)
{
    SequenceOverhangs* overhangs = NULL;
    /--* splice signals in the order of preference *--/
    Uint1 signals[NUM_SINGLE_SIGNALS] = {2,  /--* AG *--/
                                         11, /--* GT *--/
                                         13, /--* TC *--/
                                         7,  /--* CT *--/
                                         1,  /--* AC *--/
                                         4,  /--* CA *--/
                                         8,  /--* GA *--/
                                         14  /--* TG *--/};

    if (!hsp || !query) {
        return -1;
    }

    overhangs = hsp->map_info->subject_overhangs;
    ASSERT(overhangs);

    if (!overhangs) {
        return -1;
    }

    /--* FIXME: check for polyA and adapters *--/

    /--* search for a splice signal on the left edge, unless the query is aligned
       from the beginning *--/
    if (hsp->query.offset == 0) {
        hsp->map_info->left_edge = MAPPER_EXON;
    }
    else {
        int k, i;
        Boolean found = FALSE;

        if (overhangs->left_len >= 2) {
            Uint1 signal = (overhangs->left[overhangs->left_len - 2] << 2) |
                overhangs->left[overhangs->left_len - 1];

            for (k = 0;k < NUM_SINGLE_SIGNALS;k++) {
                if (signal == signals[k]) {
                    hsp->map_info->left_edge = signal;
                    hsp->map_info->left_edge |= MAPPER_SPLICE_SIGNAL;

                    found = TRUE;
                    break;
                }
            }
        }

        if (!found && overhangs->left_len >= 1) {
            Uint1 signal = (overhangs->left[overhangs->left_len - 1] << 2) |
                query[hsp->query.offset];

            for (k = 0;k < NUM_SINGLE_SIGNALS;k++) {
                if (signal == signals[k]) {
                    hsp->map_info->left_edge = signal;
                    hsp->map_info->left_edge |= MAPPER_SPLICE_SIGNAL;

                    /--* trim alignment *--/
                    s_TrimHSP(hsp, 1, TRUE, TRUE, -8, 0, -8);

                    /--* update score at some point *--/
                    found = TRUE;
                    break;
                }
            }
        }

        for (i = hsp->query.offset;!found && i < 4;i++) {
            Uint1 signal = (query[i] << 2) | query[i + 1];
            for (k = 0;k < NUM_SINGLE_SIGNALS;k++) {
                if (signal == signals[k]) {
                    hsp->map_info->left_edge = signal;
                    hsp->map_info->left_edge |= MAPPER_SPLICE_SIGNAL;

                    /--* trim alignment *--/
                    s_TrimHSP(hsp, i + 2, TRUE, TRUE, -8, 0, -8);

                    found = TRUE;
                    break;
                }
            }
        }
    }

    /--* search for the splice signal on the right edge, unless the query is
       aligned to the end *--/
    if (hsp->query.end == query_len) {
        hsp->map_info->right_edge = MAPPER_EXON;
    }
    else {
        int k, i;
        Boolean found = FALSE;

        if (overhangs->right_len >= 2) {
            Uint1 signal = (overhangs->right[0] << 2) | overhangs->right[1];
            for (k = 0;k < NUM_SINGLE_SIGNALS;k++) {
                if (signal == signals[k]) {
                    hsp->map_info->right_edge = signal;
                    hsp->map_info->right_edge |= MAPPER_SPLICE_SIGNAL;

                    found = TRUE;
                    break;
                }
            }

        }

        if (!found && overhangs->right_len >= 1) {
            Uint1 signal = (query[hsp->query.end - 1] << 2) |
                overhangs->right[0];

            for (k = 0;k < NUM_SINGLE_SIGNALS;k++) {
                if (signal == signals[k]) {
                    hsp->map_info->right_edge = signal;
                    hsp->map_info->right_edge |= MAPPER_SPLICE_SIGNAL;

                    /--* update HSP *--/
                    s_TrimHSP(hsp, 1, TRUE, FALSE, -8, 0, -8);

                    /--* update score at some point *--/
                    found = TRUE;
                    break;
                }
            }

        }

        for (i = hsp->query.end - 2;!found && i > hsp->query.end - 2 - 4;i--) {
            Uint1 signal = (query[i] << 2) | query[i + 1];
            for (k = 0;k < NUM_SINGLE_SIGNALS;k++) {
                if (signal == signals[k]) {
                    hsp->map_info->right_edge = signal;
                    hsp->map_info->right_edge |= MAPPER_SPLICE_SIGNAL;

                    /--* update HSP *--/
                    s_TrimHSP(hsp, hsp->query.end - i, TRUE, FALSE, -8, 0, -8);

                    found = TRUE;
                    break;
                }
            }
        }
    }

    return 0;
}
*/


static Int4
s_TrimOverlap(BlastHSP* first, BlastHSP* second)
{
    if (second->query.offset - first->query.end < 0) {
        Int4 overlap = first->query.end - second->query.offset;
        ASSERT(overlap >= 0);

        if (second->query.end - second->query.offset > overlap) {

            s_TrimHSP(second, overlap, TRUE, TRUE, -4, -4, -4);
        }
        else {
            s_TrimHSP(first, overlap, TRUE, FALSE, -4, -4, -4);
        }

                              
        ASSERT(first->query.end == second->query.offset);

#if _DEBUG
        s_TestHSPRanges(second);
#endif
    }

    if (second->subject.offset - first->subject.end < 0) {
        Int4 overlap = first->subject.end - second->subject.offset;
        ASSERT(overlap >= 0);

        if (second->subject.end - second->subject.offset > overlap) {

            s_TrimHSP(second, overlap, FALSE, TRUE, -4, -4, -4);
        }
        else {
            s_TrimHSP(first, overlap, FALSE, FALSE, -4, -4, -4);
        }
                              
        ASSERT(first->subject.end == second->subject.offset);
#if _DEBUG
        s_TestHSPRanges(second);
#endif
    }

    ASSERT(first->query.end <= second->query.offset);
    ASSERT(first->subject.end <= second->subject.offset);

    return 0;
}


/* Search for splice signals between two HSPs in a chain. The HSPs in the
   chain must be sorted by query position in asceding order.
*/
static Int4
s_FindSpliceJunctions(HSPChain* chains,
                      const BLAST_SequenceBlk* query_blk,
                      const BlastQueryInfo* query_info,
                      const ScoringOptions* scoring_opts)
{
    HSPChain* ch = NULL;

    if (!chains || !query_blk || !query_info || !scoring_opts) {
        return -1;
    }

    /* iterate over HSPs in the chain */
    for (ch = chains; ch; ch = ch->next) {
        HSPContainer* h = ch->hsps;
        Uint1* query = NULL;
        Int4 context;
        Int4 query_len;
        ASSERT(h);

        context = h->hsp->context;
        query = query_blk->sequence +
            query_info->contexts[context].query_offset;
        query_len = query_info->contexts[context].query_length;

        while (h->next) {
            HSPContainer* next = h->next;
            ASSERT(next);

            /* if not a spliced alignment, try merging HSPs into one */
            /* Introns are typically at least 30 bases long, and there can be 
               a few unalined query bases. */
            if ((next->hsp->subject.offset - h->hsp->subject.end -
                 (next->hsp->query.offset - h->hsp->query.end) < 30) &&

                /* this condition is needed to align unaligned query bases */
                next->hsp->subject.offset - h->hsp->subject.end <
                h->hsp->map_info->subject_overhangs->right_len) {

                /* save pointer to hsps after next */
                HSPContainer* following = h->next->next;
                BlastHSP* new_hsp = NULL;

                /* duplicate HSPContainer with the two HSPs */
                h->next->next = NULL;

                s_TrimOverlap(h->hsp, next->hsp);

                /* extend the first HSP to cover the gap between HSPs */
                if (next->hsp->query.offset - h->hsp->query.end > 1) {
                    BlastHSP* first = h->hsp;
                    BlastHSP* second = h->next->hsp;

                    s_ExtendAlignment(first, query, first->query.end,
                              second->query.offset - 1, 0,
                              second->subject.offset - 1 - first->subject.end,
                              scoring_opts, FALSE);

#if _DEBUG
                    s_TestHSPRanges(first);
#endif
                }


                /* merge HSPs */
                new_hsp = s_MergeHSPs(h->hsp, h->next->hsp, query,
                                      scoring_opts);


#if _DEBUG
                    s_TestHSPRanges(new_hsp);
#endif


                if (new_hsp) {

                    /* replace the two processed HSPs with the combined one */
                    Blast_HSPFree(h->hsp);
                    HSPContainerFree(h->next);
                    h->hsp = new_hsp;
                    h->next = following;
                }
                else {
                    /* something went wrong with merging, use the initial
                       HSPs */
                    h->next->next = following;
                    h = h->next;

                    ASSERT(!h->next ||
                           (h->hsp->query.end <= h->next->hsp->query.offset &&
                            h->hsp->subject.end <= h->next->hsp->subject.offset));
                }
            }
            /* process overlap if found */
            else if (next->hsp->query.offset <= h->hsp->query.end &&
                     next->hsp->query.offset > h->hsp->query.offset) {

                Boolean consensus_only = TRUE;
                if (h->hsp->score > 50 && next->hsp->score > 50) {
                    consensus_only = FALSE;
                }

                s_FindSpliceJunctionsForOverlaps(h->hsp, next->hsp, query,
                                                 query_len, consensus_only);

                if ((h->hsp->map_info->right_edge & MAPPER_SPLICE_SIGNAL) == 0) {

                    s_TrimOverlap(h->hsp, next->hsp);
                }


#if _DEBUG
                    s_TestHSPRanges(h->hsp);
#endif


                h = h->next;
            }
            else if (next->hsp->query.offset - h->hsp->query.end > 0 &&
                     next->hsp->query.offset - h->hsp->query.end <
                     h->hsp->map_info->subject_overhangs->right_len) {

                s_FindSpliceJunctionsForGap(h->hsp, next->hsp, query,
                                            query_len, scoring_opts);

                if ((h->hsp->map_info->right_edge & MAPPER_SPLICE_SIGNAL) == 0) {

                    s_TrimOverlap(h->hsp, next->hsp);
                }


#if _DEBUG
                    s_TestHSPRanges(h->hsp);
#endif

                h = h->next;
            }
            else {

                s_TrimOverlap(h->hsp, next->hsp);

#if _DEBUG
                    s_TestHSPRanges(h->hsp);
#endif

                h = h->next;
            }
        }

        /* recalculated chain score */
        ch->score = s_ComputeChainScore(ch, scoring_opts, query_len, TRUE);
    }

    s_TestChains(chains);

    return 0;
}


/* Find the best scoring chain of HSPs for aligning a single RNA-Seq read to
   a genome on a single strand, returns all top scoring chains */
static HSPChain* s_FindBestPath(HSPNode* nodes, Int4 num, HSPPath* path,
                                Int4 oid, Boolean is_spliced,
                                Int4 longest_intron,
                                Int4 cutoff_score,
                                const BLAST_SequenceBlk* query_blk,
                                const BlastQueryInfo* query_info,
                                const ScoringOptions* scoring_opts)
{
    int i, k;
    Int4 best_score = 0;
    const Int4 kMaxIntronLength = longest_intron;
    /* FIXME: use mismatch and/or gap extend penalty here */
    HSPChain* retval = NULL;

    if (!path || !nodes || !num) {
        return NULL;
    }
    path->num_paths = 0;
    path->score = 0;

    for (i = num - 1;i >= 0;i--) {

        BlastHSP* hsp = *(nodes[i].hsp);
        int self_score = nodes[i].best_score;
        /* FIXME: the is_spliced condition is a hack */
        for (k = i + 1;k < num && is_spliced;k++) {

            BlastHSP* newhsp = *(nodes[k].hsp);
            Int4 overlap_cost = s_GetOverlapCost(newhsp, *(nodes[i].hsp), 4);
            Int4 new_score = nodes[k].best_score + self_score - overlap_cost;

            /* FIXME: some of the conditions double others */
            const Int4 hsp_len = hsp->query.end - hsp->query.offset;
            const Int4 newhsp_len = newhsp->query.end - newhsp->query.offset;
            const Int4 overlap_len = MAX(MIN(hsp->query.end, newhsp->query.end) -
                              MAX(hsp->query.offset, newhsp->query.offset), 0);

            const Int4 subj_overlap_len =
                MAX(MIN(hsp->subject.end, newhsp->subject.end) -
                    MAX(hsp->subject.offset, newhsp->subject.offset), 0);


            /* add next HSP to the chain only if hsp, newhsp aligns to the
               subject behind hsp, and score improves */
            if (newhsp->query.offset > hsp->query.offset &&
                newhsp->query.end > hsp->query.end &&

                newhsp->subject.offset > hsp->subject.offset &&
                newhsp->subject.end > hsp->subject.end &&
                newhsp->subject.offset - hsp->subject.end < kMaxIntronLength &&
                (double)overlap_len / hsp_len < 0.75 &&
                (double)overlap_len / newhsp_len < 0.75 &&
                (double)subj_overlap_len / hsp_len < 0.75 &&
                (double)subj_overlap_len / newhsp_len < 0.75) {

                /* FIXME: this condition may not be necessary */
                if (newhsp->subject.offset - hsp->subject.end > 1) {
                    /* The function that finds splice signals modifies
                       HSPs, so we need to clone HSPs here. */
                    BlastHSP* hsp_copy = Blast_HSPClone(hsp);
                    BlastHSP* newhsp_copy = Blast_HSPClone(newhsp);

                    HSPChain* chain = HSPChainNew(hsp->context);
                    chain->hsps = HSPContainerNew(&hsp_copy);
                    chain->hsps->next = HSPContainerNew(&newhsp_copy);

                    s_FindSpliceJunctions(chain, query_blk, query_info,
                                          scoring_opts);
                    
                    /* update score: add the difference between sum of
                       two HSP scores minus overalp and the new score
                       for the merged HSP */
                    new_score += chain->score + overlap_cost - 
                        (newhsp->score + self_score);

                    chain = HSPChainFree(chain);
                }
                else if (newhsp->query.offset - hsp->query.end == 1) {
                    /* FIXME: this is a temporary hack, we need to increase
                       the score of combining two HSP into one with a
                       mismatch so that it is scored the same as the one with
                       splice signal */
                    new_score++;
                }
                else {
                    new_score = 0;
                }

                if (new_score > nodes[i].best_score) {
                    nodes[i].best_score = new_score;
                    nodes[i].path_next = &(nodes[k]);
                }
            }
        }

        /* we want to return all top scoring hits, so we save all paths with
           top score */
        if (nodes[i].best_score == best_score) {
            if (path->num_paths < MAX_NUM_HSP_PATHS) {
                path->start[path->num_paths++] = &(nodes[i]);
            }
        }
        else if (nodes[i].best_score > best_score) {
            best_score = nodes[i].best_score;
            path->start[0] = &(nodes[i]);
            path->num_paths = 1;
        }
    }
    ASSERT(path->num_paths);
    path->score = path->start[0]->best_score;

    /* Find if any paths share an HSP and remove the one with the larger index.
       Doing this at the end ensures that we find optimal path in terms of
       score and/or HSP positions. */
    for (i = 0;i < path->num_paths - 1;i++) {
        if (!path->start[i]) {
            continue;
        }
        for (k = i + 1;k < path->num_paths;k++) {
            HSPNode* node_i = path->start[i];
            if (!path->start[k]) {
                continue;
            }

            while (node_i) {
                HSPNode* node_k = path->start[k];
                while (node_k) {
                    if (node_i->hsp[0] == node_k->hsp[0]) {
                        path->start[k] = NULL;
                        break;
                    }
                    node_k = node_k->path_next;
                }
                node_i = node_i->path_next;
            }

        }
    }

    ASSERT(path->num_paths > 0);
    ASSERT(path->start[0]);

    /* conver from HSPPath to HSPChain structure */
    if (path->num_paths > 0) {
        HSPChain* ch = NULL;
        HSPContainer* hc = NULL;
        HSPNode* node = path->start[0];

        retval = HSPChainNew((*node->hsp)->context);
        if (!retval) {
            return NULL;
        }
        retval->oid = oid;
        retval->score = path->score;
        retval->hsps = hc = HSPContainerNew(node->hsp);
        node = node->path_next;
        while (node) {
            hc->next = HSPContainerNew(node->hsp);
            if (!hc->next) {
                HSPChainFree(retval);
                return NULL;
            }
            hc = hc->next;
            node = node->path_next;
        }
        ASSERT(retval->hsps);

        ch = retval;
        for (i = 1;i < path->num_paths;i++) {
            node = path->start[i];
            if (!node) {
                continue;
            }

            if (path->score < cutoff_score) {
                continue;
            }

            ch->next = HSPChainNew((*node->hsp)->context);
            ASSERT(ch->next);
            if (!ch->next) {
                HSPChainFree(retval);
                return NULL;
            }
            ch->next->oid = oid;
            ch->next->score = path->score;
            ch->next->hsps = hc = HSPContainerNew(node->hsp);
            node = node->path_next;
            while (node) {
                hc->next = HSPContainerNew(node->hsp);
                ASSERT(hc->next);
                if (!hc->next) {
                    HSPChainFree(retval);
                    return NULL;
                }
                hc = hc->next;
                node = node->path_next;
            }
            ch = ch->next;
            ASSERT(ch->hsps);
        }
    }

    ASSERT(path->num_paths == 0 || s_TestChains(retval));

    return retval;
}

/* Compare HSPs by context (accending), and score (descending). */
static int
s_CompareHSPsByContextScore(const void* v1, const void* v2)
{
   BlastHSP* h1,* h2;
   BlastHSP** hp1,** hp2;

   hp1 = (BlastHSP**) v1;
   hp2 = (BlastHSP**) v2;
   h1 = *hp1;
   h2 = *hp2;

   if (!h1 && !h2)
      return 0;
   else if (!h1)
      return -1;
   else if (!h2)
      return 1;

   if (h1->context < h2->context)
      return -1;
   if (h1->context > h2->context)
      return 1;

   if (h1->score < h2->score)
      return 1;
   if (h1->score > h2->score)
      return -1;

   return 0;
}

static int
s_CompareHSPsByContextSubjectOffset(const void* v1, const void* v2)
{
   BlastHSP* h1,* h2;
   BlastHSP** hp1,** hp2;

   hp1 = (BlastHSP**) v1;
   hp2 = (BlastHSP**) v2;
   h1 = *hp1;
   h2 = *hp2;

   if (!h1 && !h2)
      return 0;
   else if (!h1)
      return 1;
   else if (!h2)
      return -1;

   /* If these are from different contexts, don't compare offsets */
   if (h1->context < h2->context)
      return -1;
   if (h1->context > h2->context)
      return 1;

   if (h1->subject.offset < h2->subject.offset)
      return -1;
   if (h1->subject.offset > h2->subject.offset)
      return 1;

   if (h1->query.offset < h2->query.offset)
      return -1;
   if (h1->query.offset > h2->query.offset)
      return 1;

   /* tie breakers: sort by decreasing score, then
      by increasing size of query range, then by
      increasing subject range. */

   if (h1->score < h2->score)
      return 1;
   if (h1->score > h2->score)
      return -1;

   if (h1->query.end < h2->query.end)
      return 1;
   if (h1->query.end > h2->query.end)
      return -1;

   if (h1->subject.end < h2->subject.end)
      return 1;
   if (h1->subject.end > h2->subject.end)
      return -1;

   return 0;
}


/* Pair data, used for selecting optimal pairs of aligned chains */
typedef struct Pairinfo
{
    HSPChain* first;
    HSPChain* second;
    Int4 distance;
    Int4 conf;
    Int4 score;
    Int4 trim_first;    // when a chain overextends past beginning of the pair
    Int4 trim_second;   // it needs to be trimmed
    Boolean valid_pair;
} Pairinfo;


static void s_BlastHSPMapperSplicedRunCleanUp(HSPPath* path,
                                              HSPNode* nodes,
                                              Pairinfo* pair_data)
{
    HSPPathFree(path);

    if (nodes) {
        sfree(nodes);
    }

    if (pair_data) {
        sfree(pair_data);
    }
}


/* Compare two HSP chain pairs aligned for the same query and subject
   by score and distance */
static int
s_ComparePairs(const void* v1, const void* v2)
{
    Pairinfo* a = (Pairinfo*)v1;
    Pairinfo* b = (Pairinfo*)v2;

    if (a->score > b->score) {
        return -1;
    }
    if (a->score < b->score) {
        return 1;
    }

    if (a->conf < b->conf) {
        return -1;
    }
    if (a->conf > b->conf) {
        return 1;
    }

    if (a->distance < b->distance) {
        return -1;
    }
    if (a->distance > b->distance) {
        return 1;
    }

    return 0;
}


/* Trim beginning of a chain alignment up to the given subject position */
static Int4 s_TrimChainStartToSubjPos(HSPChain* chain, Int4 subj_pos,
                                      Int4 mismatch_score,
                                      Int4 gap_open_score,
                                      Int4 gap_extend_score)
{
    HSPContainer* h = NULL;
    BlastHSP* hsp = NULL;
    BlastHSP* next_hsp = NULL;
    Int4 num_left;
    Int4 old_score;

    ASSERT(chain);
    ASSERT(subj_pos >= 0);
    if (!chain || subj_pos < 0) {
        return 0;
    }

    /* first remove HSPs that are fully below the subject position */
    h = chain->hsps;
    while (h && h->hsp->subject.end < subj_pos) {
        HSPContainer* tmp = h;
        ASSERT(tmp);
        h = h->next;
        tmp->next = NULL;
        chain->score -= tmp->hsp->score;
        HSPContainerFree(tmp);
    }
    ASSERT(h);
    chain->hsps = h;
    if (!h) {
        return -1;
    }

    /* if all remaning HSPs are above the subject position, then exit */
    if (h->hsp->subject.offset >= subj_pos) {
        return 0;
    }

    /* otherwise move HSP start positions */
    hsp = h->hsp;
    num_left = subj_pos - hsp->subject.offset;
    ASSERT(num_left > 0 && num_left <= hsp->subject.end - hsp->subject.offset);

    old_score = hsp->score;
    s_TrimHSP(hsp, num_left, FALSE, TRUE, mismatch_score, gap_open_score,
              gap_extend_score);

    /* update chain score */
    chain->score -= old_score - hsp->score;

    /* remove splice signals and exon flags */
    hsp->map_info->left_edge &= 0xff ^ MAPPER_SPLICE_SIGNAL;
    hsp->map_info->left_edge &= 0xff ^ MAPPER_EXON;

    /* check whether the HSP is contained within the next one and remove it
       if it is */
    next_hsp = NULL;
    if (chain->hsps->next) {
        next_hsp = chain->hsps->next->hsp;
    }
    if (next_hsp && hsp->query.offset >= next_hsp->query.offset) {
        chain->hsps = h->next;
        h->next = NULL;
        HSPContainerFree(h);
    }

    return 0;
}

/* Trim chain alignment end up the the given subject position */
static Int4 s_TrimChainEndToSubjPos(HSPChain* chain, Int4 subj_pos,
                                    Int4 mismatch_score,
                                    Int4 gap_open_score, Int4 gap_extend_score)
{
    HSPContainer* h = NULL;
    BlastHSP* hsp = NULL;
    Int4 num_left;
    Int4 old_score;

    ASSERT(chain);
    ASSERT(subj_pos >= 0);
    if (!chain || subj_pos <= 0) {
        return -1;
    }

    /* first remove HSPs that are full above the subject position */
    h = chain->hsps;
    while (h->next && h->next->hsp->subject.end < subj_pos) {
        h = h->next;
    }

    if (h->next) {
        HSPContainer* hc = NULL;
        if (h->next->hsp->subject.offset < subj_pos) {
            h = h->next;
        }

        /* update chain score */
        for (hc = h->next;hc != NULL;hc = hc->next) {
            chain->score -= hc->hsp->score;
        }

        HSPContainerFree(h->next);
        h->next = NULL;
    }

    /* if all remaning HSPs are below the subject position, exit */
    if (h->hsp->subject.end <= subj_pos) {
        return 0;
    }

    /* otheriwse move HSP end positions */
    hsp = h->hsp;
    num_left = hsp->subject.end - subj_pos;
    ASSERT(num_left > 0 && num_left <= hsp->subject.end - hsp->subject.offset);

    old_score = hsp->score;
    s_TrimHSP(hsp, num_left, FALSE, FALSE, mismatch_score, gap_open_score,
              gap_extend_score);

    /* update chain score */
    chain->score -= old_score - hsp->score;

    /* remove splice signals and exon flags */
    hsp->map_info->right_edge &= 0xff ^ MAPPER_SPLICE_SIGNAL;
    hsp->map_info->right_edge &= 0xff ^ MAPPER_EXON;

    /* check whether the HSP is contained within the next one and remove it
       if it is */
    if (chain->hsps != h) {
        HSPContainer* hc = chain->hsps;
        while (hc && hc->next != h) {
            hc = hc->next;
        }
        ASSERT(hc && hc->next == h);
        if (hc->hsp->query.end >= h->hsp->query.end) {
            chain->score -= h->hsp->score;
            HSPContainerFree(h);
            hc->next = NULL;
        }
    }

    return 0;
}


/* Find optimal pairs */
static Boolean s_FindBestPairs(HSPChain** first_list,
                               HSPChain** second_list,
                               Int4 min_score,
                               Pairinfo** pair_info_ptr,
                               Int4* max_num_pairs,
                               Boolean is_spliced,
                               const ScoringOptions* scoring_options)
{
    HSPChain* first;
    HSPChain* second;
    Pairinfo* pair_info = *pair_info_ptr;
    Int4 conv_bonus = 0;
    Int4 num_pairs = 0;
    Boolean found = FALSE;
    const Int4 kMaxInsertSize = is_spliced ?
        MAGICBLAST_MAX_INSERT_SIZE_SPLICED :
        MAGICBLAST_MAX_INSERT_SIZE_NONSPLICED;

    /* iterate over all pairs of HSP chains for the first and second read of
       the pair and collect pair information */
    for (first = *first_list; first; first = first->next) {
        for (second = *second_list; second; second = second->next) {
            Int2 first_frame = first->hsps->hsp->query.frame;
            Int2 second_frame = second->hsps->hsp->query.frame;

            /* reallocate pair info array if necessary */
            if (num_pairs >= *max_num_pairs) {
                *max_num_pairs *= 2;
                Pairinfo* new_pair_info =
                    realloc(pair_info, *max_num_pairs * sizeof(Pairinfo));
                if (!new_pair_info) {
                    return -1;
                }
                pair_info = new_pair_info;
                *pair_info_ptr = new_pair_info;
            }

            /* collect pair information */
            pair_info[num_pairs].first = first;
            pair_info[num_pairs].second = second;
            pair_info[num_pairs].score = first->score + second->score;
            pair_info[num_pairs].trim_first = 0;
            pair_info[num_pairs].trim_second = 0;
            pair_info[num_pairs].valid_pair = 0;
            pair_info[num_pairs].distance = 0;

            /* if the chains align on the opposite strands */
            ASSERT(first_frame != 0 && second_frame != 0);
            if (SIGN(first_frame) != SIGN(second_frame)) {
                HSPChain* plus = NULL, *minus = NULL;
                Int4 plus_start, minus_start;
                HSPContainer* hsp = NULL;
                Int4 distance = 0;

                /* find which query aligns to plus and which to minus strand
                   on the subject */
                if (first_frame > 0) {
                    plus = first;
                }
                else {
                    minus = first;
                }
                if (second_frame > 0) {
                    plus = second;
                }
                else {
                    minus = second;
                }
                ASSERT(plus && minus);

                /* find start positions on the subject, for the minus strand we
                   are interested subject start position on the minus strand,
                   but the HSP has subject on plus and query on the minus
                   strand, so we need to flip the query */
                plus_start = plus->hsps->hsp->subject.offset;
                hsp = minus->hsps;
                while (hsp->next) {
                    hsp = hsp->next;
                }
                ASSERT(hsp);
                minus_start = hsp->hsp->subject.end;

                /* this is also fragment length */
                distance = minus_start - plus_start;
                pair_info[num_pairs].distance = distance;

                /* distance > 0 indicates a convergent pair (typical) */
                if (distance > 0 && distance < kMaxInsertSize) {
                    Int4 plus_end, minus_end;
                    hsp = plus->hsps;
                    while (hsp->next) {
                        hsp = hsp->next;
                    }
                    ASSERT(hsp);
                    plus_end = hsp->hsp->subject.end;
                    minus_end = minus->hsps->hsp->subject.offset;

                    /* HSP chain end going past the beginning of the chain
                       for the pair, indicates that the this end of the
                       query may be aligned to an adapater instead of a real
                       sequence and needs to be trimmed */
                    if (plus_end > minus_start || minus_end < plus_start) {
                        ASSERT(plus && minus);

                        /* check the 3' (right) side of the fragment */
                        if (plus_end > minus_start) {
                            ASSERT(plus_end - minus_start > 0);

                            /* decrease the score by the trimmed portion; we
                               assume here that the trimmed part aligns
                               perfectly, which may not be the case, but this
                               score is only used for comparison with different
                               pairs */
                            pair_info[num_pairs].score -=
                                plus_end - minus_start;

                            /* Mark which chain and up to what subject location
                               needs to be trimmed. We cannot trim here,
                               because pair_info holds poiners to chains that
                               may be part of better pairs */
                            if (plus == pair_info[num_pairs].first) {
                                pair_info[num_pairs].trim_first = minus_start;
                            }
                            else {
                                pair_info[num_pairs].trim_second = minus_start;
                            }
                        }

                        /* check the 5' (left) side doe the fragment */
                        if (minus_end < plus_start) {
                            ASSERT(plus_start - minus_end > 0);

                            pair_info[num_pairs].score -=
                                plus_start - minus_end;

                            if (minus == pair_info[num_pairs].first) {
                                pair_info[num_pairs].trim_first = plus_start;
                            }
                            else {
                                pair_info[num_pairs].trim_second = plus_start;
                            }
                        }
                    }

                    pair_info[num_pairs].conf = PAIR_CONVERGENT;

                    /* add a bonus for convergent pairs; because they are more
                       common we prefer them over different configurations
                       with a sligntly better score */
                    pair_info[num_pairs].score += conv_bonus;
                }
                else {
                    pair_info[num_pairs].conf = PAIR_DIVERGENT;
                }

                /* skip pairs with a score smaller than one found for a
                   different subject */
                if (pair_info[num_pairs].score < min_score) {
                    continue;
                }

                num_pairs++;
            }
            else {
                /* for read pairs aligned in the same direction */
                pair_info[num_pairs].conf = PAIR_PARALLEL;
                pair_info[num_pairs].score -= 1;
                num_pairs++;
            }
        }
    }

    if (num_pairs > 0) {
        Int4 i;
        Int4 best_score = pair_info[0].score;
        Int4 margin = 5;
        HSPChain* ch = NULL;
        Boolean convergent_found = FALSE;

        /* sort pair information by score, configuration distance */
        qsort(pair_info, num_pairs, sizeof(Pairinfo), s_ComparePairs);

        /* iterate over pair information and collect pairs */
        for (i=0;i < num_pairs;i++) {
            HSPChain* f = pair_info[i].first;
            HSPChain* s = pair_info[i].second;

            /* skip pairs with chains already used in other pairs
               and other configurations when convergent one was found */
            if (pair_info[i].first->pair || pair_info[i].second->pair ||
                (convergent_found && pair_info[i].conf != PAIR_CONVERGENT)) {

                continue;
            }

            if (pair_info[i].conf == PAIR_CONVERGENT) {
                convergent_found = TRUE;
            }

            /* skip pairs with scores significantly smaller than the best one
               found */
            if (best_score - pair_info[i].score > margin) {
                break;
            }

            /* link chains as a pair */
            f->pair = s;
            s->pair = f;
            f->pair_conf = s->pair_conf = pair_info[i].conf;
            pair_info[i].valid_pair = TRUE;
            found = TRUE;
        }

        /* for chains that pair with already paired chains, clone the paired
           chain */
        for (ch = *first_list; ch; ch = ch->next) {
            HSPChain* pair = NULL;

            if (ch->pair) {
                continue;
            }

            i = 0;
            while (i < num_pairs &&
                   (pair_info[i].first != ch || !pair_info[i].second->pair ||
                    pair_info[i].conf != PAIR_CONVERGENT ||
                    best_score - pair_info[i].score > margin)) {
                i++;
            }
            if (i >= num_pairs) {
                continue;
            }
            ASSERT(pair_info[i].second->pair);

            pair = CloneChain(pair_info[i].second);
            ASSERT(pair);
            ASSERT(s_TestChains(pair));
            pair_info[i].second = pair;
            ch->pair = pair;
            pair->pair = ch;
            ch->pair_conf = pair->pair_conf = pair_info[i].conf;
            pair_info[i].valid_pair = TRUE;
            HSPChainListInsert(second_list, &pair, 0, -1, FALSE);
        }

        for (ch = *second_list; ch; ch = ch->next) {
            HSPChain* pair = NULL;

            if (ch->pair) {
                continue;
            }

            i = 0;
            while (i < num_pairs &&
                   (pair_info[i].second != ch || !pair_info[i].first->pair ||
                    pair_info[i].conf != PAIR_CONVERGENT ||
                    best_score - pair_info[i].score > margin)) {
                i++;
            }
            if (i >= num_pairs) {
                continue;
            }
            ASSERT(pair_info[i].first->pair);

            pair = CloneChain(pair_info[i].first);
            ASSERT(pair);
            ASSERT(s_TestChains(pair));
            pair_info[i].first = pair;
            ch->pair = pair;
            pair->pair = ch;
            ch->pair_conf = pair->pair_conf = pair_info[i].conf;
            pair_info[i].valid_pair = TRUE;
            HSPChainListInsert(first_list, &pair, 0, -1, FALSE);
        }

        /* trim the alignments that overextend beyond the pair */
        for (i = 0;i < num_pairs;i++) {
            if (!pair_info[i].valid_pair ||
                !pair_info[i].first->pair || !pair_info[i].second->pair) {
                continue;
            }

            /* trim the first chain if its end goes beyond the start of
               second on the subject */
            if (pair_info[i].trim_first > 0) {
                if (pair_info[i].first->hsps->hsp->query.frame > 0) {
                    s_TrimChainEndToSubjPos(pair_info[i].first,
                                            pair_info[i].trim_first,
                                            scoring_options->penalty,
                                            scoring_options->gap_open,
                                            scoring_options->gap_extend);
                }
                else {
                    /* minus strand of the qurey is represented by another
                       (reverse complemented) sequence aligned like the plus
                       strand, so the end of the chain is really the beginnig
                       by query coordinates */
                    s_TrimChainStartToSubjPos(pair_info[i].first,
                                              pair_info[i].trim_first,
                                              scoring_options->penalty,
                                              scoring_options->gap_open,
                                              scoring_options->gap_extend);
                }
            }

            /* trim the second chain if its end goes beyond the start of the
               first on the subject */
            if (pair_info[i].trim_second > 0) {
                if (pair_info[i].second->hsps->hsp->query.frame > 0) {
                    s_TrimChainEndToSubjPos(pair_info[i].second,
                                            pair_info[i].trim_second,
                                            scoring_options->penalty,
                                            scoring_options->gap_open,
                                            scoring_options->gap_extend);
                }
                else {
                    s_TrimChainStartToSubjPos(pair_info[i].second,
                                              pair_info[i].trim_second,
                                              scoring_options->penalty,
                                              scoring_options->gap_open,
                                              scoring_options->gap_extend);
                }
            }

        }
    }

    return found;
}


HSPChain* FindPartialyCoveredQueries(void* data, Int4 oid, Int4 word_size)
{
    BlastHSPMapperData* spl_data = data;
    BlastQueryInfo* query_info = spl_data->query_info;
    HSPChain** saved = spl_data->saved_chains;
    HSPChain* retval = NULL;
    HSPChain* last;
    Int4 i;

    for (i = 0;i < query_info->num_queries;i++) {
        HSPChain* chain = saved[i];

        for (chain = saved[i]; chain; chain = chain->next) {
            HSPContainer* h;

            if (chain->oid != oid) {
                continue;
            }

            if (chain->score < 30) {
                continue;
            }

            h = chain->hsps;

            if (h->hsp->query.offset > word_size) {

                if (!retval) {
                    retval = CloneChain(chain);
                    last = retval;
                }
                else {
                    last->next = CloneChain(chain);
                    last = last->next;
                }

                continue;
            }

            while (h->next) {
                h = h->next;
            }
            if (query_info->contexts[h->hsp->context].query_length -
                h->hsp->query.end > word_size) {

                if (!retval) {
                    retval = CloneChain(chain);
                    last = retval;
                }
                else {
                    last->next = CloneChain(chain);
                    last = last->next;
                }
            }
        }
    }

    return retval;
}


/** Perform writing task for paired reads
 * ownership of the HSP list and sets the dereferenced pointer to NULL.
 * @param data To store results to [in][out]
 * @param hsp_list Pointer to the HSP list to save in the collector. [in]
 */
static int
s_BlastHSPMapperSplicedPairedRun(void* data, BlastHSPList* hsp_list)
{
    BlastHSPMapperData* spl_data = data;
    BlastHSPMapperParams* params = spl_data->params;
    const ScoringOptions* scoring_opts = &params->scoring_options;
    Boolean is_spliced  = params->splice;
    const Int4 kLongestIntron = params->longest_intron;
    Int4 cutoff_score = params->cutoff_score;
    Int4* cutoff_score_fun = params->cutoff_score_fun;
    Int4 cutoff_edit_dist = params->cutoff_edit_dist;
    BLAST_SequenceBlk* query_blk = spl_data->query;
    BlastQueryInfo* query_info = spl_data->query_info;
    const Int4 kDefaultMaxHsps = 1000;
    Int4 max_hsps = kDefaultMaxHsps;
    HSPNode* nodes = calloc(max_hsps, sizeof(HSPNode));
    HSPPath* path = NULL;
    BlastHSP** hsp_array;
    Int4 num_hsps = 0;
    Int4 i;

    HSPChain** saved_chains = spl_data->saved_chains;
    HSPChain* chain_array[2];
    HSPChain* first = NULL, *second = NULL;
    Int4 workspace_size = 40;
    Pairinfo* pair_workspace = calloc(workspace_size, sizeof(Pairinfo));
    /* the score bonus needs to include exon pars that were to short to be
       aligned (at either end of the chain) */
    const Int4 kPairBonus = is_spliced ? 21 : 5;

    if (!hsp_list) {
        s_BlastHSPMapperSplicedRunCleanUp(path, nodes, pair_workspace);
        return 0;
    }

    if (!params || !nodes) {
        s_BlastHSPMapperSplicedRunCleanUp(path, nodes, pair_workspace);
        return -1;
    }

    hsp_array = hsp_list->hsp_array;

    path = HSPPathNew();

    /* sort HSPs by query context and subject position for spliced aligments */
    if (is_spliced) {
        qsort(hsp_array, hsp_list->hspcnt, sizeof(BlastHSP*),
              s_CompareHSPsByContextSubjectOffset);
    }
    else {
        /* for non spliced alignments, sort HSPs by query context and score */
        qsort(hsp_array, hsp_list->hspcnt, sizeof(BlastHSP*),
              s_CompareHSPsByContextScore);
    }

    i = 0;
    chain_array[0] = NULL;
    chain_array[1] = NULL;
    while (i < hsp_list->hspcnt) {

        Int4 context = hsp_array[i]->context;
        /* is this query the first segment of a pair */
        Boolean has_pair = (query_info->contexts[context].segment_flags ==
                            eFirstSegment);
        Int4 query_idx  = context / NUM_STRANDS;
        Int4 first_context = query_idx * NUM_STRANDS;
        Int4 context_next_fragment = has_pair ? (query_idx + 2) * NUM_STRANDS :
            (query_idx + 1) * NUM_STRANDS;

        HSPChain* new_chains = NULL;

        /* iterate over contexts that belong to a read pair */
        while (i < hsp_list->hspcnt &&
               hsp_array[i]->context < context_next_fragment) {

            memset(nodes, 0, num_hsps * sizeof(HSPNode));
            num_hsps = 0;
            context = hsp_array[i]->context;

            /* collect HSPs for the current context */
            while (i < hsp_list->hspcnt && hsp_array[i]->context == context) {

                /* Overlapping portions of two consecutive subject chunks
                   may produce two HSPs representing the same alignment. We
                   drop one of them here (assuming the HSPs for the same
                   context are sorted by score or subject position). */
                BlastHSP* h = (num_hsps == 0 ? NULL : *nodes[num_hsps - 1].hsp);
                if (!h || hsp_array[i]->context != h->context ||
                    hsp_array[i]->subject.offset != h->subject.offset ||
                    hsp_array[i]->score != h->score) {

                    /* reallocate node array if necessary */
                    if (num_hsps >= max_hsps) {
                        HSPNode* new_nodes = NULL;

                        /* For non-spliced mapping we are only interested in
                           the best scoring whole HSPs. HSPs are sorted by
                           score and those that map to many places are not good
                           so the node array does not need to be extended. */
                        if (!is_spliced) {
                            break;
                        }

                        max_hsps *= 2;
                        new_nodes = calloc(max_hsps, sizeof(HSPNode));
                        if (!nodes) {
                            s_BlastHSPMapperSplicedRunCleanUp(path, nodes,
                                                              pair_workspace);
                            return -1;
                        }
                        s_HSPNodeArrayCopy(new_nodes, nodes, num_hsps);
                        free(nodes);
                        nodes = new_nodes;
                    }

                    nodes[num_hsps].hsp = &(hsp_array[i]);
                    nodes[num_hsps].best_score = hsp_array[i]->score;

                    num_hsps++;
                }
                i++;
            }

            /* find the best scoring chain of HSPs that align to exons
               parts of the query and the subject */
            new_chains = s_FindBestPath(nodes, num_hsps, path,
                                        hsp_list->oid, is_spliced,
                                        kLongestIntron, cutoff_score,
                                        query_blk, query_info, scoring_opts);

            ASSERT(new_chains);
            HSPChainListInsert(&chain_array[(context - first_context) /
                                            NUM_STRANDS],
                               &new_chains, cutoff_score, cutoff_edit_dist,
                               FALSE);

            /* skip HSPs for the same context if there are more then max
               allowed per context */
            while (i < hsp_list->hspcnt && hsp_array[i]->context == context) {
                i++;
            }
        }

#if _DEBUG
        ASSERT(!chain_array[0] || s_TestChainsSorted(chain_array[0]));
        ASSERT(!chain_array[1] || s_TestChainsSorted(chain_array[1]));
#endif

        if (is_spliced && chain_array[0]) {
            s_FindSpliceJunctions(chain_array[0], query_blk, query_info,
                                  scoring_opts);
        }

        if (is_spliced && chain_array[1]) {
            s_FindSpliceJunctions(chain_array[1], query_blk, query_info,
                                  scoring_opts);
        }

        first = chain_array[0];
        second = chain_array[1];

        /* If the chains provide sufficient score check for pairs.
           The score bonus is added to prefer a pair with a slightly smaller
           score to single chains with a better one. */
        if (first && second) {

            s_FindBestPairs(&first, &second, 0, &pair_workspace,
                            &workspace_size, is_spliced, scoring_opts);

            ASSERT(s_TestChains(first));
            ASSERT(s_TestChains(second));
        }

        /* find cutoff score for the query */
        if (cutoff_score_fun[1] != 0) {
            Int4 query_len =
                query_info->contexts[query_idx * NUM_STRANDS].query_length;
            cutoff_score = (cutoff_score_fun[0] +
                            cutoff_score_fun[1] * query_len) / 100;
        }
        else if (params->cutoff_score == 0) {
            cutoff_score = GetCutoffScore(
                  query_info->contexts[query_idx * NUM_STRANDS].query_length);
        }

        /* save all chains and remove ones with scores lower than
           best score - kPairBonus */
        if (first) {
            HSPChainListInsert(&saved_chains[query_idx], &first, cutoff_score,
                               cutoff_edit_dist, TRUE);
            HSPChainListTrim(saved_chains[query_idx], kPairBonus);


#if _DEBUG
            ASSERT(!saved_chains[query_idx] ||
                   s_TestChainsSorted(saved_chains[query_idx]));
#endif
        }
        if (second) {
            HSPChainListInsert(&saved_chains[query_idx + 1], &second,
                               cutoff_score, cutoff_edit_dist, TRUE);
            HSPChainListTrim(saved_chains[query_idx + 1], kPairBonus);


#if _DEBUG
            ASSERT(!saved_chains[query_idx + 1] ||
                   s_TestChainsSorted(saved_chains[query_idx + 1]));
#endif
        }

        /* make temporary lists empty */
        chain_array[0] = chain_array[1] = NULL;
    }

    /* delete HSPs that were not saved in results */
    hsp_list = Blast_HSPListFree(hsp_list);

    s_BlastHSPMapperSplicedRunCleanUp(path, nodes, pair_workspace);

    return 0;
}


/** Free the writer
 * @param writer The writer to free [in]
 * @return NULL.
 */
static
BlastHSPWriter*
s_BlastHSPMapperFree(BlastHSPWriter* writer)
{
   BlastHSPMapperData *data = writer->data;
   sfree(data->params);
   sfree(writer->data);
   sfree(writer);
   return NULL;
}

/** create the writer
 * @param params Pointer to the hit paramters [in]
 * @param query_info BlastQueryInfo (not used) [in]
 * @return writer
 */
static
BlastHSPWriter*
s_BlastHSPMapperPairedNew(void* params, BlastQueryInfo* query_info,
                          BLAST_SequenceBlk* query)
{
   BlastHSPWriter* writer = NULL;
   BlastHSPMapperData* data = NULL;
/*   BlastHSPMapperParams * spl_param = params; */

   /* allocate space for writer */
   writer = malloc(sizeof(BlastHSPWriter));

   /* fill up the function pointers */
   writer->InitFnPtr   = &s_BlastHSPMapperPairedInit;
   writer->FinalFnPtr  = &s_BlastHSPMapperFinal;
   writer->FreeFnPtr   = &s_BlastHSPMapperFree;
   writer->RunFnPtr    = &s_BlastHSPMapperSplicedPairedRun;

   /* allocate for data structure */
   writer->data = calloc(1, sizeof(BlastHSPMapperData));
   data = writer->data;
   data->params = params;
   data->query_info = query_info;
   data->query = query;

   return writer;
}


/*************************************************************/
/** The following are exported functions to be used by APP   */

BlastHSPMapperParams*
BlastHSPMapperParamsNew(const BlastHitSavingOptions* hit_options,
                        const BlastScoringOptions* scoring_options)
{
       BlastHSPMapperParams* retval = NULL;

       if (hit_options == NULL)
           return NULL;

       retval = (BlastHSPMapperParams*)malloc(sizeof(BlastHSPMapperParams));
       retval->hitlist_size = MAX(hit_options->hitlist_size, 10);
       retval->paired = hit_options->paired;
       retval->splice = hit_options->splice;
       retval->longest_intron = hit_options->longest_intron;
       retval->cutoff_score = hit_options->cutoff_score;
       retval->cutoff_score_fun[0] = hit_options->cutoff_score_fun[0];
       retval->cutoff_score_fun[1] = hit_options->cutoff_score_fun[1];
       retval->cutoff_edit_dist = hit_options->max_edit_distance;
       retval->program = hit_options->program_number;
       retval->scoring_options.reward = scoring_options->reward;
       retval->scoring_options.penalty = scoring_options->penalty;
       retval->scoring_options.gap_open = -scoring_options->gap_open;
       retval->scoring_options.gap_extend = -scoring_options->gap_extend;
       retval->scoring_options.no_splice_signal = -2;
       return retval;
}

BlastHSPMapperParams*
BlastHSPMapperParamsFree(BlastHSPMapperParams* opts)
{
    if ( !opts )
        return NULL;
    sfree(opts);
    return NULL;
}

BlastHSPWriterInfo*
BlastHSPMapperInfoNew(BlastHSPMapperParams* params) {
   BlastHSPWriterInfo * writer_info =
                        malloc(sizeof(BlastHSPWriterInfo));

   writer_info->NewFnPtr = &s_BlastHSPMapperPairedNew;

   writer_info->params = params;
   return writer_info;
}
