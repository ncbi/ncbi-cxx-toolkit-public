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
static DiscTemplateType 
s_GetDiscTemplateType(Int4 weight, Uint1 length, 
                                     DiscWordType type)
{
   if (weight == 11) {
      if (length == 16) {
         if (type == MB_WORD_CODING || type == MB_TWO_TEMPLATES)
            return TEMPL_11_16;
         else if (type == MB_WORD_OPTIMAL)
            return TEMPL_11_16_OPT;
      } else if (length == 18) {
         if (type == MB_WORD_CODING || type == MB_TWO_TEMPLATES)
            return TEMPL_11_18;
         else if (type == MB_WORD_OPTIMAL)
            return TEMPL_11_18_OPT;
      } else if (length == 21) {
         if (type == MB_WORD_CODING || type == MB_TWO_TEMPLATES)
            return TEMPL_11_21;
         else if (type == MB_WORD_OPTIMAL)
            return TEMPL_11_21_OPT;
      }
   } else if (weight == 12) {
      if (length == 16) {
         if (type == MB_WORD_CODING || type == MB_TWO_TEMPLATES)
            return TEMPL_12_16;
         else if (type == MB_WORD_OPTIMAL)
            return TEMPL_12_16_OPT;
      } else if (length == 18) {
         if (type == MB_WORD_CODING || type == MB_TWO_TEMPLATES)
            return TEMPL_12_18;
         else if (type == MB_WORD_OPTIMAL)
            return TEMPL_12_18_OPT;
      } else if (length == 21) {
         if (type == MB_WORD_CODING || type == MB_TWO_TEMPLATES)
            return TEMPL_12_21;
         else if (type == MB_WORD_OPTIMAL)
            return TEMPL_12_21_OPT;
      }
   }
   return TEMPL_CONTIGUOUS; /* All unsupported cases default to 0 */
}

/** Fills in the hashtable and next_pos fields of BlastMBLookupTable*
 * for the discontiguous case.
 *
 * @param query the query sequence [in]
 * @param location locations on the query to be indexed in table [in]
 * @param mb_lt the (already allocated) megablast lookup table structure [in|out]
 * @param width number of bytes in megablast word (2 or 3) [in]
 * @param lookup_options specifies the word_size and template options [in]
 * @return zero on success, negative number on failure. 
 */

static Int2 
s_FillDiscMBTable(BLAST_SequenceBlk* query, BlastSeqLoc* location,
        BlastMBLookupTable* mb_lt, Uint1 width,
        const LookupTableOptions* lookup_options)

