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
* ===========================================================================*/

/*****************************************************************************

File name: mb_lookup.c

Author: Ilya Dondoshansky

Contents: Functions responsible for the creation of a lookup table

Detailed Contents: 

******************************************************************************
 * $Revision$
 */
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/mb_lookup.h>


static char const rcsid[] = "$Id$";

MBLookupTable* MBLookupTableDestruct(MBLookupTable* mb_lt)
{
   if (!mb_lt)
      return NULL;
   if (mb_lt->hashtable)
      sfree(mb_lt->hashtable);
   if (mb_lt->next_pos)
      sfree(mb_lt->next_pos);
   if (mb_lt->next_pos2)
      sfree(mb_lt->next_pos2);
   sfree(mb_lt->pv_array);

   sfree(mb_lt);
   return mb_lt;
}

/** Convert weight, template length and template type from input options into
    an MBTemplateType enum
*/
static DiscTemplateType GetDiscTemplateType(Int2 weight, Uint1 length, 
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

/** Documentation in mb_lookup.h */
Int2 MB_LookupTableNew(BLAST_SequenceBlk* query, ListNode* location,
        MBLookupTable** mb_lt_ptr,
        const LookupTableOptions* lookup_options)
{
   Int4 query_length;
   Uint1* seq,* pos;
   Int4 index;
   Int4 ecode;
   Int4 mask;
   Int4 ecode1, ecode2;
   Uint1 val, nuc_mask = 0xfc;
   MBLookupTable* mb_lt;
   Int4 masked_word_count = 0;
   PV_ARRAY_TYPE *pv_array=NULL;
   Int4 pv_shift, pv_array_bts, pv_size, pv_index;
   Int2 word_length, extra_length;
   Int4 last_offset;
#ifdef USE_HASH_TABLE
   Int4 hash_shift, hash_mask, crc, size, length;
   Uint1* hash_buf;
#endif
   DiscTemplateType template_type;
   Boolean amb_cond;
   Boolean two_templates = 
      (lookup_options->mb_template_type == MB_TWO_TEMPLATES);
   Int2 bytes_in_word;
   Uint1 width;
   Int4 from, to;
   ListNode* loc;
   Int4 longest_chain = 0;
   Int4 pcount, pcount1=0;
   
   if (!location) {
     /* Empty sequence location provided */
     *mb_lt_ptr = NULL;
     return -1;
   }

   template_type = GetDiscTemplateType(lookup_options->word_size,
                                     lookup_options->mb_template_length, 
                                     lookup_options->mb_template_type);
   query_length = query->length;
   mb_lt = (MBLookupTable*) calloc(1, sizeof(MBLookupTable));
    
   bytes_in_word = (lookup_options->word_size + 1)/ 4;
   if (bytes_in_word < 3) 
      width = 2;
   else
      width = 3;

   mb_lt->template_type = template_type;
   mb_lt->two_templates = two_templates;
   if (two_templates)
      /* For now leave only one possibility for the second template */
      mb_lt->second_template_type = template_type + 1;

#ifdef USE_HASH_TABLE
   /* Make hash table size the smallest power of 2 greater than query length */
   for (length=query_length,size=1; length>0; length=length>>1,size++);
   mb_lt->hashsize = 1<<size;
   hash_shift = (32 - size)/2;
   hash_mask = mb_lt->hashsize - 1;
#else
   mb_lt->hashsize = (1<<(8*width));
#endif

   if (lookup_options->mb_template_length > 0) {
      mb_lt->discontiguous = TRUE;
      mb_lt->compressed_wordsize = width + 1;
      word_length = mb_lt->word_length = COMPRESSION_RATIO*(width+1);
      mb_lt->template_length = lookup_options->mb_template_length;
      extra_length = mb_lt->template_length - word_length;
      mb_lt->mask = (Int4) (-1);
      mask = (1 << (8*(width+1) - 2)) - 1;
   } else {
      mb_lt->compressed_wordsize = width;
      word_length = COMPRESSION_RATIO*width;
      mb_lt->word_length = lookup_options->word_size;
      mb_lt->template_length = COMPRESSION_RATIO*width;
      mb_lt->mask = mb_lt->hashsize - 1;
      mask = (1 << (8*width - 2)) - 1;
      extra_length = 0;
   }

   /* Scan step must be set in lookup_options; even if it is not, still 
      try to set it reasonably in mb_lt - it can't remain 0 */
   if (lookup_options->scan_step)
      mb_lt->scan_step = lookup_options->scan_step;
   else
      mb_lt->scan_step = COMPRESSION_RATIO;

   mb_lt->full_byte_scan = (mb_lt->scan_step % COMPRESSION_RATIO == 0);

   if ((mb_lt->hashtable = (Int4*) 
        calloc(mb_lt->hashsize, sizeof(Int4))) == NULL) {
      MBLookupTableDestruct(mb_lt);
      return -1;
   }

   if ((mb_lt->next_pos = (Int4*) 
        calloc((query_length+1), sizeof(Int4))) == NULL) {
      MBLookupTableDestruct(mb_lt);
      return -1;
   }

   if (two_templates) {
      if (lookup_options->word_size >= 12) {
         if ((mb_lt->hashtable2 = (Int4*) 
              calloc(mb_lt->hashsize, sizeof(Int4))) == NULL) {
            MBLookupTableDestruct(mb_lt);
            return -1;
         }
      } else {/* For weight 11 no need for extra main table */
         mb_lt->hashtable2 = mb_lt->hashtable;
      }
      if ((mb_lt->next_pos2 = (Int4*) 
           calloc((query_length+1), sizeof(Int4))) == NULL) {
         MBLookupTableDestruct(mb_lt);
         return -1;
      }
   }

   for (loc = location; loc; loc = loc->next) {
      from = ((DoubleInt*) loc->ptr)->i1;
      to = ((DoubleInt*) loc->ptr)->i2;

      seq = query->sequence_start + from;
      pos = seq + word_length;
      
      ecode = 0;

      last_offset = to + 1 - extra_length;
      amb_cond = TRUE;

      for (index = from + 1; index <= last_offset; index++) {
         val = *++seq;
         if ((val & nuc_mask) != 0) { /* ambiguity or gap */
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
                     
#ifdef USE_HASH_TABLE                     
                  hash_buf = (Uint1*)&ecode1;
                  CRC32(crc, hash_buf);
                  ecode1 = (crc>>hash_shift) & hash_mask;
#endif

                  if (two_templates) {
                     switch (template_type) {
                     case TEMPL_11_16:
                        ecode2 = GET_WORD_INDEX_11_16_OPT(ecode) | 
                           SECOND_TEMPLATE_BIT;
                        break;
                     case TEMPL_12_16:
                        ecode2 = GET_WORD_INDEX_12_16_OPT(ecode);
                        break;
                     case TEMPL_11_18:
                        ecode2 = (GET_WORD_INDEX_11_18_OPT(ecode)) |
                           (GET_EXTRA_CODE_18_OPT(seq)) | SECOND_TEMPLATE_BIT;
                        break;
                     case TEMPL_12_18:
                        ecode2 = (GET_WORD_INDEX_12_18_OPT(ecode)) |
                           (GET_EXTRA_CODE_18_OPT(seq));
                        break;
                     case TEMPL_11_21:
                        ecode2 = (GET_WORD_INDEX_11_21_OPT(ecode)) |
                           (GET_EXTRA_CODE_21_OPT(seq)) | SECOND_TEMPLATE_BIT;
                        break;
                     case TEMPL_12_21:
                        ecode2 = (GET_WORD_INDEX_12_21_OPT(ecode)) |
                           (GET_EXTRA_CODE_21_OPT(seq));
                        break;
                     default:
                        ecode2 = 0; break;
                     }
                     
#ifdef USE_HASH_TABLE                     
                     hash_buf = (Uint1*)&ecode2;
                     CRC32(crc, hash_buf);
                     ecode2 = (crc>>hash_shift) & hash_mask;
#endif
                     if (mb_lt->hashtable[ecode1] == 0 || 
                         mb_lt->hashtable2[ecode2] == 0)
                        mb_lt->num_unique_pos_added++;
                     mb_lt->next_pos2[index] = mb_lt->hashtable2[ecode2];
                     mb_lt->hashtable2[ecode2] = index;
                  } else {
                     if (mb_lt->hashtable[ecode1] == 0)
                        mb_lt->num_unique_pos_added++;
                  }
                  mb_lt->next_pos[index] = mb_lt->hashtable[ecode1];
                  mb_lt->hashtable[ecode1] = index;
               }
            }
         }
      }
   }
   
