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
#include <blast_options.h>
#include <blast_def.h>
#include <mb_lookup.h>

static char const rcsid[] = "$Id$";

MBLookupTablePtr MBLookupTableDestruct(MBLookupTablePtr mb_lt)
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
DiscTemplateType GetDiscTemplateType(Int2 weight, Uint1 length, 
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
Int2 MB_LookupTableNew(BLAST_SequenceBlkPtr query, ValNodePtr location,
        MBLookupTablePtr PNTR mb_lt_ptr,
        LookupTableOptionsPtr lookup_options)
{
   Int4 query_length;
   Uint1Ptr seq, pos;
   Int4 index;
   Int4 ecode;
   Int4 mask;
   Int4 ecode1, ecode2;
   Uint1 val, nuc_mask = 0xfc;
   MBLookupTablePtr mb_lt;
   Int4 masked_word_count = 0;
   PV_ARRAY_TYPE *pv_array=NULL;
   Int4 pv_shift, pv_array_bts, pv_size, pv_index;
   Int2 word_length, extra_length;
   Int4 last_offset;
#ifdef USE_HASH_TABLE
   Int4 hash_shift, hash_mask, crc, size, length;
   Uint1Ptr hash_buf;
#endif
   DiscTemplateType template_type;
   Boolean amb_cond;
   Boolean two_templates = 
      (lookup_options->mb_template_type == MB_TWO_TEMPLATES);
   Int2 bytes_in_word;
   Uint1 width;
   Int4 from, to;
   ValNodePtr loc;
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
   mb_lt = (MBLookupTablePtr) MemNew(sizeof(MBLookupTable));
    
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

   if ((mb_lt->hashtable = (Int4Ptr) 
        MemNew(mb_lt->hashsize*sizeof(Int4))) == NULL) {
      MBLookupTableDestruct(mb_lt);
      return -1;
   }

   if ((mb_lt->next_pos = (Int4Ptr) 
        MemNew((query_length+1)*sizeof(Int4))) == NULL) {
      MBLookupTableDestruct(mb_lt);
      return -1;
   }

   if (two_templates) {
      if (lookup_options->word_size >= 12) {
         if ((mb_lt->hashtable2 = (Int4Ptr) 
              MemNew(mb_lt->hashsize*sizeof(Int4))) == NULL) {
            MBLookupTableDestruct(mb_lt);
            return -1;
         }
      } else {/* For weight 11 no need for extra main table */
         mb_lt->hashtable2 = mb_lt->hashtable;
      }
      if ((mb_lt->next_pos2 = (Int4Ptr) 
           MemNew((query_length+1)*sizeof(Int4))) == NULL) {
         MBLookupTableDestruct(mb_lt);
         return -1;
      }
   }

   for (loc = location; loc; loc = loc->next) {
      from = ((DoubleIntPtr) loc->data.ptrvalue)->i1;
      to = ((DoubleIntPtr) loc->data.ptrvalue)->i2;

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
                  hash_buf = (Uint1Ptr)&ecode1;
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
                     hash_buf = (Uint1Ptr)&ecode2;
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
      pv_array = MemNew(pv_size*PV_ARRAY_BYTES);

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
#if ERR_POST_EX_DEFINED
                  ErrPostEx(SEV_WARNING, 0, 0, "%lx - %d", ecode, pcount);
#endif
                  masked_word_count++;
               }
               longest_chain = MAX(longest_chain, pcount);
            }
         }
      }
      if (masked_word_count)
#if ERR_POST_EX_DEFINED
	 ErrPostEx(SEV_WARNING, 0, 0, "Masked %d %dmers", masked_word_count,
		   4*width);
#endif
#if 0
   }
#endif

   mb_lt->longest_chain = longest_chain;


   *mb_lt_ptr = mb_lt;

   return 0;
}

