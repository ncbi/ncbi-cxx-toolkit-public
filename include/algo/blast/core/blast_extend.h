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

File name: blast_extend.h

Author: Ilya Dondoshansky

Contents: Structures used for BLAST extension

******************************************************************************
 * $Revision$
 * */

#ifndef __BLAST_EXTEND__
#define __BLAST_EXTEND__

#include <blast_def.h>
#include <gapxdrop.h>
#include <mbalign.h>
#include <mb_lookup.h>
#include <na_lookup.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EXTEND_WORD_BLASTN        0x00000001
#define EXTEND_WORD_DIAG_ARRAY    0x00000002
#define EXTEND_WORD_MB_STACKS     0x00000004
#define EXTEND_WORD_AG            0x00000008
#define EXTEND_WORD_VARIABLE_SIZE 0x00000010
#define EXTEND_WORD_UNGAPPED      0x00000020

#define READDB_UNPACK_BASE_N(x, N) (((x)>>(2*(N))) & 0x03)

/** Structure to hold ungapped alignment information */
typedef struct BlastUngappedData {
   Int4 q_start; /**< Start of the ungapped alignment in query */
   Int4 s_start; /**< Start of the ungapped alignment in subject */ 
   Int4 length;  /**< Length of the ungapped alignment */
   Int4 score;   /**< Score of the ungapped alignment */
   Int2 frame;   /**< Needed for translated searches */
} BlastUngappedData, *BlastUngappedDataPtr;

/** Structure to hold the initial HSP information */
typedef struct BlastInitHSP {
   Int4 q_off; /**< Offset in query */
   Int4 s_off; /**< Offset in subject */
   BlastUngappedDataPtr ungapped_data; /**< Pointer to a structure holding
                                          ungapped alignment information */
} BlastInitHSP, *BlastInitHSPPtr;

/** Structure to hold all initial HSPs for a given subject sequence */
typedef struct BlastInitHitList {
   Int4 total; /**< Total number of hits currently saved */
   Int4 allocated; /**< Available size of the offsets array */
   BlastInitHSPPtr init_hsp_array; /**< Array of offset pairs, possibly with
                                      scores */
   Boolean do_not_reallocate; /**< Can the init_hsp_array be reallocated? */
} BlastInitHitList, *BlastInitHitListPtr;

/** Structure for keeping last hit information for a diagonal */
typedef struct DiagStruct {
   Int4 last_hit; /**< Offset of the last hit */
   Int4 diag_level; /**< To what length has this hit been extended so far? */
} DiagStruct, PNTR DiagStructPtr;

/** Structure for keeping last hit information for a diagonal on a stack */
typedef struct MbStack {
   Int4 diag; /**< This hit's actual diagonal */
   Int4 level; /**< This hit's offset in the subject sequence */
   Int4 length; /**< To what length has this hit been extended so far? */
} MbStack, PNTR MbStackPtr;

#include <ncbi.h>
#include <blastkar.h>
#include <gapxdrop.h>

typedef struct BlastSeg {
   Int2 frame;  /**< Translation frame */
   Int4 offset; /**< Start of hsp */
   Int4 length; /**< Length of hsp */
   Int4 end;    /**< End of HSP */
   Int4 offset_trim; /**< Start of trimmed hsp */
   Int4 end_trim;    /**< End of trimmed HSP */
   Int4 gapped_start;/**< Where the gapped extension started. */
} BlastSeg, PNTR BlastSegPtr;

/** BLAST_NUMBER_OF_ORDERING_METHODS tells how many methods are used
 * to "order" the HSP's.
*/
#define BLAST_NUMBER_OF_ORDERING_METHODS 2

/** The following structure is used in "link_hsps" to decide between
 * two different "gapping" models.  Here link is used to hook up
 * a chain of HSP's (this is a VoidPtr as _blast_hsp is not yet
 * defined), num is the number of links, and sum is the sum score.
 * Once the best gapping model has been found, this information is
 * transferred up to the BlastHSP.  This structure should not be
 * used outside of the function link_hsps.
*/
typedef struct BlastHSPLink {
   VoidPtr link[BLAST_NUMBER_OF_ORDERING_METHODS]; /**< Used to order the HSPs
                                           (i.e., hook-up w/o overlapping). */
   Int2 num[BLAST_NUMBER_OF_ORDERING_METHODS]; /**< number of HSP in the 
                                                  ordering. */
   Int4 sum[BLAST_NUMBER_OF_ORDERING_METHODS]; /**< Sum-Score of HSP. */
   Nlm_FloatHi xsum[BLAST_NUMBER_OF_ORDERING_METHODS]; /**< Sum-Score of HSP, 
                                     multiplied by the appropriate Lambda. */
   Int4 changed;
} BlastHSPLink, PNTR BlastHSPLinkPtr;

