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

/** @file blast_lookup.h
 *  Contains definitions and prototypes for the lookup table
 *  construction and scanning phase of blastn, blastp, RPS blast
 * @todo: Re-examine code here and in unit tests for 32/64 bit portability.
 */

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_rps.h>
#include <algo/blast/core/lookup_wrap.h>

#ifndef BLAST_LOOKUP__H
#define BLAST_LOOKUP__H

#ifdef __cplusplus
extern "C" {
#endif

/* some defines for the pv_array, as this changes from 32-bit to 64-bit systems. */

#if 0
/*The 64 bit versions are disabled for now.*/

#if defined(LONG_BIT) && LONG_BIT==64

#define PV_ARRAY_TYPE Uint8     /**< The pv_array 'native' type. */
#define PV_ARRAY_BYTES 8        /**< number of BYTES in 'native' type. */
#define PV_ARRAY_BTS 6          /**< bits-to-shift from lookup_index to pv_array index. */
#define PV_ARRAY_MASK 63        /**< amount to mask off. */

#else
#endif

#define PV_ARRAY_TYPE Uint4     /**< The pv_array 'native' type. */
#define PV_ARRAY_BYTES 4        /**< number of BYTES in 'native' type. */
#define PV_ARRAY_BTS 5          /**< bits-to-shift from lookup_index to pv_array index. */
#define PV_ARRAY_MASK 31        /**< amount to mask off. */

/*#endif*/

/** Set the bit at position 'index' in the PV 
 *  array bitfield within 'lookup'
 */
#define PV_SET(lookup, index) ( (lookup)->pv[(index)>>PV_ARRAY_BTS] |= (PV_ARRAY_TYPE)1 << ((index) & PV_ARRAY_MASK) )
/** Test the bit at position 'index' in the PV 
 *  array bitfield within 'lookup'
 */
#define PV_TEST(lookup, index) ( (lookup)->pv[(index)>>PV_ARRAY_BTS] & (PV_ARRAY_TYPE)1 << ((index) & PV_ARRAY_MASK) )

#define FULL_BYTE_SHIFT 8 /**< Number of bits to shift in lookup 
                               index calculation when scanning 
                               compressed nucleotide sequence */

#define HITS_ON_BACKBONE 3 /**< maximum number of hits in one lookup
                                table cell */

/** structure defining one cell of the compacted lookup table */
typedef struct LookupBackboneCell {
    Int4 num_used;       /**< number of hits stored for this cell */

    union {
      Int4 overflow_cursor; /**< integer offset into the overflow array
                                 where the list of hits for this cell begins */
      Int4 entries[HITS_ON_BACKBONE];  /**< if the number of hits for this
                                            cell is HITS_ON_BACKBONE or less,
                                            the hits are all stored directly in
                                            the cell */
    } payload;

} LookupBackboneCell;
    
/** The basic lookup table structure for blastn
 *  and blastp searches
 */
typedef struct BlastLookupTable {
    Int4 threshold;        /**< the score threshold for neighboring words */
    Int4 neighbor_matches; /**< the number of neighboring words found while 
                                indexing the queries, used for informational/
                                debugging purposes */
    Int4 exact_matches;    /**< the number of exact matches found while 
                                indexing the queries, used for informational/
                                debugging purposes */
    Int4 mask;             /**< part of index to mask off, that is, top 
                                (wordsize*charsize) bits should be discarded. */
    Int4 word_length;      /**< Length in bases of the full word match 
                                required to trigger extension */
    Int4 wordsize;         /**< number of full bytes in a full word */
    Int4 reduced_wordsize; /**< number of bytes in a word stored in the LUT */
    Int4 charsize;         /**< number of bits for a base/residue */
    Int4 scan_step;        /**< number of bases between successive words */
    Int4 alphabet_size;    /**< number of letters in the alphabet */
    Int4 backbone_size;    /**< number of cells in the backbone */
    Int4 longest_chain;    /**< length of the longest chain on the backbone */
    Int4 ** thin_backbone; /**< the "thin" backbone. for each index cell, 
                                maintain a pointer to a dynamically-allocated 
                                chain of hits. */
    LookupBackboneCell * thick_backbone; /**< the "thick" backbone. after 
                                              queries are indexed, compact the 
                                              backbone to put at most 
                                              HITS_ON_BACKBONE hits on the 
                                              backbone, otherwise point to 
                                              some overflow storage */
    Int4 * overflow;       /**< the overflow array for the compacted 
                                lookup table */
    Int4  overflow_size;   /**< Number of elements in the overflow array */
    PV_ARRAY_TYPE *pv;     /**< Presence vector bitfield; bit positions that
                                are set indicate that the corresponding thick
                                backbone cell contains hits */
    Boolean use_pssm;      /**< if TRUE, lookup table construction will assume
                                that the underlying score matrix is position-
                                specific */
    Boolean variable_wordsize; /**< if TRUE then only full bytes are compared as initial words. */
    Boolean ag_scanning_mode;  /**< Using AG scanning mode (or stride) if TRUE, so that 
                               not every base is checked.  */
  } BlastLookupTable;
  
  /** Create a mapping from word w to the supplied query offset
 *
 * @param lookup the lookup table [in]
 * @param w pointer to the beginning of the word [in]
 * @param query_offset the offset in the query where the word occurs [in]
 * @return Zero.
 */

Int4 BlastAaLookupAddWordHit(BlastLookupTable* lookup,
                             Uint1* w,
                             Int4 query_offset);

/** Convert the chained lookup table to the pv_array and thick_backbone.
 *
 * @param lookup the lookup table [in]
 * @return Zero.
 */

Int4 _BlastAaLookupFinalize(BlastLookupTable* lookup);
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
Int4 BlastAaScanSubject(const LookupTableWrap* lookup_wrap,
                        const BLAST_SequenceBlk *subject,
                        Int4* offset,
                        Uint4 * NCBI_RESTRICT query_offsets,
                        Uint4 * NCBI_RESTRICT subject_offsets,
                        Int4 array_size);

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
 * @param table_offsets array to which hits will be copied [out]
 * @param sequence_offsets array to which hits will be copied [out]
 * @param array_size length of the offset arrays [in]
 * @return The number of hits found.
 */
Int4 BlastRPSScanSubject(const LookupTableWrap* lookup_wrap,
                        const BLAST_SequenceBlk *sequence,
                        Int4* offset,
		        Uint4 * table_offsets,
                        Uint4 * sequence_offsets,
		        Int4 array_size);

/** Create a new protein lookup table.
  * @param opt pointer to lookup table options structure [in]
  * @param lut handle to lookup table structure [in/modified]
  * @return 0 if successful, nonzero on failure
  */
  
Int4 BlastAaLookupNew(const LookupTableOptions* opt, BlastLookupTable* * lut);


/** Create a new lookup table.
  * @param opt pointer to lookup table options structure [in]
  * @param lut handle to lookup table [in/modified]
  * @param is_protein boolean indicating protein or nucleotide [in]
  * @return 0 if successful, nonzero on failure
  */
  
Int4 LookupTableNew(const LookupTableOptions* opt, BlastLookupTable* * lut, 
		    Boolean is_protein);

/** Free the lookup table.
 *  @param lookup The lookup table structure to be frees
 *  @return NULL
 */
BlastLookupTable* LookupTableDestruct(BlastLookupTable* lookup);

/** Index a protein query.
 *
 * @param lookup the lookup table [in/modified]
 * @param matrix the substitution matrix [in]
 * @param query the array of queries to index
 * @param unmasked_regions an array of BlastSeqLoc*s, each of which points to a (list of) integer pair(s) which specify the unmasked region(s) of the query [in]
 * @return Zero.
 */
Int4 BlastAaLookupIndexQuery(BlastLookupTable* lookup,
			       Int4 ** matrix,
			       BLAST_SequenceBlk* query,
			       BlastSeqLoc* unmasked_regions);

/** Index a single query.
 *
 * @param lookup the lookup table [in/modified]
 * @param matrix the substitution matrix [in]
 * @param query the array of queries to index
 * @param unmasked_regions a BlastSeqLoc* which points to a (list of) integer pair(s) which specify the unmasked region(s) of the query [in]
 * @param query_bias number added to each offset put into lookup table (only used for RPS blast database creation, otherwise 0) [in]
 * @return Zero.
 */

Int4 _BlastAaLookupIndexQuery(BlastLookupTable* lookup,
			      Int4 ** matrix,
			      BLAST_SequenceBlk* query,
			      BlastSeqLoc* unmasked_regions,
                              Int4 query_bias);

/**
 * Index a query sequence; i.e. fill a lookup table with the offsets
 * of query words score exceeds a threshold.
 *
 * @param lookup the lookup table [in/modified]
 * @param matrix the substitution matrix [in]
 * @param query the query sequence [in]
 * @param query_bias number added to each offset put into lookup table
 *                      (ordinarily 0; a nonzero value allows a succession of
 *                      query sequences to update the same lookup table)
 * @param locations the list of ranges of query offsets to examine for indexing [in]
 * @return Zero.
 */
Int4 AddNeighboringWords(BlastLookupTable* lookup,
			 Int4 ** matrix,
			 BLAST_SequenceBlk* query,
                         Int4 query_bias,
                         BlastSeqLoc* locations);

/**
 * A position-specific version of AddNeighboringWords. Note that
 * only the score matrix matters for indexing purposes, so an
 * actual query sequence is unneccessary
 *
 * @param lookup the lookup table [in/modified]
 * @param matrix the substitution matrix [in]
 * @param query_bias number added to each offset put into lookup table
 *                      (ordinarily 0; a nonzero value allows a succession of
 *                      query sequences to update the same lookup table)
 * @param locations the list of ranges of query offsets to examine for indexing
 * @return Zero.
 */
Int4 AddPSSMNeighboringWords(BlastLookupTable* lookup,
			 Int4 ** matrix,
                         Int4 query_bias,
                         BlastSeqLoc* locations);

/* RPS blast structures and functions */

#define RPS_HITS_PER_CELL 3 /**< maximum number of hits in an RPS backbone
                                 cell; this may be redundant (have the same
                                 value as HITS_ON_BACKBONE) but must be
                                 separate to guarantee binary compatibility
                                 with existing RPS blast databases */

/** structure defining one cell of the RPS lookup table */
typedef struct RPSBackboneCell {
    Int4 num_used;                   /**< number of hits in this cell */
    Int4 entries[RPS_HITS_PER_CELL]; /**< if the number of hits in this cell
                                          is RPS_HITS_PER_CELL or less, all
                                          hits go into this array. Otherwise,
                                          the first hit in the list goes into
                                          element 0 of the array, and element 1
                                          contains the byte offset into the 
                                          overflow array where the list of the
                                          remaining hits begins */
} RPSBackboneCell;

/** 
 * The basic lookup table structure for RPS blast searches
 */
typedef struct BlastRPSLookupTable {
    Int4 wordsize;      /**< number of full bytes in a full word */
    Int4 longest_chain; /**< length of the longest chain on the backbone */
    Int4 mask;          /**< part of index to mask off, that is, 
                             top (wordsize*charsize) bits should be 
                             discarded. */
    Int4 alphabet_size; /**< number of letters in the alphabet */
    Int4 charsize;      /**< number of bits for a base/residue */
    Int4 backbone_size; /**< number of cells in the backbone */
    RPSBackboneCell * rps_backbone; /**< the lookup table used for RPS blast */
    Int4 ** rps_pssm;   /**< Pointer to memory-mapped RPS Blast profile file */
    Int4 * rps_seq_offsets; /**< array of start offsets for each RPS DB seq. */
    Int4 num_profiles; /**< Number of profiles in RPS database. */
    BlastRPSAuxInfo* rps_aux_info; /**< RPS Blast auxiliary information */
    Int4 * overflow;    /**< the overflow array for the compacted 
                             lookup table */
    Int4  overflow_size;/**< Number of elements in the overflow array */
    PV_ARRAY_TYPE *pv;     /**< Presence vector bitfield; bit positions that
                                are set indicate that the corresponding thick
                                backbone cell contains hits */
} BlastRPSLookupTable;
  
/** Create a new RPS blast lookup table.
  * @param rps_info pointer to structure with RPS setup information [in]
  * @param lut handle to lookup table [in/modified]
  * @return 0 if successful, nonzero on failure
  */
  
Int4 RPSLookupTableNew(const BlastRPSInfo *rps_info, 
                       BlastRPSLookupTable* * lut);

/** Free the lookup table. 
 *  @param lookup The lookup table structure to free; note that
 *          the rps_backbone and rps_seq_offsets fields are not freed
 *          by this call, since they may refer to memory-mapped arrays
 *  @return NULL
 */
BlastRPSLookupTable* RPSLookupTableDestruct(BlastRPSLookupTable* lookup);

/********************* Nucleotide functions *******************/

/** Macro to test the presence vector array value for a lookup table index */
#define NA_PV_TEST(pv_array, index, pv_array_bts) (pv_array[(index)>>pv_array_bts]&(((PV_ARRAY_TYPE) 1)<<((index)&PV_ARRAY_MASK)))

/** Scan the compressed subject sequence, returning all word hits, using the 
 *  old BLASTn approach - looking up words at every byte (4 bases) of the 
 *  sequence. Lookup table is presumed to have a traditional BLASTn structure.
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param q_offsets Array of query positions where words are found [out]
 * @param s_offsets Array of subject positions where words are found [out]
 * @param max_hits The allocated size of the above arrays - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
Int4 BlastNaScanSubject(const LookupTableWrap* lookup_wrap,
                        const BLAST_SequenceBlk* subject,
                        Int4 start_offset,
                        Uint4* NCBI_RESTRICT q_offsets,
                        Uint4* NCBI_RESTRICT s_offsets,
                        Int4 max_hits, 
                        Int4* end_offset);

/** Scan the compressed subject sequence, returning all word hits, using the 
 *  arbitrary stride. Lookup table is presumed to have a traditional BLASTn 
 *  structure. 
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param q_offsets Array of query positions where words are found [out]
 * @param s_offsets Array of subject positions where words are found [out]
 * @param max_hits The allocated size of the above arrays - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
 * @return The number of hits found from the lookup table
*/
Int4 BlastNaScanSubject_AG(const LookupTableWrap* lookup_wrap,
                        const BLAST_SequenceBlk* subject,
                        Int4 start_offset,
                        Uint4* NCBI_RESTRICT q_offsets,
                        Uint4* NCBI_RESTRICT s_offsets,
                        Int4 max_hits, 
                        Int4* end_offset);

/** Fill the lookup table for a given query sequence or partial sequence.
 * @param lookup Pointer to the lookup table structure [in] [out]
 * @param query The query sequence [in]
 * @param location What locations on the query sequence to index? [in]
 * @return Always 0
 */
Int4 BlastNaLookupIndexQuery(BlastLookupTable* lookup, 
                             BLAST_SequenceBlk* query,
                             BlastSeqLoc* location);

#ifdef __cplusplus
}
#endif

#endif /* BLAST_LOOKUP__H */
