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
 * Author: Ilya Dondoshansky
 *
 */

/** @file mb_lookup.h
 * Functions responsible for the creation of a lookup table
 * @todo FIXME: shouldn't file description read megablast lookup table? ; use
 * doxygen comments
 */


#ifndef MBLOOKUP__H
#define MBLOOKUP__H

#ifdef __cplusplus
extern "C" {
#endif

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_lookup.h>

/* The fraction of sites that must have at least one hit to not use 
    PV_ARRAY. */
#define PV_ARRAY_FACTOR 0.5

/* Mask to determine whether a residue is an ambiguity */
#define NUC_MASK 0xfc

/* Pack a nucleotide value into an integer index */
#define PACK_EXTRA_CODE(ecode,val,mask) {ecode = ((ecode<<2) & mask) | val;}

/* Get the 2 bit base starting from the n-th bit in a sequence byte; advance 
   the base; advance sequence when last base in a byte is retrieved */
#define GET_NEXT_PACKED_NUCL(s,n,val) { val = ((*s)>>(n)) & 0x00000003; n = (n-2)&0x07; s = s + ((n>>1)&(n>>2)&0x01); }

/* OPTIMAL templates */
  
/*   1,110,110,110,110,111 - 12 of 16 */ 
/*   1,110,010,110,110,111 - 11 of 16 */ 
#define MASK1_OPT       0x0000003f
#define MASK2_OPT       0x00000f00
#define MASK3_OPT       0x0003c000
#define MASK4_12_OPT    0x00f00000
#define MASK4_11_OPT    0x00300000
#define MASK5_OPT       0xfc000000
/* 12 of 16 */ 
#define GET_WORD_INDEX_12_16_OPT(n) (((n)&MASK1_OPT) | (((n)&MASK2_OPT)>>2) | (((n)&MASK3_OPT)>>4) | (((n)&MASK4_12_OPT)>>6) | (((n)&MASK5_OPT)>>8))
/* 11 of 16 */ 
#define GET_WORD_INDEX_11_16_OPT(n) (((n)&MASK1_OPT) | (((n)&MASK2_OPT)>>2) | (((n)&MASK3_OPT)>>4) | (((n)&MASK4_11_OPT)>>6) | (((n)&MASK5_OPT)>>10))
  
/* 111,010,110,010,110,111 - 12 of 18 */ 
/* 111,010,010,110,010,111 - 11 of 18 */ 
#define MASK1_18_OPT    0x00000003
#define MASK2_12_18_OPT 0x000000f0
#define MASK2_11_18_OPT 0x00000030
#define MASK3_11_18_OPT 0x00003c00
#define MASK3_12_18_OPT 0x00000c00
#define MASK4_11_18_OPT 0x00030000
#define MASK4_12_18_OPT 0x000f0000
#define MASK5_18_OPT    0x00c00000
#define MASK6_18_OPT    0xfc000000
/* 12 of 18 */
#define GET_WORD_INDEX_12_18_OPT(n) ((((n)&MASK1_18_OPT)<<4) | (((n)&MASK2_12_18_OPT)<<2) | ((n)&MASK3_12_18_OPT) | (((n)&MASK4_12_18_OPT)>>4) | (((n)&MASK5_18_OPT)>>6) | (((n)&MASK6_18_OPT)>>8))
/* 11 of 18 */
#define GET_WORD_INDEX_11_18_OPT(n) ((((n)&MASK1_18_OPT)<<4) | (((n)&MASK2_11_18_OPT)<<2) | (((n)&MASK3_11_18_OPT)>>2) | (((n)&MASK4_11_18_OPT)>>4) | (((n)&MASK5_18_OPT)>>8) | (((n)&MASK6_18_OPT)>>10))
#define MASK_EXTRA_OPT 0x0000000f
#define GET_EXTRA_CODE_18_OPT(s) (((*(s+1))<<2) | (*(s+2))) & MASK_EXTRA_OPT

#define GET_EXTRA_CODE_PACKED_4_18_OPT(s) ((*(s))>>4)
#define GET_EXTRA_CODE_PACKED_18_OPT(s,b,val,ecode) {GET_NEXT_PACKED_NUCL(s,b,ecode); GET_NEXT_PACKED_NUCL(s,b,val); PACK_EXTRA_CODE(ecode, val,MASK_EXTRA_OPT);}

#define GET_AMBIG_CONDITION_18_OPT(s) (((*(s+1))&NUC_MASK) | (((*(s+2))&NUC_MASK)))

/* 111,010,010,110,010,010,111 - 12 of 21 */
/* 111,010,010,100,010,010,111 - 11 of 21 */
#define MASK1_21_OPT    0x00000030
#define MASK2_12_21_OPT 0x00003c00
#define MASK2_11_21_OPT 0x00003000
#define MASK3_21_OPT    0x00030000
#define MASK4_21_OPT    0x00c00000
#define MASK5_21_OPT    0xfc000000
#define GET_WORD_INDEX_12_21_OPT(n) ((((n)&MASK1_21_OPT)<<4) | ((n)&MASK2_12_21_OPT) | (((n)&MASK3_21_OPT)>>2) | (((n)&MASK4_21_OPT)>>6) | (((n)&MASK5_21_OPT)>>8))
#define GET_WORD_INDEX_11_21_OPT(n) ((((n)&MASK1_21_OPT)<<4) | (((n)&MASK2_11_21_OPT)>>2) | (((n)&MASK3_21_OPT)>>4) | (((n)&MASK4_21_OPT)>>8) | (((n)&MASK5_21_OPT)>>10))
#define MASK_EXTRA_21_OPT 0x000000ff
#define GET_EXTRA_CODE_21_OPT(s) ((((*(s+1))<<6) | ((*(s+3))<<4) | ((*(s+4))<<2) | (*(s+5))) & MASK_EXTRA_21_OPT)
#define GET_AMBIG_CONDITION_21_OPT(s) (((*(s+1))&NUC_MASK) | ((*(s+3))&NUC_MASK) | ((*(s+4))&NUC_MASK) | ((*(s+5))&NUC_MASK))
#define GET_EXTRA_CODE_PACKED_4_21_OPT(s) ((((*(s))&0x0f)<<2)|((*(s))&0xc0)|((*(s+1))>>6))
#define GET_EXTRA_CODE_PACKED_21_OPT(s,b,val,ecode) {GET_NEXT_PACKED_NUCL(s,b,ecode); GET_NEXT_PACKED_NUCL(s,b,val); GET_NEXT_PACKED_NUCL(s,b,val); PACK_EXTRA_CODE(ecode,val,MASK_EXTRA_21_OPT); GET_NEXT_PACKED_NUCL(s,b,val); PACK_EXTRA_CODE(ecode,val,MASK_EXTRA_21_OPT); GET_NEXT_PACKED_NUCL(s,b,val); PACK_EXTRA_CODE(ecode,val,MASK_EXTRA_21_OPT);}

/* CODING TEMPLATES */

/*    111,110,110,110,110,1 - 12 of 16 */
/*    110,110,110,110,110,1 - 11 of 16 */
#define MASK1    0x00000003
#define MASK2    0x000000f0
#define MASK3    0x00003c00
#define MASK4    0x000f0000
#define MASK5_12 0xffc00000
#define MASK5_11 0x03c00000
#define MASK6    0xf0000000
/* 12 of 16 */
#define GET_WORD_INDEX_12_16(n) (((n)&MASK1) | (((n)&MASK2)>>2) | (((n)&MASK3)>>4) | (((n)&MASK4)>>6) | (((n)&MASK5_12)>>8))
/* 11 of 16 */
#define GET_WORD_INDEX_11_16(n) (((n)&MASK1) | (((n)&MASK2)>>2) | (((n)&MASK3)>>4) | (((n)&MASK4)>>6) | (((n)&MASK5_11)>>8) | (((n)&MASK6)>>10))

/* 10,110,110,110,110,110,1 - 12 of 18 */
/* 10,110,110,010,110,110,1 - 11 of 18 */
#define MASK1_18    0x0000000f
#define MASK2_18    0x000003c0
#define MASK3_11_18 0x00003000
#define MASK3_12_18 0x0000f000
#define MASK4_18    0x003c0000
#define MASK5_18    0x0f000000
#define MASK6_18    0xc0000000

/* 12 of 18 */
#define GET_WORD_INDEX_12_18(n) ((((n)&MASK1_18)<<2) | ((n)&MASK2_18) | (((n)&MASK3_12_18)>>2) | (((n)&MASK4_18)>>4) | (((n)&MASK5_18)>>6) | (((n)&MASK6_18)>>8))
/* 11 of 18 */
#define GET_WORD_INDEX_11_18(n) ((((n)&MASK1_18)<<2) | ((n)&MASK2_18) | (((n)&MASK3_11_18)>>2) | (((n)&MASK4_18)>>6) | (((n)&MASK5_18)>>8) | (((n)&MASK6_18)>>10))

#define MASK_EXTRA_18 0x00000003
#define GET_EXTRA_CODE_18(s) ((*(s+2)) & MASK_EXTRA_18)
#define GET_EXTRA_CODE_PACKED_4_18(s) (((*(s))>>4) & MASK_EXTRA_18)
#define GET_EXTRA_CODE_PACKED_18(s,b,val,ecode) {GET_NEXT_PACKED_NUCL(s,b,val); GET_NEXT_PACKED_NUCL(s,b,ecode);}
#define GET_AMBIG_CONDITION_18(s) ((*(s+2))&NUC_MASK)

/* 10,010,110,110,110,010,110,1 - 12 of 21 */
/* 10,010,110,010,110,010,110,1 - 11 of 21 */
#define MASK1_21    0x00000003
#define MASK2_21    0x000003c0
#define MASK3_12_21 0x0000f000
#define MASK3_11_21 0x00003000
#define MASK4_21    0x003c0000
#define MASK5_21    0x03000000
#define MASK6_21    0xc0000000
#define GET_WORD_INDEX_12_21(n) ((((n)&MASK1_21)<<6) | (((n)&MASK2_21)<<2) | ((n)&MASK3_12_21) | (((n)&MASK4_21)>>2) | (((n)&MASK5_21)>>4) | (((n)&MASK6_21)>>8))
#define GET_WORD_INDEX_11_21(n) ((((n)&MASK1_21)<<6) | (((n)&MASK2_21)<<2) | ((n)&MASK3_11_21) | (((n)&MASK4_21)>>4) | (((n)&MASK5_21)>>6) | (((n)&MASK6_21)>>10))
#define MASK_EXTRA_21 0x0000003f
#define GET_EXTRA_CODE_21(s) ((((*(s+2))<<4) | ((*(s+3))<<2) | (*(s+5))) & MASK_EXTRA_21)
#define GET_AMBIG_CONDITION_21(s) (((*(s+2))&NUC_MASK) | ((*(s+3))&NUC_MASK) | ((*(s+5))&NUC_MASK))
#define GET_EXTRA_CODE_PACKED_4_21(s) (((*(s))&0x3c)|((*(s+1))>>6))
#define GET_EXTRA_CODE_PACKED_21(s,b,val,ecode) {GET_NEXT_PACKED_NUCL(s,b,val); GET_NEXT_PACKED_NUCL(s,b,ecode); GET_NEXT_PACKED_NUCL(s,b,val); PACK_EXTRA_CODE(ecode,val,MASK_EXTRA_21); GET_NEXT_PACKED_NUCL(s,b,val); GET_NEXT_PACKED_NUCL(s,b,val); PACK_EXTRA_CODE(ecode,val,MASK_EXTRA_21);}

/** The lookup table structure used for Mega BLAST, generally with width 12 */
typedef struct MBLookupTable {
   Int4 hashsize;       /**< = 2^(8*width) */ 
   Int4 mask;           /**< hashsize - 1 */
   Int2 compressed_wordsize;/**< Number of bytes in intersection between 
                               consecutive words */
   Int2 word_length;      /**< The length of the initial word without the 
                             extra part */
   Boolean discontiguous; /**< Are discontiguous words used? */
   Uint1 template_length; /**< Length of the discontiguous word template */
   Uint1 template_type; /**< Type of the discontiguous word template */
   Boolean two_templates; /**< Use two templates simultaneously */
   Uint1 second_template_type; /**< Type of the second discontiguous word 
                                  template */
   Int4 scan_step;     /**< Step size for scanning the database */
   Boolean full_byte_scan; 
   Int4* hashtable;   /**< Array of positions              */
   Int4* hashtable2;  /**< Array of positions for second template */
   Int4* next_pos;    /**< Extra positions stored here     */
   Int4* next_pos2;   /**< Extra positions for the second template */
   Int4 num_unique_pos_added; /**< Number of positions added to the l.t. */
   PV_ARRAY_TYPE *pv_array;/**< Presence vector, used for quick presence 
                              check */
   Int4 pv_array_bts; /**< The exponent of 2 by which pv_array is smaller than
                          the backbone */
   Int4 longest_chain; /**< Largest number of query positions for a given 
                          word */
} MBLookupTable;

/**
 * Create the lookup table for Mega BLAST 
 * @param query The query sequence block (if concatenated sequence, the 
 *        individual strands/sequences must be separated by a 0x0f byte)[in]
 * @param location The locations to be included in the lookup table,
 *        e.g. [0,length-1] for full sequence. NULL means no sequence. [in]
 * @param mb_lt_ptr Pointer to the lookup table to be created [out]
 * @param lookup_options Options for lookup table creation [in]
 */
Int2 MB_LookupTableNew(BLAST_SequenceBlk* query, ListNode* location,
                       MBLookupTable** mb_lt_ptr,
                       const LookupTableOptions* lookup_options);

/** 
 * Deallocate memory used by the Mega BLAST lookup table
 */
MBLookupTable* MBLookupTableDestruct(MBLookupTable* mb_lt);

/* General types of discontiguous word templates */   
typedef enum {
   MB_WORD_CODING = 0,
   MB_WORD_OPTIMAL = 1,
   MB_TWO_TEMPLATES = 2
} DiscWordType;

/* Enumeration of all discontiguous word templates; the enumerated values 
 *  encode the weight, template length and type information 
 */
typedef enum {
   TEMPL_CONTIGUOUS = 0,
   TEMPL_11_16 = 1,
   TEMPL_11_16_OPT = 2,
   TEMPL_12_16 = 3,
   TEMPL_12_16_OPT = 4,
   TEMPL_11_18 = 5,
   TEMPL_11_18_OPT = 6,
   TEMPL_12_18 = 7,
   TEMPL_12_18_OPT = 8,
   TEMPL_11_21 = 9,
   TEMPL_11_21_OPT = 10,
   TEMPL_12_21 = 11,
   TEMPL_12_21_OPT = 12
} DiscTemplateType;

/** Scan the compressed subject sequence, returning all word hits, using the
 * old MegaBLAST approach - looking up words at every byte (4 bases) of the 
 * sequence. Lookup table is presumed to have a traditional MegaBLAST 
 * structure.
 * @param lookup Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param q_offsets Array of query positions where words are found [out]
 * @param s_offsets Array of subject positions where words are found [out]
 * @param max_hits The allocated size of the above arrays - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
Int4 MB_ScanSubject(const LookupTableWrap* lookup,
       const BLAST_SequenceBlk* subject, Int4 start_offset,
       Uint4* q_offsets, Uint4* s_offsets, Int4 max_hits,
       Int4* end_offset);

/** Scan the compressed subject sequence, returning all word hits, looking up 
 * discontiguous words. Lookup table is presumed to have a traditional 
 * MegaBLAST structure containing discontiguous word positions.
 * @param lookup Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param q_offsets Array of query positions where words are found [out]
 * @param s_offsets Array of subject positions where words are found [out]
 * @param max_hits The allocated size of the above arrays - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
Int4 MB_DiscWordScanSubject(const LookupTableWrap* lookup,
       const BLAST_SequenceBlk* subject, Int4 start_offset, 
       Uint4* q_offsets, Uint4* s_offsets, Int4 max_hits,     
       Int4* end_offset);

/** Scan the compressed subject sequence, returning all word hits, using the 
 * arbitrary stride. Lookup table is presumed to have a traditional MegaBLAST 
 * structure.
 * @param lookup Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param q_offsets Array of query positions where words are found [out]
 * @param s_offsets Array of subject positions where words are found [out]
 * @param max_hits The allocated size of the above arrays - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
Int4 MB_AG_ScanSubject(const LookupTableWrap* lookup,
       const BLAST_SequenceBlk* subject, Int4 start_offset,
       Uint4* q_offsets, Uint4* s_offsets, Int4 max_hits,
       Int4* end_offset); 

#ifdef __cplusplus
}
#endif

#endif /* MBLOOKUP__H */
