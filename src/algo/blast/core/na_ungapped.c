/* $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
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
 */

/** @file na_ungapped.c
 * Nucleotide ungapped extension routines
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif                          /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/na_ungapped.h>
#include <algo/blast/core/blast_nalookup.h>
#include <algo/blast/core/blast_nascan.h>
#include <algo/blast/core/mb_indexed_lookup.h>
#include <algo/blast/core/blast_util.h> /* for NCBI2NA_UNPACK_BASE macros */

#include "index_ungapped.h"
#include "masksubj.inl"

/** Check to see if an index->q_pos pair exists in MB lookup table 
 * @param lookup_wrap The lookup table wrap structure [in]
 * @param index The index to the lookup table [in]
 * @param q_pos The offset of query to be looked up [in]
 * @return TRUE if the pair is found to exist in the lookup table 
 */
static Boolean
s_MBLookup(const LookupTableWrap *lookup_wrap,
           Int4                   index,
           Int4                   q_pos)
{
    BlastMBLookupTable* mb_lt = (BlastMBLookupTable *) lookup_wrap->lut;
    PV_ARRAY_TYPE *pv = mb_lt->pv_array;
    Int4 q_off;

    index &= (mb_lt->hashsize-1);
    ++q_pos;

    if (! PV_TEST(pv, index, mb_lt->pv_array_bts)) {
        return FALSE;
    }

    q_off = mb_lt->hashtable[index];
    while (q_off) {
        if (q_off == q_pos) return TRUE;
        q_off = mb_lt->next_pos[q_off];
    }

    return FALSE;
}

/** Check to see if an index->q_pos pair exists in SmallNa lookup table
 * @param lookup_wrap The lookup table wrap structure [in]
 * @param index The index to the lookup table [in]
 * @param q_pos The offset of query to be looked up [in]
 * @return TRUE if the pair is found to exist in the lookup table 
 */
static Boolean
s_SmallNaLookup(const LookupTableWrap *lookup_wrap,
                Int4                   index,
                Int4                   q_pos)
{
    BlastSmallNaLookupTable* lookup = 
                         (BlastSmallNaLookupTable *) lookup_wrap->lut;
    Int2 *overflow = lookup->overflow;
    Int4 src_off;

    index = lookup->final_backbone[index & lookup->mask];

    if (index == q_pos) return TRUE;
    if (index == -1 || index >= 0) return FALSE;

    src_off = -index;
    index = overflow[src_off++];
    do {
        if (index == q_pos) return TRUE;
        index = overflow[src_off++];
    } while (index >=0);

    return FALSE;
}
        
/** Check to see if an index->q_pos pair exists in Na lookup table 
 * @param lookup_wrap The lookup table wrap structure [in]
 * @param index The index to the lookup table [in]
 * @param q_pos The offset of query to be looked up [in]
 * @return TRUE if the pair is found to exist in the lookup table 
 */
static Boolean
s_NaLookup(const LookupTableWrap *lookup_wrap,
           Int4                   index,
           Int4                   q_pos)
{
    BlastNaLookupTable* lookup = (BlastNaLookupTable *) lookup_wrap->lut;
    PV_ARRAY_TYPE *pv = lookup->pv;
    Int4 num_hits, i;
    Int4 *lookup_pos;

    index &= (lookup->mask);

    if (! PV_TEST(pv, index, PV_ARRAY_BTS)) {
        return FALSE;
    }

    num_hits = lookup->thick_backbone[index].num_used;

    lookup_pos = (num_hits <= NA_HITS_PER_CELL) ?
                 lookup->thick_backbone[index].payload.entries :
                 lookup->overflow + lookup->thick_backbone[index].payload.overflow_cursor;

    for (i=0; i<num_hits; ++i) {
        if (lookup_pos[i] == q_pos) return TRUE;
    }
        
    return FALSE;
}    

/** Perform ungapped extension of a word hit, using a score
 *  matrix and extending one base at a time
 * @param query The query sequence [in]
 * @param subject The subject sequence [in]
 * @param matrix The scoring matrix [in]
 * @param q_off The offset of a word in query [in]
 * @param s_off The offset of a word in subject [in]
 * @param X The drop-off parameter for the ungapped extension [in]
 * @param ungapped_data The ungapped extension information [out]
 */
static void
s_NuclUngappedExtendExact(BLAST_SequenceBlk * query,
                          BLAST_SequenceBlk * subject, Int4 ** matrix,
                          Int4 q_off, Int4 s_off, Int4 X,
                          BlastUngappedData * ungapped_data)
{
    Uint1 *q;
    Int4 sum, score;
    Uint1 ch;
    Uint1 *subject0, *sf, *q_beg, *q_end, *s, *start;
    Int2 remainder, base;
    Int4 q_avail, s_avail;

    base = 3 - (s_off % 4);

    subject0 = subject->sequence;
    q_avail = query->length - q_off;
    s_avail = subject->length - s_off;

    q = q_beg = q_end = query->sequence + q_off;
    s = subject0 + s_off / COMPRESSION_RATIO;
    if (q_off < s_off) {
        start = subject0 + (s_off - q_off) / COMPRESSION_RATIO;
        remainder = 3 - ((s_off - q_off) % COMPRESSION_RATIO);
    } else {
        start = subject0;
        remainder = 3;
    }

    score = 0;
    sum = 0;

/*
   There is a trick in the loop below that you can't see from the code.
   X is negative and when sum becomes more negative than X we break out of
   the loop.  The reason that X is guaranteed to become negative is because
   there is a sentinel at the beginning of the query sequence, so if you hit
   that you get a big negative value.
*/

    /* extend to the left */
    while ((s > start) || (s == start && base < remainder)) {
        if (base == 3) {
            s--;
            base = 0;
        } else {
            ++base;
        }
        ch = *s;
        if ((sum += matrix[*--q][NCBI2NA_UNPACK_BASE(ch, base)]) > 0) {
            q_beg = q;
            score += sum;
            sum = 0;
        } else if (sum < X) {
            break;
        }
    }

    ungapped_data->q_start = q_beg - query->sequence;
    ungapped_data->s_start = s_off - (q_off - ungapped_data->q_start);

    if (q_avail < s_avail) {
        sf = subject0 + (s_off + q_avail) / COMPRESSION_RATIO;
        remainder = 3 - ((s_off + q_avail) % COMPRESSION_RATIO);
    } else {
        sf = subject0 + (subject->length) / COMPRESSION_RATIO;
        remainder = 3 - ((subject->length) % COMPRESSION_RATIO);
    }

    /* extend to the right */
    q = query->sequence + q_off;
    s = subject0 + s_off / COMPRESSION_RATIO;
    sum = 0;
    base = 3 - (s_off % COMPRESSION_RATIO);

    while (s < sf || (s == sf && base > remainder)) {
        ch = *s;
        if ((sum += matrix[*q++][NCBI2NA_UNPACK_BASE(ch, base)]) > 0) {
            q_end = q;
            score += sum;
            sum = 0;
        } else if (sum < X)
            break;
        if (base == 0) {
            base = 3;
            s++;
        } else
            base--;
    }

    ungapped_data->length = q_end - q_beg;
    ungapped_data->score = score;
}

/** Perform ungapped extension of a word hit. Use an approximate method
 * and revert to rigorous ungapped alignment if the approximate score
 * is high enough
 * @param query The query sequence [in]
 * @param subject The subject sequence [in]
 * @param matrix The scoring matrix [in]
 * @param q_off The offset of a word in query [in]
 * @param s_match_end The first offset in the subject sequence that 
 *              is not known to exactly match the query sequence [in]
 * @param s_off The offset of a word in subject [in]
 * @param X The drop-off parameter for the ungapped extension [in]
 * @param ungapped_data The ungapped extension information [out]
 * @param score_table Array containing precompute sums of
 *                    match and mismatch scores [in]
 * @param reduced_cutoff Score beyond which a rigorous extension is used [in]
 */