/** Structure holding all information about an HSP */
typedef struct BlastHSP {
   struct BlastHSP PNTR next; /**< The next HSP */
   struct BlastHSP PNTR prev; /**< The previous HSP. */
   BlastHSPLink  hsp_link;
   Boolean linked_set;        /**< Is this HSp part of a linked set? */
   Int2 ordering_method;/**< Which method (max or no max for gaps) was used? */
   Int4 num;            /**< How many HSP's make up this (sum) segment */
   BLAST_Score sumscore;/**< Sumscore of a set of "linked" HSP's. */
   Boolean start_of_chain; /**< If TRUE, this HSP starts a chain along the 
                              "link" pointer. */
   BLAST_Score score;         /**< This HSP's raw score */
   Int4 num_ident;         /**< Number of identical base pairs in this HSP */
   Nlm_FloatHi evalue;        /**< This HSP's e-value */
   BlastSeg query;            /**< Query sequence info. */
   BlastSeg subject;          /**< Subject sequence info. */
   Int2     context;          /**< Context number of query */
   GapXEditBlockPtr gap_info; /**< ALL gapped alignment is here */
   Int4 num_ref;              /**< Number of references in the linked set */
   Int4 linked_to;            /**< Where this HSP is linked to? */
} BlastHSP, PNTR BlastHSPPtr;
   
/** Structure containing parameters needed for initial word extension.
 * Only one copy of this structure is needed, regardless of how many
 * contexts there are.
*/
typedef struct BLAST_DiagTable {
   DiagStructPtr diag_array;/**< Array to hold latest hits for all diagonals */
   Int4 diag_array_length; /**< Smallest power of 2 longer than query length */
   Int4 diag_mask; /**< Used to mask off everything above
                          min_diag_length (mask = min_diag_length-1). */
   Int4 offset; /**< "offset" added to query and subject position
                   so that "diag_level" and "last_hit" don't have
                   to be zeroed out every time. */
   Int4 window; /**< The "window" size, within which two (or more)
                   hits must be found in order to be extended. */
   Boolean multiple_hits;/**< Used by BLAST_ExtendWordInit to decide whether
                            or not to prepare the structure for multiple-hit
                            type searches. If TRUE, multiple hits are not
                            neccessary, but possible. */
   Int4 actual_window; /**< The actual window used if the multiple
                          hits method was used and a hit was found. */
} BLAST_DiagTable, PNTR BLAST_DiagTablePtr;

typedef struct MB_StackTable {
   Int4 num_stacks; /**< Number of stacks to be used for storing hit offsets
                       by MegaBLAST */
   Int4Ptr stack_index; /**< Current number of elements in each stack */
   Int4Ptr stack_size;  /**< Available memory for each stack */
   MbStackPtr PNTR estack; /**< Array of stacks for most recent hits */
} MB_StackTable, PNTR MB_StackTablePtr;
   
/** Structure for keeping initial word extension information */
typedef struct BLAST_ExtendWord {
   BLAST_DiagTablePtr diag_table; /**< Diagonal array and related parameters */
   MB_StackTablePtr stack_table; /**< Stacks and related parameters */ 
} BLAST_ExtendWord, PNTR BLAST_ExtendWordPtr;

/** Initializes the word extension structure
 * @param query The query sequence [in]
 * @param word_options Options for initial word extension [in]
 * @param dblen The total length of the database [in]
 * @param dbseq_num The total number of sequences in the database [in]
 * @param ewp_ptr Pointer to the word extension structure [out]
 */
Int2 BLAST_ExtendWordInit(BLAST_SequenceBlkPtr query,
   BlastInitialWordOptionsPtr word_options,
   Int8 dblen, Int4 dbseq_num, BLAST_ExtendWordPtr PNTR ewp_ptr);

/** Allocate memory for the BlastInitHitList structure */
BlastInitHitListPtr BLAST_InitHitListNew(void);

  /** Free memory for the BlastInitList structure */
BlastInitHitListPtr BLAST_InitHitListDestruct(BlastInitHitListPtr init_hitlist);

/** Traditional Mega BLAST initial word extension
 * @param word_options Options for the initial word extension [in]
 * @param scan_step The stride for scanning the database (1 or 4) [in]
 * @param hit_length The length of the initial words found in the lookup 
 *        table [in]
 * @param word_length The length which the hit must reach before it can be 
 *        saved [in]
 * @param ewp The structure holding the information about previously found
 *        initial hits, that have not qualified for saving yet [in] [out]
 * @param s_off The word offset in the subject sequence [in]
 * @param q_off The word offset in the query sequence [in]
 * @param init_hitlist The structure where the hits are saved [in] [out]
 */
Int4
MB_ExtendInitialHit(BlastInitialWordOptionsPtr word_options, 
             Uint1 scan_step, Int4 hit_length, Int4 word_length, 
             BLAST_ExtendWordPtr ewp, Int4 s_off, Int4 q_off,
             BlastInitHitListPtr init_hitlist);

/** Finds all words for a given subject sequence, satisfying the wordsize and 
 *  discontiguous template conditions, and performs initial (exact match) 
 *  extension.
 * @param subject The subject sequence [in]
 * @param query The query sequence (needed only for the discontiguous word 
 *        case) [in]
 * @param lookup Pointer to the (wrapper) lookup table structure. Only
 *        traditional MegaBlast lookup table supported. [in]
 * @param matrix The scoring matrix [in]
 * @param word_params Parameters for the initial word extension [in]
 * @param ewp Structure needed for initial word information maintenance [in]
 * @param q_offsets pointer to previously-allocated query offset array [in]
 * @param s_offsets pointer to previously-allocated query offset array [in]
 * @param max_hits size of offset arrays [in]
 * @param init_hitlist Structure to hold all hits information. Has to be 
 *        allocated up front [out]
 */
