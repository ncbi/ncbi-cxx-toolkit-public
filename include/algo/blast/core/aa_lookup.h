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

*/

#include <blast_def.h>
#include <blast_options.h>

#ifndef AA_LOOKUP__H
#define AA_LOOKUP__H

#ifdef __cplusplus
extern "C" {
#endif

/* some defines for the pv_array, as this changes from 32-bit to 64-bit systems. */
#if defined(LONG_BIT) && LONG_BIT==64

#define PV_ARRAY_TYPE Uint8     /* The pv_array 'native' type. */
#define PV_ARRAY_BYTES 8        /* number of BYTES in 'native' type. */
#define PV_ARRAY_BTS 6          /* bits-to-shift from lookup_index to pv_array index. */
#define PV_ARRAY_MASK 63        /* amount to mask off. */

#else

#define PV_ARRAY_TYPE Uint4     /* The pv_array 'native' type. */
#define PV_ARRAY_BYTES 4        /* number of BYTES in 'native' type. */
#define PV_ARRAY_BTS 5          /* bits-to-shift from lookup_index to pv_array index. */
#define PV_ARRAY_MASK 31        /* amount to mask off. */

#endif

#define PV_SET(lookup, index) ( (lookup)->pv[(index)>>PV_ARRAY_BTS] |= 1 << ((index) & PV_ARRAY_MASK) )
#define PV_TEST(lookup, index) ( (lookup)->pv[(index)>>PV_ARRAY_BTS] & 1 << ((index) & PV_ARRAY_MASK) )

  /* structure defining one cell of the compacted lookup table */
  /* stores the number of hits and
      up to three hits if the total number of hits is <= 3
        or
      a pointer to more hits if the total number of hits is > 3
  */

#define HITS_ON_BACKBONE 3

  typedef struct LookupBackboneCell {
    Int4 num_used;       /* num valid positions */

    union {
      Int4Ptr overflow;
      Int4 entries[HITS_ON_BACKBONE];
    } payload;

  } LookupBackboneCell,* LookupBackboneCellPtr;
    
  typedef struct LookupTable {
    Int4 threshold; /* the score threshold for neighboring words */
    Int4 neighbor_matches; /* the number of neighboring words found while indexing the queries, used for informational/debugging purposes */
    Int4 exact_matches; /* the number of exact matches found while indexing the queries, used for informational/debugging purposes */
    Int4 mask; /* part of index to mask off, that is, top (wordsize*charsize) bits should be discarded. */
    Int4 word_length; /* Length in bases of the full word match required to 
			  trigger extension */
    Int4 wordsize; /* number of full bytes in a full word */
    Int4 reduced_wordsize; /* number of bytes in a word stored in the LT */
    Int4 charsize; /* number of bits for a base/residue */
    Int4 scan_step; /* number of bases between successive words */
    Int4 alphabet_size; /* number of letters in the alphabet */
    Int4 backbone_size; /* number of cells in the backbone */
    Int4 longest_chain; /* length of the longest chain on the backbone */
    Int4 ** thin_backbone; /* the "thin" backbone. for each index cell, maintain a pointer to a dynamically-allocated chain of hits. */
    LookupBackboneCell * thick_backbone; /* the "thick" backbone. after queries are indexed, compact the backbone to put at most HITS_ON_BACKBONE hits on the backbone, otherwise point to some overflow storage */
    Int4 * overflow; /* the overflow array for the compacted lookup table */
    PV_ARRAY_TYPE *pv; /* presence vector. a bit vector indicating which cells are occupied */
    Uint1Ptr neighbors; /* neighboring word array */
    Int4 neighbors_length; /* length of neighboring word array */
  } LookupTable, * LookupTablePtr;
  
  /** Create a mapping from word w to the supplied query offset
 *
 * @param lookup the lookup table [in]
 * @param w pointer to the beginning of the word [in]
 * @param query_offset the offset in the query where the word occurs [in]
 * @return Zero.
 */

Int4 BlastAaLookupAddWordHit(LookupTablePtr lookup,
                             Uint1Ptr w,
                             Int4 query_offset);

/** Convert the chained lookup table to the pv_array and thick_backbone.
 *
 * @param lookup the lookup table [in]
 * @return Zero.
 */

Int4 _BlastAaLookupFinalize(LookupTablePtr lookup);
/**
 * Scans the subject sequence from "offset" to the end of the sequence.
 * Copies at most array_size hits.
 * Returns the number of hits found.
 * If there isn't enough room to copy all the hits, return early, and update
 * "offset". 
 *
 * @param lookup_wrap the lookup table [in]
 * @param subject the subject sequence [in]
 * @param offset the offset in the subject at which to begin scanning [in/out]
 * @param query_offsets array to which hits will be copied [out]
 * @param subject_offsets array to which hits will be copied [out]
 * @param array_size length of the offset arrays [in]
 * @return The number of hits found.
 */
Int4 BlastAaScanSubject(const LookupTableWrapPtr lookup_wrap, /* in: the LUT */
                        const BLAST_SequenceBlk *subject,
                        Int4Ptr offset,
                        Uint4 * query_offsets, /* out: pointer to the array to which hits will be copied */
		        Uint4 * subject_offsets, /* out : pointer to the array where offsets will be stored */
		        Int4 array_size);

/** Create a new protein lookup table.
  * @param opt pointer to lookup table options structure [in]
  * @param lut handle to lookup table structure [in/modified]
  */
  
Int4 BlastAaLookupNew(const LookupTableOptionsPtr opt, LookupTablePtr * lut);


/** Create a new lookup table.
  * @param opt pointer to lookup table options structure [in]
  * @param lut handle to lookup table [in/modified]
  * @param is_protein boolean indicating protein or nucleotide [in]
  */
  
Int4 LookupTableNew(const LookupTableOptionsPtr opt, LookupTablePtr * lut, 
		    Boolean is_protein);

/** Index an array of queries.
 *
 * @param lookup the lookup table [in/modified]
 * @param matrix the substitution matrix [in]
 * @param query the array of queries to index
 * @param unmasked_regions an array of ListNodePtrs, each of which points to a (list of) integer pair(s) which specify the unmasked region(s) of the query [in]
 * @param num_queries the number of queries [in]
 * @return Zero.
 */
Int4 BlastAaLookupIndexQueries(LookupTablePtr lookup,
			       Int4 ** matrix,
			       BLAST_SequenceBlkPtr query,
			       ListNodePtr unmasked_regions,
			       Int4 num_queries);

/** Index a single query.
 *
 * @param lookup the lookup table [in/modified]
 * @param matrix the substitution matrix [in]
 * @param query the array of queries to index
 * @param unmasked_regions a ListNodePtr which points to a (list of) integer pair(s) which specify the unmasked region(s) of the query [in]
i
 * @return Zero.
 */

Int4 _BlastAaLookupIndexQuery(LookupTablePtr lookup,
			      Int4 ** matrix,
			      BLAST_SequenceBlkPtr query,
			      ListNodePtr unmasked_regions);

/** Create a sequence containing all possible words as subsequences.
 *
 * @param lookup the lookup table [in]
 * @return Zero.
 */

Int4 MakeAllWordSequence(LookupTablePtr lookup);

/**
 * Find the words in the neighborhood of w, that is, those whose
 * score is greater than t.
 *
 * For typical searches against a database, a sequence containing
 * all possible words (as created by MakeAllWordSequence() is used.
 *
 * For blast-two-sequences type applications, it is not necessary to
 * find all neighboring words; it is sufficient to use the words
 * occurring in the subject sequence.
 *
 * @param lookup the lookup table [in/modified]
 * @param matrix the substitution matrix [in]
 * @param query the query sequence [in]
 * @param offset the offset of the word
 * @return Zero.
 */

Int4 AddNeighboringWords(LookupTablePtr lookup,
			 Int4 ** matrix,
			 BLAST_SequenceBlkPtr query,
			 Int4 offset);

#define SET_HIGH_BIT(x) (x |= 0x80000000)
#define CLEAR_HIGH_BIT(x) (x &= 0x7FFFFFFF)
#define TEST_HIGH_BIT(x) ( ((x) >> 31) & 1 )

#ifdef __cplusplus
}
#endif

#endif /* AA_LOOKUP__H */
