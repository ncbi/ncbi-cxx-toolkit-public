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
 */

/** @file blast_inline.h
 * Functions from algo/blast/core that should be inlined if possible.
 */

#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_lookup.h>
#include <algo/blast/core/mb_lookup.h>

/** Forms a lookup table index for the 11-of-16 coding template in
 *  discontiguous megablast
 * @param accum accumulator containing the 2-bit bases that will
 *              be used to create the index. Bases most recently
 *              added to the accumulator are in the low-order bits
 * @return The 22-bit lookup table index
 */
static NCBI_INLINE Int4 DiscontigIndex_11_16_Coding(Uint8 accum)
{
    Uint4 lo = (Uint4)accum;
    return ((lo & 0x00000003)      ) |
           ((lo & 0x000000f0) >>  2) |
           ((lo & 0x00003c00) >>  4) |
           ((lo & 0x000f0000) >>  6) |
           ((lo & 0x03c00000) >>  8) |
           ((lo & 0xf0000000) >> 10);
}

/** Forms a lookup table index for the 11-of-16 optimal template in
 *  discontiguous megablast
 * @param accum accumulator containing the 2-bit bases that will
 *              be used to create the index. Bases most recently
 *              added to the accumulator are in the low-order bits
 * @return The 22-bit lookup table index
 */
static NCBI_INLINE Int4 DiscontigIndex_11_16_Optimal(Uint8 accum)
{
    Uint4 lo = (Uint4)accum;
    return ((lo & 0x0000003f)      ) |
           ((lo & 0x00000f00) >>  2) |
           ((lo & 0x0003c000) >>  4) |
           ((lo & 0x00300000) >>  6) |
           ((lo & 0xfc000000) >> 10);
}

/** Forms a lookup table index for the 11-of-18 coding template in
 *  discontiguous megablast
 * @param accum accumulator containing the 2-bit bases that will
 *              be used to create the index. Bases most recently
 *              added to the accumulator are in the low-order bits
 * @return The 22-bit lookup table index
 */
static NCBI_INLINE Int4 DiscontigIndex_11_18_Coding(Uint8 accum)
{
    Uint4 lo = (Uint4)accum;
    Uint4 hi = (Uint4)(accum >> 32);
    return ((lo & 0x00000003)      ) |
           ((lo & 0x000000f0) >>  2) |
           ((lo & 0x00003c00) >>  4) |
           ((lo & 0x00030000) >>  6) |
           ((lo & 0x03c00000) >> 10) |
           ((lo & 0xf0000000) >> 12) |
           ((hi & 0x0000000c) << 18);
}

/** Forms a lookup table index for the 11-of-18 optimal template in
 *  discontiguous megablast
 * @param accum accumulator containing the 2-bit bases that will
 *              be used to create the index. Bases most recently
 *              added to the accumulator are in the low-order bits
 * @return The 22-bit lookup table index
 */
static NCBI_INLINE Int4 DiscontigIndex_11_18_Optimal(Uint8 accum)
{
    Uint4 lo = (Uint4)accum;
    Uint4 hi = (Uint4)(accum >> 32);
    return ((lo & 0x0000003f)      ) |
           ((lo & 0x00000300) >>  2) |
           ((lo & 0x0003c000) >>  6) |
           ((lo & 0x00300000) >>  8) |
           ((lo & 0x0c000000) >> 12) |
           ((lo & 0xc0000000) >> 14) |
           ((hi & 0x0000000f) << 18);
}

/** Forms a lookup table index for the 11-of-21 coding template in
 *  discontiguous megablast
 * @param accum accumulator containing the 2-bit bases that will
 *              be used to create the index. Bases most recently
 *              added to the accumulator are in the low-order bits
 * @return The 22-bit lookup table index
 */
static NCBI_INLINE Int4 DiscontigIndex_11_21_Coding(Uint8 accum)
{
    Uint4 lo = (Uint4)accum;
    Uint4 hi = (Uint4)(accum >> 32);
    return ((lo & 0x00000003)      ) |
           ((lo & 0x000000f0) >>  2) |
           ((lo & 0x00000c00) >>  4) |
           ((lo & 0x000f0000) >>  8) |
           ((lo & 0x00c00000) >> 10) |
           ((lo & 0xf0000000) >> 14) |
           ((hi & 0x0000000c) << 16) |
           ((hi & 0x00000300) << 12);
}

/** Forms a lookup table index for the 11-of-21 optimal template in
 *  discontiguous megablast
 * @param accum accumulator containing the 2-bit bases that will
 *              be used to create the index. Bases most recently
 *              added to the accumulator are in the low-order bits
 * @return The 24-bit lookup table index
 */
