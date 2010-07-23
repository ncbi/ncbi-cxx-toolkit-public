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

/** @file blast_aascan.c
 * Functions for accessing hits in the protein BLAST lookup table.
 */

#include <algo/blast/core/blast_aascan.h>
#include <algo/blast/core/blast_aalookup.h>
#include "masksubj.inl"

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif                          /* SKIP_DOXYGEN_PROCESSING */

/**
 * Scans the subject sequence from "offset" to the end of the sequence.
 * Copies at most array_size hits.
 * Returns the number of hits found.
 * If there isn't enough room to copy all the hits, return early, and update
 * "offset". 
 *
 * @param lookup_wrap the lookup table [in]
 * @param subject the subject sequence [in]
 * @param offset_pairs Array to which hits will be copied [out]
 * @param array_size length of the offset arrays [in]
 * @return The number of hits found.
 */
static Int4 s_BlastAaScanSubject(const LookupTableWrap * lookup_wrap,
                                 const BLAST_SequenceBlk * subject,
                                 BlastOffsetPair * NCBI_RESTRICT offset_pairs,
                                 Int4 array_size,
                                 Int4 * s_range)
{
    Int4 index;
    Uint1 *s = NULL;
    Uint1 *s_first = NULL;
    Uint1 *s_last = NULL;
    Int4 numhits = 0;           /* number of hits found for a given subject
                                   offset */
    Int4 totalhits = 0;         /* cumulative number of hits found */
    PV_ARRAY_TYPE *pv;
    BlastAaLookupTable *lookup;
    AaLookupBackboneCell *bbc;
    Int4 *ovfl;
    Int4 word_length;

    ASSERT(lookup_wrap->lut_type == eAaLookupTable);
    lookup = (BlastAaLookupTable *) lookup_wrap->lut;
    ASSERT(lookup->bone_type == eBackbone);
    pv = lookup->pv;
    bbc = (AaLookupBackboneCell *) lookup->thick_backbone;
    ovfl = (Int4 *) lookup->overflow;
    word_length = lookup->word_length;

    while (s_DetermineScanningOffsets(subject, word_length, word_length, s_range)) {
    s_first=subject->sequence + s_range[1];
    s_last=subject->sequence + s_range[2];

    /* prime the index */
    index = ComputeTableIndex(word_length - 1,
                              lookup->charsize, s_first);

    for (s = s_first; s <= s_last; s++) {
        /* compute the index value */
        index = ComputeTableIndexIncremental(word_length, 
                                             lookup->charsize,
                                             lookup->mask, s, index);

        /* if there are hits... */
        if (PV_TEST(pv, index, PV_ARRAY_BTS)) {
            numhits = bbc[index].num_used;

            ASSERT(numhits != 0);

            /* ...and there is enough space in the destination array, */
            if (numhits <= (array_size - totalhits))
                /* ...then copy the hits to the destination */
            {
                Int4 *src;
                if (numhits <= AA_HITS_PER_CELL)
                    /* hits live in thick_backbone */
                    src = bbc[index].payload.entries;
                else
                    /* hits live in overflow array */
                    src = &(ovfl[bbc[index].payload.overflow_cursor]);

                /* copy the hits. */
                {
                    Int4 i;
                    Int4 s_off = s - subject->sequence;
                    for (i = 0; i < numhits; i++) {
                        offset_pairs[i + totalhits].qs_offsets.q_off = src[i];
                        offset_pairs[i + totalhits].qs_offsets.s_off = s_off;
                    }
                }

                totalhits += numhits;
            } else
                /* not enough space in the destination array; return early */
            {
                s_range[1] = s - subject->sequence;
                return totalhits;
            }
        }
    } /* end for */
    s_range[1] = s - subject->sequence;
    } /* end while */

    /* if we get here, we fell off the end of the sequence */
    return totalhits;
}