static void
s_NuclUngappedExtend(BLAST_SequenceBlk * query,
                     BLAST_SequenceBlk * subject, Int4 ** matrix,
                     Int4 q_off, Int4 s_match_end, Int4 s_off,
                     Int4 X, BlastUngappedData * ungapped_data,
                     const Int4 * score_table, Int4 reduced_cutoff)
{
    Uint1 *q_start = query->sequence;
    Uint1 *s_start = subject->sequence;
    Uint1 *q;
    Uint1 *s;
    Int4 sum, score;
    Int4 i, len;
    Uint1 *new_q;
    Int4 q_ext, s_ext;

    /* The left extension begins behind (q_ext,s_ext); this is the first
       4-base boundary after s_off. */

    len = (COMPRESSION_RATIO - (s_off % COMPRESSION_RATIO)) %
        COMPRESSION_RATIO;
    q_ext = q_off + len;
    s_ext = s_off + len;
    q = q_start + q_ext;
    s = s_start + s_ext / COMPRESSION_RATIO;
    len = MIN(q_ext, s_ext) / COMPRESSION_RATIO;

    score = 0;
    sum = 0;
    new_q = q;

    for (i = 0; i < len; s--, q -= 4, i++) {
        Uint1 s_byte = s[-1];
        Uint1 q_byte = (q[-4] << 6) | (q[-3] << 4) | (q[-2] << 2) | q[-1];

        sum += score_table[q_byte ^ s_byte];
        if (sum > 0) {
            new_q = q - 4;
            score += sum;
            sum = 0;
        }
        if (sum < X) {
            break;
        }
    }

    /* record the start point of the extension */

    ungapped_data->q_start = new_q - q_start;
    ungapped_data->s_start = s_ext - (q_ext - ungapped_data->q_start);

    /* the right extension begins at the first bases not examined by the
       previous loop */

    q = q_start + q_ext;
    s = s_start + s_ext / COMPRESSION_RATIO;
    len = MIN(query->length - q_ext, subject->length - s_ext) /
        COMPRESSION_RATIO;
    sum = 0;
    new_q = q;

    for (i = 0; i < len; s++, q += 4, i++) {
        Uint1 s_byte = s[0];
        Uint1 q_byte = (q[0] << 6) | (q[1] << 4) | (q[2] << 2) | q[3];

        sum += score_table[q_byte ^ s_byte];
        if (sum > 0) {
            new_q = q + 3;
            score += sum;
            sum = 0;
        }
        if (sum < X) {
            break;
        }
    }

    if (score >= reduced_cutoff) {
        /* the current score is high enough; throw away the alignment and
           recompute it rigorously */
        s_NuclUngappedExtendExact(query, subject, matrix, q_off,
                                  s_off, X, ungapped_data);
    } else {
        /* record the length and score of the extension. Make sure the
           alignment extends at least to s_match_end */
        ungapped_data->score = score;
        ungapped_data->length = MAX(s_match_end - ungapped_data->s_start,
                                    (new_q - q_start) -
                                    ungapped_data->q_start + 1);
    }
}

/**
 * Attempt to retrieve information associated with diagonal diag.
 * @param table The hash table [in]
 * @param diag The diagonal to be retrieved [in]
 * @param level The offset of the last hit on the specified diagonal [out]
 * @param hit_saved Whether or not the last hit on the specified diagonal was saved [out]
 * @param hit_length length of the last hit on the specified diagonal [out]
 * @return 1 if successful, 0 if no hit was found on the specified diagonal.
 */
static NCBI_INLINE Int4 s_BlastDiagHashRetrieve(BLAST_DiagHash * table,
                                                Int4 diag, Int4 * level,
                                                Int4 * hit_len,
                                                Int4 * hit_saved)
{
    /* see http://lxr.linux.no/source/include/linux/hash.h */
    /* mod operator will be strength-reduced to an and by the compiler */
    Uint4 bucket = ((Uint4) diag * 0x9E370001) % DIAGHASH_NUM_BUCKETS;
    Uint4 index = table->backbone[bucket];

    while (index) {
        if (table->chain[index].diag == diag) {
            *level = table->chain[index].level;
            *hit_len = table->chain[index].hit_len;
            *hit_saved = table->chain[index].hit_saved;
            return 1;
        }

        index = table->chain[index].next;
    }
    return 0;
}

/**
 * Attempt to store information associated with diagonal diag.
 * Cleans up defunct entries along the way or allocates more space if necessary.
 * @param table The hash table [in]
 * @param diag The diagonal to be stored [in]
 * @param level The offset of the hit to be stored [in]
 * @param len The length of the hit to be stored [in]
 * @param hit_saved Whether or not this hit was stored [in]
 * @param s_end Needed to clean up defunct entries [in]
 * @param window_size Needed to clean up defunct entries [in]
 * @return 1 if successful, 0 if memory allocation failed.
 */
static NCBI_INLINE Int4 s_BlastDiagHashInsert(BLAST_DiagHash * table,
                                              Int4 diag, Int4 level,
                                              Int4 len,
                                              Int4 hit_saved,
                                              Int4 s_off,
                                              Int4 window_size)
{
    Uint4 bucket = ((Uint4) diag * 0x9E370001) % DIAGHASH_NUM_BUCKETS;
    Uint4 index = table->backbone[bucket];
    DiagHashCell *cell = NULL;

    while (index) {
        /* if we find what we're looking for, save into it */
        if (table->chain[index].diag == diag) {
            table->chain[index].level = level;
            table->chain[index].hit_len = len;
            table->chain[index].hit_saved = hit_saved;
            return 1;
        }
        /* otherwise, if this hit is stale, save into it. */
        else {
            /* if this hit is stale, save into it. */
            if ( s_off - table->chain[index].level > window_size) {
                table->chain[index].diag = diag;
                table->chain[index].level = level;
                table->chain[index].hit_len = len;
                table->chain[index].hit_saved = hit_saved;
                return 1;
            }
        }
        index = table->chain[index].next;
    }

    /* if we got this far, we were unable to replace any existing entries. */

    /* if there's no more room, allocate more */
    if (table->occupancy == table->capacity) {
        table->capacity *= 2;
        table->chain =
            realloc(table->chain, table->capacity * sizeof(DiagHashCell));
        if (table->chain == NULL)
            return 0;
    }

    cell = table->chain + table->occupancy;
    cell->diag = diag;
    cell->level = level;
    cell->hit_len = len;
    cell->hit_saved = hit_saved;
    cell->next = table->backbone[bucket];
    table->backbone[bucket] = table->occupancy;
    table->occupancy++;

    return 1;
}

/** Test to see if seed->q_off exists in lookup table
 * @param lookup_wrap The lookup table wrap structure [in]
 * @param subject Subject sequence data [in]
 * @s_off The starting offset of the seed [in]
 * @lut_word_length The length of the lookup word [in]
 */
static NCBI_INLINE Boolean
s_IsSeedMasked(const LookupTableWrap   * lookup_wrap,
               const BLAST_SequenceBlk * subject,
               Int4                      s_off,
               Int4                      lut_word_length,
               Int4                      q_pos)
{
    Uint1 *s  = subject->sequence + s_off / COMPRESSION_RATIO;
    Int4 shift = 2* (16 - s_off % COMPRESSION_RATIO - lut_word_length);
    Int4 index = (s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3]) >> shift;
    return !(((T_Lookup_Callback)(lookup_wrap->lookup_callback))
                                         (lookup_wrap, index, q_pos));
}

/** Check the mini-extended word against masked query regions, and do right
 * extension if necessary.
 * @param query Query sequence data [in]
 * @param subject Subject sequence data [in]
 * @param q_off Query offset [in][out]
 * @param s_off Subject offset [in][out]
 * @param locations of the masked query regions [in]
 * @param query_info of the masked query regions [in]
 * @param s_range the open bound of subject region [in]
 * @param word_length length of word [in]
 * @param lut_word_length length of lookup table word [in]
 * @param check_double check to see if it is a double word [in]
 * @param extended if successful, the actual bases extended [out]
 * @return 0,1,2  for non-word, single word, and double word
 */