{
   BlastSeqLoc* loc;
   Int4 mask;
   Int4 word_length, extra_length;
   DiscTemplateType template_type;
   const Uint1 kNucMask = 0xfc;
   const Boolean kTwoTemplates = 
      (lookup_options->mb_template_type == MB_TWO_TEMPLATES);
   PV_ARRAY_TYPE *pv_array=NULL;
   Int4 pv_array_bts;
   /* The calculation of the longest chain can be cpu intensive for long queries or sets of queries. 
      So we use a helper_array to keep track of this, but compress it by kCompressionFactor so it stays 
      in memory.  Hence we only end up with a conservative (high) estimate for longest_chain, but this does
      not seem to affect the overall performance of the rest of the program. */
   Uint4 longest_chain=0;
   Uint4* helper_array;     /* Helps to estimate longest chain. */
   const Int4 kCompressionFactor=2048; /* compress helper_array by factor of 2048. */

   ASSERT(mb_lt);
   ASSERT(lookup_options->mb_template_length > 0);

   template_type = s_GetDiscTemplateType(lookup_options->word_size,
                      lookup_options->mb_template_length, 
                      (DiscWordType)lookup_options->mb_template_type);


   mb_lt->template_type = template_type;
   mb_lt->two_templates = kTwoTemplates;
   if (kTwoTemplates) /* For now leave only one possibility for the second template */
      mb_lt->second_template_type = template_type + 1;


   if (kTwoTemplates) {
      if ((mb_lt->hashtable2 = (Int4*) 
           calloc(mb_lt->hashsize, sizeof(Int4))) == NULL) {
         MBLookupTableDestruct(mb_lt);
         return -1;
      }
      if ((mb_lt->next_pos2 = (Int4*) 
           calloc((query->length+1), sizeof(Int4))) == NULL) {
         MBLookupTableDestruct(mb_lt);
         return -1;
      }
   }

   mb_lt->discontiguous = TRUE;
   mb_lt->compressed_wordsize = width + 1;
   word_length = mb_lt->word_length = COMPRESSION_RATIO*(width+1);
   mb_lt->template_length = lookup_options->mb_template_length;
   extra_length = mb_lt->template_length - word_length;
   mb_lt->mask = (Int4) (-1);
   mask = (1 << (8*(width+1) - 2)) - 1;

   pv_array = mb_lt->pv_array;
   pv_array_bts = mb_lt->pv_array_bts;

   helper_array = (Uint4*) calloc(mb_lt->hashsize/kCompressionFactor, sizeof(Uint4));
   if (helper_array == NULL)
	return -1;

   for (loc = location; loc; loc = loc->next) {
      Boolean amb_cond = TRUE;
      /* We want index to be always pointing to the start of the word.
         Since sequence pointer points to the end of the word, subtract
         word length from the loop boundaries. 
      */
      Int4 from = loc->ssr->left;
      Int4 to = loc->ssr->right - word_length;
      Int4 ecode = 0;
      Int4 ecode1 = 0;
      Int4 ecode2 = 0;
      Int4 last_offset;
      Int4 index;
      Uint1* pos;
      Uint1* seq;
      Uint1 val;

      seq = query->sequence_start + from;
      pos = seq + word_length;
      
      /* Also add 1 to all indices, because lookup table indices count 
         from 1. */
      from -= word_length - 2;
      last_offset = to - extra_length + 2;

      for (index = from; index <= last_offset; index++) {
         val = *++seq;
         if ((val & kNucMask) != 0) { /* ambiguity or gap */
            ecode = 0;
            pos = seq + word_length;
         } else {
            /* get next base */
            ecode = ((ecode & mask) << 2) + val;
            if (seq >= pos) {
               if (extra_length) {
                  switch (template_type) {
                  case TEMPL_11_18: case TEMPL_12_18:
                     amb_cond = !GET_AMBIG_CONDITION_18(seq);
                     break;
                  case TEMPL_11_18_OPT: case TEMPL_12_18_OPT:
                     amb_cond = !GET_AMBIG_CONDITION_18_OPT(seq);
                     break;
                  case TEMPL_11_21: case TEMPL_12_21:
                     amb_cond = !GET_AMBIG_CONDITION_21(seq);
                     break;
                  case TEMPL_11_21_OPT: case TEMPL_12_21_OPT:
                     amb_cond = !GET_AMBIG_CONDITION_21_OPT(seq);
                     break;
                  default:
                     break;
                  }
               }
                     
               if (amb_cond) {
                  switch (template_type) {
                  case TEMPL_11_16:
                     ecode1 = GET_WORD_INDEX_11_16(ecode);
                     break;
                  case TEMPL_12_16:
                     ecode1 = GET_WORD_INDEX_12_16(ecode);
                     break;
                  case TEMPL_11_16_OPT:
                     ecode1 = GET_WORD_INDEX_11_16_OPT(ecode);
                     break;
                  case TEMPL_12_16_OPT:
                     ecode1 = GET_WORD_INDEX_12_16_OPT(ecode);
                     break;
                  case TEMPL_11_18:
                     ecode1 = (GET_WORD_INDEX_11_18(ecode)) |
                               (GET_EXTRA_CODE_18(seq));
                     break;
                  case TEMPL_12_18:
                     ecode1 = (GET_WORD_INDEX_12_18(ecode)) |
                               (GET_EXTRA_CODE_18(seq));
                     break;
                  case TEMPL_11_18_OPT:
                     ecode1 = (GET_WORD_INDEX_11_18_OPT(ecode)) |
                               (GET_EXTRA_CODE_18_OPT(seq));
                     break;
                  case TEMPL_12_18_OPT:
                     ecode1 = (GET_WORD_INDEX_12_18_OPT(ecode)) |
                               (GET_EXTRA_CODE_18_OPT(seq));
                     break;
                  case TEMPL_11_21:
                     ecode1 = (GET_WORD_INDEX_11_21(ecode)) |
                               (GET_EXTRA_CODE_21(seq));
                     break;
                  case TEMPL_12_21:
                     ecode1 = (GET_WORD_INDEX_12_21(ecode)) |
                               (GET_EXTRA_CODE_21(seq));
                     break;
                  case TEMPL_11_21_OPT:
                     ecode1 = (GET_WORD_INDEX_11_21_OPT(ecode)) |
                               (GET_EXTRA_CODE_21_OPT(seq));
                     break;
                  case TEMPL_12_21_OPT:
                     ecode1 = (GET_WORD_INDEX_12_21_OPT(ecode)) |
                               (GET_EXTRA_CODE_21_OPT(seq));
                     break;
                  default: /* Contiguous word */
                     ecode1 = ecode;
                     break;
                  }
                     
                  if (kTwoTemplates) {
                     switch (template_type) {
                     case TEMPL_11_16:
                        ecode2 = GET_WORD_INDEX_11_16_OPT(ecode);
                        break;
                     case TEMPL_12_16:
                        ecode2 = GET_WORD_INDEX_12_16_OPT(ecode);
                        break;
                     case TEMPL_11_18:
                        ecode2 = (GET_WORD_INDEX_11_18_OPT(ecode)) |
                           (GET_EXTRA_CODE_18_OPT(seq));
                        break;
                     case TEMPL_12_18:
                        ecode2 = (GET_WORD_INDEX_12_18_OPT(ecode)) |
                           (GET_EXTRA_CODE_18_OPT(seq));
                        break;
                     case TEMPL_11_21:
                        ecode2 = (GET_WORD_INDEX_11_21_OPT(ecode)) |
                           (GET_EXTRA_CODE_21_OPT(seq));
                        break;
                     case TEMPL_12_21:
                        ecode2 = (GET_WORD_INDEX_12_21_OPT(ecode)) |
                           (GET_EXTRA_CODE_21_OPT(seq));
                        break;
                     default:
                        ecode2 = 0; break;
                     }
                     
                     if (mb_lt->hashtable[ecode1] == 0 ||
                         mb_lt->hashtable2[ecode2] == 0)
                     {
                        mb_lt->num_unique_pos_added++;
                     }

                     if (mb_lt->hashtable2[ecode2] == 0)
                     {
                        pv_array[ecode2>>pv_array_bts] |= 
                                    (((PV_ARRAY_TYPE) 1)<<(ecode2&PV_ARRAY_MASK));
                     }
                     else
                        longest_chain = MAX(longest_chain, helper_array[ecode2/kCompressionFactor]++); 

                     if (mb_lt->hashtable[ecode1] == 0)
                     {
                        pv_array[ecode1>>pv_array_bts] |= 
                                    (((PV_ARRAY_TYPE) 1)<<(ecode1&PV_ARRAY_MASK));
                     }
                     else
                        longest_chain = MAX(longest_chain, helper_array[ecode1/kCompressionFactor]++); 

                     /* This could overestimate the longest chain again by a factor of 2. */
                     /* Are two helper_arrays needed, or is that overkill? */
                     mb_lt->next_pos2[index] = mb_lt->hashtable2[ecode2];
                     mb_lt->hashtable2[ecode2] = index;
                  } else {
                     if (mb_lt->hashtable[ecode1] == 0)
                     {
                        mb_lt->num_unique_pos_added++;
                        pv_array[ecode1>>pv_array_bts] |= 
                                    (((PV_ARRAY_TYPE) 1)<<(ecode1&PV_ARRAY_MASK));
                     }
                     else
                        longest_chain = MAX(longest_chain, helper_array[ecode1/kCompressionFactor]++); 
                  }
                  mb_lt->next_pos[index] = mb_lt->hashtable[ecode1];
                  mb_lt->hashtable[ecode1] = index;
               }
            }
         }
      }
   }
   sfree(helper_array);
   mb_lt->longest_chain = (Int4) (longest_chain+2);

   return 0;
}
/** Fills in the hashtable and next_pos fields of BlastMBLookupTable*
 * for the contiguous case.
 *
 * @param query the query sequence [in]
 * @param location locations on the query to be indexed in table [in]
 * @param mb_lt the (already allocated) megablast lookup table structure [in|out]
 * @param width number of bytes in megablast word (2 or 3) [in]
 * @param lookup_options specifies the word_size [in]
 * @return zero on success, negative number on failure. 
 */