static NCBI_INLINE Int4 DiscontigIndex_11_21_Optimal(Uint8 accum)
{
    Uint4 lo = (Uint4)accum;
    Uint4 hi = (Uint4)(accum >> 32);
    return ((lo & 0x0000003f)      ) |
           ((lo & 0x00000300) >>  2) |
           ((lo & 0x0000c000) >>  6) |
           ((lo & 0x00c00000) >> 12) |
           ((lo & 0x0c000000) >> 14) |
           ((hi & 0x00000003) << 14) |
           ((hi & 0x000003f0) << 12);
}

/** Forms a lookup table index for the 12-of-16 coding template in
 *  discontiguous megablast
 * @param accum accumulator containing the 2-bit bases that will
 *              be used to create the index. Bases most recently
 *              added to the accumulator are in the low-order bits
 * @return The 24-bit lookup table index
 */
static NCBI_INLINE Int4 DiscontigIndex_12_16_Coding(Uint8 accum)
{
    Uint4 lo = (Uint4)accum;
    return ((lo & 0x00000003)      ) |
           ((lo & 0x000000f0) >>  2) |
           ((lo & 0x00003c00) >>  4) |
           ((lo & 0x000f0000) >>  6) |
           ((lo & 0xffc00000) >>  8);
}

/** Forms a lookup table index for the 12-of-16 optimal template in
 *  discontiguous megablast
 * @param accum accumulator containing the 2-bit bases that will
 *              be used to create the index. Bases most recently
 *              added to the accumulator are in the low-order bits
 * @return The 24-bit lookup table index
 */
static NCBI_INLINE Int4 DiscontigIndex_12_16_Optimal(Uint8 accum)
{
    Uint4 lo = (Uint4)accum;
    return ((lo & 0x0000003f)     ) |
           ((lo & 0x00000f00) >> 2) |
           ((lo & 0x0003c000) >> 4) |
           ((lo & 0x00f00000) >> 6) |
           ((lo & 0xfc000000) >> 8);
}

/** Forms a lookup table index for the 12-of-18 coding template in
 *  discontiguous megablast
 * @param accum accumulator containing the 2-bit bases that will
 *              be used to create the index. Bases most recently
 *              added to the accumulator are in the low-order bits
 * @return The 24-bit lookup table index
 */
static NCBI_INLINE Int4 DiscontigIndex_12_18_Coding(Uint8 accum)
{
    Uint4 lo = (Uint4)accum;
    Uint4 hi = (Uint4)(accum >> 32);
    return ((lo & 0x00000003)      ) |
           ((lo & 0x000000f0) >>  2) |
           ((lo & 0x00003c00) >>  4) |
           ((lo & 0x000f0000) >>  6) |
           ((lo & 0x03c00000) >>  8) |
           ((lo & 0xf0000000) >> 10) |
           ((hi & 0x0000000c) << 20);
}

/** Forms a lookup table index for the 12-of-18 optimal template in
 *  discontiguous megablast
 * @param accum accumulator containing the 2-bit bases that will
 *              be used to create the index. Bases most recently
 *              added to the accumulator are in the low-order bits
 * @return The 24-bit lookup table index
 */
static NCBI_INLINE Int4 DiscontigIndex_12_18_Optimal(Uint8 accum)
{
    Uint4 lo = (Uint4)accum;
    Uint4 hi = (Uint4)(accum >> 32);
    return ((lo & 0x0000003f)      ) |
           ((lo & 0x00000f00) >>  2) |
           ((lo & 0x0000c000) >>  4) |
           ((lo & 0x00f00000) >>  8) |
           ((lo & 0x0c000000) >> 10) |
           ((lo & 0xc0000000) >> 12) |
           ((hi & 0x0000000f) << 20);
}

/** Forms a lookup table index for the 12-of-21 coding template in
 *  discontiguous megablast
 * @param accum accumulator containing the 2-bit bases that will
 *              be used to create the index. Bases most recently
 *              added to the accumulator are in the low-order bits
 * @return The 24-bit lookup table index
 */
static NCBI_INLINE Int4 DiscontigIndex_12_21_Coding(Uint8 accum)
{
    Uint4 lo = (Uint4)accum;
    Uint4 hi = (Uint4)(accum >> 32);
    return ((lo & 0x00000003)      ) |
           ((lo & 0x000000f0) >>  2) |
           ((lo & 0x00000c00) >>  4) |
           ((lo & 0x000f0000) >>  8) |
           ((lo & 0x03c00000) >> 10) |
           ((lo & 0xf0000000) >> 12) |
           ((hi & 0x0000000c) << 18) |
           ((hi & 0x00000300) << 14);
}

