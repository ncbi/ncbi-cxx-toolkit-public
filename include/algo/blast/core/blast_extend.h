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
 * Author:  Ilya Dondoshansky
 *
 */

/** @file blast_extend.h
 * Structures used for BLAST extension @todo FIXME: elaborate description
 * rename to nt_ungapped.h?
 */

#ifndef __BLAST_EXTEND__
#define __BLAST_EXTEND__

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_parameters.h>
#include <algo/blast/core/lookup_wrap.h>
#include <algo/blast/core/blast_diagnostics.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Structure to hold ungapped alignment information */
typedef struct BlastUngappedData {
   Int4 q_start; /**< Start of the ungapped alignment in query */
   Int4 s_start; /**< Start of the ungapped alignment in subject */ 
   Int4 length;  /**< Length of the ungapped alignment */
   Int4 score;   /**< Score of the ungapped alignment */
   Int2 frame;   /**< Needed for translated searches */
} BlastUngappedData;

/** Structure to hold the initial HSP information */
typedef struct BlastInitHSP {
   Int4 q_off; /**< Offset in query */
   Int4 s_off; /**< Offset in subject */
   BlastUngappedData* ungapped_data; /**< Pointer to a structure holding
                                          ungapped alignment information */
} BlastInitHSP;

/** Structure to hold all initial HSPs for a given subject sequence */
typedef struct BlastInitHitList {
   Int4 total; /**< Total number of hits currently saved */
   Int4 allocated; /**< Available size of the offsets array */
   BlastInitHSP* init_hsp_array; /**< Array of offset pairs, possibly with
                                      scores */
   Boolean do_not_reallocate; /**< Can the init_hsp_array be reallocated? */
} BlastInitHitList;

/** Structure for keeping last hit information for a diagonal */
typedef struct DiagStruct {
   Int4 last_hit; /**< Offset of the last hit */
   Int4 diag_level; /**< To what length has this hit been extended so far? */
} DiagStruct;

/** Structure for keeping last hit information for a diagonal on a stack, 
 * when eDiagUpdate method is used for initial hit extension.
 */
typedef struct MB_Stack {
   Int4 diag; /**< This hit's actual diagonal */
   Int4 level; /**< This hit's offset in the subject sequence */
   Int4 length; /**< To what length has this hit been extended so far? */
} MB_Stack;

/** Structure for keeping last hit information for a diagonal on a stack, when 
 * eRight or eRightAndLeft methods are used for initial hit extension.
 */
typedef struct BlastnStack {
   Int4 diag; /**< This hit's actual diagonal */
   Int4 level; /**< This hit's offset in the subject sequence */
}  BlastnStack;
  
/** Structure containing parameters needed for initial word extension.
 * Only one copy of this structure is needed, regardless of how many
 * contexts there are.
*/
typedef struct BLAST_DiagTable {
   DiagStruct* hit_level_array;/**< Array to hold latest hits and their 
                                  lengths for all diagonals */
   Uint4* last_hit_array; /**< Array of latest hits on all diagonals, with 
                             top byte indicating whether hit has been 
                             saved. */
   Int4 diag_array_length; /**< Smallest power of 2 longer than query length */
   Int4 diag_mask; /**< Used to mask off everything above
                          min_diag_length (mask = min_diag_length-1). */
   Int4 offset; /**< "offset" added to query and subject position
                   so that "diag_level" and "last_hit" don't have
                   to be zeroed out every time. */
   Int4 window; /**< The "window" size, within which two (or more)
                   hits must be found in order to be extended. */
   Boolean multiple_hits;/**< Used by BlastExtendWordNew to decide whether
                            or not to prepare the structure for multiple-hit
                            type searches. If TRUE, multiple hits are not
                            neccessary, but possible. */
   Int4 actual_window; /**< The actual window used if the multiple
                          hits method was used and a hit was found. */
} BLAST_DiagTable;

/** Structure containing an array of stacks for keeping track of the 
 * initial word matches. Can be used in megablast. */
typedef struct BLAST_StackTable {
   Int4 num_stacks; /**< Number of stacks to be used for storing hit offsets
                       by MegaBLAST */
   Int4* stack_index; /**< Current number of elements in each stack */
   Int4* stack_size;  /**< Available memory for each stack */
   MB_Stack** mb_stack_array; /**< Array of stacks for most recent hits with
                                 lengths. */
   BlastnStack** bn_stack_array; /**< Array of stacks for most recent hits 
                                    without lengths. */
} BLAST_StackTable;
   
/** Structure for keeping initial word extension information */
typedef struct Blast_ExtendWord {
   BLAST_DiagTable* diag_table; /**< Diagonal array and related parameters */
   BLAST_StackTable* stack_table; /**< Stacks and related parameters */ 
} Blast_ExtendWord;