/** same function for small lookup table */
static Int4 s_BlastSmallAaScanSubject(const LookupTableWrap * lookup_wrap,
                                 const BLAST_SequenceBlk * subject,
                                 BlastOffsetPair * NCBI_RESTRICT offset_pairs,
                                 Int4 array_size,
                                 Int4 * s_range)
{
    Int4 index;
    Uint1 *s = NULL;
    Uint1 *s_first = NULL;
    Uint1 *s_last = NULL;
    Int4 numhits = 0;           /* number of hits found for a given subject
                                   offset */
    Int4 totalhits = 0;         /* cumulative number of hits found */
    PV_ARRAY_TYPE *pv;
    BlastAaLookupTable *lookup;
    AaLookupSmallboneCell *bbc;
    Uint2 *ovfl;
    Int4 word_length;

    ASSERT(lookup_wrap->lut_type == eAaLookupTable);
    lookup = (BlastAaLookupTable *) lookup_wrap->lut;
    ASSERT(lookup->bone_type == eSmallbone);
    pv = lookup->pv;   
    bbc = (AaLookupSmallboneCell *) lookup->thick_backbone;
    ovfl = (Uint2 *) lookup->overflow;
    word_length = lookup->word_length;

    while (s_DetermineScanningOffsets(subject, word_length, word_length, s_range)) {
    s_first=subject->sequence + s_range[1];
    s_last=subject->sequence + s_range[2];

    /* prime the index */
    index = ComputeTableIndex(word_length - 1,
                              lookup->charsize, s_first);

    for (s = s_first; s <= s_last; s++) {
        /* compute the index value */
        index = ComputeTableIndexIncremental(word_length, 
                                             lookup->charsize,
                                             lookup->mask, s, index);

        /* if there are hits... */
        if (PV_TEST(pv, index, PV_ARRAY_BTS)) {
            numhits = bbc[index].num_used;

            ASSERT(numhits != 0);

            /* ...and there is enough space in the destination array, */
            if (numhits <= (array_size - totalhits))
                /* ...then copy the hits to the destination */
            {
                Uint2 *src;
                if (numhits <= AA_HITS_PER_CELL)
                    /* hits live in thick_backbone */
                    src = bbc[index].payload.entries;
                else
                    /* hits live in overflow array */
                    src = &(ovfl[bbc[index].payload.overflow_cursor]);

                /* copy the hits. */
                {
                    Int4 i;
                    Int4 s_off = s - subject->sequence;
                    for (i = 0; i < numhits; i++) {
                        offset_pairs[i + totalhits].qs_offsets.q_off = src[i];
                        offset_pairs[i + totalhits].qs_offsets.s_off = s_off;
                    }
                }

                totalhits += numhits;
            } else
                /* not enough space in the destination array; return early */
            {
                s_range[1] = s - subject->sequence;
                return totalhits;
            }
        }
    } /* end for */
    s_range[1] = s - subject->sequence;

    } /* end while */
    /* if we get here, we fell off the end of the sequence */
    return totalhits;
}

/**
 * Scans the subject sequence from "offset" to the end of the sequence,
 * assuming a compressed protein alphabet
 * Copies at most array_size hits.
 * Returns the number of hits found.
 * If there isn't enough room to copy all the hits, return early, and update
 * "offset". 
 *
 * @param lookup_wrap the lookup table [in]
 * @param subject the subject sequence [in]
 * @param offset the offset in the subject at which to begin scanning [in/out]
 * @param offset_pairs Array to which hits will be copied [out]
 * @param array_size length of the offset arrays [in]
 * @return The number of hits found.
 */
