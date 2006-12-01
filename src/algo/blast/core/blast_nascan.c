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

/** @file blast_nascan.c
 * Functions for scanning nucleotide BLAST lookup tables.
 */

#include <algo/blast/core/blast_nalookup.h>
#include <algo/blast/core/blast_nascan.h>
#include <algo/blast/core/blast_util.h> /* for NCBI2NA_UNPACK_BASE */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif                          /* SKIP_DOXYGEN_PROCESSING */

/** shift amount that covers 4 bases */
#define FULL_BYTE_SHIFT 8

/**
* Retrieve the number of query offsets associated with this subject word.
* @param lookup The lookup table to read from. [in]
* @param index The index value of the word to retrieve. [in]
* @return The number of query offsets associated with this subject word.
*/
static NCBI_INLINE Int4 s_BlastLookupGetNumHits(BlastNaLookupTable * lookup,
                                                Int4 index)
{
    PV_ARRAY_TYPE *pv = lookup->pv;
    if (PV_TEST(pv, index, PV_ARRAY_BTS))
        return lookup->thick_backbone[index].num_used;
    else
        return 0;
}

/** 
* Copy query offsets from the lookup table to the array of offset pairs.
* @param lookup The lookup table to read from. [in]
* @param index The index value of the word to retrieve. [in]
* @param offset_pairs A pointer into the destination array. [out]
* @param s_off The subject offset to be associated with the retrieved query offset(s). [in]
*/
static NCBI_INLINE void s_BlastLookupRetrieve(BlastNaLookupTable * lookup,
                                              Int4 index,
                                              BlastOffsetPair * offset_pairs,
                                              Int4 s_off)
{
    Int4 *lookup_pos;
    Int4 num_hits = lookup->thick_backbone[index].num_used;
    Int4 i;

    /* determine if hits live in the backbone or the overflow array */

    if (num_hits <= NA_HITS_PER_CELL)
        lookup_pos = lookup->thick_backbone[index].payload.entries;
    else
        lookup_pos = lookup->overflow +
            lookup->thick_backbone[index].payload.overflow_cursor;

    /* Copy the (query,subject) offset pairs to the destination. */
    for(i=0;i<num_hits;i++) {
        offset_pairs[i].qs_offsets.q_off = lookup_pos[i];
        offset_pairs[i].qs_offsets.s_off = s_off;
    }

    return;
}