static Int4
s_TypeOfWord(BLAST_SequenceBlk * query,
             BLAST_SequenceBlk * subject,
             Int4 *q_off, Int4 *s_off,
             BlastSeqLoc* locations, 
             BlastQueryInfo * query_info,
             Uint4 s_range,
             Uint4 word_length,
             Uint4 lut_word_length,
             const LookupTableWrap* lookup_wrap,
             Boolean check_double,
             Int4 * extended)
{
    Int4 context, q_range;
    Int4 ext_to, ext_max;
    Int4 q_end = *q_off + word_length;
    Int4 s_end = *s_off + word_length;
    Int4 s_pos, q_pos;

    *extended = 0;

    /* No need to check if mini-extension is not performed.
       It turns out that we may skip checking for double-hit in 2-hit 
       algo case as well -- they will come in as 2 hits naturally*/
    if (word_length == lut_word_length) return 1;

    /* Find the query context boundary */
    context = BSearchContextInfo(q_end, query_info);
    q_range = query_info->contexts[context].query_offset 
            + query_info->contexts[context].query_length;

    /* check query mask at two ends */
    if (locations) {
        /* check for right end first */
        if (s_IsSeedMasked(lookup_wrap, subject, 
                           s_end - lut_word_length, 
                           lut_word_length, 
                           q_end - lut_word_length)) return 0;

        /* search for valid left end and reposition q_off */
        for (; TRUE; ++(*s_off), ++(*q_off)) {
            if (!s_IsSeedMasked(lookup_wrap, subject,
                                *s_off, lut_word_length, *q_off)) break;
        }
    }

    ext_to = word_length - (q_end - (*q_off));
    ext_max = MIN(q_range - q_end, s_range - s_end);

    /* shift to the right, and check query mask inside */
    if (ext_to || locations) {

        if (ext_to > ext_max) return 0;
        q_end += ext_to;
        s_end += ext_to;

        for (s_pos = s_end - lut_word_length, 
             q_pos = q_end - lut_word_length;
             s_pos > *s_off; 
             s_pos -= lut_word_length,
             q_pos -= lut_word_length) {
             if (s_IsSeedMasked(lookup_wrap, subject,
                           s_pos, lut_word_length, q_pos)) return 0;
        }

        (*extended) = ext_to;
    }

    /* if we get here, single word check is passed */
    if (!check_double) return 1;

    /* do right extension to double word
       Note: unlike the single word check, here we need to determine the 
       precise value of maximal possible *extend */
    ext_to += word_length;
    ext_max = MIN(ext_max, ext_to);

    /* try seed by seed */
    for (s_pos = s_end, q_pos = q_end; 
         *extended + lut_word_length <= ext_max; 
         s_pos += lut_word_length, 
         q_pos += lut_word_length,
         (*extended) += lut_word_length) {
         if (s_IsSeedMasked(lookup_wrap, subject, s_pos, 
                            lut_word_length, q_pos)) break;
    }

    /* try base by base */
    s_pos -= (lut_word_length - 1);
    q_pos -= (lut_word_length - 1);
    while (*extended < ext_max) {
        if (s_IsSeedMasked(lookup_wrap, subject, s_pos,
                               lut_word_length, q_pos)) return 1;
        (*extended)++;
        ++s_pos;
        ++q_pos;
    }
        
    return ((ext_max == ext_to) ? 2 : 1);
}

/** Perform ungapped extension given an offset pair, and save the initial 
 * hit information if the hit qualifies. This function assumes that the
 * exact match has already been extended to the word size parameter. It also
 * supports a two-hit version, where extension is performed only after two hits
 * are detected within a window.
 * @param query The query sequence [in]
 * @param subject The subject sequence [in]
 * @param q_off The offset in the query sequence [in]
 * @param s_off The offset in the subject sequence [in]
 * @param query_mask Structure containing query mask ranges [in]
 * @param query_info Structure containing query context ranges [in]
 * @param s_range Subject range [in]
 * @param word_length The length of the hit [in]
 * @param word_lut_length The length of the lookup table word [in]
 * @param word_params The parameters related to initial word extension [in]
 * @param matrix the substitution matrix for ungapped extension [in]
 * @param diag_table Structure containing diagonal table with word extension 
 *                   information [in]
 * @param init_hitlist The structure containing information about all 
 *                     initial hits [in] [out]
 * @return 1 if hit was extended, 0 if not
 */
static Int4
s_BlastnDiagTableExtendInitialHit(BLAST_SequenceBlk * query,
                             BLAST_SequenceBlk * subject, 
                             Int4 q_off, Int4 s_off, 
                             BlastSeqLoc* query_mask,
                             BlastQueryInfo * query_info,
                             Int4 s_range,
                             Int4 word_length, Int4 lut_word_length,
                             const LookupTableWrap * lut,
                             const BlastInitialWordParameters * word_params,
                             Int4 ** matrix, 
                             BLAST_DiagTable * diag_table,
                             BlastInitHitList * init_hitlist)
{
    Int4 diag, real_diag;
    Int4 s_end, s_off_pos, s_end_pos;
    Int4 word_type = 0;
    Int4 extended = 0;
    BlastUngappedData *ungapped_data;
    BlastUngappedData dummy_ungapped_data;
    Int4 window_size = word_params->options->window_size;
    Int4 hit_ready = 1;
    Int4 last_hit, hit_saved;
    DiagStruct *hit_level_array;
    BlastUngappedCutoffs *cutoffs = NULL;
    Boolean two_hits = (window_size > 0);
    Boolean off_found = FALSE;
    Int4 Delta = MIN(word_params->options->scan_range, window_size - word_length);

    hit_level_array = diag_table->hit_level_array;
    ASSERT(hit_level_array);

    diag = s_off + diag_table->diag_array_length - q_off;
    real_diag = diag & diag_table->diag_mask;
    last_hit = hit_level_array[real_diag].last_hit;
    hit_saved = hit_level_array[real_diag].flag;
    s_end = s_off + word_length;
    s_off_pos = s_off + diag_table->offset;
    s_end_pos = s_end + diag_table->offset;

    /* hit within the explored area should be rejected*/
    if (s_off_pos < last_hit) return 0;

    if (two_hits && (hit_saved || s_end_pos > last_hit + window_size )) {
        /* check the masks for the word; also check to see if this
           first hit qualifies for a double-word */
        word_type = s_TypeOfWord(query, subject, &q_off, &s_off,
                                 query_mask, query_info, s_range, 
                                 word_length, lut_word_length, lut, TRUE, &extended);
        if (!word_type) return 0;
        /* update the right end*/
        s_end += extended;
        s_end_pos += extended;

        /* for single word, also try off diagonals */
        if (word_type == 1) {
            /* try off-diagonals */
            Int4 orig_diag = real_diag + diag_table->diag_array_length;
            Int4 s_a = s_off_pos + word_length - window_size;
            Int4 s_b = s_end_pos - 2 * word_length;
            Int4 delta;
            if (Delta < 0) Delta = 0;
            for (delta = 1; delta <= Delta ; ++delta) {
                Int4 off_diag  = (orig_diag + delta) & diag_table->diag_mask;
                Int4 off_s_end = hit_level_array[off_diag].last_hit;
                Int4 off_s_l   = diag_table->hit_len_array[off_diag];
                if ( off_s_l
                 && off_s_end - delta >= s_a 
                 && off_s_end - off_s_l <= s_b) {
                    off_found = TRUE;
                    break;
                }
                off_diag  = (orig_diag - delta) & diag_table->diag_mask;
                off_s_end = hit_level_array[off_diag].last_hit;
                off_s_l   = diag_table->hit_len_array[off_diag];
                if ( off_s_l
                 && off_s_end >= s_a 
                 && off_s_end - off_s_l + delta <= s_b) {
                    off_found = TRUE;
                    break;
                }
            }
            if (!off_found) {
                /* This is a new hit */
                hit_ready = 0;
            }
        }
    } else {
        /* check the masks for the word */
        if(!s_TypeOfWord(query, subject, &q_off, &s_off,
                        query_mask, query_info, s_range, 
                        word_length, lut_word_length, lut, FALSE, &extended)) return 0;
        /* update the right end*/
        s_end += extended;
        s_end_pos += extended;
    }
       
    if (hit_ready) {
        if (word_params->ungapped_extension) {
            Int4 context = BSearchContextInfo(q_off, query_info);
            cutoffs = word_params->cutoffs + context;
            ungapped_data = &dummy_ungapped_data;

            /* 
             * Skip use of the scoring table and go straight to the matrix
             * based extension if matrix_only_scoring is set.  Used by
             * app rmblastn.
             * -RMH-
             */
            if ( word_params->options->program_number == eBlastTypeBlastn &&
                 word_params->matrix_only_scoring )
            {
               s_NuclUngappedExtendExact(query, subject, matrix, q_off,
                                  s_off, -(cutoffs->x_dropoff), ungapped_data);
            }else {
               s_NuclUngappedExtend(query, subject, matrix, q_off, s_end, s_off,
                                 -(cutoffs->x_dropoff), ungapped_data,
                                 word_params->nucl_score_table,
                                 cutoffs->reduced_nucl_cutoff_score);
            }

            if (off_found || ungapped_data->score >= cutoffs->cutoff_score) {
                BlastUngappedData *final_data =
                    (BlastUngappedData *) malloc(sizeof(BlastUngappedData));
                *final_data = *ungapped_data;
                BLAST_SaveInitialHit(init_hitlist, q_off, s_off, final_data);
                s_end_pos = ungapped_data->length + ungapped_data->s_start
                          + diag_table->offset;
            } else {
                hit_ready = 0;
            }
        } else {
            ungapped_data = NULL;
            BLAST_SaveInitialHit(init_hitlist, q_off, s_off, ungapped_data);
        }
    } 

    hit_level_array[real_diag].last_hit = s_end_pos;
    hit_level_array[real_diag].flag = hit_ready;
    if (two_hits) {
        diag_table->hit_len_array[real_diag] = (hit_ready) ? 0 : s_end_pos - s_off_pos;
    }

    return hit_ready;
}