static Int4 s_BlastCompressedAaScanSubject(
                              const LookupTableWrap * lookup_wrap,
                              const BLAST_SequenceBlk * subject,
                              BlastOffsetPair * NCBI_RESTRICT offset_pairs,
                              Int4 array_size,
                              Int4 * s_range)
{
    Int4 index=0;
    Int4 preshift; /* used for 2-stage index calculation */
    Uint1 *s = NULL;
    Uint1 *s_first = NULL;
    Uint1 *s_last = NULL;
    Int4 numhits = 0;     /* number of hits found for one subject offset */
    Int4 totalhits = 0;         /* cumulative number of hits found */
    PV_ARRAY_TYPE *pv;
    Int4 pv_array_bts;
    BlastCompressedAaLookupTable *lookup;

    Int4 word_length;
    Int4 recip;               /* reciprocal of compressed word size */
    Int4* scaled_compress_table;
    Int4 skip = 0;         /* skip counter - how many letters left to skip*/
    Uint1 next_char;           /* prefetch variable */
    Int4 compressed_char;     /* translated letter */
    Int4 compressed_alphabet_size;
               
    ASSERT(lookup_wrap->lut_type == eCompressedAaLookupTable);
    lookup = (BlastCompressedAaLookupTable *) lookup_wrap->lut;
    word_length = lookup->word_length;

    while (s_DetermineScanningOffsets(subject, word_length, word_length, s_range)) {
    s_first=subject->sequence + s_range[1];
    s_last=subject->sequence + s_range[2];

    compressed_alphabet_size = lookup->compressed_alphabet_size;
    scaled_compress_table = lookup->scaled_compress_table;
    recip = lookup->reciprocal_alphabet_size;
    pv = lookup->pv;
    pv_array_bts = lookup->pv_array_bts;

    /* prime the index */
    for(s = s_first; s <= s_last; s++){
        index = s_ComputeCompressedIndex(word_length - 1, s,
                                         compressed_alphabet_size,
                                         &skip, lookup);
        if(!skip)
          break;
    }

    next_char = ((s <= s_last)? s[word_length-1] : 0);
    preshift = (Int4)((((Int8)index) * recip) >> 32); 

    /* main scanning loop */
    for (; s <= s_last; s++) {
       /* compute the index value */

       compressed_char = scaled_compress_table[next_char];
       next_char = s[word_length];
      
       if(compressed_char < 0){ /* flush (rare) "bad" character(s) */
         preshift = 0;
         s++;
         for(skip = word_length-1; skip && (s <= s_last) ; s++){
           compressed_char = scaled_compress_table[next_char];
           next_char = s[word_length];
           
           if(compressed_char < 0){ /* not again! */
             skip = word_length-1;
             preshift = 0;
             continue;
           }
           
           index = preshift + compressed_char;
           preshift = (Int4)((((Int8)( index )) * recip) >> 32);
           skip--;
         }
         
         s--; /*undo the following increment*/
         continue;
       }

       /* we have to remove the oldest letter from the
          index and add in the next letter. The latter is easy,
          but since the compressed alphabet size is not a
          power of two the former requires a remainder and
          multiply, assuming the old letter is in the high part
          of the index. For this reason, we reverse the order
          of the letters and keep the oldest in the low part
          of index, so that a single divide (implemented via
          reciprocal multiplication) does the removal.
          Index calculation done in two steps to let the CPU do 
          out-of-order execution. */
       
       index = preshift + compressed_char;
       preshift = (Int4)((((Int8)( index )) * recip) >> 32);

       /* if there are hits */
       if (PV_TEST(pv, index, pv_array_bts)) {
          Int4 s_off = s - subject->sequence;

          CompressedLookupBackboneCell* backbone_cell = 
                                        lookup->backbone + index;
         
          numhits = backbone_cell->num_used;
         
          /* and there is enough space in the destination array */
          if (numhits <= (array_size - totalhits)) {
           
             /* copy the hits to the destination */
           
             Int4 i;
             Int4 *query_offsets;
             BlastOffsetPair *dest = offset_pairs + totalhits;
           
             if (numhits <= COMPRESSED_HITS_PER_BACKBONE_CELL) {
                /* hits all live in the backbone */
             
                query_offsets = backbone_cell->payload.query_offsets;
                for (i = 0; i < numhits; i++) {
                   dest[i].qs_offsets.q_off = query_offsets[i];
                   dest[i].qs_offsets.s_off = s_off;
                }
             } 
             else { 
                /* hits are in the backbone cell and in the overflow list */
                CompressedOverflowCell* curr_cell = 
                                    backbone_cell->payload.overflow_list.head;
                /* we know the overflow list has at least one cell,
                   so it's safe to speculatively fetch the pointer
                   to further cells */
                CompressedOverflowCell* next_cell = curr_cell->next;

                /* the number of hits in the linked list of cells has
                   1 added to it; the extra hit was spilled from the
                   backbone when the list was first created */
                Int4 first_cell_entries = (numhits -
                                     COMPRESSED_HITS_PER_BACKBONE_CELL) %
                                     COMPRESSED_HITS_PER_OVERFLOW_CELL + 1;

                /* copy hits from backbone */
                query_offsets = 
                         backbone_cell->payload.overflow_list.query_offsets;
                for(i = 0; i < COMPRESSED_HITS_PER_BACKBONE_CELL - 1; i++) {
                   dest[i].qs_offsets.q_off = query_offsets[i];
                   dest[i].qs_offsets.s_off = s_off;
                }
              
                /* handle the overflow list */
              
                /* first cell can be partially filled */
                query_offsets = curr_cell->query_offsets;
                dest += i;
                for (i = 0; i < first_cell_entries; i++) {
                   dest[i].qs_offsets.q_off = query_offsets[i];
                   dest[i].qs_offsets.s_off = s_off;
                }

                /* handle the rest of the list */

                if (next_cell != NULL) {
                   curr_cell = next_cell;
                   while (curr_cell != NULL) {
                      query_offsets = curr_cell->query_offsets;
                      curr_cell = curr_cell->next;    /* prefetch */
                      dest += i;
                      for (i = 0; i < COMPRESSED_HITS_PER_OVERFLOW_CELL; i++) {
                         dest[i].qs_offsets.q_off = query_offsets[i];
                         dest[i].qs_offsets.s_off = s_off;
                      }
                   }
                }
             }

             totalhits += numhits;
          } 
          else
              /* not enough space in the destination array */
          {
              s_range[1] = s - subject->sequence;
              return totalhits;
          }
       }
    } /* end for */
    s_range[1] = s - subject->sequence;
    } /* end while */

    /* if we get here, we fell off the end of the sequence */
    return totalhits;
}

