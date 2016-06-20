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

/** @file blast_lookup.h
 *  Common definitions for protein and nucleotide lookup tables
 */

#ifndef ALGO_BLAST_CORE__BLAST_LOOKUP__H
#define ALGO_BLAST_CORE__BLAST_LOOKUP__H

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/core/blast_def.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PV_ARRAY_TYPE Uint4     /**< The pv_array 'native' type. */
#define PV_ARRAY_BYTES 4        /**< number of BYTES in 'native' type. */
#define PV_ARRAY_BTS 5          /**< bits-to-shift from lookup_index to pv_array index. */
#define PV_ARRAY_MASK 31        /**< amount to mask off. */

/** Set the bit at position 'index' in the PV 
 *  array bitfield within 'lookup'
 */
#define PV_SET(lookup, index, shift)    \
    lookup[(index) >> (shift)] |= (PV_ARRAY_TYPE)1 << ((index) & PV_ARRAY_MASK)

/** Test the bit at position 'index' in the PV 
 *  array bitfield within 'lookup'
 */
#define PV_TEST(lookup, index, shift)                   \
      ( lookup[(index) >> (shift)] &                    \
        ((PV_ARRAY_TYPE)1 << ((index) & PV_ARRAY_MASK)) )

/** Add a single query offset to a generic lookup table
 *
 * @param backbone The current list of hashtable cells [in][out]
 * @param wordsize Number of letters in a word [in]
 * @param charsize Number of bits in one letter [in]
 * @param seq pointer to the beginning of the word [in]
 * @param query_offset the offset in the query where the word occurs [in]
 */
void BlastLookupAddWordHit(Int4 **backbone, Int4 wordsize,
                           Int4 charsize, Uint1* seq,
                           Int4 query_offset);

/** Add all applicable query offsets to a generic lookup table
 *
 * @param backbone The current list of hashtable cells [in][out]
 * @param word_length Number of letters in a word [in]
 * @param charsize Number of bits in one letter [in]
 * @param lut_word_length Width of the lookup table in letters
 *                      (must be <= word_length) [in]
 * @param query The query sequence [in]
 * @param locations What locations on the query sequence to index? [in]
 */
void BlastLookupIndexQueryExactMatches(Int4 **backbone,
                                       Int4 word_length,
                                       Int4 charsize,
                                       Int4 lut_word_length,
                                       BLAST_SequenceBlk * query,
                                       BlastSeqLoc * locations);

/** Given a word, compute its index value from scratch.
 *
 * @param wordsize length of the word, in residues [in]
 * @param charsize length of one residue, in bits [in]
 * @param word pointer to the beginning of the word [in]
 * @return the computed index value
 */

static NCBI_INLINE Int4 ComputeTableIndex(Int4 wordsize,
				  Int4 charsize,
				  const Uint1* word)
{
  Int4 i;
  Int4 index = 0;

  for(i = 0; i < wordsize; i++) {
    index = (index << charsize) | word[i];
  }

  return index;
}

/** Given a word, compute its index value, reusing a previously 
 *  computed index value.
 *
 * @param wordsize length of the word - 1, in residues [in]
 * @param charsize length of one residue, in bits [in]
 * @param mask value used to mask the index so that only the bottom wordsize * charsize bits remain [in]
 * @param word pointer to the beginning of the word [in]
 * @param index the current value of the index [in]
 * @return the computed index value
 */

static NCBI_INLINE Int4 ComputeTableIndexIncremental(Int4 wordsize,
					     Int4 charsize,
					     Int4 mask,
					     const Uint1* word,
                                             Int4 index)
{
  return ((index << charsize) | word[wordsize - 1]) & mask;
}


/** Thin backbone cell for nucleotide lookup table with hashed words */
typedef struct BackboneCell
{
    Uint4 word;
    Int4* offsets;
    Int4 num_offsets;
    Int4 allocated;
    struct BackboneCell* next;
} BackboneCell;


/** Hash function type for the lookup table */
typedef Uint4 (*TNaLookupHashFunction)(Uint1*, Uint4);


BackboneCell* BackboneCellFree(BackboneCell* cell);

/** Create a new cell for a given word and offset
 *@param word Nucleotide word in 2na [in]
 *@param offset Offset for the word [in]
 *@param size Size of the offset array to be allocated [in]
 */
BackboneCell* BackboneCellNew(Uint4 word, Int4 offset, Int4 size);


/** Add all applicable query offsets to a hashed lookup table
 *
 * @param backbone The current list of hashtable cells [in][out]
 * @param word_length Number of letters in a word [in]
 * @param charsize Number of bits in one letter [in]
 * @param lut_word_length Width of the lookup table in letters
 *                      (must be <= word_length) [in]
 * @param query The query sequence [in]
 * @param locations What locations on the query sequence to index? [in]
 * @param hash_func Hash function for words in 2na [in]
 * @param counts Word counts in a database, to limit lookup table by databse
 *               word frequency [in]
 */
void BlastHashLookupIndexQueryExactMatches(BackboneCell **backbone,
                                           Int4 word_length,
                                           Int4 charsize,
                                           Int4 lut_word_length,
                                           BLAST_SequenceBlk* query,
                                           BlastSeqLoc* locations,
                                           TNaLookupHashFunction hash_func,
                                           Uint4 mask,
                                           Uint1* counts);



#ifdef __cplusplus
}
#endif

#endif /* !ALGO_BLAST_CORE__BLAST_LOOKUP__H */