/** Perform ungapped extension given an offset pair, and save the initial 
 * hit information if the hit qualifies. This function assumes that the
 * exact match has already been extended to the word size parameter. It also
 * supports a two-hit version, where extension is performed only after two hits
 * are detected within a window.
 * @param query The query sequence [in]
 * @param subject The subject sequence [in]
 * @param q_off The offset in the query sequence [in]
 * @param s_off The offset in the subject sequence [in]
 * @param query_mask Structure containing query mask ranges [in]
 * @param query_info Structure containing query context ranges [in]
 * @param s_range Subject range [in]
 * @param word_length The length of the hit [in]
 * @param word_lut_length The length of the lookup table word [in]
 * @param word_params The parameters related to initial word extension [in]
 * @param matrix the substitution matrix for ungapped extension [in]
 * @param hash_table Structure containing initial hits [in] [out]
 * @param init_hitlist The structure containing information about all 
 *                     initial hits [in] [out]
 * @return 1 if hit was extended, 0 if not
 */
static Int4
s_BlastnDiagHashExtendInitialHit(BLAST_SequenceBlk * query,
                               BLAST_SequenceBlk * subject, 
                               Int4 q_off, Int4 s_off, 
                               BlastSeqLoc* query_mask,
                               BlastQueryInfo * query_info,
                               Int4 s_range,
                               Int4 word_length, Int4 lut_word_length,
                               const LookupTableWrap * lut,
                               const BlastInitialWordParameters * word_params,
                               Int4 ** matrix,
                               BLAST_DiagHash * hash_table,
                               BlastInitHitList * init_hitlist)
{
    Int4 diag;
    Int4 s_end, s_off_pos, s_end_pos, s_l;
    Int4 word_type = 0;
    Int4 extended = 0;
    BlastUngappedData *ungapped_data;
    BlastUngappedData dummy_ungapped_data;
    Int4 window_size = word_params->options->window_size;
    Int4 hit_ready = 1; 
    Int4 last_hit, hit_saved = 0;
    BlastUngappedCutoffs *cutoffs = NULL;
    Boolean two_hits = (window_size > 0);
    Boolean off_found = FALSE;
    Int4 Delta = MIN(word_params->options->scan_range, window_size - word_length);
    Int4 rc;

    diag = s_off - q_off;
    s_end = s_off + word_length;
    s_off_pos = s_off + hash_table->offset;
    s_end_pos = s_end + hash_table->offset;

    rc = s_BlastDiagHashRetrieve(hash_table, diag, &last_hit, &s_l, &hit_saved);

    /* if there is no record in hashtable, we set last_hit to be a very negative number */
    if(!rc)  last_hit = 0;

    /* hit within the explored area should be rejected*/
    if (s_off_pos < last_hit) return 0;

    if (two_hits && (hit_saved || s_end_pos > last_hit + window_size )) {
        /* check the masks for the word; also check to see if this
           first hit qualifies for a double-hit */
        word_type = s_TypeOfWord(query, subject, &q_off, &s_off,
                                 query_mask, query_info, s_range,
                                 word_length, lut_word_length, lut, TRUE, &extended);
        if (!word_type) return 0;
        /* update the right end*/
        s_end += extended;
        s_end_pos += extended;

        /* for single word, also try off diagonals */
        if (word_type == 1) {
            /* try off-diagonal */
            Int4 s_a = s_off_pos + word_length - window_size;
            Int4 s_b = s_end_pos - 2 * word_length;
            Int4 delta;
            if (Delta < 0) Delta = 0;
            for (delta = 1; delta <= Delta; ++delta) {
                Int4 off_s_end = 0;
                Int4 off_s_l = 0;
                Int4 off_hit_saved = 0;
                Int4 off_rc = s_BlastDiagHashRetrieve(hash_table, diag + delta, 
                              &off_s_end, &off_s_l, &off_hit_saved);
                if ( off_rc
                  && off_s_l
                  && off_s_end - delta >= s_a
                  && off_s_end - off_s_l <= s_b) {
                     off_found = TRUE;
                     break;
                }
                off_rc = s_BlastDiagHashRetrieve(hash_table, diag - delta, 
                              &off_s_end, &off_s_l, &off_hit_saved);
                if ( off_rc
                  && off_s_l
                  && off_s_end >= s_a
                  && off_s_end - off_s_l + delta <= s_b) {
                     off_found = TRUE;
                     break;
                }
            }
            if (!off_found) {
                /* This is a new hit */
                hit_ready = 0;
            }
        }
    } else {
        /* check the masks for the word */
        if (!s_TypeOfWord(query, subject, &q_off, &s_off,
                          query_mask, query_info, s_range,
                          word_length, lut_word_length, lut, FALSE, &extended)) return 0;
        /* update the right end*/
        s_end += extended;
        s_end_pos += extended;
    }

    if (hit_ready) {
        if (word_params->ungapped_extension) {
            /* Perform ungapped extension */
            Int4 context = BSearchContextInfo(q_off, query_info);
            cutoffs = word_params->cutoffs + context;
            ungapped_data = &dummy_ungapped_data;

            /* 
             * Skip use of the scoring table and go straight to the matrix
             * based extension if matrix_only_scoring is set.  Used by
             * app rmblastn.
             * -RMH-
             */
            if ( word_params->options->program_number == eBlastTypeBlastn &&                          word_params->matrix_only_scoring )
            {
                s_NuclUngappedExtendExact(query, subject, matrix, q_off,
                                  s_off, -(cutoffs->x_dropoff), ungapped_data);
            }else {
                s_NuclUngappedExtend(query, subject, matrix, q_off, s_end,
                                 s_off, -(cutoffs->x_dropoff),
                                 ungapped_data,
                                 word_params->nucl_score_table,
                                 cutoffs->reduced_nucl_cutoff_score);
            }

            if (off_found || ungapped_data->score >= cutoffs->cutoff_score) {
                BlastUngappedData *final_data =
                    (BlastUngappedData *) malloc(sizeof(BlastUngappedData));
                *final_data = *ungapped_data;
                BLAST_SaveInitialHit(init_hitlist, q_off, s_off, final_data);
                s_end_pos = ungapped_data->length + ungapped_data->s_start 
                          + hash_table->offset;
            } else {
                hit_ready = 0;
            }
        } else {
            ungapped_data = NULL;
            BLAST_SaveInitialHit(init_hitlist, q_off, s_off, ungapped_data);
        }
    } 
    
    s_BlastDiagHashInsert(hash_table, diag, s_end_pos, 
                          (hit_ready) ? 0 : s_end_pos - s_off_pos,
                          hit_ready, s_off_pos, window_size + Delta + 1);

    return hit_ready;
}

