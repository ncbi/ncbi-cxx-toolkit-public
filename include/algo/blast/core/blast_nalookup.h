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

/** @file blast_nalookup.h
 *  Routines for creating nucleotide BLAST lookup tables.
 */

#ifndef ALGO_BLAST_CORE__BLAST_NTLOOKUP__H
#define ALGO_BLAST_CORE__BLAST_NTLOOKUP__H

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_lookup.h>
#include <algo/blast/core/blast_options.h>

#ifdef __cplusplus
extern "C" {
#endif

/** choose the type of nucleotide lookup table to be used
 * for a blast search
 * @param lookup_options Options for lookup table creation [in]
 * @param approx_table_entries An upper bound on the number of words
 *        that must be added to the lookup table [in]
 * @param lut_width The number of nucleotides in one lookup table word [out]
 @param query_length Number of letters in the query [in]
 * @return the lookup table type chosen
 */
ELookupTableType
BlastChooseNaLookupTable(const LookupTableOptions* lookup_options,	
                         Int4 approx_table_entries, Int4 query_length,
                         Int4 *lut_width);

/*------------------------- Small lookup table --------------------------*/

/** Lookup table structure for blastn searches with small queries */
typedef struct BlastSmallNaLookupTable {
    Int4 mask;             /**< part of index to mask off, that is, top 
                                (wordsize*charsize) bits should be discarded. */
    Int4 word_length;      /**< Length in bases of the full word match 
                                required to trigger extension */
    Int4 lut_word_length;  /**< Length in bases of a word indexed by the
                                lookup table */
    Int4 scan_step;        /**< number of bases between successive words */
    Int4 backbone_size;    /**< number of cells in the backbone */
    Int4 longest_chain;    /**< length of the longest chain on the backbone */
    Int2 * final_backbone; /**< backbone structure used when scanning for 
                                 hits */
    Int2 * overflow;       /**< the overflow array for the compacted 
                                lookup table */
    Int4  overflow_size;   /**< Number of elements in the overflow array */
    void *scansub_callback;  /**< function for scanning subject sequences */
    void *extend_callback;  /**< function for extending hits */
    BlastSeqLoc* locations; /**< masked locations. */
} BlastSmallNaLookupTable;

/** Create a new small nucleotide lookup table.
 * @param query The query sequence block (if concatenated sequence, the 
 *        individual strands/sequences must be separated by a 0x0f byte)[in]
 * @param locations The locations to be included in the lookup table,
 *        e.g. [0,length-1] for full sequence. NULL means no sequence. [in]
 * @param lut Pointer to the lookup table to be created [out]
 * @param opt Options for lookup table creation [in]
 * @param lut_width The number of nucleotides in one lookup table word [in]
 * @return 0 if successful, nonzero on failure
 */
Int4 BlastSmallNaLookupTableNew(BLAST_SequenceBlk* query,
                                BlastSeqLoc* locations,
                                BlastSmallNaLookupTable * *lut,
                                const LookupTableOptions * opt,
                                Int4 lut_width);

/** Free a small nucleotide lookup table.
 *  @param lookup The lookup table structure to be freed
 *  @return NULL
 */
BlastSmallNaLookupTable* BlastSmallNaLookupTableDestruct(
                                        BlastSmallNaLookupTable* lookup);

/*----------------------- Standard lookup table -------------------------*/

#define NA_HITS_PER_CELL 3 /**< maximum number of hits in one lookup
                                table cell */

/** structure defining one cell of the compacted lookup table */
typedef struct NaLookupBackboneCell {
    Int4 num_used;             /**< number of hits stored for this cell */

    union {
      Int4 overflow_cursor;    /**< integer offset into the overflow array
                                 where the list of hits for this cell begins */
      Int4 entries[NA_HITS_PER_CELL];  /**< if the number of hits for this
                                            cell is NA_HITS_PER_CELL or less,
                                            the hits are all stored directly in
                                            the cell */
    } payload;  /**< UNION that specifies either entries stored right 
                     on the backbone if fewer than NA_HITS_PER_CELL 
                     are present or a pointer to where the hits are 
                     stored (off-backbone). */
} NaLookupBackboneCell;
    
/** The basic lookup table structure for blastn searches
 */
typedef struct BlastNaLookupTable {
    Int4 mask;             /**< part of index to mask off, that is, top 
                                (wordsize*charsize) bits should be discarded. */
    Int4 word_length;      /**< Length in bases of the full word match 
                                required to trigger extension */
    Int4 lut_word_length;  /**< Length in bases of a word indexed by the
                                lookup table */
    Int4 scan_step;        /**< number of bases between successive words */
    Int4 backbone_size;    /**< number of cells in the backbone */
    Int4 longest_chain;    /**< length of the longest chain on the backbone */
    NaLookupBackboneCell * thick_backbone; /**< the "thick" backbone. after 
                                              queries are indexed, compact the 
                                              backbone to put at most 
                                              NA_HITS_PER_CELL hits on the 
                                              backbone, otherwise point to 
                                              some overflow storage */
    Int4 * overflow;       /**< the overflow array for the compacted 
                                lookup table */
    Int4  overflow_size;   /**< Number of elements in the overflow array */
    PV_ARRAY_TYPE *pv;     /**< Presence vector bitfield; bit positions that
                                are set indicate that the corresponding thick
                                backbone cell contains hits */
    void *scansub_callback;  /**< function for scanning subject sequences */
    void *extend_callback;  /**< function for extending hits */
} BlastNaLookupTable;
  
/** Create a new nucleotide lookup table.
 * @param query The query sequence block (if concatenated sequence, the 
 *        individual strands/sequences must be separated by a 0x0f byte)[in]
 * @param locations The locations to be included in the lookup table,
 *        e.g. [0,length-1] for full sequence. NULL means no sequence. [in]
 * @param lut Pointer to the lookup table to be created [out]
 * @param opt Options for lookup table creation [in]
 * @param lut_width The number of nucleotides in one lookup table word [in]
 * @return 0 if successful, nonzero on failure
 */
Int4 BlastNaLookupTableNew(BLAST_SequenceBlk* query,
                           BlastSeqLoc* locations,
                           BlastNaLookupTable * *lut,
                           const LookupTableOptions * opt,
                           Int4 lut_width);

/** Free a nucleotide lookup table.
 *  @param lookup The lookup table structure to be freed
 *  @return NULL
 */
BlastNaLookupTable* BlastNaLookupTableDestruct(BlastNaLookupTable* lookup);

/*----------------------- Megablast lookup table -------------------------*/

/** General types of discontiguous word templates */   
typedef enum {
   eMBWordCoding = 0,
   eMBWordOptimal = 1,
   eMBWordTwoTemplates = 2
} EDiscWordType;

/** Enumeration of all discontiguous word templates; the enumerated values 
 * encode the weight, template length and type information 
 *
 * <PRE>
 *  Optimal word templates:
 * Number of 1's in a template is word size (weight); 
 * total number of 1's and 0's - template length.
 *   1,110,110,110,110,111      - 12 of 16
 *   1,110,010,110,110,111      - 11 of 16 
 * 111,010,110,010,110,111      - 12 of 18
 * 111,010,010,110,010,111      - 11 of 18
 * 111,010,010,110,010,010,111  - 12 of 21
 * 111,010,010,100,010,010,111  - 11 of 21
 *  Coding word templates:
 *    111,110,110,110,110,1     - 12 of 16
 *    110,110,110,110,110,1     - 11 of 16
 * 10,110,110,110,110,110,1     - 12 of 18
 * 10,110,110,010,110,110,1     - 11 of 18
 * 10,010,110,110,110,010,110,1 - 12 of 21
 * 10,010,110,010,110,010,110,1 - 11 of 21
 * </PRE>
 *
 * Sequence data processed by these templates is assumed to be arranged
 * from left to right
 *
 * Index values are calculated by masking the respective pieces of sequence so
 * only bits corresponding to a contiguous string of 1's in a template are 
 * left, then shifting the masked value to a correct position in the final
 * 22- or 24-bit lookup table index, which is the sum of such shifts. 
 */
typedef enum {
   eDiscTemplateContiguous = 0,
   eDiscTemplate_11_16_Coding = 1,
   eDiscTemplate_11_16_Optimal = 2,
   eDiscTemplate_12_16_Coding = 3,
   eDiscTemplate_12_16_Optimal = 4,
   eDiscTemplate_11_18_Coding = 5,
   eDiscTemplate_11_18_Optimal = 6,
   eDiscTemplate_12_18_Coding = 7,
   eDiscTemplate_12_18_Optimal = 8,
   eDiscTemplate_11_21_Coding = 9,
   eDiscTemplate_11_21_Optimal = 10,
   eDiscTemplate_12_21_Coding = 11,
   eDiscTemplate_12_21_Optimal = 12
} EDiscTemplateType;

/** The lookup table structure used for Mega BLAST */
typedef struct BlastMBLookupTable {
   Int4 word_length;      /**< number of exact letter matches that will trigger
                             an ungapped extension */
   Int4 lut_word_length;  /**< number of letters in a lookup table word */
   Int4 hashsize;       /**< = 4^(lut_word_length) */ 
   Boolean discontiguous; /**< Are discontiguous words used? */
   Int4 template_length; /**< Length of the discontiguous word template */
   EDiscTemplateType template_type; /**< Type of the discontiguous 
                                        word template */
   Boolean two_templates; /**< Use two templates simultaneously */
   EDiscTemplateType second_template_type; /**< Type of the second 
                                               discontiguous word template */
   Int4 scan_step;     /**< Step size for scanning the database */
   Int4* hashtable;   /**< Array of positions              */
   Int4* hashtable2;  /**< Array of positions for second template */
   Int4* next_pos;    /**< Extra positions stored here     */
   Int4* next_pos2;   /**< Extra positions for the second template */
   PV_ARRAY_TYPE *pv_array;/**< Presence vector, used for quick presence 
                              check */
   Int4 pv_array_bts; /**< The exponent of 2 by which pv_array is smaller than
                          the backbone */
   Int4 longest_chain; /**< Largest number of query positions for a given 
                          word */
   void *scansub_callback;  /**< function for scanning subject sequences */
   void *extend_callback;  /**< function for extending hits */

   Int4 num_unique_pos_added; /**< Number of positions added to the l.t. */
   Int4 num_words_added; /**< Number of words added to the l.t. */

} BlastMBLookupTable;

/**
 * Create the lookup table for Mega BLAST 
 * @param query The query sequence block (if concatenated sequence, the 
 *        individual strands/sequences must be separated by a 0x0f byte)[in]
 * @param location The locations to be included in the lookup table,
 *        e.g. [0,length-1] for full sequence. NULL means no sequence. [in]
 * @param mb_lt_ptr Pointer to the lookup table to be created [out]
 * @param lookup_options Options for lookup table creation [in]
 * @param approx_table_entries An upper bound on the number of words
 *        that must be added to the lookup table [in]
 * @param lut_width The number of nucleotides in one lookup table word [in]
 */
Int2 BlastMBLookupTableNew(BLAST_SequenceBlk* query, BlastSeqLoc* location,
                           BlastMBLookupTable** mb_lt_ptr,
                           const LookupTableOptions* lookup_options,
                           Int4 approx_table_entries,
                           Int4 lut_width);

/** 
 * Deallocate memory used by the Mega BLAST lookup table
 */
BlastMBLookupTable* BlastMBLookupTableDestruct(BlastMBLookupTable* mb_lt);

/*----------------------- Discontiguous Megablast -------------------------*/

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
 * @param accum The accumulator [in]
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

#ifdef __cplusplus
}
#endif

#endif /* !ALGO_BLAST_CORE__BLAST_NTLOOKUP__H */