/** Add one query-subject pair to the list of such pairs retrieved
 *  from the RPS blast lookup table.
 * @param b the List in which the current pair will be placed [in/out]
 * @param q_off query offset [in]
 * @param s_off subject offset [in]
 */
static void s_AddToRPSBucket(RPSBucket * b, Uint4 q_off, Uint4 s_off)
{
    BlastOffsetPair *offset_pairs = b->offset_pairs;
    Int4 i = b->num_filled;
    if (i == b->num_alloc) {
        b->num_alloc *= 2;
        offset_pairs = b->offset_pairs =
            (BlastOffsetPair *) realloc(b->offset_pairs,
                                        b->num_alloc *
                                        sizeof(BlastOffsetPair));
    }
    offset_pairs[i].qs_offsets.q_off = q_off;
    offset_pairs[i].qs_offsets.s_off = s_off;
    b->num_filled++;
}

/**
 * Scans the RPS query sequence from "offset" to the end of the sequence.
 * Copies at most array_size hits.
 * Returns the number of hits found.
 * If there isn't enough room to copy all the hits, return early, and update
 * "offset". 
 *
 * @param lookup_wrap the lookup table [in]
 * @param sequence the subject sequence [in]
 * @param offset the offset in the subject at which to begin scanning [in/out]
 * @return The number of hits found.
 */