/** Perform ungapped extensions on the hits retrieved from
 * blastn/megablast lookup tables, skipping the mini-extension process
 * @param offset_pairs Array of query and subject offsets. [in]
 * @param num_hits Size of the above arrays [in]
 * @param word_params Parameters for word extension [in]
 * @param lookup_wrap Lookup table wrapper structure [in]
 * @param query Query sequence data [in]
 * @param subject Subject sequence data [in]
 * @param matrix Scoring matrix for ungapped extension [in]
 * @param query_info Structure containing query context ranges [in]
 * @param ewp Word extension structure containing information about the 
 *            extent of already processed hits on each diagonal [in]
 * @param init_hitlist Structure to keep the extended hits. 
 *                     Must be allocated outside of this function [in] [out]
 * @param s_range The subject range [in]
 * @return Number of hits extended. 
 */
static Int4
s_BlastNaExtendDirect(const BlastOffsetPair * offset_pairs, Int4 num_hits,
                      const BlastInitialWordParameters * word_params,
                      LookupTableWrap * lookup_wrap,
                      BLAST_SequenceBlk * query,
                      BLAST_SequenceBlk * subject, Int4 ** matrix,
                      BlastQueryInfo * query_info,
                      Blast_ExtendWord * ewp,
                      BlastInitHitList * init_hitlist,
                      Uint4 s_range)
{
    Int4 index = 0;
    Int4 hits_extended = 0;
    Int4 word_length;

    if (lookup_wrap->lut_type == eMBLookupTable) {
        BlastMBLookupTable *lut = (BlastMBLookupTable *) lookup_wrap->lut;
        word_length = (lut->discontiguous) ? lut->template_length : lut->word_length;
        ASSERT(word_length == lut->lut_word_length || lut->discontiguous);
    } 
    else if (lookup_wrap->lut_type == eSmallNaLookupTable) {
        BlastSmallNaLookupTable *lut = 
                        (BlastSmallNaLookupTable *) lookup_wrap->lut;
        word_length = lut->word_length;
    } 
    else {
        BlastNaLookupTable *lut = (BlastNaLookupTable *) lookup_wrap->lut;
        word_length = lut->word_length;
    }

    if (word_params->container_type == eDiagHash) {
        for (; index < num_hits; ++index) {
            Int4 s_offset = offset_pairs[index].qs_offsets.s_off;
            Int4 q_offset = offset_pairs[index].qs_offsets.q_off;

            hits_extended += s_BlastnDiagHashExtendInitialHit(query, subject, 
                                                q_offset, s_offset,  
                                                NULL,
                                                query_info, s_range, 
                                                word_length, word_length,
                                                lookup_wrap,
                                                word_params, matrix,
                                                ewp->hash_table,
                                                init_hitlist);
        }
    } 
    else {
        for (; index < num_hits; ++index) {
            Int4 s_offset = offset_pairs[index].qs_offsets.s_off;
            Int4 q_offset = offset_pairs[index].qs_offsets.q_off;

            hits_extended += s_BlastnDiagTableExtendInitialHit(query, subject, 
                                                q_offset, s_offset,  
                                                NULL,
                                                query_info, s_range, 
                                                word_length, word_length,
                                                lookup_wrap,
                                                word_params, matrix,
                                                ewp->diag_table,
                                                init_hitlist);
        }
    }
    return hits_extended;
}

/** Perform exact match extensions on the hits retrieved from
 * blastn/megablast lookup tables, assuming an arbitrary number of bases 
 * in a lookup and arbitrary start offset of each hit. Also 
 * update the diagonal structure.
 * @param offset_pairs Array of query and subject offsets [in]
 * @param num_hits Size of the above arrays [in]
 * @param word_params Parameters for word extension [in]
 * @param lookup_wrap Lookup table wrapper structure [in]
 * @param query Query sequence data [in]
 * @param subject Subject sequence data [in]
 * @param matrix Scoring matrix for ungapped extension [in]
 * @param query_info Structure containing query context ranges [in]
 * @param ewp Word extension structure containing information about the 
 *            extent of already processed hits on each diagonal [in]
 * @param init_hitlist Structure to keep the extended hits. 
 *                     Must be allocated outside of this function [in] [out]
 * @param s_range The subject range [in]
 * @return Number of hits extended. 
 */