Int4 BlastNaScanSubject_AG(const LookupTableWrap * lookup_wrap,
                           const BLAST_SequenceBlk * subject,
                           Int4 start_offset,
                           BlastOffsetPair * NCBI_RESTRICT offset_pairs,
                           Int4 max_hits, Int4 * end_offset)
{
    BlastNaLookupTable *lookup;
    Uint1 *s;
    Uint1 *abs_start;
    Int4 s_off;
    Int4 num_hits;
    Int4 index;
    Int4 total_hits = 0;
    Int4 scan_step;
    Int4 mask;
    Int4 lut_word_length;
    Int4 last_offset;

    ASSERT(lookup_wrap->lut_type == eNaLookupTable);
    lookup = (BlastNaLookupTable *) lookup_wrap->lut;

    abs_start = subject->sequence;
    mask = lookup->mask;
    scan_step = lookup->scan_step;
    lut_word_length = lookup->lut_word_length;
    last_offset = subject->length - lut_word_length;

    ASSERT(lookup->scan_step > 0);

    if (lut_word_length > 5) {

        /* perform scanning for lookup tables of width 6, 7, and 8. These
           widths require two bytes of the compressed subject sequence, and
           possibly a third if the word is not aligned on a 4-base boundary */

        if (scan_step % COMPRESSION_RATIO == 0) {

            /* for strides that are a multiple of 4, words are always aligned 
               and two bytes of the subject sequence will always hold a
               complete word (plus possible extra bases that must be shifted
               away). s_end below always points to the second-to-last byte of
               subject, so we will never fetch the byte beyond the end of
               subject */

            Uint1 *s_end = abs_start + last_offset / COMPRESSION_RATIO;
            Int4 shift = 2 * (FULL_BYTE_SHIFT - lut_word_length);
            s = abs_start + start_offset / COMPRESSION_RATIO;
            scan_step = scan_step / COMPRESSION_RATIO;

            for (; s <= s_end; s += scan_step) {
                index = s[0] << 8 | s[1];
                index = index >> shift;

                num_hits = s_BlastLookupGetNumHits(lookup, index);
                if (num_hits == 0)
                    continue;
                if (num_hits > (max_hits - total_hits))
                    break;

                s_BlastLookupRetrieve(lookup,
                                      index,
                                      offset_pairs + total_hits,
                                      (s - abs_start) * COMPRESSION_RATIO);
                total_hits += num_hits;
            }
            *end_offset = (s - abs_start) * COMPRESSION_RATIO;
        } else {
            /* when the stride is not a multiple of 4, extra bases may occur
               both before and after every word read from the subject
               sequence. The portion of each 12-base region that contains the 
               actual word depends on the offset of the word and the lookup
               table width, and must be recalculated for each 12-base region

               Unlike the aligned stride case, the scanning can walk off the
               subject array for lut_word_length = 6 or 7 (length 8 may also
               do this, but only if the subject is a multiple of 4 bases in
               size, and in that case there is a sentinel byte). We avoid
               this by first handling all the cases where 12-base regions
               fit, then handling the last few offsets separately */

            Int4 last_offset3 = last_offset;
            switch (subject->length % COMPRESSION_RATIO) {
            case 2:
                if (lut_word_length == 6)
                    last_offset3--;
                break;
            case 3:
                if (lut_word_length == 6)
                    last_offset3 -= 2;
                else if (lut_word_length == 7)
                    last_offset3--;
                break;
            }

            for (s_off = start_offset; s_off <= last_offset3;
                 s_off += scan_step) {

                Int4 shift =
                    2 * (12 - (s_off % COMPRESSION_RATIO + lut_word_length));
                s = abs_start + (s_off / COMPRESSION_RATIO);

                index = s[0] << 16 | s[1] << 8 | s[2];
                index = (index >> shift) & mask;

                num_hits = s_BlastLookupGetNumHits(lookup, index);
                if (num_hits == 0)
                    continue;
                if (num_hits > (max_hits - total_hits))
                    break;

                s_BlastLookupRetrieve(lookup,
                                      index,
                                      offset_pairs + total_hits, s_off);
                total_hits += num_hits;
            }

            /* repeat the loop but only read two bytes at a time. For
               lut_word_length = 6 the loop runs at most twice, and for
               lut_word_length = 7 it runs at most once */

            for (; s_off > last_offset3 && s_off <= last_offset;
                 s_off += scan_step) {

                Int4 shift =
                    2 * (8 - (s_off % COMPRESSION_RATIO + lut_word_length));
                s = abs_start + (s_off / COMPRESSION_RATIO);

                index = s[0] << 8 | s[1];
                index = (index >> shift) & mask;

                num_hits = s_BlastLookupGetNumHits(lookup, index);
                if (num_hits == 0)
                    continue;
                if (num_hits > (max_hits - total_hits))
                    break;

                s_BlastLookupRetrieve(lookup,
                                      index,
                                      offset_pairs + total_hits, s_off);
                total_hits += num_hits;
            }
            *end_offset = s_off;
        }
    } else {
        /* perform scanning for lookup tables of width 4 and 5. Here the
           stride will never be a multiple of 4 (these tables are only used
           for very small word sizes). The last word is always two bytes from 
           the end of the sequence, unless the table width is 4 and subect is 
           a multiple of 4 bases; in that case there is a sentinel byte, so
           the following does not need to be corrected when scanning near the 
           end of the subject sequence */

        for (s_off = start_offset; s_off <= last_offset; s_off += scan_step) {

            Int4 shift =
                2 * (8 - (s_off % COMPRESSION_RATIO + lut_word_length));
            s = abs_start + (s_off / COMPRESSION_RATIO);

            index = s[0] << 8 | s[1];
            index = (index >> shift) & mask;

            num_hits = s_BlastLookupGetNumHits(lookup, index);
            if (num_hits == 0)
                continue;
            if (num_hits > (max_hits - total_hits))
                break;

            s_BlastLookupRetrieve(lookup,
                                  index,
                                  offset_pairs + total_hits,
                                  s_off);
            total_hits += num_hits;
        }
        *end_offset = s_off;
    }

    return total_hits;
}