static Int2 
s_FillContigMBTable(BLAST_SequenceBlk* query, BlastSeqLoc* location,
        BlastMBLookupTable* mb_lt, Uint1 width,
        const LookupTableOptions* lookup_options)

{
   BlastSeqLoc* loc;
   const Uint1 kNucMask = 0xfc;
   /* 12-mers (or perhaps 8-mers) are used to build the lookup table and this is what kWordLength specifies. */
   const Int4 kWordLength = COMPRESSION_RATIO*width;   
   /* The user probably specified a much larger word size (like 28) and this is what full_word_size is. */
   Int4 full_word_size;
   const Int4 kMask = (1 << (8*width - 2)) - 1;
   const Int4 kCompressionFactor=2048; /* compress helper_array by factor of 2048. */
   PV_ARRAY_TYPE *pv_array=NULL;
   Int4 pv_array_bts;
   /* The calculation of the longest chain can be cpu intensive for long queries or sets of queries. 
      So we use a helper_array to keep track of this, but compress it by kCompressionFactor so it stays 
      in memory.  Hence we only end up with a conservative (high) estimate for longest_chain, but this does
      not seem to affect the overall performance of the rest of the program. */
   Uint4 longest_chain=0;
   Uint4* helper_array;     /* Helps to estimate longest chain. */


   ASSERT(mb_lt);

   mb_lt->compressed_wordsize = width;
   full_word_size = mb_lt->word_length = lookup_options->word_size;
   mb_lt->mask = mb_lt->hashsize - 1;

   pv_array = mb_lt->pv_array;
   pv_array_bts = mb_lt->pv_array_bts;

   helper_array = (Uint4*) calloc(mb_lt->hashsize/kCompressionFactor, sizeof(Uint4));
   if (helper_array == NULL)
	return -1;

   for (loc = location; loc; loc = loc->next) {
      /* We want index to be always pointing to the start of the word.
         Since sequence pointer points to the end of the word, subtract
         word length from the loop boundaries. 
      */
      Int4 from = loc->ssr->left;
      Int4 to = loc->ssr->right - kWordLength;
      Int4 ecode = 0;
      Int4 last_offset;
      Int4 index;
      Uint1* pos;
      Uint1* seq;
      Uint1 val;
     
      if (full_word_size > (loc->ssr->right - loc->ssr->left + 1))
         continue; /* case of unmasked region >=  kWordLength but < full_word_size  so no hits should be generated. */

      seq = query->sequence_start + from;
      pos = seq + kWordLength;
      
      /* Also add 1 to all indices, because lookup table indices count 
         from 1. */
      from -= kWordLength - 2;
      last_offset = to + 2;

      for (index = from; index <= last_offset; index++) {
         val = *++seq;
         if ((val & kNucMask) != 0) { /* ambiguity or gap */
            ecode = 0;
            pos = seq + kWordLength;
         } else {
            /* get next base */
            ecode = ((ecode & kMask) << 2) + val;
            if (seq >= pos) {
                  if (mb_lt->hashtable[ecode] == 0)
                  {
                        mb_lt->num_unique_pos_added++;
                        pv_array[ecode>>pv_array_bts] |= 
                                    (((PV_ARRAY_TYPE) 1)<<(ecode&PV_ARRAY_MASK));
                  }
                  else
                  {
                        longest_chain = MAX(longest_chain, helper_array[ecode/kCompressionFactor]++); 
                  }
                  mb_lt->next_pos[index] = mb_lt->hashtable[ecode];
                  mb_lt->hashtable[ecode] = index;
            }
         }
      }
   }
   mb_lt->longest_chain = (Int4) (longest_chain+2);
   sfree(helper_array);

   return 0;
}