/** Initializes the word extension structure
 * @param lookup_wrap Pointer to lookup table. Allows to distinguish different 
 *                    cases. [in]
 * @param query_length Length of the query sequence [in]
 * @param word_params Parameters for initial word extension [in]
 * @param subject_length Average length of a subject sequence, used to 
 *                       calculate average search space. [in]
 * @param ewp_ptr Pointer to the word extension structure [out]
 */
Int2 BlastExtendWordNew(const LookupTableWrap* lookup_wrap, Uint4 query_length,
                        const BlastInitialWordParameters* word_params,
                        Uint4 subject_length, Blast_ExtendWord** ewp_ptr);

/** Allocate memory for the BlastInitHitList structure */
BlastInitHitList* BLAST_InitHitListNew(void);

/** Free the ungapped data substructures and reset initial HSP count to 0 */
void BlastInitHitListReset(BlastInitHitList* init_hitlist);

/** Free memory for the BlastInitList structure */
BlastInitHitList* BLAST_InitHitListFree(BlastInitHitList* init_hitlist);

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
 * @param ungapped_stats Various hit counts. Not filled if NULL [out]
 */
Int2 MB_WordFinder(BLAST_SequenceBlk* subject,
		   BLAST_SequenceBlk* query, 
		   LookupTableWrap* lookup,
		   Int4** matrix, 
		   const BlastInitialWordParameters* word_params,
		   Blast_ExtendWord* ewp,
		   Uint4* q_offsets,
		   Uint4* s_offsets,
		   Int4 max_hits,
		   BlastInitHitList* init_hitlist, 
         BlastUngappedStats* ungapped_stats);

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
 * @param ungapped_stats Various hit counts. Not filled if NULL [out]
 */
Int2 BlastNaWordFinder(BLAST_SequenceBlk* subject, 
		       BLAST_SequenceBlk* query,
		       LookupTableWrap* lookup_wrap,
		       Int4** matrix,
		       const BlastInitialWordParameters* word_params, 
		       Blast_ExtendWord* ewp,
		       Uint4* q_offsets,
		       Uint4* s_offsets,
		       Int4 max_hits,
		       BlastInitHitList* init_hitlist, 
             BlastUngappedStats* ungapped_stats);

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
 * @param ungapped_stats Various hit counts. Not filled if NULL [out]
 */
Int2 BlastNaWordFinder_AG(BLAST_SequenceBlk* subject, 
			  BLAST_SequenceBlk* query,
			  LookupTableWrap* lookup_wrap,
			  Int4** matrix,
			  const BlastInitialWordParameters* word_params, 
			  Blast_ExtendWord* ewp,
			  Uint4* q_offsets,
			  Uint4* s_offsets,
			  Int4 max_hits,
			  BlastInitHitList* init_hitlist, 
           BlastUngappedStats* ungapped_stats);

/** Save the initial hit data into the initial hit list structure.
 * @param init_hitlist the structure holding all the initial hits 
 *        information [in] [out]
 * @param q_off The query sequence offset [in]
 * @param s_off The subject sequence offset [in]
 * @param ungapped_data The information about the ungapped extension of this 
 *        hit [in]
 */
Boolean BLAST_SaveInitialHit(BlastInitHitList* init_hitlist, 
           Int4 q_off, Int4 s_off, BlastUngappedData* ungapped_data); 

/** Deallocate memory for the word extension structure */
Blast_ExtendWord* BlastExtendWordFree(Blast_ExtendWord* ewp);

/** Add a new initial (ungapped) HSP to an initial hit list.
 * @param ungapped_hsps Hit list where to save a new HSP [in] [out]
 * @param q_start Starting offset in query [in]
 * @param s_start Starting offset in subject [in]
 * @param q_off Offset in query, where lookup table hit was found. [in]
 * @param s_off Offset in subject, where lookup table hit was found. [in]
 * @param len Length of the ungapped match [in]
 * @param score Score of the ungapped match [in]
 */
void 
BlastSaveInitHsp(BlastInitHitList* ungapped_hsps, Int4 q_start, Int4 s_start, 
                 Int4 q_off, Int4 s_off, Int4 len, Int4 score);

/** Sort array of initial HSPs by score. 
 * @param init_hitlist Initial hit list structure to check. [in]
 */
void 
Blast_InitHitListSortByScore(BlastInitHitList* init_hitlist);

/** Check if array of initial HSPs is sorted by score. 
 * @param init_hitlist Initial hit list structure to check. [in]
 * @return TRUE if sorted, FALSE otherwise.
*/
Boolean Blast_InitHitListIsSortedByScore(BlastInitHitList* init_hitlist);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_EXTEND__ */
