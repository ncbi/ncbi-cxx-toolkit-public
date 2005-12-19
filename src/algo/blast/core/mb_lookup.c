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

/** @file mb_lookup.c
 * Functions responsible for the creation and scanning
 * of megablast lookup tables
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/mb_lookup.h>
#include <algo/blast/core/lookup_util.h>
#include "blast_inline.h"

BlastMBLookupTable* MBLookupTableDestruct(BlastMBLookupTable* mb_lt)
{
   if (!mb_lt)
      return NULL;
   if (mb_lt->hashtable)
      sfree(mb_lt->hashtable);
   if (mb_lt->next_pos)
      sfree(mb_lt->next_pos);
   if (mb_lt->hashtable2)
      sfree(mb_lt->hashtable2);
   if (mb_lt->next_pos2)
      sfree(mb_lt->next_pos2);
   sfree(mb_lt->pv_array);

   sfree(mb_lt);
   return mb_lt;
}

/** Convert weight, template length and template type from input options into
    an MBTemplateType enum
*/
static EDiscTemplateType 
s_GetDiscTemplateType(Int4 weight, Uint1 length, 
                                     EDiscWordType type)
{
   if (weight == 11) {
      if (length == 16) {
         if (type == eMBWordCoding || type == eMBWordTwoTemplates)
            return eDiscTemplate_11_16_Coding;
         else if (type == eMBWordOptimal)
            return eDiscTemplate_11_16_Optimal;
      } else if (length == 18) {
         if (type == eMBWordCoding || type == eMBWordTwoTemplates)
            return eDiscTemplate_11_18_Coding;
         else if (type == eMBWordOptimal)
            return eDiscTemplate_11_18_Optimal;
      } else if (length == 21) {
         if (type == eMBWordCoding || type == eMBWordTwoTemplates)
            return eDiscTemplate_11_21_Coding;
         else if (type == eMBWordOptimal)
            return eDiscTemplate_11_21_Optimal;
      }
   } else if (weight == 12) {
      if (length == 16) {
         if (type == eMBWordCoding || type == eMBWordTwoTemplates)
            return eDiscTemplate_12_16_Coding;
         else if (type == eMBWordOptimal)
            return eDiscTemplate_12_16_Optimal;
      } else if (length == 18) {
         if (type == eMBWordCoding || type == eMBWordTwoTemplates)
            return eDiscTemplate_12_18_Coding;
         else if (type == eMBWordOptimal)
            return eDiscTemplate_12_18_Optimal;
      } else if (length == 21) {
         if (type == eMBWordCoding || type == eMBWordTwoTemplates)
            return eDiscTemplate_12_21_Coding;
         else if (type == eMBWordOptimal)
            return eDiscTemplate_12_21_Optimal;
      }
   }
   return eDiscTemplateContiguous; /* All unsupported cases default to 0 */
}

/** Fills in the hashtable and next_pos fields of BlastMBLookupTable*
 * for the discontiguous case.
 *
 * @param query the query sequence [in]
 * @param location locations on the query to be indexed in table [in]
 * @param mb_lt the (already allocated) megablast lookup 
 *              table structure [in|out]
 * @param lookup_options specifies the word_size and template options [in]
 * @return zero on success, negative number on failure. 
 */

static Int2 
s_FillDiscMBTable(BLAST_SequenceBlk* query, BlastSeqLoc* location,
        BlastMBLookupTable* mb_lt,
        const LookupTableOptions* lookup_options)