/* Documentation in mb_lookup.h */
Int2 MB_LookupTableNew(BLAST_SequenceBlk* query, BlastSeqLoc* location,
        BlastMBLookupTable** mb_lt_ptr,
        const LookupTableOptions* lookup_options)
{
   Int4 pv_shift, pv_size;
   Int2 status=0;
   Int2 bytes_in_word;
   Uint1 width;
   BlastMBLookupTable* mb_lt;
   const Int4 kSmallQueryCutoff = 15000;
   const Int4 kLargeQueryCutoff = 800000;
   
   *mb_lt_ptr = NULL;

   if (!location || !query) {
     /* Empty sequence location provided */
     return -1;
   }

   if ((mb_lt = (BlastMBLookupTable*) calloc(1, sizeof(BlastMBLookupTable))) == NULL)
   {
	return -1;
   }
    
   bytes_in_word = (lookup_options->word_size + 1)/ 4;
   if (bytes_in_word < 3) 
      width = 2;
   else
      width = 3;


   if (width == 2) {
      mb_lt->hashsize = 1 << 16;
      pv_shift = 0;
   }
   else {
      Int4 from, to;
      BlastSeqLoc* loc;
      Int4 table_entries=0;
      /* determine the approximate number of hashtable entries */
      for (loc = location; loc; loc = loc->next) {
         from = loc->ssr->left;
         to = loc->ssr->right;
         table_entries += (to - from);
      }

      /* To fit in the external cache of latter-day micro-
         processors, the PV array must be compressed. pv_shift
	 below is the power of two that the array size is
	 divided by. The target PV array size is 128 kBytes.

	 If the query is too small or too large, the compression 
	 should be higher. Small queries don't reuse the PV array,
	 and large queries saturate it. In either case, cache
	 is better used on other data. */

      if (lookup_options->word_size == 11 && lookup_options->mb_template_length > 0) {
         mb_lt->hashsize = 1 << 22;
         if( table_entries <= kSmallQueryCutoff ||
	     table_entries >= kLargeQueryCutoff )
            pv_shift = 3;
	 else
            pv_shift = 2;
      }
      else {
         mb_lt->hashsize = 1 << 24;
         if( table_entries <= kSmallQueryCutoff ||
	     table_entries >= kLargeQueryCutoff )
            pv_shift = 5;
	 else
            pv_shift = 4;
      }
   }


   if (lookup_options->mb_template_length)
   {
        mb_lt->full_byte_scan = lookup_options->full_byte_scan;  /* Only applies to discontiguous megablast. */
        /* The next two are not allowed in discontiguous megablast. */
        mb_lt->ag_scanning_mode = FALSE;
        mb_lt->variable_wordsize = FALSE; 
        mb_lt->scan_step = 0;
   }
   else
   {
        mb_lt->ag_scanning_mode = TRUE; /* FIXME, are there wordsize where this should be FALSE? */
        mb_lt->scan_step = CalculateBestStride(lookup_options->word_size, lookup_options->variable_wordsize, 
                  lookup_options->lut_type);
        mb_lt->variable_wordsize = lookup_options->variable_wordsize;
   }

   if ((mb_lt->hashtable = (Int4*) 
        calloc(mb_lt->hashsize, sizeof(Int4))) == NULL) {
      MBLookupTableDestruct(mb_lt);
      return -1;
   }

   if ((mb_lt->next_pos = (Int4*) 
        calloc((query->length+1), sizeof(Int4))) == NULL) {
      MBLookupTableDestruct(mb_lt);
      return -1;
   }

   mb_lt->pv_array_bts = PV_ARRAY_BTS + pv_shift; 

   pv_size = mb_lt->hashsize>>(mb_lt->pv_array_bts);
   /* Size measured in PV_ARRAY_TYPE's */
   if ((mb_lt->pv_array = calloc(PV_ARRAY_BYTES, pv_size)) == NULL)
   {
      MBLookupTableDestruct(mb_lt);
      return -1;
   }

   if (lookup_options->mb_template_length)
     status = s_FillDiscMBTable(query, location, mb_lt, width, lookup_options);
   else
     status = s_FillContigMBTable(query, location, mb_lt, width, lookup_options);

   if (status > 0)
      return status;

   *mb_lt_ptr = mb_lt;

   return 0;
}