Int4 BlastSmallNaScanSubject_AG(const LookupTableWrap * lookup_wrap,
                                const BLAST_SequenceBlk * subject,
                                Int4 start_offset,
                                BlastOffsetPair * NCBI_RESTRICT offset_pairs,
                                Int4 max_hits, Int4 * end_offset)
{
    BlastSmallNaLookupTable *lookup;
    Uint1 *s;
    Uint1 *abs_start;
    Int4 s_off;
    Int4 num_hits;
    Int4 index;
    Int4 total_hits = 0;
    Int4 scan_step;
    Int4 mask;
    Int4 lut_word_length;
    Int4 last_offset;
    Int2 *backbone;
    Int2 *overflow;

    ASSERT(lookup_wrap->lut_type == eSmallNaLookupTable);
    lookup = (BlastSmallNaLookupTable *) lookup_wrap->lut;

    abs_start = subject->sequence;
    mask = lookup->mask;
    scan_step = lookup->scan_step;
    lut_word_length = lookup->lut_word_length;
    last_offset = subject->length - lut_word_length;

    backbone = lookup->final_backbone;
    overflow = lookup->overflow;
    max_hits -= lookup->longest_chain;

    ASSERT(lookup->scan_step > 0);

    if (lut_word_length > 5) {

        /* perform scanning for lookup tables of width 6, 7, and 8. These
           widths require two bytes of the compressed subject sequence, and
           possibly a third if the word is not aligned on a 4-base boundary */

        if (scan_step % COMPRESSION_RATIO == 0) {

            /* for strides that are a multiple of 4, words are always aligned 
               and two bytes of the subject sequence will always hold a
               complete word (plus possible extra bases that must be shifted
               away). s_end below always points to the second-to-last byte of
               subject, so we will never fetch the byte beyond the end of
               subject */

            Uint1 *s_end = abs_start + last_offset / COMPRESSION_RATIO;
            Int4 shift = 2 * (FULL_BYTE_SHIFT - lut_word_length);
            s = abs_start + start_offset / COMPRESSION_RATIO;
            scan_step = scan_step / COMPRESSION_RATIO;

            for (; s <= s_end; s += scan_step) {
                index = s[0] << 8 | s[1];
                index = backbone[index >> shift];
                if (index == -1)
                    continue;
                if (total_hits > max_hits)
                    break;
        
                s_off = (s - abs_start) * COMPRESSION_RATIO;
        
                if (index >= 0) {
                    offset_pairs[total_hits].qs_offsets.q_off = index;
                    offset_pairs[total_hits].qs_offsets.s_off = s_off;
                    num_hits = 1;
                }
                else {
                    Int4 src_off = -(index + 2);
                    index = overflow[src_off++];
                    num_hits = 0;
                    do {
                        offset_pairs[total_hits+num_hits].qs_offsets.q_off = index;
                        offset_pairs[total_hits+num_hits].qs_offsets.s_off = s_off;
                        num_hits++;
                        index = overflow[src_off++];
                    } while (index >= 0);
                }
                total_hits += num_hits;
            }
            *end_offset = (s - abs_start) * COMPRESSION_RATIO;
        } else {
            /* when the stride is not a multiple of 4, extra bases may occur
               both before and after every word read from the subject
               sequence. The portion of each 12-base region that contains the 
               actual word depends on the offset of the word and the lookup
               table width, and must be recalculated for each 12-base region

               Unlike the aligned stride case, the scanning can walk off the
               subject array for lut_word_length = 6 or 7 (length 8 may also
               do this, but only if the subject is a multiple of 4 bases in
               size, and in that case there is a sentinel byte). We avoid
               this by first handling all the cases where 12-base regions
               fit, then handling the last few offsets separately */

            Int4 last_offset3 = last_offset;
            switch (subject->length % COMPRESSION_RATIO) {
            case 2:
                if (lut_word_length == 6)
                    last_offset3--;
                break;
            case 3:
                if (lut_word_length == 6)
                    last_offset3 -= 2;
                else if (lut_word_length == 7)
                    last_offset3--;
                break;
            }

            for (s_off = start_offset; s_off <= last_offset3;
                 s_off += scan_step) {

                Int4 shift =
                    2 * (12 - (s_off % COMPRESSION_RATIO + lut_word_length));
                s = abs_start + (s_off / COMPRESSION_RATIO);

                index = s[0] << 16 | s[1] << 8 | s[2];
                index = backbone[(index >> shift) & mask];

                if (index == -1)
                    continue;
                if (total_hits > max_hits)
                    break;
        
                if (index >= 0) {
                    offset_pairs[total_hits].qs_offsets.q_off = index;
                    offset_pairs[total_hits].qs_offsets.s_off = s_off;
                    num_hits = 1;
                }
                else {
                    Int4 src_off = -(index + 2);
                    index = overflow[src_off++];
                    num_hits = 0;
                    do {
                        offset_pairs[total_hits+num_hits].qs_offsets.q_off = index;
                        offset_pairs[total_hits+num_hits].qs_offsets.s_off = s_off;
                        num_hits++;
                        index = overflow[src_off++];
                    } while (index >= 0);
                }
                total_hits += num_hits;
            }

            /* repeat the loop but only read two bytes at a time. For
               lut_word_length = 6 the loop runs at most twice, and for
               lut_word_length = 7 it runs at most once */

            for (; s_off > last_offset3 && s_off <= last_offset;
                 s_off += scan_step) {

                Int4 shift =
                    2 * (8 - (s_off % COMPRESSION_RATIO + lut_word_length));
                s = abs_start + (s_off / COMPRESSION_RATIO);

                index = s[0] << 8 | s[1];
                index = backbone[(index >> shift) & mask];

                if (index == -1)
                    continue;
                if (total_hits > max_hits)
                    break;
        
                if (index >= 0) {
                    offset_pairs[total_hits].qs_offsets.q_off = index;
                    offset_pairs[total_hits].qs_offsets.s_off = s_off;
                    num_hits = 1;
                }
                else {
                    Int4 src_off = -(index + 2);
                    index = overflow[src_off++];
                    num_hits = 0;
                    do {
                        offset_pairs[total_hits+num_hits].qs_offsets.q_off = index;
                        offset_pairs[total_hits+num_hits].qs_offsets.s_off = s_off;
                        num_hits++;
                        index = overflow[src_off++];
                    } while (index >= 0);
                }
                total_hits += num_hits;
            }
            *end_offset = s_off;
        }
    } else {
        /* perform scanning for lookup tables of width 4 and 5. Here the
           stride will never be a multiple of 4 (these tables are only used
           for very small word sizes). The last word is always two bytes from 
           the end of the sequence, unless the table width is 4 and subect is 
           a multiple of 4 bases; in that case there is a sentinel byte, so
           the following does not need to be corrected when scanning near the 
           end of the subject sequence */

        for (s_off = start_offset; s_off <= last_offset; s_off += scan_step) {

            Int4 shift =
                2 * (8 - (s_off % COMPRESSION_RATIO + lut_word_length));
            s = abs_start + (s_off / COMPRESSION_RATIO);

            index = s[0] << 8 | s[1];
            index = backbone[(index >> shift) & mask];

            if (index == -1)
                continue;
            if (total_hits > max_hits)
                break;
    
            if (index >= 0) {
                offset_pairs[total_hits].qs_offsets.q_off = index;
                offset_pairs[total_hits].qs_offsets.s_off = s_off;
                num_hits = 1;
            }
            else {
                Int4 src_off = -(index + 2);
                index = overflow[src_off++];
                num_hits = 0;
                do {
                    offset_pairs[total_hits+num_hits].qs_offsets.q_off = index;
                    offset_pairs[total_hits+num_hits].qs_offsets.s_off = s_off;
                    num_hits++;
                    index = overflow[src_off++];
                } while (index >= 0);
            }
            total_hits += num_hits;
        }
        *end_offset = s_off;
    }

    return total_hits;
}