{
   BlastSeqLoc* loc;
   EDiscTemplateType template_type;
   EDiscTemplateType second_template_type = eDiscTemplateContiguous;
   const Uint1 kNucMask = 0xfc;
   const Boolean kTwoTemplates = 
      (lookup_options->mb_template_type == eMBWordTwoTemplates);
   PV_ARRAY_TYPE *pv_array=NULL;
   Int4 pv_array_bts;
   Int4 index;
   Int4 template_length;
   /* The calculation of the longest chain can be cpu intensive for 
      long queries or sets of queries. So we use a helper_array to 
      keep track of this, but compress it by kCompressionFactor so 
      it stays in cache.  Hence we only end up with a conservative 
      (high) estimate for longest_chain, but this does not seem to 
      affect the overall performance of the rest of the program. */
   Uint4 longest_chain;
   Uint4* helper_array = NULL;     /* Helps to estimate longest chain. */
   Uint4* helper_array2 = NULL;    /* Helps to estimate longest chain. */
   const Int4 kCompressionFactor=2048; /* compress helper_array by this much */

   ASSERT(mb_lt);
   ASSERT(lookup_options->mb_template_length > 0);

   mb_lt->next_pos = (Int4 *)calloc(query->length + 1, sizeof(Int4));
   helper_array = (Uint4*) calloc(mb_lt->hashsize/kCompressionFactor, 
                                  sizeof(Uint4));
   if (mb_lt->next_pos == NULL || helper_array == NULL)
      return -1;

   template_type = s_GetDiscTemplateType(lookup_options->word_size,
                      lookup_options->mb_template_length, 
                      (EDiscWordType)lookup_options->mb_template_type);

   ASSERT(template_type != eDiscTemplateContiguous);

   mb_lt->template_type = template_type;
   mb_lt->two_templates = kTwoTemplates;
   /* For now leave only one possibility for the second template.
      Note that the intention here is to select both the coding
      and the optimal templates for one combination of word size
      and template length. */
   if (kTwoTemplates) {
      second_template_type = mb_lt->second_template_type = template_type + 1;

      mb_lt->hashtable2 = (Int4*)calloc(mb_lt->hashsize, sizeof(Int4));
      mb_lt->next_pos2 = (Int4*)calloc(query->length + 1, sizeof(Int4));
      helper_array2 = (Uint4*) calloc(mb_lt->hashsize/kCompressionFactor, 
                                      sizeof(Uint4));
      if (mb_lt->hashtable2 == NULL ||
          mb_lt->next_pos2 == NULL ||
          helper_array2 == NULL)
         return -1;
   }

   mb_lt->discontiguous = TRUE;
   mb_lt->template_length = lookup_options->mb_template_length;
   template_length = lookup_options->mb_template_length;
   pv_array = mb_lt->pv_array;
   pv_array_bts = mb_lt->pv_array_bts;

   for (loc = location; loc; loc = loc->next) {
      Int4 from;
      Int4 to;
      Uint8 accum = 0;
      Int4 ecode1 = 0;
      Int4 ecode2 = 0;
      Uint1* pos;
      Uint1* seq;
      Uint1 val;

      /* A word is added to the table after the last base 
         in the word is read in. At that point, the start 
         offset of the word is template_length - 1 positions 
         behind. This index is also incremented, because 
         lookup table indices are 1-based (offset 0 is reserved). */

      from = loc->ssr->left - (template_length - 2);
      to = loc->ssr->right - (template_length - 2);
      seq = query->sequence_start + loc->ssr->left;
      pos = seq + template_length;

      for (index = from; index <= to; index++) {
         val = *++seq;
         /* if an ambiguity is encountered, do not add
            any words that would contain it */
         if ((val & kNucMask) != 0) {
            accum = 0;
            pos = seq + template_length;
            continue;
         }

         /* get next base */
         accum = (accum << 2) | val;
         if (seq < pos)
            continue;

#ifdef LOOKUP_VERBOSE
         mb_lt->num_words_added++;
#endif
         /* compute the hashtable index for the first template
            and add 'index' at that position */

         ecode1 = ComputeDiscontiguousIndex(accum, template_type);
         if (mb_lt->hashtable[ecode1] == 0) {
#ifdef LOOKUP_VERBOSE
            mb_lt->num_unique_pos_added++;
#endif
            pv_array[ecode1>>pv_array_bts] |= 
                         (((PV_ARRAY_TYPE) 1)<<(ecode1&PV_ARRAY_MASK));
         }
         else {
            helper_array[ecode1/kCompressionFactor]++; 
         }
         mb_lt->next_pos[index] = mb_lt->hashtable[ecode1];
         mb_lt->hashtable[ecode1] = index;

         if (!kTwoTemplates)
            continue;

         /* repeat for the second template, if applicable */
         
         ecode2 = ComputeDiscontiguousIndex(accum, second_template_type);
         if (mb_lt->hashtable2[ecode2] == 0) {
#ifdef LOOKUP_VERBOSE
            mb_lt->num_unique_pos_added++;
#endif
            pv_array[ecode2>>pv_array_bts] |= 
                         (((PV_ARRAY_TYPE) 1)<<(ecode2&PV_ARRAY_MASK));
         }
         else {
            helper_array2[ecode2/kCompressionFactor]++; 
         }
         mb_lt->next_pos2[index] = mb_lt->hashtable2[ecode2];
         mb_lt->hashtable2[ecode2] = index;
      }
   }

   longest_chain = 2;
   for (index = 0; index < mb_lt->hashsize / kCompressionFactor; index++)
       longest_chain = MAX(longest_chain, helper_array[index]);
   mb_lt->longest_chain = longest_chain;
   sfree(helper_array);

   if (kTwoTemplates) {
      longest_chain = 2;
      for (index = 0; index < mb_lt->hashsize / kCompressionFactor; index++)
         longest_chain = MAX(longest_chain, helper_array2[index]);
      mb_lt->longest_chain += longest_chain;
      sfree(helper_array2);
   }
   return 0;
}