#ifdef USE_HASH_TABLE
   /* If hash table is used instead of index table, no need for extra reduction
      of pv_array size */
   pv_shift = 0;
#else
   /* For 12-mer based lookup table need to make presense bit array much 
      smaller, so it stays in cache, even though this allows for collisions */
   pv_shift = (width < 3) ? 0 : 5;
#endif
   mb_lt->pv_array_bts = pv_array_bts = PV_ARRAY_BTS + pv_shift;

#ifdef QUESTION_PV_ARRAY_USE
   if (mb_lt->num_unique_pos_added < 
       (PV_ARRAY_FACTOR*(mb_lt->hashsize>>pv_shift))) {
#endif
      pv_size = mb_lt->hashsize>>pv_array_bts;
      /* Size measured in PV_ARRAY_TYPE's */
      pv_array = calloc(PV_ARRAY_BYTES, pv_size);

      for (index=0; index<mb_lt->hashsize; index++) {
         if (mb_lt->hashtable[index] != 0 ||
             (two_templates && mb_lt->hashtable2[index] != 0))
            pv_array[index>>pv_array_bts] |= 
               (((PV_ARRAY_TYPE) 1)<<(index&PV_ARRAY_MASK));
      }
      mb_lt->pv_array = pv_array;
#ifdef QUESTION_PV_ARRAY_USE
   }