Int4 BlastNaScanSubject(const LookupTableWrap * lookup_wrap,
                        const BLAST_SequenceBlk * subject,
                        Int4 start_offset,
                        BlastOffsetPair * NCBI_RESTRICT offset_pairs,
                        Int4 max_hits, Int4 * end_offset)
{
    Uint1 *s;
    Uint1 *abs_start, *s_end;
    BlastNaLookupTable *lookup;
    Int4 num_hits;
    Int4 total_hits = 0;
    Int4 lut_word_length;

    ASSERT(lookup_wrap->lut_type == eNaLookupTable);
    lookup = (BlastNaLookupTable *) lookup_wrap->lut;

    lut_word_length = lookup->lut_word_length;
    ASSERT(lut_word_length == 8);

    abs_start = subject->sequence;
    s = abs_start + start_offset / COMPRESSION_RATIO;
    s_end = abs_start + (subject->length - lut_word_length) /
        COMPRESSION_RATIO;

    for (; s <= s_end; s++) {

        Int4 index = s[0] << 8 | s[1];

        num_hits = s_BlastLookupGetNumHits(lookup, index);
        if (num_hits == 0)
            continue;
        if (num_hits > (max_hits - total_hits))
            break;

        s_BlastLookupRetrieve(lookup,
                              index,
                              offset_pairs + total_hits,
                              (s - abs_start) * COMPRESSION_RATIO);
        total_hits += num_hits;
    }
    *end_offset = (s - abs_start) * COMPRESSION_RATIO;

    return total_hits;
}

