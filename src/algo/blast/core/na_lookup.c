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

File name: na_lookup.c

Author: Ilya Dondoshansky

Contents: Functions for accessing the lookup tables for nucleotide BLAST

******************************************************************************
 * $Revision$
 * */

#include <blast_def.h>
#include <aa_lookup.h>
#include <na_lookup.h>
#include <mb_lookup.h>

static char const rcsid[] = "$Id$";

#define FULL_BYTE_SHIFT 8

/** Given a starting position of a word in a compressed nucleotide sequence, 
 *  compute this word's lookup table index
 * @param length The length of the word [in]
 * @param word Pointer to the start of the word [in]
 * @param index Lookup table index [out]
 * @return Pointer to the next byte after the end of the word
 */
static NCBI_INLINE Uint1Ptr BlastNaLookupInitIndex(Int4 length,
		          const Uint1Ptr word, Int4Ptr index)
{
   Int4 i;
   
   *index = 0;
   for (i = 0; i < length; ++i)
      *index = ((*index)<<FULL_BYTE_SHIFT) | word[i];
   return word + length;
}

/** Recompute the word index given its previous value and the new location 
 *  of the last byte of the word
 * @param scan_shift The number of bits to shift the previous word [in]
 * @param mask The mask to cut off the higher bits of the previous word [in]
 * @param word Pointer to the beginning of the previous word [in]
 * @param index The lookup index value for the previous word [in]
 * @return The value of the lookup index for the next word 
 */
static NCBI_INLINE Int4 BlastNaLookupComputeIndex(Int4 scan_shift, Int4 mask, 
		      const Uint1Ptr word, Int4 index)
{
   return (((index)<<scan_shift) & mask) | *(word);  
   
}

/** Given a word computed from full bytes of a compressed sequence, 
 *  shift it by 0-3 bases 
 * @param s Pointer to the start of a word in the compressed sequence [in]
 * @param index The unadjusted lookup index [in] 
 * @param mask The mask to cut off the unneeded bits from the shifted word [in]
 * @param bit By how many bits the word should be shifted for adjusted index
 *        recomputation
 * @return The adjusted value of the lookup index
 */
static NCBI_INLINE Int4 BlastNaLookupAdjustIndex(Uint1Ptr s, Int4 index, 
                      Int4 mask, Uint1 bit)
{
   return (((index)<<bit) & mask) | ((*s)>>(FULL_BYTE_SHIFT-bit));

}