#endif

   /* Now remove the hash entries that have too many positions */
#if 0
   if (lookup_options->max_positions>0) {
#endif
      for (pv_index = 0; pv_index < pv_size; ++pv_index) {
         if (pv_array[pv_index]) {
            for (ecode = pv_index<<pv_array_bts; 
                 ecode < (pv_index+1)<<pv_array_bts; ++ecode) {
               for (index=mb_lt->hashtable[ecode], pcount=0; 
                    index>0; index=mb_lt->next_pos[index], pcount++);
               if (two_templates) {
                  for (index=mb_lt->hashtable2[ecode], pcount1=0; 
                       index>0; index=mb_lt->next_pos2[index], pcount1++);
               }
               if ((pcount=MAX(pcount,pcount1))>lookup_options->max_positions) {
                  mb_lt->hashtable[ecode] = 0;
                  if (two_templates)
                     mb_lt->hashtable2[ecode] = 0;
#ifdef ERR_POST_EX_DEFINED
                  ErrPostEx(SEV_WARNING, 0, 0, "%lx - %d", ecode, pcount);
#endif
                  masked_word_count++;
               }
               longest_chain = MAX(longest_chain, pcount);
            }
         }
      }
      if (masked_word_count) {
#ifdef ERR_POST_EX_DEFINED
	 ErrPostEx(SEV_WARNING, 0, 0, "Masked %d %dmers", masked_word_count,
		   4*width);
#endif
      }
#if 0
   }
#endif

   mb_lt->longest_chain = longest_chain;


   *mb_lt_ptr = mb_lt;

   return 0;
}

/** This function is identical to BlastNaLookupInitIndex in blast_lookup.c. 
 * It is copied here to allow inlining.
 */
static NCBI_INLINE Uint1* BlastNaLookupInitIndex(Int4 length,
		          const Uint1* word, Int4* index)
{
   Int4 i;
   
   *index = 0;
   for (i = 0; i < length; ++i)
      *index = ((*index)<<FULL_BYTE_SHIFT) | word[i];
   return (Uint1 *) (word + length);
}

/** This function is identical to BlastNaLookupComputeIndex 
 * in blast_lookup.c. 
 * It is copied here to allow inlining.
 */
static NCBI_INLINE Int4 BlastNaLookupComputeIndex(Int4 scan_shift, Int4 mask, 
		      const Uint1* word, Int4 index)
{
   return (((index)<<scan_shift) & mask) | *(word);  
}

/** This function is identical to BlastNaLookupAdjustIndex 
 * in blast_lookup.c. 
 * It is copied here to allow inlining.
 */