Int4 MB_AG_ScanSubject(const LookupTableWrap* lookup_wrap,
       const BLAST_SequenceBlk* subject, Int4 start_offset,
       Uint4* q_offsets, Uint4* s_offsets, Int4 max_hits,  
       Int4* end_offset)
{
   BlastMBLookupTable* mb_lt;
   Uint1* s;
   Uint1* abs_start;
   Int4  index=0, s_off;
   
   Int4 q_off;
   PV_ARRAY_TYPE *pv_array;
   Int4 total_hits = 0;
   Int4 compressed_wordsize, compressed_scan_step, word_size;
   Uint1 pv_array_bts;
   
   ASSERT(lookup_wrap->lut_type == MB_LOOKUP_TABLE);
   mb_lt = (BlastMBLookupTable*) lookup_wrap->lut;

   pv_array = mb_lt->pv_array;
   pv_array_bts = mb_lt->pv_array_bts;

   /* Since the test for number of hits here is done after adding them, 
      subtract the longest chain length from the allowed offset array size. */
   max_hits -= mb_lt->longest_chain;

   abs_start = subject->sequence;
   s = abs_start + start_offset/COMPRESSION_RATIO;
   compressed_scan_step = mb_lt->scan_step / COMPRESSION_RATIO;
   compressed_wordsize = mb_lt->compressed_wordsize;
   word_size = compressed_wordsize*COMPRESSION_RATIO;

   index = 0;
   
   /* NB: s in this function always points to the start of the word!
    */
   if (mb_lt->scan_step % COMPRESSION_RATIO == 0) {
      Uint1* s_end = abs_start + subject->length/COMPRESSION_RATIO - 
         compressed_wordsize;
      for ( ; s <= s_end; s += compressed_scan_step) {
         BlastNaLookupInitIndex(compressed_wordsize, s, &index);
         
         if (NA_PV_TEST(pv_array, index, pv_array_bts)) {
            q_off = mb_lt->hashtable[index];
            s_off = (s - abs_start)*COMPRESSION_RATIO;
            if (q_off && (total_hits >= max_hits))
               break;
            while (q_off) {
               q_offsets[total_hits] = q_off - 1;
               s_offsets[total_hits++] = s_off;
               q_off = mb_lt->next_pos[q_off];
            }
         }
      }
      *end_offset = (s - abs_start)*COMPRESSION_RATIO;
   } else {
      Int4 last_offset = subject->length - word_size;
      Uint1 bit;
      Int4 adjusted_index;

      for (s_off = start_offset; s_off <= last_offset; 
           s_off += mb_lt->scan_step) {
         s = abs_start + (s_off / COMPRESSION_RATIO);
         bit = 2*(s_off % COMPRESSION_RATIO);
         /* Compute index for a word made of full bytes */
         s = BlastNaLookupInitIndex(compressed_wordsize, s, &index);
         /* Adjust the word index by the base within a byte */
         adjusted_index = BlastNaLookupAdjustIndex(s, index, mb_lt->mask,
                                                   bit);
         if (NA_PV_TEST(pv_array, adjusted_index, pv_array_bts)) {
            q_off = mb_lt->hashtable[adjusted_index];
            if (q_off && (total_hits >= max_hits))
               break;
            while (q_off) {
               q_offsets[total_hits] = q_off - 1;
               s_offsets[total_hits++] = s_off; 
               q_off = mb_lt->next_pos[q_off];
            }
         }
      }
      *end_offset = s_off;
   }

   return total_hits;
}

