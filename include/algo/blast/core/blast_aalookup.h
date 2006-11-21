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

/** @file blast_aalookup.h
 *  Routines for creating protein BLAST lookup tables.
 *  Contains definitions and prototypes for the lookup 
 *  table construction phase of blastp and RPS blast.
 */

#ifndef ALGO_BLAST_CORE__BLAST_AALOOKUP__H
#define ALGO_BLAST_CORE__BLAST_AALOOKUP__H

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_lookup.h>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_rps.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AA_HITS_PER_CELL 3 /**< maximum number of hits in one lookup
                                table cell */

/** structure defining one cell of the compacted lookup table */
typedef struct AaLookupBackboneCell {
    Int4 num_used;       /**< number of hits stored for this cell */

    union {
      Int4 overflow_cursor; /**< integer offset into the overflow array
                                 where the list of hits for this cell begins */
      Int4 entries[AA_HITS_PER_CELL];  /**< if the number of hits for this
                                            cell is AA_HITS_PER_CELL or less,
                                            the hits are all stored directly in
                                            the cell */
    } payload;  /**< union that specifies either entries stored right on 
                     the backbone if fewer than AA_HITS_PER_CELL are 
                     present or a pointer to where the hits are stored 
                     (off-backbone). */
} AaLookupBackboneCell;
    
/** The basic lookup table structure for blastp searches
 */
typedef struct BlastAaLookupTable {
    Int4 threshold;        /**< the score threshold for neighboring words */
    Int4 mask;             /**< part of index to mask off, that is, top 
                                (wordsize*charsize) bits should be discarded. */
    Int4 charsize;         /**< number of bits for a base/residue */
    Int4 word_length;      /**< Length in letters of the full word match 
                                required to trigger extension */
    Int4 lut_word_length;  /**< Length in bases of a word indexed by the
                                lookup table */
    Int4 alphabet_size; /**< number of letters in the alphabet */
    Int4 backbone_size;    /**< number of cells in the backbone */
    Int4 longest_chain;    /**< length of the longest chain on the backbone */
    Int4 ** thin_backbone; /**< the "thin" backbone. for each index cell, 
                                maintain a pointer to a dynamically-allocated 
                                chain of hits. */
    AaLookupBackboneCell * thick_backbone; /**< the "thick" backbone. after 
                                              queries are indexed, compact the 
                                              backbone to put at most 
                                              AA_HITS_PER_CELL hits on the 
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

    Int4 neighbor_matches; /**< the number of neighboring words found while 
                                indexing the queries, used for informational/
                                debugging purposes */
    Int4 exact_matches;    /**< the number of exact matches found while 
                                indexing the queries, used for informational/
                                debugging purposes */
} BlastAaLookupTable;
  
/** Pack the data structures comprising a protein lookup table
 * into their final form
 *
 * @param lookup the lookup table [in]
 * @return Zero.
 */
Int4 BlastAaLookupFinalize(BlastAaLookupTable* lookup);

/** Create a new protein lookup table.
  * @param opt pointer to lookup table options structure [in]
  * @param lut handle to lookup table structure [in/modified]
  * @return 0 if successful, nonzero on failure
  */
  
Int4 BlastAaLookupTableNew(const LookupTableOptions* opt, 
                           BlastAaLookupTable* * lut);


/** Free the lookup table.
 *  @param lookup The lookup table structure to be freed
 *  @return NULL
 */
BlastAaLookupTable* BlastAaLookupTableDestruct(BlastAaLookupTable* lookup);

/** Index a protein query.
 *
 * @param lookup the lookup table [in/modified]
 * @param matrix the substitution matrix [in]
 * @param query the array of queries to index
 * @param unmasked_regions a BlastSeqLoc* which points to a (list of) 
 *                        integer pair(s) which specify the unmasked region(s) 
 *                        of the query [in]
 * @param query_bias number added to each offset put into lookup table 
 *              (only used for RPS blast database creation, otherwise 0) [in]
 */
void BlastAaLookupIndexQuery(BlastAaLookupTable* lookup,
			     Int4 ** matrix,
			     BLAST_SequenceBlk* query,
			     BlastSeqLoc* unmasked_regions,
                             Int4 query_bias);

/* RPS blast structures and functions */

#define RPS_HITS_PER_CELL 3 /**< maximum number of hits in an RPS backbone
                                 cell; this may be redundant (have the same
                                 value as AA_HITS_PER_CELL) but must be
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

/** The number of regions into which the concatenated RPS blast
    database is split via bucket sorting */
#define RPS_BUCKET_SIZE 2048
                           

/** structure used for bucket sorting offsets retrieved
    from the RPS blast lookup table. */
typedef struct RPSBucket {
    Int4 num_filled;    /**< number of offset pairs currently in bucket */
    Int4 num_alloc;     /**< max number of offset pairs bucket can hold */
    BlastOffsetPair *offset_pairs; /**< list of offset pairs */
} RPSBucket;

/** 
 * The basic lookup table structure for RPS blast searches
 */
typedef struct BlastRPSLookupTable {
    Int4 wordsize;      /**< number of full bytes in a full word */
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
    Int4 * overflow;    /**< the overflow array for the compacted 
                             lookup table */
    Int4  overflow_size;/**< Number of elements in the overflow array */
    PV_ARRAY_TYPE *pv;     /**< Presence vector bitfield; bit positions that
                                are set indicate that the corresponding thick
                                backbone cell contains hits */

    Int4 num_buckets;        /**< number of buckets used to sort offsets
                                  retrieved from the lookup table */
    RPSBucket *bucket_array; /**< list of buckets */
} BlastRPSLookupTable;
  
/** Create a new RPS blast lookup table.
  * @param rps_info pointer to structure with RPS setup information [in]
  * @param lut handle to lookup table [in/modified]
  * @return 0 if successful, nonzero on failure
  */
  
Int2 RPSLookupTableNew(const BlastRPSInfo *rps_info, 
                       BlastRPSLookupTable* * lut);

/** Free the lookup table. 
 *  @param lookup The lookup table structure to free; note that
 *          the rps_backbone and rps_seq_offsets fields are not freed
 *          by this call, since they may refer to memory-mapped arrays
 *  @return NULL
 */
BlastRPSLookupTable* RPSLookupTableDestruct(BlastRPSLookupTable* lookup);

#ifdef __cplusplus
}
#endif

#endif /* !ALGO_BLAST_CORE__BLAST_AALOOKUP__H */