static NCBI_INLINE Int4 BlastNaLookupAdjustIndex(Uint1* s, Int4 index, 
                      Int4 mask, Uint1 bit)
{
   return (((index)<<bit) & mask) | ((*s)>>(FULL_BYTE_SHIFT-bit));
}

Int4 MB_AG_ScanSubject(const LookupTableWrap* lookup_wrap,
       const BLAST_SequenceBlk* subject, Int4 start_offset,
       Uint4* q_offsets, Uint4* s_offsets, Int4 max_hits,  
       Int4* end_offset)
{
   MBLookupTable* mb_lt = (MBLookupTable*) lookup_wrap->lut;
   Uint1* s;
   Uint1* abs_start;
   Int4  index=0, s_off;
   
   Int4 q_off;
   PV_ARRAY_TYPE *pv_array = mb_lt->pv_array;
   Int4 total_hits = 0;
   Int4 compressed_wordsize, compressed_scan_step, word_size;
   Boolean full_byte_scan = mb_lt->full_byte_scan;
   Uint1 pv_array_bts = mb_lt->pv_array_bts;
   
   abs_start = subject->sequence;
   s = abs_start + start_offset/COMPRESSION_RATIO;
   compressed_scan_step = mb_lt->scan_step / COMPRESSION_RATIO;
   compressed_wordsize = mb_lt->compressed_wordsize;
   word_size = compressed_wordsize*COMPRESSION_RATIO;

   index = 0;
   
   /* NB: s in this function always points to the start of the word!
    */
   if (full_byte_scan) {
      Uint1* s_end = abs_start + subject->length/COMPRESSION_RATIO - 
         compressed_wordsize;
      for ( ; s <= s_end; s += compressed_scan_step) {
         BlastNaLookupInitIndex(compressed_wordsize, s, &index);
         
         if (NA_PV_TEST(pv_array, index, pv_array_bts)) {
            q_off = mb_lt->hashtable[index];
            s_off = 
               ((s - abs_start) + compressed_wordsize)*COMPRESSION_RATIO;
            while (q_off) {
               q_offsets[total_hits] = q_off;
               s_offsets[total_hits++] = s_off;
               q_off = mb_lt->next_pos[q_off];
            }
            if (total_hits >= max_hits)
               break;
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
            while (q_off) {
               q_offsets[total_hits] = q_off;
               s_offsets[total_hits++] = s_off + word_size; 
               q_off = mb_lt->next_pos[q_off];
            }
            if (total_hits >= max_hits)
               break;
         }
      }
      *end_offset = s_off;
   }

   return total_hits;
}

/** Given a word packed into an integer, compute a discontiguous word lookup 
 *  index.
 * @param subject Pointer to the next byte of the sequence after the end of 
 *        the word (needed when word template is longer than 16 bases) [in]
 * @param word A piece of the sequence packed into an integer [in]
 * @param template_type What type of discontiguous word template to use [in]
 * @param second_template_bit When index has fewer bits than the lookup table 
 *        width, the indices for the second template are distinguished from 
 *        those for the first template by setting a special bit. [in]
 * @return The lookup table index of the discontiguous word [out]
 */