Int4 BlastRPSScanSubject(const LookupTableWrap * lookup_wrap,
                         const BLAST_SequenceBlk * sequence,
                         Int4 * offset)
{
    Int4 index;
    Int4 table_correction;
    Uint1 *s = NULL;
    Uint1 *abs_start = sequence->sequence;
    Uint1 *s_first = NULL;
    Uint1 *s_last = NULL;
    Int4 numhits = 0;           /* number of hits found for a given subject
                                   offset */
    Int4 totalhits = 0;         /* cumulative number of hits found */
    BlastRPSLookupTable *lookup;
    RPSBackboneCell *cell;
    RPSBucket *bucket_array;
    PV_ARRAY_TYPE *pv;
    /* Buffer a large number of hits at once. The number of hits is
       independent of the search, because the structures that will contain
       them grow dynamically. A large number is needed because cache reuse
       requires that many hits to the same neighborhood of the concatenated
       database are available at any given time */
    const Int4 max_hits = 4000000;

    ASSERT(lookup_wrap->lut_type == eRPSLookupTable);
    lookup = (BlastRPSLookupTable *) lookup_wrap->lut;
    bucket_array = lookup->bucket_array;

    /* empty the previous collection of hits */

    for (index = 0; index < lookup->num_buckets; index++)
        bucket_array[index].num_filled = 0;

    s_first = abs_start + *offset;
    s_last = abs_start + sequence->length - lookup->wordsize;
    pv = lookup->pv;

    /* Calling code expects the returned sequence offsets to refer to the
       *first letter* in a word. The legacy RPS blast lookup table stores
       offsets to the *last* letter in each word, and so a correction is
       needed */

    table_correction = lookup->wordsize - 1;

    /* prime the index */
    index = ComputeTableIndex(lookup->wordsize - 1,
                          lookup->charsize, s_first);

    for (s = s_first; s <= s_last; s++) {
        /* compute the index value */
        index = ComputeTableIndexIncremental(lookup->wordsize, 
                                             lookup->charsize,
                                             lookup->mask, s, index);

        /* if there are hits... */
        if (PV_TEST(pv, index, PV_ARRAY_BTS)) {
            cell = &lookup->rps_backbone[index];
            numhits = cell->num_used;

            ASSERT(numhits != 0);

            if (numhits <= (max_hits - totalhits)) {
                Int4 *src;
                Int4 i;
                Uint4 q_off;
                Uint4 s_off = s - abs_start;
                if (numhits <= RPS_HITS_PER_CELL) {
                    for (i = 0; i < numhits; i++) {
                        q_off = cell->entries[i] - table_correction;
                        s_AddToRPSBucket(bucket_array +
                                         q_off / RPS_BUCKET_SIZE, q_off,
                                         s_off);
                    }
                } else {
                    /* hits (past the first) live in overflow array */
                    src =
                        lookup->overflow + (cell->entries[1] / sizeof(Int4));
                    q_off = cell->entries[0] - table_correction;
                    s_AddToRPSBucket(bucket_array + q_off / RPS_BUCKET_SIZE,
                                     q_off, s_off);
                    for (i = 0; i < (numhits - 1); i++) {
                        q_off = src[i] - table_correction;
                        s_AddToRPSBucket(bucket_array +
                                         q_off / RPS_BUCKET_SIZE, q_off,
                                         s_off);
                    }
                }

                totalhits += numhits;
            } else
                /* not enough space in the destination array; return early */
            {
                break;
            }
        }
    }

    /* if we get here, we fell off the end of the sequence */
    *offset = s - abs_start;

    return totalhits;
}

void BlastChooseProteinScanSubject(LookupTableWrap *lookup_wrap)
{
    if (lookup_wrap->lut_type == eAaLookupTable) {
        BlastAaLookupTable *lut = (BlastAaLookupTable *)(lookup_wrap->lut);
        /* normal backbone */
        if(lut->bone_type == eBackbone)
           lut->scansub_callback = (void *)s_BlastAaScanSubject;
        /* small bone*/
        else
           lut->scansub_callback = (void *)s_BlastSmallAaScanSubject;
    }
    else if (lookup_wrap->lut_type == eCompressedAaLookupTable) {
        BlastCompressedAaLookupTable *lut = 
                        (BlastCompressedAaLookupTable *)(lookup_wrap->lut);
        lut->scansub_callback = (void *)s_BlastCompressedAaScanSubject;
    }
}