Int4 BlastSmallNaScanSubject(const LookupTableWrap * lookup_wrap,
                             const BLAST_SequenceBlk * subject,
                             Int4 start_offset,
                             BlastOffsetPair * NCBI_RESTRICT offset_pairs,
                             Int4 max_hits, Int4 * end_offset)
{
    Uint1 *s;
    Uint1 *abs_start, *s_end;
    BlastSmallNaLookupTable *lookup;
    Int4 total_hits = 0;
    Int4 lut_word_length;
    Int2 *backbone;
    Int2 *overflow;

    ASSERT(lookup_wrap->lut_type == eSmallNaLookupTable);
    lookup = (BlastSmallNaLookupTable *) lookup_wrap->lut;

    lut_word_length = lookup->lut_word_length;
    ASSERT(lut_word_length == 8);

    abs_start = subject->sequence;
    s = abs_start + start_offset / COMPRESSION_RATIO;
    s_end = abs_start + (subject->length - lut_word_length) /
        COMPRESSION_RATIO;

    backbone = lookup->final_backbone;
    overflow = lookup->overflow;
    max_hits -= lookup->longest_chain;

    for (; s <= s_end; s++) {

        Int4 index = s[0] << 8 | s[1];
        Int4 num_hits, s_off;

        index = backbone[index];
        if (index == -1)
            continue;

        if (total_hits > max_hits)
            break;

        s_off = (s - abs_start) * COMPRESSION_RATIO;

        if (index >= 0) {
            offset_pairs[total_hits].qs_offsets.q_off = index;
            offset_pairs[total_hits].qs_offsets.s_off = s_off;
            num_hits = 1;
        }
        else {
            Int4 src_off = -(index + 2);
            index = overflow[src_off++];
            num_hits = 0;
            do {
                offset_pairs[total_hits+num_hits].qs_offsets.q_off = index;
                offset_pairs[total_hits+num_hits].qs_offsets.s_off = s_off;
                num_hits++;
                index = overflow[src_off++];
            } while (index >= 0);
        }
            
        total_hits += num_hits;
    }
    *end_offset = (s - abs_start) * COMPRESSION_RATIO;

    return total_hits;
}

/**
* Determine if this subject word occurs in the query.
* @param lookup The lookup table to read from. [in]
* @param index The index value of the word to retrieve. [in]
* @return 1 if there are hits, 0 otherwise.
*/
static NCBI_INLINE Int4 s_BlastMBLookupHasHits(BlastMBLookupTable * lookup,
                                               Int4 index)
{
    PV_ARRAY_TYPE *pv = lookup->pv_array;
    Int4 pv_array_bts = lookup->pv_array_bts;

    if (PV_TEST(pv, index, pv_array_bts))
        return 1;
    else
        return 0;
}

/** 
* Copy query offsets from the lookup table to the array of offset pairs.
* @param lookup The lookup table to read from. [in]
* @param index The index value of the word to retrieve. [in]
* @param offset_pairs A pointer into the destination array. [out]
* @param s_off The subject offset to be associated with the retrieved query offset(s). [in]
* @return The number of hits copied.
*/
static NCBI_INLINE Int4 s_BlastMBLookupRetrieve(BlastMBLookupTable * lookup,
                                                Int4 index,
                                                BlastOffsetPair * offset_pairs,
                                                Int4 s_off)
{
    Int4 i=0;
    Int4 q_off = lookup->hashtable[index];

    while (q_off) {
        offset_pairs[i].qs_offsets.q_off   = q_off - 1;
        offset_pairs[i++].qs_offsets.s_off = s_off;
        q_off = lookup->next_pos[q_off];
    }
    return i;
}