Int4 MB_ScanSubject(const LookupTableWrap* lookup,
       const BLAST_SequenceBlk* subject, Int4 start_offset, 
       Uint4* q_offsets, Uint4* s_offsets, Int4 max_hits,
       Int4* end_offset) 
{
   Uint1* abs_start,* s_end;
   Uint1* s;
   Int4 hitsfound = 0;
   Uint4 query_offset, subject_offset;
   Int4 index;
   BlastMBLookupTable* mb_lt;
   Uint4* q_ptr = q_offsets,* s_ptr = s_offsets;
   PV_ARRAY_TYPE *pv_array;
   Uint1 pv_array_bts;
   Int4 compressed_wordsize;

   ASSERT(lookup->lut_type == MB_LOOKUP_TABLE);
   mb_lt = (BlastMBLookupTable*) lookup->lut;

   pv_array = mb_lt->pv_array;
   pv_array_bts = mb_lt->pv_array_bts;
   compressed_wordsize = mb_lt->compressed_wordsize;

   /* Since the test for number of hits here is done after adding them, 
      subtract the longest chain length from the allowed offset array size.
   */
   max_hits -= mb_lt->longest_chain;

   abs_start = subject->sequence;
   s = abs_start + start_offset/COMPRESSION_RATIO;
   s_end = abs_start + (*end_offset)/COMPRESSION_RATIO;

   s = BlastNaLookupInitIndex(mb_lt->compressed_wordsize, s, &index);

   /* In the following code, s points to the byte right after the end of 
      the word. */
   while (s <= s_end) {
      if (NA_PV_TEST(pv_array, index, pv_array_bts)) {
         query_offset = mb_lt->hashtable[index];
         subject_offset = 
            ((s - abs_start) - compressed_wordsize)*COMPRESSION_RATIO;
         if (query_offset && (hitsfound >= max_hits))
            break;
         while (query_offset) {
            *(q_ptr++) = query_offset - 1;
            *(s_ptr++) = subject_offset;
            ++hitsfound;
            query_offset = mb_lt->next_pos[query_offset];
         }
      }
      /* Compute the next value of the lookup index 
         (shifting sequence by a full byte) */
      index = BlastNaLookupComputeIndex(FULL_BYTE_SHIFT, mb_lt->mask, 
                                        s++, index);
   }

   *end_offset = 
     ((s - abs_start) - compressed_wordsize)*COMPRESSION_RATIO;

   return hitsfound;
}

