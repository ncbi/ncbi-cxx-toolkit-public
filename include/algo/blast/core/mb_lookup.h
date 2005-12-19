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
 *
 * Author: Ilya Dondoshansky
 *
 */

/** @file mb_lookup.h
 * Declarations of functions and structures for 
 * creating and scanning megablast lookup tables
 */


#ifndef MBLOOKUP__H
#define MBLOOKUP__H

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_lookup.h>

#ifdef __cplusplus
extern "C" {
#endif

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

/** Mask to determine whether a residue is an ambiguity. */
#define NUC_MASK 0xfc

/** The lookup table structure used for Mega BLAST, generally with width 12 */
typedef struct BlastMBLookupTable {
   Int4 word_length;      /**< number of exact letter matches that will trigger
                             an ungapped extension */
   Int4 lut_word_length;  /**< number of letters in a lookup table word */
   Int4 hashsize;       /**< = 4^(lut_word_length) */ 
   Boolean discontiguous; /**< Are discontiguous words used? */
   Uint1 template_length; /**< Length of the discontiguous word template */
   EDiscTemplateType template_type; /**< Type of the discontiguous 
                                        word template */
   Boolean two_templates; /**< Use two templates simultaneously */
   EDiscTemplateType second_template_type; /**< Type of the second 
                                               discontiguous word template */
   Int4 scan_step;     /**< Step size for scanning the database */
   Boolean full_byte_scan; /**< In discontiguous case: is scanning done by full
                              bytes or at each sequence base (2 bits)? */
   Int4* hashtable;   /**< Array of positions              */
   Int4* hashtable2;  /**< Array of positions for second template */
   Int4* next_pos;    /**< Extra positions stored here     */
   Int4* next_pos2;   /**< Extra positions for the second template */
   Int4 num_unique_pos_added; /**< Number of positions added to the l.t. */
   Int4 num_words_added; /**< Number of words added to the l.t. */
   PV_ARRAY_TYPE *pv_array;/**< Presence vector, used for quick presence 
                              check */
   Int4 pv_array_bts; /**< The exponent of 2 by which pv_array is smaller than
                          the backbone */
   Int4 longest_chain; /**< Largest number of query positions for a given 
                          word */
   Boolean variable_wordsize; /**< if TRUE then only full bytes are compared as initial words. */
   Boolean ag_scanning_mode;  /**< Using AG scanning mode (or stride) if TRUE, so that 
                               not every base is checked.  */
} BlastMBLookupTable;

/**
 * Create the lookup table for Mega BLAST 
 * @param query The query sequence block (if concatenated sequence, the 
 *        individual strands/sequences must be separated by a 0x0f byte)[in]
 * @param location The locations to be included in the lookup table,
 *        e.g. [0,length-1] for full sequence. NULL means no sequence. [in]
 * @param mb_lt_ptr Pointer to the lookup table to be created [out]
 * @param lookup_options Options for lookup table creation [in]
 * @param approx_table_entries An estimate of the number of words
 *        that must be added to the lookup table [in]
 */
Int2 MB_LookupTableNew(BLAST_SequenceBlk* query, BlastSeqLoc* location,
                       BlastMBLookupTable** mb_lt_ptr,
                       const LookupTableOptions* lookup_options,
                       Int4 approx_table_entries);

/** 
 * Deallocate memory used by the Mega BLAST lookup table
 */
BlastMBLookupTable* MBLookupTableDestruct(BlastMBLookupTable* mb_lt);

/** Scan the compressed subject sequence, returning all word hits, looking up 
 * discontiguous words. Lookup table is presumed to have a traditional 
 * MegaBLAST structure containing discontiguous word positions.
 * @param lookup Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                     found [out]
 * @param max_hits The allocated size of the above arrays - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
Int4 MB_DiscWordScanSubject(const LookupTableWrap* lookup,
       const BLAST_SequenceBlk* subject, Int4 start_offset, 
       BlastOffsetPair* NCBI_RESTRICT offset_pairs, Int4 max_hits,     
       Int4* end_offset);

/** Scan the compressed subject sequence, returning all word hits, using the 
 * arbitrary stride. Lookup table is presumed to have a traditional MegaBLAST 
 * structure.
 * @param lookup Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                     found [out]
 * @param max_hits The allocated size of the above arrays - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
Int4 MB_AG_ScanSubject(const LookupTableWrap* lookup,
       const BLAST_SequenceBlk* subject, Int4 start_offset,
       BlastOffsetPair* NCBI_RESTRICT offset_pairs, Int4 max_hits,
       Int4* end_offset); 

#ifdef __cplusplus
}
#endif

#endif /* MBLOOKUP__H */