static Int4
s_BlastNaExtend(const BlastOffsetPair * offset_pairs, Int4 num_hits,
                const BlastInitialWordParameters * word_params,
                LookupTableWrap * lookup_wrap,
                BLAST_SequenceBlk * query,
                BLAST_SequenceBlk * subject, Int4 ** matrix,
                BlastQueryInfo * query_info,
                Blast_ExtendWord * ewp,
                BlastInitHitList * init_hitlist,
                Uint4 s_range)
{
    Int4 index = 0;
    Int4 hits_extended = 0;
    Int4 word_length, lut_word_length, ext_to;
    BlastSeqLoc* masked_locations = NULL;

    if (lookup_wrap->lut_type == eMBLookupTable) {
        BlastMBLookupTable *lut = (BlastMBLookupTable *) lookup_wrap->lut;
        word_length = lut->word_length;
        lut_word_length = lut->lut_word_length;
        masked_locations = lut->masked_locations;
    } 
    else {
        BlastNaLookupTable *lut = (BlastNaLookupTable *) lookup_wrap->lut;
        word_length = lut->word_length;
        lut_word_length = lut->lut_word_length;
        masked_locations = lut->masked_locations;
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

    for (; index < num_hits; ++index) {
        Int4 s_offset = offset_pairs[index].qs_offsets.s_off;
        Int4 q_offset = offset_pairs[index].qs_offsets.q_off;

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
        /* check the diagonal on which the hit lies. The boundaries
           extend from the first match of the hit to one beyond the last
           match */

        if (word_params->container_type == eDiagHash) {
            hits_extended += s_BlastnDiagHashExtendInitialHit(query, subject, 
                                                q_offset, s_offset,  
                                                masked_locations, 
                                                query_info, s_range, 
                                                word_length, lut_word_length,
                                                lookup_wrap,
                                                word_params, matrix,
                                                ewp->hash_table,
                                                init_hitlist);
        } else {
            hits_extended += s_BlastnDiagTableExtendInitialHit(query, subject, 
                                                q_offset, s_offset,  
                                                masked_locations, 
                                                query_info, s_range, 
                                                word_length, lut_word_length,
                                                lookup_wrap,
                                                word_params, matrix,
                                                ewp->diag_table,
                                                init_hitlist);
        }
    }
    return hits_extended;
}

/** Perform exact match extensions on the hits retrieved from
 * blastn/megablast lookup tables, assuming the number of bases in a lookup
 * table word, and the start offset of each hit, is a multiple
 * of 4. Also update the diagonal structure.
 * @param offset_pairs Array of query and subject offsets. [in]
 * @param num_hits Size of the above arrays [in]
 * @param word_params Parameters for word extension [in]
 * @param lookup_wrap Lookup table wrapper structure [in]
 * @param query Query sequence data [in]
 * @param subject Subject sequence data [in]
 * @param matrix Scoring matrix for ungapped extension [in]
 * @param query_info Structure containing query context ranges [in]
 * @param ewp Word extension structure containing information about the 
 *            extent of already processed hits on each diagonal [in]
 * @param init_hitlist Structure to keep the extended hits. 
 *                     Must be allocated outside of this function [in] [out]
 * @param s_range The subject range [in]
 * @return Number of hits extended. 
 */
static Int4
s_BlastNaExtendAligned(const BlastOffsetPair * offset_pairs, Int4 num_hits,
                       const BlastInitialWordParameters * word_params,
                       LookupTableWrap * lookup_wrap,
                       BLAST_SequenceBlk * query, BLAST_SequenceBlk * subject,
                       Int4 ** matrix, BlastQueryInfo * query_info,
                       Blast_ExtendWord * ewp,
                       BlastInitHitList * init_hitlist,
                       Uint4 s_range)
{
    Int4 index = 0;
    Int4 hits_extended = 0;
    Int4 word_length, lut_word_length, ext_to;
    BlastSeqLoc* masked_locations = NULL;

    if (lookup_wrap->lut_type == eMBLookupTable) {
        BlastMBLookupTable *lut = (BlastMBLookupTable *) lookup_wrap->lut;
        word_length = lut->word_length;
        lut_word_length = lut->lut_word_length;
        masked_locations = lut->masked_locations;
    } 
    else {
        BlastNaLookupTable *lut = (BlastNaLookupTable *) lookup_wrap->lut;
        word_length = lut->word_length;
        lut_word_length = lut->lut_word_length;
        masked_locations = lut->masked_locations;
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

    for (; index < num_hits; ++index) {
        Int4 s_offset = offset_pairs[index].qs_offsets.s_off;
        Int4 q_offset = offset_pairs[index].qs_offsets.q_off;

        /* begin with the left extension; the initialization is slightly
           faster. q below points to the first base of the lookup table hit
           and s points to the first four bases of the hit (which is
           guaranteed to be aligned on a byte boundary) */

        Int4 ext_left = 0;
        Int4 ext_max = MIN(ext_to, s_offset);
        Uint1 *q = query->sequence + q_offset;
        Uint1 *s = subject->sequence + s_offset / COMPRESSION_RATIO;

        for (; ext_left < ext_max; s--, q -= 4, ++ext_left) {
            Uint1 byte = s[-1];

            if ((byte & 3) != q[-1] || ++ext_left == ext_max)
                break;
            if (((byte >> 2) & 3) != q[-2] || ++ext_left == ext_max)
                break;
            if (((byte >> 4) & 3) != q[-3] || ++ext_left == ext_max)
                break;
            if ((byte >> 6) != q[-4])
                break;
        }

        /* do the right extension if the left extension did not find all the
           bases required. Begin at the first base past the lookup table hit 
           and move forwards */

        if (ext_left < ext_to) {
            Int4 ext_right = 0;
            ext_max = ext_to -ext_left;
            if (s_offset + lut_word_length + ext_max > s_range) 
                continue;
            q = query->sequence + q_offset + lut_word_length;
            s = subject->sequence + (s_offset + lut_word_length) / COMPRESSION_RATIO;

            for (; ext_right < ext_max; s++, q += 4, ++ext_right) {
                Uint1 byte = s[0];

                if ((byte >> 6) != q[0] || ++ext_right == ext_max)
                    break;
                if (((byte >> 4) & 3) != q[1] || ++ext_right == ext_max)
                    break;
                if (((byte >> 2) & 3) != q[2] || ++ext_right == ext_max)
                    break;
                if ((byte & 3) != q[3])
                    break;
            }

            /* check if enough extra matches were found */
            if (ext_left + ext_right < ext_to)
                continue;
        }

        q_offset -= ext_left;
        s_offset -= ext_left;

        /* check the diagonal on which the hit lies. The boundaries extend
           from the first match of the hit to one beyond the last match */

        if (word_params->container_type == eDiagHash) {
            hits_extended += s_BlastnDiagHashExtendInitialHit(query, subject, 
                                                q_offset, s_offset,  
                                                masked_locations, 
                                                query_info, s_range, 
                                                word_length, lut_word_length,
                                                lookup_wrap,
                                                word_params, matrix,
                                                ewp->hash_table,
                                                init_hitlist);
        } else {
            hits_extended += s_BlastnDiagTableExtendInitialHit(query, subject, 
                                                q_offset, s_offset,  
                                                masked_locations, 
                                                query_info, s_range, 
                                                word_length, lut_word_length,
                                                lookup_wrap,
                                                word_params, matrix,
                                                ewp->diag_table,
                                                init_hitlist);
        }
    }
    return hits_extended;
}

/** Entry i of this list gives the number of pairs of
 * bits that are zero in the bit pattern of i, looking 
 * from right to left
 */
static const Uint1 s_ExactMatchExtendLeft[256] = {
4, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 
2, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 
2, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 
2, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 
3, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 
2, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 
2, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 
2, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 
3, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 
2, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 
2, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 
2, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 
3, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 
2, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 
2, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 
2, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 
};

/** Entry i of this list gives the number of pairs of
 * bits that are zero in the bit pattern of i, looking 
 * from left to right
 */
static const Uint1 s_ExactMatchExtendRight[256] = {
4, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
};

/** Perform exact match extensions on the hits retrieved from
 * small-query lookup tables. Assumes the number of bases in a lookup
 * table word and the start offset of each hit is a multiple
 * of 4, and also that the word size is within 4 bases of the lookup
 * table width
 * @param offset_pairs Array of query and subject offsets. [in]
 * @param num_hits Size of the above arrays [in]
 * @param word_params Parameters for word extension [in]
 * @param lookup_wrap Lookup table wrapper structure [in]
 * @param query Query sequence data [in]
 * @param subject Subject sequence data [in]
 * @param matrix Scoring matrix for ungapped extension [in]
 * @param query_info Structure containing query context ranges [in]
 * @param ewp Word extension structure containing information about the 
 *            extent of already processed hits on each diagonal [in]
 * @param init_hitlist Structure to keep the extended hits. 
 *                     Must be allocated outside of this function [in] [out]
 * @param s_range The subject range [in]
 * @return Number of hits extended. 
 */
static Int4
s_BlastSmallNaExtendAlignedOneByte(const BlastOffsetPair * offset_pairs, 
                       Int4 num_hits,
                       const BlastInitialWordParameters * word_params,
                       LookupTableWrap * lookup_wrap,
                       BLAST_SequenceBlk * query, BLAST_SequenceBlk * subject,
                       Int4 ** matrix, BlastQueryInfo * query_info,
                       Blast_ExtendWord * ewp,
                       BlastInitHitList * init_hitlist,
                       Uint4 s_range)
{
    Int4 index = 0;
    Int4 hits_extended = 0;
    BlastSmallNaLookupTable *lut = (BlastSmallNaLookupTable *) lookup_wrap->lut;
    Int4 word_length = lut->word_length;
    Int4 lut_word_length = lut->lut_word_length;
    Int4 ext_to = word_length - lut_word_length;
    Uint1 *q = query->compressed_nuc_seq;
    Uint1 *s = subject->sequence;

    for (; index < num_hits; ++index) {
        Int4 s_offset = offset_pairs[index].qs_offsets.s_off;
        Int4 q_offset = offset_pairs[index].qs_offsets.q_off;
        Int4 ext_left = 0;

        Int4 context = BSearchContextInfo(q_offset, query_info);
        Int4 q_start = query_info->contexts[context].query_offset;
        Int4 q_range = q_start + query_info->contexts[context].query_length;

        /* the seed is assumed to start on a multiple of 4 bases
           in the subject sequence. Look for up to 4 exact matches
           to the left. The index into q[] below can
           technically be negative, but the compressed version
           of the query has extra pad bytes before q[0] */

        if (s_offset > 0) {
            Uint1 q_byte = q[q_offset - 4];
            Uint1 s_byte = s[s_offset / COMPRESSION_RATIO - 1];
            ext_left = s_ExactMatchExtendLeft[q_byte ^ s_byte];
            ext_left = MIN(MIN(ext_left, ext_to), q_offset - q_start);
        }

        /* look for up to 4 exact matches to the right of the seed */

        if (ext_left < ext_to) {
            Uint1 q_byte = q[q_offset + lut_word_length];
            Uint1 s_byte = s[(s_offset + lut_word_length) / COMPRESSION_RATIO];
            Int4 ext_right = s_ExactMatchExtendRight[q_byte ^ s_byte];
            ext_right = MIN(MIN(ext_right, s_range - (s_offset + lut_word_length)), 
                                           q_range - (q_offset + lut_word_length));
            if (ext_left + ext_right < ext_to)
                continue;
        }

        q_offset -= ext_left;
        s_offset -= ext_left;
        
        if (word_params->container_type == eDiagHash) {
            hits_extended += s_BlastnDiagHashExtendInitialHit(query, subject,
                                                q_offset, s_offset,  
                                                lut->masked_locations, 
                                                query_info, s_range, 
                                                word_length, lut_word_length,
                                                lookup_wrap,
                                                word_params, matrix,
                                                ewp->hash_table,
                                                init_hitlist);
        }
        else {
            hits_extended += s_BlastnDiagTableExtendInitialHit(query, subject, 
                                                q_offset, s_offset,  
                                                lut->masked_locations, 
                                                query_info, s_range, 
                                                word_length, lut_word_length,
                                                lookup_wrap,
                                                word_params, matrix,
                                                ewp->diag_table,
                                                init_hitlist);
        }
    }
    return hits_extended;
}


/** Perform exact match extensions on the hits retrieved from
 * small-query blastn lookup tables, assuming an arbitrary number of bases 
 * in a lookup and arbitrary start offset of each hit. Also 
 * update the diagonal structure.
 * @param offset_pairs Array of query and subject offsets [in]
 * @param num_hits Size of the above arrays [in]
 * @param word_params Parameters for word extension [in]
 * @param lookup_wrap Lookup table wrapper structure [in]
 * @param query Query sequence data [in]
 * @param subject Subject sequence data [in]
 * @param matrix Scoring matrix for ungapped extension [in]
 * @param query_info Structure containing query context ranges [in]
 * @param ewp Word extension structure containing information about the 
 *            extent of already processed hits on each diagonal [in]
 * @param init_hitlist Structure to keep the extended hits. 
 *                     Must be allocated outside of this function [in] [out]
 * @param s_range The subject range [in]
 * @return Number of hits extended. 
 */
static Int4
s_BlastSmallNaExtend(const BlastOffsetPair * offset_pairs, Int4 num_hits,
                     const BlastInitialWordParameters * word_params,
                     LookupTableWrap * lookup_wrap,
                     BLAST_SequenceBlk * query,
                     BLAST_SequenceBlk * subject, Int4 ** matrix,
                     BlastQueryInfo * query_info,
                     Blast_ExtendWord * ewp,
                     BlastInitHitList * init_hitlist,
                     Uint4 s_range)
{
    Int4 index = 0;
    Int4 hits_extended = 0;
    BlastSmallNaLookupTable *lut = (BlastSmallNaLookupTable *) lookup_wrap->lut;
    Int4 word_length = lut->word_length; 
    Int4 lut_word_length = lut->lut_word_length; 
    Uint1 *q = query->compressed_nuc_seq;
    Uint1 *s = subject->sequence;

    for (; index < num_hits; ++index) {
        Int4 s_offset = offset_pairs[index].qs_offsets.s_off;
        Int4 q_offset = offset_pairs[index].qs_offsets.q_off;
        Int4 s_off;
        Int4 q_off;
        Int4 ext_left = 0;
        Int4 ext_right = 0;
        Int4 context = BSearchContextInfo(q_offset, query_info);
        Int4 q_start = query_info->contexts[context].query_offset;
        Int4 q_range = q_start + query_info->contexts[context].query_length;
        Int4 ext_max = MIN(MIN(word_length - lut_word_length, s_offset), q_offset - q_start);

        /* Start the extension at the first multiple of 4 bases in
           the subject sequence to the right of the seed.
           Collect exact matches in groups of four, until a
           mismatch is encountered or the expected number of
           matches is found. The index into q[] below can
           technically be negative, but the compressed version
           of the query has extra pad bytes before q[0] */

        Int4 rsdl = COMPRESSION_RATIO - (s_offset % COMPRESSION_RATIO);
        s_offset += rsdl;
        q_offset += rsdl;
        ext_max  += rsdl;

        s_off = s_offset;
        q_off = q_offset;

        while (ext_left < ext_max) {
            Uint1 q_byte = q[q_off - 4];
            Uint1 s_byte = s[s_off / COMPRESSION_RATIO - 1];
            Uint1 bases = s_ExactMatchExtendLeft[q_byte ^ s_byte];
            ext_left += bases;
            if (bases < 4)
                break;
            q_off -= 4;
            s_off -= 4;
        }
        ext_left = MIN(ext_left, ext_max);

        /* extend to the right. The extension begins at the first
           base not examined by the left extension */

        s_off = s_offset;
        q_off = q_offset;
        ext_max = MIN(MIN(word_length - ext_left, s_range - s_off), q_range - q_off);
        while (ext_right < ext_max) {
            Uint1 q_byte = q[q_off];
            Uint1 s_byte = s[s_off / COMPRESSION_RATIO];
            Uint1 bases = s_ExactMatchExtendRight[q_byte ^ s_byte];
            ext_right += bases;
            if (bases < 4)
                break;
            q_off += 4;
            s_off += 4;
        }
        ext_right = MIN(ext_right, ext_max);

        if (ext_left + ext_right < word_length)
            continue;

        q_offset -= ext_left;
        s_offset -= ext_left;
        
        if (word_params->container_type == eDiagHash) {
            hits_extended += s_BlastnDiagHashExtendInitialHit(query, subject, 
                                                q_offset, s_offset,  
                                                lut->masked_locations, 
                                                query_info, s_range, 
                                                word_length, lut_word_length,
                                                lookup_wrap,
                                                word_params, matrix,
                                                ewp->hash_table,
                                                init_hitlist);
        } else {
            hits_extended += s_BlastnDiagTableExtendInitialHit(query, subject, 
                                                q_offset, s_offset,  
                                                lut->masked_locations, 
                                                query_info, s_range, 
                                                word_length, lut_word_length,
                                                lookup_wrap,
                                                word_params, matrix,
                                                ewp->diag_table,
                                                init_hitlist);
        }
    }
    return hits_extended;
}

/* Description in na_ungapped.h */

Int2 BlastNaWordFinder(BLAST_SequenceBlk * subject,
                       BLAST_SequenceBlk * query,
                       BlastQueryInfo * query_info,
                       LookupTableWrap * lookup_wrap,
                       Int4 ** matrix,
                       const BlastInitialWordParameters * word_params,
                       Blast_ExtendWord * ewp,
                       BlastOffsetPair * offset_pairs,
                       Int4 max_hits,
                       BlastInitHitList * init_hitlist,
                       BlastUngappedStats * ungapped_stats)
{
    Int4 hitsfound, total_hits = 0;
    Int4 hits_extended = 0;
    TNaScanSubjectFunction scansub = NULL;
    TNaExtendFunction extend = NULL;
    Int4 scan_range[3];
    Int4 word_length;
    Int4 lut_word_length;

    if (lookup_wrap->lut_type == eSmallNaLookupTable) {
        BlastSmallNaLookupTable *lookup = 
                                (BlastSmallNaLookupTable *) lookup_wrap->lut;
        word_length = lookup->word_length;
        lut_word_length = lookup->lut_word_length;
        scansub = (TNaScanSubjectFunction)lookup->scansub_callback;
        extend = (TNaExtendFunction)lookup->extend_callback;
    }
    else if (lookup_wrap->lut_type == eMBLookupTable) {
        BlastMBLookupTable *lookup = 
                                (BlastMBLookupTable *) lookup_wrap->lut;
        if (lookup->discontiguous) {
            word_length = lookup->template_length;
            lut_word_length = lookup->template_length;
        } else {
            word_length = lookup->word_length;
            lut_word_length = lookup->lut_word_length;
        }
        scansub = (TNaScanSubjectFunction)lookup->scansub_callback;
        extend = (TNaExtendFunction)lookup->extend_callback;
    }
    else {
        BlastNaLookupTable *lookup = 
                                (BlastNaLookupTable *) lookup_wrap->lut;
        word_length = lookup->word_length;
        lut_word_length = lookup->lut_word_length;
        scansub = (TNaScanSubjectFunction)lookup->scansub_callback;
        extend = (TNaExtendFunction)lookup->extend_callback;
    }

    scan_range[0] = 0;  /* subject seq mask index */
    scan_range[1] = 0;	/* start pos of scan */
    scan_range[2] = subject->length - lut_word_length; /*end pos (inclusive) of scan*/

    /* if sequence is masked, fall back to generic scanner and extender */
    if (subject->mask_type != eNoSubjMasking) {
        scansub = (TNaScanSubjectFunction) 
                  BlastChooseNucleotideScanSubjectAny(lookup_wrap);
        if (extend != (TNaExtendFunction)s_BlastNaExtendDirect) {
             extend = (lookup_wrap->lut_type == eSmallNaLookupTable) 
                    ? (TNaExtendFunction)s_BlastSmallNaExtend
                    : (TNaExtendFunction)s_BlastNaExtend;
        }
        /* generic scanner permits any (non-aligned) starting offset */
        scan_range[1] = subject->seq_ranges[0].left + word_length - lut_word_length;
        scan_range[2] = subject->seq_ranges[0].right - lut_word_length;
    }

    ASSERT(scansub);
    ASSERT(extend);

    while(s_DetermineScanningOffsets(subject, word_length, lut_word_length, scan_range)) {

        hitsfound = scansub(lookup_wrap, subject, offset_pairs, max_hits, &scan_range[1]);

        if (hitsfound == 0)
            continue;

        total_hits += hitsfound;
        hits_extended += extend(offset_pairs, hitsfound, word_params,
                                lookup_wrap, query, subject, matrix, 
                                query_info, ewp, init_hitlist, scan_range[2] + lut_word_length);
    }

    Blast_ExtendWordExit(ewp, subject->length);

    Blast_UngappedStatsUpdate(ungapped_stats, total_hits, hits_extended,
                              init_hitlist->total);

    if (word_params->ungapped_extension)
        Blast_InitHitListSortByScore(init_hitlist);

    return 0;
}

Int2 MB_IndexedWordFinder( 
        BLAST_SequenceBlk * subject,
        BLAST_SequenceBlk * query,
        BlastQueryInfo * query_info,
        LookupTableWrap * lookup_wrap,
        Int4 ** matrix,
        const BlastInitialWordParameters * word_params,
        Blast_ExtendWord * ewp,
        BlastOffsetPair * offset_pairs,
        Int4 max_hits,
        BlastInitHitList * init_hitlist,
        BlastUngappedStats * ungapped_stats)
{ 
    BlastInitHSP * hsp, * new_hsp, * hsp_end;
    BlastUngappedData dummy_ungapped_data;
    BlastUngappedData * ungapped_data = 0;
    ir_diag_hash * hash = 0;
    ir_hash_entry * e = 0;
    Uint4 word_size;
    Uint4 q_off, s_off;
    Uint4 diag, key;
    Int4 oid = subject->oid;
    Int4 chunk = subject->chunk;
    Int4 context;
    BlastUngappedCutoffs *cutoffs;
    T_MB_IdbGetResults get_results = 
                        (T_MB_IdbGetResults)lookup_wrap->read_indexed_db;
    ASSERT(get_results);
    word_size = get_results(lookup_wrap->lut, oid, chunk, init_hitlist);

    if( word_size > 0 && word_params->ungapped_extension ) {
        hash = ir_hash_create();
        new_hsp = hsp = init_hitlist->init_hsp_array;
        hsp_end = hsp + init_hitlist->total;

        for( ; hsp < hsp_end; ++hsp ) {
            q_off = hsp->offsets.qs_offsets.q_off;
            s_off = hsp->offsets.qs_offsets.s_off;
            diag = IR_DIAG( q_off, s_off );
            key  = IR_KEY( diag );
            e = IR_LOCATE( hash, diag, key );
            if( e != 0 ) {
                if( q_off + word_size - 1 > e->diag_data.qend ) {
                    context = BSearchContextInfo(q_off, query_info);
                    cutoffs = word_params->cutoffs + context;
                    s_NuclUngappedExtend( 
                            query, subject, matrix, 
                            q_off, s_off + word_size, s_off,
                            -(cutoffs->x_dropoff), &dummy_ungapped_data,
                            word_params->nucl_score_table,
                            cutoffs->reduced_nucl_cutoff_score);

                    if( dummy_ungapped_data.score >= cutoffs->cutoff_score ) {
                        ungapped_data = 
                            (BlastUngappedData *)malloc(sizeof(BlastUngappedData));
                        *ungapped_data = dummy_ungapped_data;
                        if( new_hsp != hsp ) *new_hsp = *hsp;
                        new_hsp->ungapped_data = ungapped_data;
                        ++new_hsp;
                    }

                    if( e->diag_data.diag != diag ) e->diag_data.diag = diag;
                    e->diag_data.qend = dummy_ungapped_data.q_start + dummy_ungapped_data.length - 1;
                }
            }
            else {
                if( new_hsp != hsp ) *new_hsp = *hsp;
                ++new_hsp;
            }
        }

        init_hitlist->total = new_hsp - init_hitlist->init_hsp_array;
        hash = ir_hash_destroy( hash );
    }

    if (word_params->ungapped_extension)
        Blast_InitHitListSortByScore(init_hitlist);

    return 0;
}

void BlastChooseNaExtend(LookupTableWrap * lookup_wrap)
{
    if (lookup_wrap->lut_type == eMBLookupTable) {
        BlastMBLookupTable *lut;
        lookup_wrap->lookup_callback = (void *)s_MBLookup;
        lut = (BlastMBLookupTable *) lookup_wrap->lut;

        if (lut->lut_word_length == lut->word_length || lut->discontiguous)
            lut->extend_callback = (void *)s_BlastNaExtendDirect;
        else if (lut->lut_word_length % COMPRESSION_RATIO == 0 &&
                 lut->scan_step % COMPRESSION_RATIO == 0)
            lut->extend_callback = (void *)s_BlastNaExtendAligned;
        else
            lut->extend_callback = (void *)s_BlastNaExtend;
    } 
    else if (lookup_wrap->lut_type == eSmallNaLookupTable) {
        BlastSmallNaLookupTable *lut;
        lookup_wrap->lookup_callback = (void *)s_SmallNaLookup;
        lut = (BlastSmallNaLookupTable *) lookup_wrap->lut;

        if (lut->lut_word_length == lut->word_length)
            lut->extend_callback = (void *)s_BlastNaExtendDirect;
        else if (lut->lut_word_length % COMPRESSION_RATIO == 0 &&
                 lut->scan_step % COMPRESSION_RATIO == 0 &&
                 lut->word_length - lut->lut_word_length <= 4)
            lut->extend_callback = (void *)s_BlastSmallNaExtendAlignedOneByte;
        else
            lut->extend_callback = (void *)s_BlastSmallNaExtend;
    }
    else {
        BlastNaLookupTable *lut;
        lookup_wrap->lookup_callback = (void *)s_NaLookup;
        lut = (BlastNaLookupTable *) lookup_wrap->lut;

        if (lut->lut_word_length == lut->word_length)
            lut->extend_callback = (void *)s_BlastNaExtendDirect;
        else if (lut->lut_word_length % COMPRESSION_RATIO == 0 &&
                 lut->scan_step % COMPRESSION_RATIO == 0)
            lut->extend_callback = (void *)s_BlastNaExtendAligned;
        else
            lut->extend_callback = (void *)s_BlastNaExtend;
    }
}