/* Description in na_lookup.h */
Int4 BlastNaScanSubject_AG(const LookupTableWrapPtr lookup_wrap,
       const BLAST_SequenceBlkPtr subject, Int4 start_offset,
       Uint4Ptr q_offsets, Uint4Ptr s_offsets, Int4 max_hits,  
       Int4Ptr end_offset)
{
   LookupTablePtr lookup = (LookupTablePtr) lookup_wrap->lut;
   Uint1Ptr s;
   Uint1Ptr abs_start;
   Int4  index=0, s_off;
   
   Int4Ptr lookup_pos;
   Int4 num_hits;
   Int4 q_off;
   PV_ARRAY_TYPE *pv_array = lookup->pv;
   Int4 total_hits = 0;
   Int4 compressed_wordsize, compressed_scan_step;
   Boolean full_byte_scan = (lookup->scan_step % COMPRESSION_RATIO == 0);
   Int4 i;

   abs_start = subject->sequence;
   s = abs_start + start_offset/COMPRESSION_RATIO;
   compressed_scan_step = lookup->scan_step / COMPRESSION_RATIO;
   compressed_wordsize = lookup->reduced_wordsize;

   index = 0;
   
   /* NB: s in this function always points to the start of the word!
    */
   if (full_byte_scan) {
      Uint1Ptr s_end = abs_start + subject->length/COMPRESSION_RATIO - 
         compressed_wordsize;
      for ( ; s <= s_end; s += compressed_scan_step) {
         index = 0;
         for (i = 0; i < compressed_wordsize; ++i)
            index = ((index)<<FULL_BYTE_SHIFT) | s[i];
         
         if (NA_PV_TEST(pv_array, index, PV_ARRAY_BTS)) {
            num_hits = lookup->thick_backbone[index].num_used;
            if (num_hits == 0)
               continue;
            if (num_hits > (max_hits - total_hits))
               break;
            if ( num_hits <= HITS_ON_BACKBONE )
               /* hits live in thick_backbone */
               lookup_pos = lookup->thick_backbone[index].payload.entries;
            else
               /* hits live in overflow array */
               lookup_pos = (Int4Ptr)
                  (lookup->thick_backbone[index].payload.overflow);
            
            s_off = 
               ((s - abs_start) + compressed_wordsize)*COMPRESSION_RATIO;
            while (num_hits) {
               q_off = *((Uint4 *) lookup_pos); /* get next query offset */
               lookup_pos++;
               num_hits--;
               
               q_offsets[total_hits] = q_off;
               s_offsets[total_hits++] = s_off;
            }
         }
      }
      *end_offset = (s - abs_start)*COMPRESSION_RATIO;
   } else {
      Int4 reduced_word_length = compressed_wordsize*COMPRESSION_RATIO;
      Int4 last_offset = subject->length - reduced_word_length;
      Uint1 bit;
      Int4 adjusted_index;

      for (s_off = start_offset; s_off <= last_offset; 
           s_off += lookup->scan_step) {
         s = abs_start + (s_off / COMPRESSION_RATIO);
         bit = 2*(s_off % COMPRESSION_RATIO);
         /* Compute index for a word made of full bytes */
         index = 0;
         for (i = 0; i < compressed_wordsize; ++i)
            index = ((index)<<FULL_BYTE_SHIFT) | (*s++);
         adjusted_index = 
            (((index)<<bit) & lookup->mask) | ((*s)>>(FULL_BYTE_SHIFT-bit));
         
         if (NA_PV_TEST(pv_array, adjusted_index, PV_ARRAY_BTS)) {
            num_hits = lookup->thick_backbone[adjusted_index].num_used;
            if (num_hits == 0)
               continue;
            if (num_hits > (max_hits - total_hits))
               break;
            if ( num_hits <= HITS_ON_BACKBONE )
               /* hits live in thick_backbone */
               lookup_pos = lookup->thick_backbone[adjusted_index].payload.entries;
            else
               /* hits live in overflow array */
               lookup_pos = (Int4Ptr)
                 (lookup->thick_backbone[adjusted_index].payload.overflow);
            
            while (num_hits) {
               q_off = *((Uint4 *) lookup_pos); /* get next query offset */
               lookup_pos++;
               num_hits--;
               
               q_offsets[total_hits] = q_off;
               s_offsets[total_hits++] = s_off + reduced_word_length;
            }
         }
      }
      *end_offset = s_off;
   }

   return total_hits;
}