/** Fills in the hashtable and next_pos fields of BlastMBLookupTable*
 * for the contiguous case.
 *
 * @param query the query sequence [in]
 * @param location locations on the query to be indexed in table [in]
 * @param mb_lt the (already allocated) megablast lookup table structure [in|out]
 * @param lookup_options specifies the word_size [in]
 * @return zero on success, negative number on failure. 
 */

static Int2 
s_FillContigMBTable(BLAST_SequenceBlk* query, 
        BlastSeqLoc* location,
        BlastMBLookupTable* mb_lt, 
        const LookupTableOptions* lookup_options)

{
   BlastSeqLoc* loc;
   const Uint1 kNucMask = 0xfc;
   /* 12-mers (or perhaps 8-mers) are used to build the lookup table 
      and this is what kLutWordLength specifies. */
   const Int4 kLutWordLength = mb_lt->lut_word_length;
   const Int4 kLutMask = mb_lt->hashsize - 1;
   /* The user probably specified a much larger word size (like 28) 
      and this is what full_word_size is. */
   Int4 full_word_size = mb_lt->word_length;
   Int4 index;
   PV_ARRAY_TYPE *pv_array;
   Int4 pv_array_bts;
   /* The calculation of the longest chain can be cpu intensive for 
      long queries or sets of queries. So we use a helper_array to 
      keep track of this, but compress it by kCompressionFactor so 
      it stays in cache.  Hence we only end up with a conservative 
      (high) estimate for longest_chain, but this does not seem to 
      affect the overall performance of the rest of the program. */
   const Int4 kCompressionFactor=2048; /* compress helper_array by this much */
   Uint4 longest_chain;
   Uint4* helper_array;


   ASSERT(mb_lt);

   mb_lt->next_pos = (Int4 *)calloc(query->length + 1, sizeof(Int4));
   if (mb_lt->next_pos == NULL)
      return -1;

   pv_array = mb_lt->pv_array;
   pv_array_bts = mb_lt->pv_array_bts;

   helper_array = (Uint4*) calloc(mb_lt->hashsize/kCompressionFactor, 
                                  sizeof(Uint4));
   if (helper_array == NULL)
	return -1;

   for (loc = location; loc; loc = loc->next) {
      /* We want index to be always pointing to the start of the word.
         Since sequence pointer points to the end of the word, subtract
         word length from the loop boundaries.  */
      Int4 from = loc->ssr->left;
      Int4 to = loc->ssr->right - kLutWordLength;
      Int4 ecode = 0;
      Int4 last_offset;
      Uint1* pos;
      Uint1* seq;
      Uint1 val;

     /* case of unmasked region >=  kLutWordLength but < full_word_size,
        so no hits should be generated. */
      if (full_word_size > (loc->ssr->right - loc->ssr->left + 1))
         continue; 

      seq = query->sequence_start + from;
      pos = seq + kLutWordLength;
      
      /* Also add 1 to all indices, because lookup table indices count 
         from 1. */
      from -= kLutWordLength - 2;
      last_offset = to + 2;

      for (index = from; index <= last_offset; index++) {
         val = *++seq;
         /* if an ambiguity is encountered, do not add
            any words that would contain it */
         if ((val & kNucMask) != 0) {
            ecode = 0;
            pos = seq + kLutWordLength;
            continue;
         }

         /* get next base */
         ecode = ((ecode << 2) & kLutMask) + val;
         if (seq < pos) 
            continue;

#ifdef LOOKUP_VERBOSE
         mb_lt->num_words_added++;
#endif
         if (mb_lt->hashtable[ecode] == 0) {
#ifdef LOOKUP_VERBOSE
            mb_lt->num_unique_pos_added++;
#endif
            pv_array[ecode>>pv_array_bts] |= 
                         (((PV_ARRAY_TYPE) 1)<<(ecode&PV_ARRAY_MASK));
         }
         else {
            helper_array[ecode/kCompressionFactor]++; 
         }
         mb_lt->next_pos[index] = mb_lt->hashtable[ecode];
         mb_lt->hashtable[ecode] = index;
      }
   }

   longest_chain = 2;
   for (index = 0; index < mb_lt->hashsize / kCompressionFactor; index++)
       longest_chain = MAX(longest_chain, helper_array[index]);

   mb_lt->longest_chain = longest_chain;
   sfree(helper_array);
   return 0;
}