static NCBI_INLINE Int4 ComputeDiscontiguousIndex(Uint1* subject, Int4 word,
                  Uint1 template_type, Int4 second_template_bit)
{
   Int4 index;
   Int4 extra_code;   

   switch (template_type) {
   case TEMPL_11_16:
      index = GET_WORD_INDEX_11_16(word) | second_template_bit;
      break;
   case TEMPL_12_16:
      index = GET_WORD_INDEX_12_16(word);
      break;
   case TEMPL_11_16_OPT:
      index = GET_WORD_INDEX_11_16_OPT(word) | second_template_bit;
      break;
   case TEMPL_12_16_OPT:
      index = GET_WORD_INDEX_12_16_OPT(word);
      break;
   case TEMPL_11_18: 
     extra_code = (Int4) GET_EXTRA_CODE_PACKED_4_18(subject);
     index = (GET_WORD_INDEX_11_18(word) | extra_code) | second_template_bit;
     break;
   case TEMPL_12_18: 
      extra_code = (Int4) GET_EXTRA_CODE_PACKED_4_18(subject);
      index = (GET_WORD_INDEX_12_18(word) | extra_code);
      break;
   case TEMPL_11_18_OPT: 
      extra_code = (Int4) GET_EXTRA_CODE_PACKED_4_18_OPT(subject);
      index = (GET_WORD_INDEX_11_18_OPT(word) | extra_code) | 
         second_template_bit;
      break;
   case TEMPL_12_18_OPT:
      extra_code = (Int4) GET_EXTRA_CODE_PACKED_4_18_OPT(subject);
      index = (GET_WORD_INDEX_12_18_OPT(word) | extra_code);
      break;
   case TEMPL_11_21: 
      extra_code = (Int4) GET_EXTRA_CODE_PACKED_4_21(subject);
      index = (GET_WORD_INDEX_11_21(word) | extra_code) | 
         second_template_bit;
      break;
   case TEMPL_12_21:
      extra_code = (Int4) GET_EXTRA_CODE_PACKED_4_21(subject);
      index = (GET_WORD_INDEX_12_21(word) | extra_code);
      break;
   case TEMPL_11_21_OPT: 
      extra_code = (Int4) GET_EXTRA_CODE_PACKED_4_21_OPT(subject);
      index = (GET_WORD_INDEX_11_21_OPT(word) | extra_code) | 
         second_template_bit;
      break;
   case TEMPL_12_21_OPT:
      extra_code = (Int4) GET_EXTRA_CODE_PACKED_4_21_OPT(subject);
      index = (GET_WORD_INDEX_12_21_OPT(word) | extra_code);
         break;
   default: 
      extra_code = 0; 
      index = 0;
      break;
   }
#ifdef USE_HASH_TABLE
   hash_buf = (Uint1*)&index;
   CRC32(crc, hash_buf);
   index = (crc>>hash_shift) & hash_mask;
#endif

   return index;
}