Int4 MB_DiscWordScanSubject(const LookupTableWrap* lookup, 
       const BLAST_SequenceBlk* subject, Int4 start_offset,
       Uint4* q_offsets, Uint4* s_offsets, Int4 max_hits, 
       Int4* end_offset)
{
   Uint1* s;
   Uint1* s_start,* abs_start;
   Int4 hitsfound = 0;
   Uint4 query_offset, subject_offset;
   Int4 word, index, index2=0;
   BlastMBLookupTable* mb_lt;
   Boolean two_templates;
   Uint1 template_type;
   Uint1 second_template_type;
   PV_ARRAY_TYPE *pv_array;
   Uint1 pv_array_bts;
   Int4 compressed_wordsize;
   Uint4 word_end_offset;
   Uint4 last_end_offset = *end_offset;

   ASSERT(lookup->lut_type == MB_LOOKUP_TABLE);
   mb_lt = (BlastMBLookupTable*) lookup->lut;

   two_templates = mb_lt->two_templates;
   template_type = mb_lt->template_type;
   second_template_type = mb_lt->second_template_type;
   pv_array = mb_lt->pv_array;
   pv_array_bts = mb_lt->pv_array_bts;
   compressed_wordsize = mb_lt->compressed_wordsize;
   word_end_offset = start_offset + mb_lt->word_length;

   /* Since the test for number of hits here is done after adding them, 
      subtract the longest chain length from the allowed offset array size. */
   max_hits -= mb_lt->longest_chain;

   abs_start = subject->sequence;
   s_start = abs_start + start_offset/COMPRESSION_RATIO;

   s = BlastNaLookupInitIndex(mb_lt->compressed_wordsize, s_start, &word);

   /* s now points to the byte right after the end of the current word */
   if (mb_lt->full_byte_scan) {

     while (word_end_offset <= last_end_offset) {
       index = ComputeDiscontiguousIndex(s, word, template_type);

       if (two_templates) {
          index2 = ComputeDiscontiguousIndex(s, word, second_template_type);
       }
       if (NA_PV_TEST(pv_array, index, pv_array_bts)) {
          query_offset = mb_lt->hashtable[index];
          subject_offset = 
             ((s - abs_start) - compressed_wordsize)*COMPRESSION_RATIO;

          /* test for completion before the current chain has
             been added. This test counts for both templates
             (they both add hits or neither one does) */
          if (query_offset && (hitsfound >= max_hits))
             break;
          while (query_offset) {
             q_offsets[hitsfound] = query_offset - 1;
             s_offsets[hitsfound++] = subject_offset;
             query_offset = mb_lt->next_pos[query_offset];
          }
       }
       if (two_templates && NA_PV_TEST(pv_array, index2, pv_array_bts)) {
          query_offset = mb_lt->hashtable2[index2];
          subject_offset = 
             ((s - abs_start) - compressed_wordsize)*COMPRESSION_RATIO;
          while (query_offset) {
             q_offsets[hitsfound] = query_offset - 1;
             s_offsets[hitsfound++] = subject_offset;
             query_offset = mb_lt->next_pos2[query_offset];
          }
       }
       word = BlastNaLookupComputeIndex(FULL_BYTE_SHIFT, 
                           mb_lt->mask, s++, word);
       word_end_offset += COMPRESSION_RATIO;
     }
   } else {
      const Int4 kScanStep = 1; /* scan one letter at a time. */
      const Int4 kScanShift = 2; /* scan 2 bits (that is, one letter) at a time. */
      Uint1 bit = 2*(start_offset % COMPRESSION_RATIO);
      Int4 adjusted_word;

      while (word_end_offset <= last_end_offset) {
         /* Adjust the word index by the base within a byte */
         adjusted_word = BlastNaLookupAdjustIndex(s, word, mb_lt->mask,
                                         bit);
   
         index = ComputeDiscontiguousIndex_1b(s, adjusted_word, bit,
                    template_type);
       
         if (two_templates)
            index2 = ComputeDiscontiguousIndex_1b(s, adjusted_word, bit,
                        second_template_type);

         if (NA_PV_TEST(pv_array, index, pv_array_bts)) {
            query_offset = mb_lt->hashtable[index];
            subject_offset = 
               ((s - abs_start) - compressed_wordsize)*COMPRESSION_RATIO
               + bit/2;
            /* test for completion before the current chain has
               been added. This test counts for both templates
               (they both add hits or neither one does) */
            if (query_offset && (hitsfound >= max_hits))
               break;
            while (query_offset) {
               q_offsets[hitsfound] = query_offset - 1;
               s_offsets[hitsfound++] = subject_offset;
               query_offset = mb_lt->next_pos[query_offset];
            }
         }
         if (two_templates && NA_PV_TEST(pv_array, index2, pv_array_bts)) {
            query_offset = mb_lt->hashtable2[index2];
            subject_offset = 
               ((s - abs_start) - compressed_wordsize)*COMPRESSION_RATIO
               + bit/2;
            while (query_offset) {
               q_offsets[hitsfound] = query_offset - 1;
               s_offsets[hitsfound++] = subject_offset;
               query_offset = mb_lt->next_pos2[query_offset];
            }
         }
         bit += kScanShift;
         word_end_offset += kScanStep;

         if (bit >= FULL_BYTE_SHIFT) {
            /* Advance to the next full byte */
            bit -= FULL_BYTE_SHIFT;
            word = BlastNaLookupComputeIndex(FULL_BYTE_SHIFT, 
                                mb_lt->mask, s++, word);
         }
      }
   }
   *end_offset = word_end_offset - mb_lt->word_length;

   return hitsfound;
}