/* Documentation in mb_lookup.h */
Int2 MB_LookupTableNew(BLAST_SequenceBlk* query, BlastSeqLoc* location,
        BlastMBLookupTable** mb_lt_ptr,
        const LookupTableOptions* lookup_options,
        Int4 approx_table_entries)
{
   Int4 pv_shift, pv_size;
   Int2 status = 0;
   BlastMBLookupTable* mb_lt;
   const Int4 kAlphabetSize = BLAST2NA_SIZE;
   const Int4 kTargetPVSize = 131072;
   const Int4 kSmallQueryCutoff = 15000;
   const Int4 kLargeQueryCutoff = 800000;
   
   *mb_lt_ptr = NULL;

   if (!location || !query) {
     /* Empty sequence location provided */
     return -1;
   }

   mb_lt = (BlastMBLookupTable*)calloc(1, sizeof(BlastMBLookupTable));
   if (mb_lt == NULL) {
	return -1;
   }
    
   /* first determine how 'wide' the lookup table is,
      i.e. how many nucleotide letters are in a single entry */

   if (lookup_options->mb_template_length > 0) {

      /* for discontiguous megablast the choice is fixed */
      if (lookup_options->word_size == 11)
         mb_lt->lut_word_length = 11;
      else
         mb_lt->lut_word_length = 12;
   }
   else {
       /* Choose the smallest lookup table that will not
          be congested; this is a compromise between good
          cache performance and few table accesses. The 
          number of entries where one table width becomes
          better than another is probably machine-dependent */

      ASSERT(lookup_options->word_size >= 10);

      switch(lookup_options->word_size) {
      case 10:
         if (approx_table_entries < 18000)
            mb_lt->lut_word_length = 9;
         else
            mb_lt->lut_word_length = 10;
         break;
      case 11:
         if (approx_table_entries < 18000)
            mb_lt->lut_word_length = 9;
         else if (approx_table_entries < 180000)
            mb_lt->lut_word_length = 10;
         else
            mb_lt->lut_word_length = 11;
         break;
      case 12:
         if (approx_table_entries < 18000)
            mb_lt->lut_word_length = 9;
         else if (approx_table_entries < 60000)
            mb_lt->lut_word_length = 10;
         else if (approx_table_entries < 900000)
            mb_lt->lut_word_length = 11;
         else
            mb_lt->lut_word_length = 12;
         break;
      default:
         if (approx_table_entries < 300000)
            mb_lt->lut_word_length = 11;
         else
            mb_lt->lut_word_length = 12;
         break;
      }

      mb_lt->lut_word_length = MIN(lookup_options->word_size,
                                   mb_lt->lut_word_length);
   }

   mb_lt->word_length = lookup_options->word_size;
   mb_lt->hashsize = iexp(kAlphabetSize, mb_lt->lut_word_length);
   mb_lt->hashtable = (Int4*)calloc(mb_lt->hashsize, sizeof(Int4));
   if (mb_lt->hashtable == NULL) {
      MBLookupTableDestruct(mb_lt);
      return -1;
   }

   /* Allocate the PV array. To fit in the external cache of 
      latter-day microprocessors, the PV array cannot have one
      bit for for every lookup table entry. Instead we choose
      a size that should fit in cache and make a single bit
      of the PV array handle multiple hashtable entries if
      necessary.

      If the query is too small or too large, the compression 
      should be higher. Small queries don't reuse the PV array,
      and large queries saturate it. In either case, cache
      is better used on something else */

   if (mb_lt->hashsize <= 8 * kTargetPVSize)
      pv_size = mb_lt->hashsize >> PV_ARRAY_BTS;
   else
      pv_size = kTargetPVSize / PV_ARRAY_BYTES;

   if(approx_table_entries <= kSmallQueryCutoff ||
      approx_table_entries >= kLargeQueryCutoff) {
         pv_size = pv_size / 2;
   }
   mb_lt->pv_array_bts = ilog2(mb_lt->hashsize / pv_size);
   mb_lt->pv_array = calloc(PV_ARRAY_BYTES, pv_size);
   if (mb_lt->pv_array == NULL) {
      MBLookupTableDestruct(mb_lt);
      return -1;
   }

   if (lookup_options->mb_template_length > 0) {
        /* discontiguous megablast */
        mb_lt->full_byte_scan = lookup_options->full_byte_scan; 
        mb_lt->ag_scanning_mode = FALSE;
        mb_lt->variable_wordsize = FALSE; 
        mb_lt->scan_step = 0;
        status = s_FillDiscMBTable(query, location, mb_lt, lookup_options);
   }
   else {
        /* contiguous megablast */
        mb_lt->ag_scanning_mode = TRUE;
        mb_lt->scan_step = CalculateBestStride(lookup_options->word_size, 
                                            lookup_options->variable_wordsize, 
                                            mb_lt->lut_word_length);
        mb_lt->variable_wordsize = lookup_options->variable_wordsize;
        status = s_FillContigMBTable(query, location, mb_lt, lookup_options);
   }

   if (status > 0) {
      MBLookupTableDestruct(mb_lt);
      return status;
   }

   *mb_lt_ptr = mb_lt;

#ifdef LOOKUP_VERBOSE
   printf("lookup table size: %d (%d letters)\n", mb_lt->hashsize,
                                        mb_lt->lut_word_length);
   printf("words in table: %d\n", mb_lt->num_words_added);
   printf("filled entries: %d (%f%%)\n", mb_lt->num_unique_pos_added,
                        100.0 * mb_lt->num_unique_pos_added / mb_lt->hashsize);
   printf("PV array size: %d bytes (%d table entries/bit)\n",
                                 pv_size * PV_ARRAY_BYTES, 
                                 mb_lt->hashsize / (pv_size << PV_ARRAY_BTS));
   printf("longest chain: %d\n", mb_lt->longest_chain);
#endif
   return 0;
}