/** Compute the lookup table index for the first word template, given a word 
 * position, template type and previous value of the word, in case of 
 * one-base (2 bit) database scanning.
 * @param word_start Pointer to the start of a word in the sequence [in]
 * @param word The word packed into an integer value [in]
 * @param sequence_bit By how many bits the real word start is shifted within 
 *        a compressed sequence byte [in]
 * @param template_type What discontiguous word template to use for index 
 *        computation [in]
 * @param second_template_bit Bit to set if this index is for a second 
 *        template [in]
 * @return The lookup index for the discontiguous word.
*/
static NCBI_INLINE Int4 ComputeDiscontiguousIndex_1b(const Uint1* word_start, 
                      Int4 word, Uint1 sequence_bit, Uint1 template_type,
                      Int4 second_template_bit)
{
   Int4 index;
   Uint1* subject = (Uint1 *) word_start;
   Uint1 bit;
   Int4 extra_code, tmpval;   

   /* Prepare auxiliary variables for extra code calculation */
   tmpval = 0;
   extra_code = 0;
   /* The bits in an integer byte are counted in a reverse order than in a
      sequence byte */
   bit = 6 - sequence_bit;

   switch (template_type) {
   case TEMPL_11_16:
      index = GET_WORD_INDEX_11_16(word) | second_template_bit;
      break;
   case TEMPL_12_16:
      index = GET_WORD_INDEX_12_16(word);
      break;
   case TEMPL_11_16_OPT:
      index = GET_WORD_INDEX_11_16_OPT(word) | second_template_bit;
      break;
   case TEMPL_12_16_OPT:
      index = GET_WORD_INDEX_12_16_OPT(word);
      break;
   case TEMPL_11_18: 
      GET_EXTRA_CODE_PACKED_18(subject, bit, tmpval, extra_code);
      index = (GET_WORD_INDEX_11_18(word) | extra_code) | 
         second_template_bit;
      break;
   case TEMPL_12_18: 
      GET_EXTRA_CODE_PACKED_18(subject, bit, tmpval, extra_code);
      index = (GET_WORD_INDEX_12_18(word) | extra_code);
      break;
   case TEMPL_11_18_OPT: 
      GET_EXTRA_CODE_PACKED_18_OPT(subject, bit, tmpval, extra_code);
      index = (GET_WORD_INDEX_11_18_OPT(word) | extra_code) | 
         second_template_bit;
      break;
   case TEMPL_12_18_OPT:
      GET_EXTRA_CODE_PACKED_18_OPT(subject, bit, tmpval, extra_code);
      index = (GET_WORD_INDEX_12_18_OPT(word) | extra_code);
      break;
   case TEMPL_11_21: 
      GET_EXTRA_CODE_PACKED_21(subject, bit, tmpval, extra_code);
      index = (GET_WORD_INDEX_11_21(word) | extra_code) | 
         second_template_bit;
      break;
   case TEMPL_12_21:
      GET_EXTRA_CODE_PACKED_21(subject, bit, tmpval, extra_code);
      index = (GET_WORD_INDEX_12_21(word) | extra_code);
      break;
   case TEMPL_11_21_OPT: 
      GET_EXTRA_CODE_PACKED_21_OPT(subject, bit, tmpval, extra_code);
      index = (GET_WORD_INDEX_11_21_OPT(word) | extra_code) | 
         second_template_bit;
      break;
   case TEMPL_12_21_OPT:
      GET_EXTRA_CODE_PACKED_21_OPT(subject, bit, tmpval, extra_code);
      index = (GET_WORD_INDEX_12_21_OPT(word) | extra_code);
         break;
   default: 
      extra_code = 0; 
      index = 0;
      break;
   }
#ifdef USE_HASH_TABLE
   hash_buf = (Uint1*)&index;
   CRC32(crc, hash_buf);
   index = (crc>>hash_shift) & hash_mask;
#endif
  
   return index;
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
   MBLookupTable* mb_lt = (MBLookupTable*) lookup->lut;
   Uint4* q_ptr = q_offsets,* s_ptr = s_offsets;
   PV_ARRAY_TYPE *pv_array = mb_lt->pv_array;
   Uint1 pv_array_bts = mb_lt->pv_array_bts;

#ifdef DEBUG_LOG
   FILE *logfp0 = fopen("new0.log", "a");
#endif   

   abs_start = subject->sequence;
   s = abs_start + start_offset/COMPRESSION_RATIO;
   s_end = abs_start + (*end_offset)/COMPRESSION_RATIO;

   s = BlastNaLookupInitIndex(mb_lt->compressed_wordsize, s, &index);

   while (s <= s_end) {
      if (NA_PV_TEST(pv_array, index, pv_array_bts)) {
         query_offset = mb_lt->hashtable[index];
         subject_offset = (s - abs_start)*COMPRESSION_RATIO;
         while (query_offset) {
#ifdef DEBUG_LOG
            fprintf(logfp0, "%ld\t%ld\t%ld\n", query_offset, 
                    subject_offset, index);
#endif
            *(q_ptr++) = query_offset;
            *(s_ptr++) = subject_offset;
            ++hitsfound;
            query_offset = mb_lt->next_pos[query_offset];
         }
         if (hitsfound >= max_hits)
            break;
      }
      /* Compute the next value of the lookup index 
         (shifting sequence by a full byte) */
      index = BlastNaLookupComputeIndex(FULL_BYTE_SHIFT, mb_lt->mask, 
                                        s++, index);
   }

   *end_offset = 
     ((s - abs_start) - mb_lt->compressed_wordsize)*COMPRESSION_RATIO;
#ifdef DEBUG_LOG
   fclose(logfp0);
#endif

   return hitsfound;
}