/* Description in na_lookup.h */
Int4 MB_AG_ScanSubject(const LookupTableWrapPtr lookup_wrap,
       const BLAST_SequenceBlkPtr subject, Int4 start_offset,
       Uint4Ptr q_offsets, Uint4Ptr s_offsets, Int4 max_hits,  
       Int4Ptr end_offset)
{
   MBLookupTablePtr mb_lt = (MBLookupTablePtr) lookup_wrap->lut;
   Uint1Ptr s;
   Uint1Ptr abs_start;
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
      Uint1Ptr s_end = abs_start + subject->length/COMPRESSION_RATIO - 
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

/* Description in na_lookup.h */
Int4 BlastNaScanSubject(const LookupTableWrapPtr lookup_wrap,
       const BLAST_SequenceBlkPtr subject, Int4 start_offset,
       Uint4Ptr q_offsets, Uint4Ptr s_offsets, Int4 max_hits, 
       Int4Ptr end_offset)
{
   Uint1Ptr s;
   Uint1Ptr abs_start, s_end;
   Int4  index=0, s_off;
   LookupTablePtr lookup = (LookupTablePtr) lookup_wrap->lut;
   Int4Ptr lookup_pos;
   Int4 num_hits;
   Int4 q_off;
   PV_ARRAY_TYPE *pv_array = lookup->pv;
   Int4 total_hits = 0;
   Boolean full_byte_scan = (lookup->scan_step % COMPRESSION_RATIO == 0);
   Int4 i;

   abs_start = subject->sequence;
   s = abs_start + start_offset/COMPRESSION_RATIO;
   /* s_end points to the place right after the last full sequence byte */ 
   s_end = abs_start + (*end_offset)/COMPRESSION_RATIO;
   index = 0;
   
   /* Compute the first index */
   for (i = 0; i < lookup->reduced_wordsize; ++i)
      index = ((index)<<FULL_BYTE_SHIFT) | *s++;

   if (full_byte_scan) {
      /* s points to the byte right after the end of the current word */
      while (s <= s_end) {
         if (NA_PV_TEST(pv_array, index, PV_ARRAY_BTS)) {
            num_hits = lookup->thick_backbone[index].num_used;
            if (num_hits == 0)
               continue;
            if (num_hits > (max_hits - total_hits))
               break;
            if ( num_hits <= HITS_ON_BACKBONE )
               /* hits live in thick_backbone */
               lookup_pos = lookup->thick_backbone[index].payload.entries;
            else
               /* hits live in overflow array */
               lookup_pos = 
                  (Int4Ptr)(lookup->thick_backbone[index].payload.overflow);
            
            /* Save the hits offsets */
            s_off = (s - abs_start)*COMPRESSION_RATIO;
            while (num_hits) {
               q_off = *((Uint4 *) lookup_pos); /* get next query offset */
               lookup_pos++;
               num_hits--;
               
               q_offsets[total_hits] = q_off;
               s_offsets[total_hits++] = s_off;
            }
         }

         /* Compute the next value of the index */
         index = (((index)<<FULL_BYTE_SHIFT) & lookup->mask) | (*s++);  

      }
      /* Ending offset should point to the start of the word that ends 
         at s */
      *end_offset = ((s - abs_start) - lookup->reduced_wordsize)*COMPRESSION_RATIO;
   } else {
      Int4 scan_shift = 2*lookup->scan_step;
      Uint1 bit = 2*(start_offset % COMPRESSION_RATIO);
      Int4 adjusted_index;

      /* s points to the byte right after the end of the current word */
      while (s <= s_end) {
         /* Adjust the word index by the base within a byte */
         adjusted_index = BlastNaLookupAdjustIndex(s, index, lookup->mask,
                                                   bit);
         
         if (NA_PV_TEST(pv_array, index, PV_ARRAY_BTS)) {
            num_hits = lookup->thick_backbone[index].num_used;
            if (num_hits == 0)
               continue;
            if (num_hits > (max_hits - total_hits))
               break;
            if ( num_hits <= HITS_ON_BACKBONE )
               /* hits live in thick_backbone */
               lookup_pos = lookup->thick_backbone[index].payload.entries;
            else
               /* hits live in overflow array */
               lookup_pos = 
                  (Int4Ptr)(lookup->thick_backbone[index].payload.overflow);
            
            /* Save the hits offsets */
            s_off = (s - abs_start)*COMPRESSION_RATIO + bit/2;
            while (num_hits) {
               q_off = *((Uint4 *) lookup_pos); /* get next query offset */
               lookup_pos++;
               num_hits--;
               
               q_offsets[total_hits] = q_off;
               s_offsets[total_hits++] = s_off;
            }
         }
         bit += scan_shift;
         if (bit >= FULL_BYTE_SHIFT) {
            /* Advance to the next full byte */
            bit -= FULL_BYTE_SHIFT;
            index = BlastNaLookupComputeIndex(FULL_BYTE_SHIFT, 
                                lookup->mask, s++, index);
         }
      }
      /* Ending offset should point to the start of the word that ends 
         at s */
      *end_offset = 
         ((s - abs_start) - lookup->reduced_wordsize)*COMPRESSION_RATIO
         + bit/2;
   }

   return total_hits;
}

static LookupTablePtr LookupTableDestruct(LookupTablePtr lookup)
{
   sfree(lookup->thick_backbone);
   sfree(lookup->overflow);
   sfree(lookup->pv);
   sfree(lookup);
   return NULL;
}

/* Description in na_lookup.h */
LookupTableWrapPtr BlastLookupTableDestruct(LookupTableWrapPtr lookup)
{
   if (lookup->lut_type == MB_LOOKUP_TABLE) {
      lookup->lut = (VoidPtr) 
         MBLookupTableDestruct((MBLookupTablePtr)lookup->lut);
   } else {
      lookup->lut = (VoidPtr) 
         LookupTableDestruct((LookupTablePtr)lookup->lut);
   }
   sfree(lookup);
   return NULL;
}
  
/** Compute the lookup index for a word in an uncompressed sequence, without 
 *  using any previous index information.
 * @param lookup Pointer to the traditional BLASTn lookup table structure [in]
 * @param word Pointer to the start of the word [in]
 * @param index The lookup index [out]
 */
#define BLAST2NA_MASK 0xfc
static NCBI_INLINE Int2
Na_LookupComputeIndex(LookupTablePtr lookup, Uint1Ptr word, Int4Ptr index)
{
   Int4 i;
   Int4 wordsize = lookup->reduced_wordsize*COMPRESSION_RATIO; /* i.e. 8 or 4 */

   *index = 0;
   for (i = 0; i < wordsize; ++i) {
      if ((word[i] & BLAST2NA_MASK) != 0) {
	 *index = 0;
	 return -1;
      } else {
	 *index = (((*index)<<lookup->charsize) & lookup->mask) | word[i];
      }
   }
   return 0;
}

/** Add a word information to the lookup table 
 * @param lookup Pointer to the lookup table structure [in] [out]
 * @param w Pointer to the start of a word [in]
 * @param query_offset Offset into the query sequence where this word ends [in]
 */
static Int4 BlastNaLookupAddWordHit(LookupTablePtr lookup, Uint1Ptr w,
                                    Int4 query_offset)
{
  Int4 index=0;
  Int4 chain_size = 0; /* Total number of elements in the chain */
  Int4 hits_in_chain = 0; /* Number of occupied elements in the chain, not 
                             including the zeroth and first positions */ 
  Int4Ptr chain = NULL;

  /* compute its index */
  if (Na_LookupComputeIndex(lookup, w, &index) == -1)
     /* Word contains ambiguities, skip it */
     return 0;

  assert(index < lookup->backbone_size);
      
  /* If backbone cell is null, initialize a new chain */
  if (lookup->thin_backbone[index] == NULL)
    {
      chain_size = 8;
      hits_in_chain = 0;
      chain = malloc( chain_size * sizeof(Int4) );
      chain[0] = chain_size;
      chain[1] = hits_in_chain;
      lookup->thin_backbone[index] = chain;
    }
  else
    /* Otherwise, use the existing chain */
    {
      chain = lookup->thin_backbone[index];
      chain_size = chain[0];
      hits_in_chain = chain[1];
    }
  
  /* If the chain is full, allocate more room */
  if ( (hits_in_chain + 2) == chain_size )
    {
      chain_size = chain_size * 2;
      chain = realloc(chain, chain_size * sizeof(Int4) );
      lookup->thin_backbone[index] = chain;
      chain[0] = chain_size;
    }
  
  /* Add the hit */
  chain[ chain[1] + 2 ] = query_offset;
  chain[1] += 1;

  return 0;
}

/** Fill the BLASTn lookup table.
 * @param lookup The lookup table structure [in] [out]
 * @param query The query sequence [in]
 * @param location What part of the query should be indexed? [in]
 */
Int4 BlastNaLookupIndexQuery(LookupTablePtr lookup, BLAST_SequenceBlkPtr query,
			ValNodePtr location)
{
  ValNodePtr loc;
  Int4 from, to;
  Int4 offset;
  Uint1Ptr sequence;

  for(loc=location; loc; loc=loc->next) {
     from = ((DoubleIntPtr) loc->data.ptrvalue)->i1;
     to = ((DoubleIntPtr) loc->data.ptrvalue)->i2;
     
     sequence = query->sequence + from;
     /* Make the offsets point to the ends of the words */
     from += lookup->reduced_wordsize*COMPRESSION_RATIO;
     for(offset = from; offset < to; offset++) {
	BlastNaLookupAddWordHit(lookup, sequence, offset);
	++sequence;
     }
  }

  return 0;
}

/*****************************************************************/
/*   STARTING CODE SPECIFIC TO MEGA BLAST                        */
/*****************************************************************/

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
static NCBI_INLINE Int4 ComputeDiscontiguousIndex(Uint1Ptr subject, Int4 word,
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
   hash_buf = (Uint1Ptr)&index;
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
static NCBI_INLINE Int4 ComputeDiscontiguousIndex_1b(const Uint1Ptr word_start, 
                      Int4 word, Uint1 sequence_bit, Uint1 template_type,
                      Int4 second_template_bit)
{
   Int4 index;
   Uint1Ptr subject = word_start;
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
   hash_buf = (Uint1Ptr)&index;
   CRC32(crc, hash_buf);
   index = (crc>>hash_shift) & hash_mask;
#endif
  
   return index;
}

/* Description in na_lookup.h */
Int4 MB_ScanSubject(const LookupTableWrapPtr lookup,
       const BLAST_SequenceBlkPtr subject, Int4 start_offset, 
       Uint4Ptr q_offsets, Uint4Ptr s_offsets, Int4 max_hits,
       Int4Ptr end_offset) 
{
   Uint1Ptr abs_start, s_end;
   Uint1Ptr s;
   Int4 hitsfound = 0;
   Uint4 query_offset, subject_offset;
   Int4 index;
   MBLookupTablePtr mb_lt = (MBLookupTablePtr) lookup->lut;
   Uint4Ptr q_ptr = q_offsets, s_ptr = s_offsets;
   PV_ARRAY_TYPE *pv_array = mb_lt->pv_array;
   Uint1 pv_array_bts = mb_lt->pv_array_bts;

#ifdef DEBUG_LOG
   FILE *logfp0 = FileOpen("new0.log", "a");
#endif   

   abs_start = subject->sequence;
   s = abs_start + start_offset;
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
   FileClose(logfp0);
#endif

   return hitsfound;
}

/* Description in na_lookup.h */
Int4 MB_DiscWordScanSubject(const LookupTableWrapPtr lookup, 
       const BLAST_SequenceBlkPtr subject, Int4 start_offset,
       Uint4Ptr q_offsets, Uint4Ptr s_offsets, Int4 max_hits, 
       Int4Ptr end_offset)
{
   Uint1Ptr s;
   Uint1Ptr s_start, abs_start, s_end;
   Int4 hitsfound = 0;
   Uint4 query_offset, subject_offset;
   Int4 word, index, index2=0;
   MBLookupTablePtr mb_lt = (MBLookupTablePtr) lookup->lut;
   Uint4Ptr q_ptr = q_offsets, s_ptr = s_offsets;
   Boolean full_byte_scan = mb_lt->full_byte_scan;
   Boolean two_templates = mb_lt->two_templates;
   Uint1 template_type = mb_lt->template_type;
   Uint1 second_template_type = mb_lt->second_template_type;
   PV_ARRAY_TYPE *pv_array = mb_lt->pv_array;
   Uint1 pv_array_bts = mb_lt->pv_array_bts;

#ifdef DEBUG_LOG
   FILE *logfp0 = FileOpen("new0.log", "a");
#endif   
   
   abs_start = subject->sequence;
   s_start = abs_start + start_offset;
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
   FileClose(logfp0);
#endif

   return hitsfound;
}