Int4 MB_AG_ScanSubject(const LookupTableWrap* lookup_wrap,
       const BLAST_SequenceBlk* subject, Int4 start_offset,
       BlastOffsetPair* NCBI_RESTRICT offset_pairs, Int4 max_hits,  
       Int4* end_offset)
{
   BlastMBLookupTable* mb_lt;
   Uint1* s;
   Uint1* abs_start;
   Int4 s_off;
   Int4 last_offset;
   Int4 index;
   Int4 mask;
   PV_ARRAY_TYPE *pv_array;
   Int4 total_hits = 0;
   Int4 lut_word_length;
   Int4 scan_step;
   Uint1 pv_array_bts;
   
   ASSERT(lookup_wrap->lut_type == eMBLookupTable);
   mb_lt = (BlastMBLookupTable*) lookup_wrap->lut;

   pv_array = mb_lt->pv_array;
   pv_array_bts = mb_lt->pv_array_bts;

   /* Since the test for number of hits here is done after adding them, 
      subtract the longest chain length from the allowed offset array size. */
   max_hits -= mb_lt->longest_chain;

   abs_start = subject->sequence;
   lut_word_length = mb_lt->lut_word_length;
   mask = mb_lt->hashsize - 1;
   scan_step = mb_lt->scan_step;
   last_offset = subject->length - lut_word_length;
   ASSERT(lut_word_length == 9 || 
          lut_word_length == 10 ||
          lut_word_length == 11 ||
          lut_word_length == 12);

   if (lut_word_length > 9) {
      /* perform scanning for lookup tables of width 10, 11, and 12.
         These widths require three bytes of the compressed subject
         sequence, and possibly a fourth if the word is not known
         to be aligned on a 4-base boundary */

      if (scan_step % COMPRESSION_RATIO == 0) {

         /* for strides that are a multiple of 4, words are
            always aligned and three bytes of the subject sequence 
            will always hold a complete word (plus possible extra bases 
            that must be shifted away). s_end below always points to
            the third-to-last byte of the subject sequence, so we will
            never fetch the byte beyond the end of subject */

         Uint1* s_end = abs_start + (subject->length - lut_word_length) /
                                                   COMPRESSION_RATIO;
         Int4 shift = 2 * (12 - lut_word_length);
         s = abs_start + start_offset / COMPRESSION_RATIO;
         scan_step = scan_step / COMPRESSION_RATIO;
   
         for ( ; s <= s_end; s += scan_step) {
            
            index = s[0] << 16 | s[1] << 8 | s[2];
            index = index >> shift;
      
            if (s_BlastMBLookupHasHits(mb_lt, index)) {
                if (total_hits >= max_hits)
                    break;
                s_off = (s - abs_start)*COMPRESSION_RATIO;
                total_hits += s_BlastMBLookupRetrieve(mb_lt,
                    index,
                    offset_pairs + total_hits,
                    s_off);
            }
         }
         *end_offset = (s - abs_start)*COMPRESSION_RATIO;
      }
      else {
         /* when the stride is not a multiple of 4, extra bases
            may occur both before and after every word read from
            the subject sequence. The portion of each 16-base
            region that contains the actual word depends on the
            offset of the word and the lookup table width, and
            must be recalculated for each 16-base region
          
            Unlike the aligned stride case, the scanning can
            walk off the subject array for lut_word_length = 10 or 11
            (length 12 may also do this, but only if the subject is a
            multiple of 4 bases in size, and in that case there is 
            a sentinel byte). We avoid this by first handling all 
            the cases where 16-base regions fit, then handling the 
            last few offsets separately */

         Int4 last_offset4 = last_offset;
         switch (subject->length % COMPRESSION_RATIO) {
         case 2:
             if (lut_word_length == 10)
                 last_offset4--;
             break;
         case 3:
             if (lut_word_length == 10)
                 last_offset4 -= 2;
             else if (lut_word_length == 11)
                 last_offset4--;
             break;
         default:
             break;
         }

         for (s_off = start_offset; s_off <= last_offset4; s_off += scan_step) {
      
            Int4 shift = 2*(16 - (s_off % COMPRESSION_RATIO + lut_word_length));
            s = abs_start + (s_off / COMPRESSION_RATIO);
      
            index = s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3];
            index = (index >> shift) & mask;
      
            if (s_BlastMBLookupHasHits(mb_lt, index)) {
                if (total_hits >= max_hits)
                    break;
                total_hits += s_BlastMBLookupRetrieve(mb_lt,
                    index,
                    offset_pairs + total_hits,
                    s_off);
            }
         }

         /* repeat the loop but only read three bytes at a time. For
            lut_word_length = 10 the loop runs at most twice, and for
            lut_word_length = 11 it runs at most once */

         for (; s_off > last_offset4 && s_off <= last_offset; 
                                                s_off += scan_step) {
      
            Int4 shift = 2*(12 - (s_off % COMPRESSION_RATIO + lut_word_length));
            s = abs_start + (s_off / COMPRESSION_RATIO);
      
            index = s[0] << 16 | s[1] << 8 | s[2];
            index = (index >> shift) & mask;
      
            if (s_BlastMBLookupHasHits(mb_lt, index)) {
                if (total_hits >= max_hits)
                    break;
                total_hits += s_BlastMBLookupRetrieve(mb_lt,
                    index,
                    offset_pairs + total_hits,
                    s_off);
            }
         }
         *end_offset = s_off;
      }
   }
   else {
      /* perform scanning for a lookup tables of width 9.
         Here the stride will never be a multiple of 4 
         (this table is only used for small word sizes).
         The last word is always three bytes from the end of the 
         sequence, so the following does not need to be corrected 
         when scanning near the end of the subject sequence */

      for (s_off = start_offset; s_off <= last_offset; s_off += scan_step) {
   
         Int4 shift = 2*(12 - (s_off % COMPRESSION_RATIO + lut_word_length));
         s = abs_start + (s_off / COMPRESSION_RATIO);
   
         index = s[0] << 16 | s[1] << 8 | s[2];
         index = (index >> shift) & mask;
   
         if (s_BlastMBLookupHasHits(mb_lt, index)) {
             if (total_hits >= max_hits)
                 break;
             total_hits += s_BlastMBLookupRetrieve(mb_lt,
                 index,
                 offset_pairs + total_hits,
                 s_off);
         }
      }
      *end_offset = s_off;
   }

   return total_hits;
}