Int4 MB_DiscWordScanSubject(const LookupTableWrap* lookup, 
       const BLAST_SequenceBlk* subject, Int4 start_offset,
       Uint4* q_offsets, Uint4* s_offsets, Int4 max_hits, 
       Int4* end_offset)
{
   Uint1* s;
   Uint1* s_start,* abs_start,* s_end;
   Int4 hitsfound = 0;
   Uint4 query_offset, subject_offset;
   Int4 word, index, index2=0;
   MBLookupTable* mb_lt = (MBLookupTable*) lookup->lut;
   Uint4* q_ptr = q_offsets,* s_ptr = s_offsets;
   Boolean full_byte_scan = mb_lt->full_byte_scan;
   Boolean two_templates = mb_lt->two_templates;
   Uint1 template_type = mb_lt->template_type;
   Uint1 second_template_type = mb_lt->second_template_type;
   PV_ARRAY_TYPE *pv_array = mb_lt->pv_array;
   Uint1 pv_array_bts = mb_lt->pv_array_bts;

#ifdef DEBUG_LOG
   FILE *logfp0 = fopen("new0.log", "a");
#endif   
   
   abs_start = subject->sequence;
   s_start = abs_start + start_offset/COMPRESSION_RATIO;
   s_end = abs_start + (*end_offset)/COMPRESSION_RATIO;

   s = BlastNaLookupInitIndex(mb_lt->compressed_wordsize, s_start, &word);

   /* s now points to the byte right after the end of the current word */
   if (full_byte_scan) {

     while (s <= s_end) {
       index = ComputeDiscontiguousIndex(s, word, template_type, 0);

       if (two_templates) {
          index2 = ComputeDiscontiguousIndex(s, word, second_template_type, 
                                             SECOND_TEMPLATE_BIT);
       }
       if (NA_PV_TEST(pv_array, index, pv_array_bts)) {
          query_offset = mb_lt->hashtable[index];
          subject_offset = (s - abs_start)*COMPRESSION_RATIO;
          while (query_offset) {
#ifdef DEBUG_LOG
             fprintf(logfp0, "%ld\t%ld\t%ld\n", query_offset, 
                     subject_offset, index);
#endif
             *(q_ptr++) = query_offset;
             *(s_ptr++) = subject_offset;
             ++hitsfound;
             query_offset = mb_lt->next_pos[query_offset];
          }
          if (hitsfound >= max_hits)
             break;
       }
       if (two_templates && NA_PV_TEST(pv_array, index2, pv_array_bts)) {
          query_offset = mb_lt->hashtable[index2];
          subject_offset = (s - abs_start)*COMPRESSION_RATIO;
          while (query_offset) {
             q_offsets[hitsfound] = query_offset;
             s_offsets[hitsfound++] = subject_offset;
             query_offset = mb_lt->next_pos[query_offset];
          }
       }
       word = BlastNaLookupComputeIndex(FULL_BYTE_SHIFT, 
                           mb_lt->mask, s++, word);
     }
   } else {
      Int4 scan_shift = 2*mb_lt->scan_step;
      Uint1 bit = 2*(start_offset % COMPRESSION_RATIO);
      Int4 adjusted_word;

      while (s <= s_end) {
         /* Adjust the word index by the base within a byte */
         adjusted_word = BlastNaLookupAdjustIndex(s, word, mb_lt->mask,
                                         bit);
   
         index = ComputeDiscontiguousIndex_1b(s, adjusted_word, bit,
                    template_type, 0);
       
         if (two_templates)
            index2 = ComputeDiscontiguousIndex_1b(s, adjusted_word, bit,
                        second_template_type, SECOND_TEMPLATE_BIT);

         if (NA_PV_TEST(pv_array, index, pv_array_bts)) {
            query_offset = mb_lt->hashtable[index];
            subject_offset = (s - abs_start)*COMPRESSION_RATIO + bit/2;
            while (query_offset) {
#ifdef DEBUG_LOG
               fprintf(logfp0, "%ld\t%ld\t%ld\n", query_offset, 
                       subject_offset, index);
#endif
               *(q_ptr++) = query_offset;
               *(s_ptr++) = subject_offset;
               ++hitsfound;
               query_offset = mb_lt->next_pos[query_offset];
            }
            if (hitsfound >= max_hits)
               break;
         }
         if (two_templates && NA_PV_TEST(pv_array, index2, pv_array_bts)) {
            query_offset = mb_lt->hashtable[index2];
            subject_offset = (s - abs_start)*COMPRESSION_RATIO + bit/2;
            while (query_offset) {
               q_offsets[hitsfound] = query_offset;
               s_offsets[hitsfound++] = subject_offset;
               query_offset = mb_lt->next_pos[query_offset];
            }
         }
         bit += scan_shift;
         if (bit >= FULL_BYTE_SHIFT) {
            /* Advance to the next full byte */
            bit -= FULL_BYTE_SHIFT;
            word = BlastNaLookupComputeIndex(FULL_BYTE_SHIFT, 
                                mb_lt->mask, s++, word);
         }
      }
   }
   *end_offset = 
     ((s - abs_start) - mb_lt->compressed_wordsize)*COMPRESSION_RATIO;
#ifdef DEBUG_LOG
   fclose(logfp0);
#endif

   return hitsfound;
}