Int4 MB_AG_ScanSubject(const LookupTableWrap* lookup_wrap,
       const BLAST_SequenceBlk* subject, Int4 start_offset,
       BlastOffsetPair* NCBI_RESTRICT offset_pairs, Int4 max_hits,  
       Int4* end_offset)
{
   BlastMBLookupTable* mb_lt;
   Uint1* s;
   Uint1* abs_start;
   Int4 q_off, s_off;
   Int4 last_offset;
   Int4 index;
   Int4 mask;
   PV_ARRAY_TYPE *pv_array;
   Int4 total_hits = 0;
   Int4 lut_word_length;
   Int4 scan_step;
   Uint1 pv_array_bts;
   
   ASSERT(lookup_wrap->lut_type == MB_LOOKUP_TABLE);
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
            that must be shifted away). s_end below points to either
            the second to last or third to last byte of the subject 
            sequence; all subject sequences must therefore have a 
            sentinel byte */

         Uint1* s_end = abs_start + (subject->length - lut_word_length) /
                                                   COMPRESSION_RATIO;
         Int4 shift = 2 * (12 - lut_word_length);
         s = abs_start + start_offset / COMPRESSION_RATIO;
         scan_step = scan_step / COMPRESSION_RATIO;
   
         for ( ; s <= s_end; s += scan_step) {
            
            index = s[0] << 16 | s[1] << 8 | s[2];
            index = index >> shift;
      
            if (NA_PV_TEST(pv_array, index, pv_array_bts)) {
               if (total_hits >= max_hits)
                  break;
               q_off = mb_lt->hashtable[index];
               s_off = (s - abs_start)*COMPRESSION_RATIO;
               while (q_off) {
                  offset_pairs[total_hits].qs_offsets.q_off = q_off - 1;
                  offset_pairs[total_hits++].qs_offsets.s_off = s_off;
                  q_off = mb_lt->next_pos[q_off];
               }
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
            must be recalculated for each 16-base region */

         for (s_off = start_offset; s_off <= last_offset; s_off += scan_step) {
      
            Int4 shift = 2*(16 - (s_off % COMPRESSION_RATIO + lut_word_length));
            s = abs_start + (s_off / COMPRESSION_RATIO);
      
            index = s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3];
            index = (index >> shift) & mask;
      
            if (NA_PV_TEST(pv_array, index, pv_array_bts)) {
               if (total_hits >= max_hits)
                  break;
               q_off = mb_lt->hashtable[index];
               while (q_off) {
                  offset_pairs[total_hits].qs_offsets.q_off = q_off - 1;
                  offset_pairs[total_hits++].qs_offsets.s_off = s_off;
                  q_off = mb_lt->next_pos[q_off];
               }
            }
         }
         *end_offset = s_off;
      }
   }
   else {
      /* perform scanning for a lookup tables of width 9
         Here the stride will never be a multiple of 4 
         (this table is only used for very small word sizes) */

      for (s_off = start_offset; s_off <= last_offset; s_off += scan_step) {
   
         Int4 shift = 2*(12 - (s_off % COMPRESSION_RATIO + lut_word_length));
         s = abs_start + (s_off / COMPRESSION_RATIO);
   
         index = s[0] << 16 | s[1] << 8 | s[2];
         index = (index >> shift) & mask;
   
         if (NA_PV_TEST(pv_array, index, pv_array_bts)) {
            if (total_hits >= max_hits)
               break;
            q_off = mb_lt->hashtable[index];
            while (q_off) {
               offset_pairs[total_hits].qs_offsets.q_off = q_off - 1;
               offset_pairs[total_hits++].qs_offsets.s_off = s_off;
               q_off = mb_lt->next_pos[q_off];
            }
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

   ASSERT(lookup->lut_type == MB_LOOKUP_TABLE);
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
  
         if (NA_PV_TEST(pv_array, index, pv_array_bts)) {
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

            if (NA_PV_TEST(pv_array, index2, pv_array_bts)) {
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
       
         if (NA_PV_TEST(pv_array, index, pv_array_bts)) {
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

            if (NA_PV_TEST(pv_array, index2, pv_array_bts)) {
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