Int4 MB_WordFinder(BLAST_SequenceBlkPtr subject,
		   BLAST_SequenceBlkPtr query, 
		   LookupTableWrapPtr lookup,
		   Int4Ptr PNTR matrix, 
		   BlastInitialWordParametersPtr word_params,
		   BLAST_ExtendWordPtr ewp,
		   Uint4Ptr q_offsets,
		   Uint4Ptr s_offsets,
		   Int4 max_hits,
		   BlastInitHitListPtr init_hitlist);

/** Perform ungapped extension of a word hit
 * @param query The query sequence [in]
 * @param subject The subject sequence [in]
 * @param matrix The scoring matrix [in]
 * @param q_off The offset of a word in query [in]
 * @param s_off The offset of a word in subject [in]
 * @param cutoff The minimal score the ungapped alignment must reach [in]
 * @param X The drop-off parameter for the ungapped extension [in]
 * @param ungapped_data The ungapped extension information [out]
 * @return TRUE if ungapped alignment score is below cutoff, indicating that 
 *         this HSP should be deleted.
 */
Boolean
BlastnWordUngappedExtend(BLAST_SequenceBlkPtr query, 
   BLAST_SequenceBlkPtr subject, Int4Ptr PNTR matrix, 
   Int4 q_off, Int4 s_off, Int4 cutoff, Int4 X, 
   BlastUngappedDataPtr PNTR ungapped_data);

/** Finds all initial hits for a given subject sequence, that satisfy the 
 *  wordsize condition, and pass the ungapped extension test.
 * @param subject The subject sequence [in]
 * @param query The query sequence (needed only for the discontiguous word 
 *        case) [in]
 * @param lookup_wrap Pointer to the (wrapper) lookup table structure. Only
 *        traditional BLASTn lookup table supported. [in]
 * @param matrix The scoring matrix [in]
 * @param word_params Parameters for the initial word extension [in]
 * @param ewp Structure needed for initial word information maintenance [in]
 * @param q_offsets pointer to previously-allocated query offset array [in]
 * @param s_offsets pointer to previously-allocated subject offset array [in]
 * @param max_hits size of offset arrays [in]
 * @param init_hitlist Structure to hold all hits information. Has to be 
 *        allocated up front [out]
 */
Int4 BlastNaWordFinder(BLAST_SequenceBlkPtr subject, 
		       BLAST_SequenceBlkPtr query,
		       LookupTableWrapPtr lookup_wrap,
		       Int4Ptr PNTR matrix,
		       BlastInitialWordParametersPtr word_params, 
		       BLAST_ExtendWordPtr ewp,
		       Uint4Ptr q_offsets,
		       Uint4Ptr s_offsets,
		       Int4 max_hits,
		       BlastInitHitListPtr init_hitlist);

/** Finds all words for a given subject sequence, satisfying the wordsize and 
 *  discontiguous template conditions, and performs initial (exact match) 
 *  extension.
 * @param subject The subject sequence [in]
 * @param query The query sequence (needed only for the discontiguous word 
 *        case) [in]
 * @param lookup_wrap Pointer to the (wrapper) lookup table structure. Only
 *        traditional BLASTn lookup table supported.[in]
 * @param matrix The scoring matrix [in]
 * @param word_params Parameters for the initial word extension [in]
 * @param ewp Structure needed for initial word information maintenance [in]
 * @param q_offsets pointer to previously-allocated query offset array [in]
 * @param s_offsets pointer to previously-allocated subject offset array [in]
 * @param max_hits size of offset arrays [in]
 * @param init_hitlist Structure to hold all hits information. Has to be 
 *        allocated up front [out]
 */
Int4 BlastNaWordFinder_AG(BLAST_SequenceBlkPtr subject, 
			  BLAST_SequenceBlkPtr query,
			  LookupTableWrapPtr lookup_wrap,
			  Int4Ptr PNTR matrix,
			  BlastInitialWordParametersPtr word_params, 
			  BLAST_ExtendWordPtr ewp,
			  Uint4Ptr q_offsets,
			  Uint4Ptr s_offsets,
			  Int4 max_hits,
			  BlastInitHitListPtr init_hitlist);

/** Save the initial hit data into the initial hit list structure.
 * @param init_hitlist the structure holding all the initial hits 
 *        information [in] [out]
 * @param q_off The query sequence offset [in]
 * @param s_off The subject sequence offset [in]
 * @param ungapped_data The information about the ungapped extension of this 
 *        hit [in]
 */
Boolean BLAST_SaveInitialHit(BlastInitHitListPtr init_hitlist, 
           Int4 q_off, Int4 s_off, BlastUngappedDataPtr ungapped_data); 

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_EXTEND__ */