/** Forms a lookup table index for the 12-of-21 optimal template in
 *  discontiguous megablast
 * @param accum accumulator containing the 2-bit bases that will
 *              be used to create the index. Bases most recently
 *              added to the accumulator are in the low-order bits
 * @return The 24-bit lookup table index
 */
static NCBI_INLINE Int4 DiscontigIndex_12_21_Optimal(Uint8 accum)
{
    Uint4 lo = (Uint4)accum;
    Uint4 hi = (Uint4)(accum >> 32);
    return ((lo & 0x0000003f)      ) |
           ((lo & 0x00000300) >>  2) |
           ((lo & 0x0000c000) >>  6) |
           ((lo & 0x00f00000) >> 10) |
           ((lo & 0x0c000000) >> 12) |
           ((hi & 0x00000003) << 16) |
           ((hi & 0x000003f0) << 14);
}

/** Given an accumulator containing packed bases, compute the discontiguous
 *  word index specified by template_type. Only the low-order (2 *
 *  template_length) bits of the accumulator are used; the base most recently
 *  added to the accumulator is in the two lowest bits.
*
 * @param The accumulator [in]
 * @param template_type What type of discontiguous word template to use [in]
 * @return The lookup table index of the discontiguous word
 */
static NCBI_INLINE Int4 ComputeDiscontiguousIndex(Uint8 accum,
                                    EDiscTemplateType template_type)
{
   Int4 index;

   switch (template_type) {
   case eDiscTemplate_11_16_Coding:
      index = DiscontigIndex_11_16_Coding(accum);
      break;
   case eDiscTemplate_12_16_Coding:
      index = DiscontigIndex_12_16_Coding(accum);
      break;
   case eDiscTemplate_11_16_Optimal:
      index = DiscontigIndex_11_16_Optimal(accum);
      break;
   case eDiscTemplate_12_16_Optimal:
      index = DiscontigIndex_12_16_Optimal(accum);
      break;
   case eDiscTemplate_11_18_Coding: 
      index = DiscontigIndex_11_18_Coding(accum);
     break;
   case eDiscTemplate_12_18_Coding: 
      index = DiscontigIndex_12_18_Coding(accum);
      break;
   case eDiscTemplate_11_18_Optimal: 
      index = DiscontigIndex_11_18_Optimal(accum);
      break;
   case eDiscTemplate_12_18_Optimal:
      index = DiscontigIndex_12_18_Optimal(accum);
      break;
   case eDiscTemplate_11_21_Coding: 
      index = DiscontigIndex_11_21_Coding(accum);
      break;
   case eDiscTemplate_12_21_Coding:
      index = DiscontigIndex_12_21_Coding(accum);
      break;
   case eDiscTemplate_11_21_Optimal: 
      index = DiscontigIndex_11_21_Optimal(accum);
      break;
   case eDiscTemplate_12_21_Optimal:
      index = DiscontigIndex_12_21_Optimal(accum);
      break;
   default:
      index = 0;
      break;
   }

   return index;
}

static NCBI_INLINE void  _ComputeIndex(Int4 wordsize,
				  Int4 charsize,
				  Int4 mask,
				  const Uint1* word,
				  Int4* index);

static NCBI_INLINE void  _ComputeIndexIncremental(Int4 wordsize,
					     Int4 charsize,
					     Int4 mask,
					     const Uint1* word,
					     Int4* index);

/** Given a word, compute its index value from scratch.
 *
 * @param wordsize length of the word, in residues [in]
 * @param charsize length of one residue, in bits [in]
 * @param mask value used to mask the index so that only the bottom wordsize * charsize bits remain [in]
 * @param word pointer to the beginning of the word [in]
 * @param index the computed index value [out] 
 */

static NCBI_INLINE void  _ComputeIndex(Int4 wordsize,
				  Int4 charsize,
				  Int4 mask,
				  const Uint1* word,
				  Int4* index)
{
  Int4 i;

  *index = 0;

  for(i=0;i<wordsize;i++)
{
    *index = ((*index << charsize) | word[i]) & mask;
}

  return;
}

/** Given a word, compute its index value, reusing a previously 
 *  computed index value.
 *
 * @param wordsize length of the word - 1, in residues [in]
 * @param charsize length of one residue, in bits [in]
 * @param mask value used to mask the index so that only the bottom wordsize * charsize bits remain [in]
 * @param word pointer to the beginning of the word [in]
 * @param index the computed index value [in/out]
 */