Int4 MB_DiscWordScanSubject(const LookupTableWrap* lookup, 
       const BLAST_SequenceBlk* subject, Int4 start_offset,
       BlastOffsetPair* NCBI_RESTRICT offset_pairs, Int4 max_hits, 
       Int4* end_offset)
{
   Uint1* s;
   Int4 total_hits = 0;
   Uint4 q_off, s_off;
   Int4 index, index2=0;
   Uint8 accum;
   Boolean two_templates;
   BlastMBLookupTable* mb_lt;
   EDiscTemplateType template_type;
   EDiscTemplateType second_template_type;
   PV_ARRAY_TYPE *pv_array;
   Uint1 pv_array_bts;
   Uint4 template_length;
   Uint4 curr_base;
   Uint4 last_base;

   ASSERT(lookup->lut_type == eMBLookupTable);
   mb_lt = (BlastMBLookupTable*) lookup->lut;

   /* Since the test for number of hits here is done after adding them, 
      subtract the longest chain length from the allowed offset array size. */
   max_hits -= mb_lt->longest_chain;

   two_templates = mb_lt->two_templates;
   template_type = mb_lt->template_type;
   second_template_type = mb_lt->second_template_type;
   pv_array = mb_lt->pv_array;
   pv_array_bts = mb_lt->pv_array_bts;
   template_length = mb_lt->template_length;
   last_base = *end_offset;

   s = subject->sequence + start_offset / COMPRESSION_RATIO;
   accum = (Uint8)(*s++);
   curr_base = COMPRESSION_RATIO - (start_offset % COMPRESSION_RATIO);

   if (mb_lt->full_byte_scan) {

      /* The accumulator is filled with the first (template_length -
         COMPRESSION_RATIO) bases to scan, 's' points to the first byte
         of 'sequence' that contains bases not in the accumulator,
         and curr_base is the offset of the next base to add */

      while (curr_base < template_length - COMPRESSION_RATIO) {
         accum = accum << FULL_BYTE_SHIFT | *s++;
         curr_base += COMPRESSION_RATIO;
      }
      if (curr_base > template_length - COMPRESSION_RATIO) {
         accum = accum >> (2 * (curr_base - 
                    (template_length - COMPRESSION_RATIO)));
         s--;
      }
      curr_base = start_offset + template_length;

      while (curr_base <= last_base) {
          
         /* add the next 4 bases to the accumulator. These are
            shifted into the low-order 8 bits. curr_base was already
            incremented, but that doesn't change how the next bases
            are shifted in */

         accum = (accum << FULL_BYTE_SHIFT);
         switch (curr_base % COMPRESSION_RATIO) {
         case 0: accum |= s[0]; break;
         case 1: accum |= (s[0] << 2 | s[1] >> 6); break;
         case 2: accum |= (s[0] << 4 | s[1] >> 4); break;
         case 3: accum |= (s[0] << 6 | s[1] >> 2); break;
         }
         s++;

         index = ComputeDiscontiguousIndex(accum, template_type);
  
         if (PV_TEST(pv_array, index, pv_array_bts)) {
            q_off = mb_lt->hashtable[index];
            s_off = curr_base - template_length;
            while (q_off) {
               offset_pairs[total_hits].qs_offsets.q_off = q_off - 1;
               offset_pairs[total_hits++].qs_offsets.s_off = s_off;
               q_off = mb_lt->next_pos[q_off];
            }
         }
         if (two_templates) {
            index2 = ComputeDiscontiguousIndex(accum, second_template_type);

            if (PV_TEST(pv_array, index2, pv_array_bts)) {
               q_off = mb_lt->hashtable2[index2];
               s_off = curr_base - template_length;
               while (q_off) {
                  offset_pairs[total_hits].qs_offsets.q_off = q_off - 1;
                  offset_pairs[total_hits++].qs_offsets.s_off = s_off;
                  q_off = mb_lt->next_pos2[q_off];
               }
            }
         }

         curr_base += COMPRESSION_RATIO;

         /* test for buffer full. This test counts 
            for both templates (they both add hits 
            or neither one does) */
         if (total_hits >= max_hits)
            break;

      }
   } else {
      const Int4 kScanStep = 1; /* scan one letter at a time. */
      const Int4 kScanShift = 2; /* scan 2 bits (i.e. one base) at a time*/

      /* The accumulator is filled with the first (template_length -
         kScanStep) bases to scan, 's' points to the first byte
         of 'sequence' that contains bases not in the accumulator,
         and curr_base is the offset of the next base to add */

      while (curr_base < template_length - kScanStep) {
         accum = accum << FULL_BYTE_SHIFT | *s++;
         curr_base += COMPRESSION_RATIO;
      }
      if (curr_base > template_length - kScanStep) {
         accum = accum >> (2 * (curr_base - (template_length - kScanStep)));
         s--;
      }
      curr_base = start_offset + template_length;

      while (curr_base <= last_base) {

         /* shift the next base into the low-order bits of the accumulator */
         accum = (accum << kScanShift) | 
                 NCBI2NA_UNPACK_BASE(s[0], 
                         3 - (curr_base - kScanStep) % COMPRESSION_RATIO);
         if (curr_base % COMPRESSION_RATIO == 0)
            s++;

         index = ComputeDiscontiguousIndex(accum, template_type);
       
         if (PV_TEST(pv_array, index, pv_array_bts)) {
            q_off = mb_lt->hashtable[index];
            s_off = curr_base - template_length;
            while (q_off) {
               offset_pairs[total_hits].qs_offsets.q_off = q_off - 1;
               offset_pairs[total_hits++].qs_offsets.s_off = s_off;
               q_off = mb_lt->next_pos[q_off];
            }
         }
         if (two_templates) {
            index2 = ComputeDiscontiguousIndex(accum, second_template_type);

            if (PV_TEST(pv_array, index2, pv_array_bts)) {
               q_off = mb_lt->hashtable2[index2];
               s_off = curr_base - template_length;
               while (q_off) {
                  offset_pairs[total_hits].qs_offsets.q_off = q_off - 1;
                  offset_pairs[total_hits++].qs_offsets.s_off = s_off;
                  q_off = mb_lt->next_pos2[q_off];
               }
            }
         }

         curr_base += kScanStep;

         /* test for buffer full. This test counts 
            for both templates (they both add hits 
            or neither one does) */
         if (total_hits >= max_hits)
            break;
      }
   }
   *end_offset = curr_base - template_length;

   return total_hits;
}