static NCBI_INLINE void  _ComputeIndexIncremental(Int4 wordsize,
					     Int4 charsize,
					     Int4 mask,
					     const Uint1* word,
					     Int4* index)
{
  *index = ((*index << charsize) | word[wordsize - 1]) & mask;
  return;
}


/* Given a starting position of a word in a compressed nucleotide sequence, 
 * compute this word's lookup table index
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

/* Recompute the word index given its previous value and the new location 
 *  of the last byte of the word
 */
static NCBI_INLINE Int4 BlastNaLookupComputeIndex(Int4 scan_shift, Int4 mask, 
		      const Uint1* word, Int4 index)
{
   return (((index)<<scan_shift) & mask) | *(word);  
   
}

/* Given a word computed from full bytes of a compressed sequence, 
 *  shift it by 0-3 bases 
 */
static NCBI_INLINE Int4 BlastNaLookupAdjustIndex(Uint1* s, Int4 index, 
                      Int4 mask, Uint1 bit)
{
   return (((index)<<bit) & mask) | ((*s)>>(FULL_BYTE_SHIFT-bit));

}


#define BLAST2NA_MASK 0xfc

/** Compute the lookup index for a word in an uncompressed sequence, without 
 *  using any previous index information.
 * @param lookup Pointer to the traditional BLASTn lookup table structure [in]
 * @param word Pointer to the start of the word [in]
 * @param index The lookup index [out]
 */
static NCBI_INLINE Int2
Na_LookupComputeIndex(BlastLookupTable* lookup, Uint1* word, Int4* index)
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

/** Pack 4 sequence bytes into a one byte integer, assuming sequence contains
 *  no ambiguities.
 */
#define	PACK_WORD(q) ((q[0]<<6) + (q[1]<<4) + (q[2]<<2) + q[3]) 

/** Compare a given number of bytes of an compressed subject sequence with
 *  the non-compressed query sequence.
 * @param q Pointer to the first byte to be compared in the query sequence [in]
 * @param s Pointer to the first byte to be compared in the subject 
 *        sequence [in]
 * @param extra_bytes Number of compressed bytes to compare [in]
 * @return TRUE if sequences are identical, FALSE if mismatch is found.
 */
static NCBI_INLINE Boolean BlastNaCompareExtraBytes(Uint1* q, Uint1* s, 
                                               Int4 extra_bytes)
{
   Int4 index;
   
   for (index = 0; index < extra_bytes; ++index) {
      if (*s++ != PACK_WORD(q))
	 return FALSE;
      q += COMPRESSION_RATIO;
   }
   return TRUE;
}

/** Perform mini extension (up to max_left <= 4 bases) to the left; 
 * @param q Pointer to the query base right after the ones to be extended [in]
 * @param s Pointer to a byte in the compressed subject sequence that is to be 
 *        tested for extension [in]
 * @param max_left Maximal number of bits to compare [in]
 * @return Number of matched bases
*/
static NCBI_INLINE Uint1 
BlastNaMiniExtendLeft(Uint1* q, const Uint1* s, Uint1 max_left)
{
   Uint1 left = 0;

   for (left = 0; left < max_left; ++left) {
      if (NCBI2NA_UNPACK_BASE(*s, left) != *--q) {
	 break;
      }
   }
   return left;
}

/** Perform mini extension (up to max_right <= 4 bases) to the right; 
 * @param q Pointer to the start of the extension in the query [in]
 * @param s Pointer to a byte in the compressed subject sequence that is to be 
 *        tested for extension [in]
 * @param max_right Maximal number of bits to compare [in]
 * @return Number of matched bases
*/
static NCBI_INLINE Uint1 
BlastNaMiniExtendRight(Uint1* q, const Uint1* s, Uint1 max_right)
{
   Uint1 right;
   Uint1 index = 3;
   
   for (right = 0; right < max_right; ++right, --index) {
      if (NCBI2NA_UNPACK_BASE(*s, index) != *q++) {
	 break;
      }
   }
   return right;
}

/** Returns the index in the MaskLoc given a context number for the query.
 * If the query is nucleotide
 *
 * @param is_na the query is nucleotide
 * @param context offset in the QueryInfo array
 * @return index in the maskloc
 */
static NCBI_INLINE Int2 BlastGetMaskLocIndexFromContext(Boolean is_na, Int2 context)
{
     return (is_na ? context / 2 : context);
}

/** Determines whether this is a nucleotide query and whether this a minus strand or not
 *
 * @param is_na the query is nucleotide
 * @param context offset in the QueryInfo array
 * @return TRUE if this is minus strand
 */
static NCBI_INLINE Boolean BlastIsReverseStrand(Boolean is_na, Int2 context)
{
     return (is_na && ((context & 1) != 0));

}
