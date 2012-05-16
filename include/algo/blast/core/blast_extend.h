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
 * Ungapped extension structures that are common to nucleotide
 * and protein extension routines.
 * Also includes associative data structures used
 * to track progress of extensions on each diagonal. Protein
 * searches only use DiagTable; nucleotide searches can use 
 * either DiagTable or DiagHash.
 */

#ifndef ALGO_BLAST_CORE__BLAST_EXTEND__H
#define ALGO_BLAST_CORE__BLAST_EXTEND__H

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/core/blast_parameters.h>
#include <algo/blast/core/lookup_wrap.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Number of hash buckets in BLAST_DiagHash */
#define DIAGHASH_NUM_BUCKETS 512

/** Default hash chain length */
#define DIAGHASH_CHAIN_LENGTH 256

/** Structure for keeping last hit information for a diagonal */
typedef struct DiagStruct {
   signed int last_hit   : 31; /**< Offset of the last hit */
   unsigned int flag      : 1 ; /**< Reset the next extension? */
} DiagStruct;

/** Structure for keeping last hit information for a diagonal in a hash table, when 
 * eRight or eRightAndLeft methods are used for initial hit extension.
 */
typedef struct DiagHashCell {
   Int4 diag;            /**< This hit's diagonal */
   signed int level      : 31; /**< This hit's offset in the subject sequence */
   unsigned int hit_saved : 1;  /**< Whether or not this hit has been saved */
   Int4  hit_len;        /**< The length of last hit */
   Uint4 next;           /**< Offset of next element in the chain */
}  DiagHashCell;
  
/** Structure containing parameters needed for initial word extension.
 * Only one copy of this structure is needed, regardless of how many
 * contexts there are.
*/
typedef struct BLAST_DiagTable {
   DiagStruct* hit_level_array;/**< Array to hold latest hits and their 
                                  lengths for all diagonals */
   Uint1* hit_len_array; /**< Array to hold the lengthof the latest hit */
   Int4 diag_array_length; /**< Smallest power of 2 longer than query length */
   Int4 diag_mask; /**< Used to mask off everything above
                          min_diag_length (mask = min_diag_length-1). */
   Int4 offset; /**< "offset" added to query and subject position
                   so that "last_hit" doesn't have
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

/** Track initial word matches using hashing with chaining. Can be used in blastn. */
typedef struct BLAST_DiagHash {
   Uint4 num_buckets;   /**< Number of buckets to be used for storing hit offsets */
   Uint4 occupancy;     /**< Number of occupied elements */
   Uint4 capacity;      /**< Total number of elements */
   Uint4 *backbone;     /**< Array of offsets to heads of chains. */
   DiagHashCell *chain; /**< Array of data cells. */
   Int4 offset;         /**< "offset" added to query and subject position so that "last_hit" doesn't have to be zeroed out every time. */
   Int4 window;         /**< The "window" size, within which two (or more) hits must be found in order to be extended. */
} BLAST_DiagHash;
   
/** Structure for keeping initial word extension information */
typedef struct Blast_ExtendWord {
   BLAST_DiagTable* diag_table; /**< Diagonal array and related parameters */
   BLAST_DiagHash* hash_table; /**< Hash table and related parameters */ 
} Blast_ExtendWord;

/** Initializes the word extension structure
 * @param query_length Length of the query sequence [in]
 * @param word_params Parameters for initial word extension [in]
 * @param ewp_ptr Pointer to the word extension structure [out]
 */
NCBI_XBLAST_EXPORT
Int2 BlastExtendWordNew(Uint4 query_length,
                        const BlastInitialWordParameters* word_params,
                        Blast_ExtendWord** ewp_ptr);

/** Deallocate memory for the word extension structure */
NCBI_XBLAST_EXPORT
Blast_ExtendWord* BlastExtendWordFree(Blast_ExtendWord* ewp);

/** Update the word extension structure after scanning of each subject sequence
 * @param ewp The structure holding word extension information [in] [out]
 * @param subject_length The length of the subject sequence that has just been
 *        processed [in]
 */
NCBI_XBLAST_EXPORT
Int2 Blast_ExtendWordExit(Blast_ExtendWord * ewp, Int4 subject_length);

/****************** Ungapped Alignments ********************************/

/** Minimal size of an array of initial word hits, allocated up front. */
#define MIN_INIT_HITLIST_SIZE 100

/** Structure to hold ungapped alignment information */
typedef struct BlastUngappedData {
   Int4 q_start; /**< Start of the ungapped alignment in query */
   Int4 s_start; /**< Start of the ungapped alignment in subject */ 
   Int4 length;  /**< Length of the ungapped alignment */
   Int4 score;   /**< Score of the ungapped alignment */
} BlastUngappedData;

/** Structure to hold the initial HSP information */
typedef struct BlastInitHSP {
    BlastOffsetPair offsets; /**< Offsets in query and subject, or, in PHI
                                BLAST, start and end of pattern in subject. */
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

/** Allocate memory for the BlastInitHitList structure */
NCBI_XBLAST_EXPORT
BlastInitHitList* BLAST_InitHitListNew(void);

/** Move the contents of a BlastInitHitList structure. 
 * @param dst Destination hitlist [in][out]
 * @param src Source hitlist (gets emptied of hits) [in][out]
 */
NCBI_XBLAST_EXPORT
void BlastInitHitListMove(BlastInitHitList * dst, 
                          BlastInitHitList * src);

/** Free the ungapped data substructures and reset initial HSP count to 0 */
NCBI_XBLAST_EXPORT
void BlastInitHitListReset(BlastInitHitList* init_hitlist);

/** Free memory for the BlastInitList structure */
NCBI_XBLAST_EXPORT
BlastInitHitList* BLAST_InitHitListFree(BlastInitHitList* init_hitlist);

/** Save the initial hit data into the initial hit list structure.
 * @param init_hitlist the structure holding all the initial hits 
 *        information [in] [out]
 * @param q_off The query sequence offset [in]
 * @param s_off The subject sequence offset [in]
 * @param ungapped_data The information about the ungapped extension of this 
 *        hit [in]
 */
NCBI_XBLAST_EXPORT
Boolean BLAST_SaveInitialHit(BlastInitHitList* init_hitlist, 
           Int4 q_off, Int4 s_off, BlastUngappedData* ungapped_data); 

/** Add a new initial (ungapped) HSP to an initial hit list.
 * @param ungapped_hsps Hit list where to save a new HSP [in] [out]
 * @param q_start Starting offset in query [in]
 * @param s_start Starting offset in subject [in]
 * @param q_off Offset in query, where lookup table hit was found. [in]
 * @param s_off Offset in subject, where lookup table hit was found. [in]
 * @param len Length of the ungapped match [in]
 * @param score Score of the ungapped match [in]
 */
NCBI_XBLAST_EXPORT
void 
BlastSaveInitHsp(BlastInitHitList* ungapped_hsps, Int4 q_start, Int4 s_start, 
                 Int4 q_off, Int4 s_off, Int4 len, Int4 score);

/** Sort array of initial HSPs by score. 
 * @param init_hitlist Initial hit list structure to check. [in]
 */
NCBI_XBLAST_EXPORT
void 
Blast_InitHitListSortByScore(BlastInitHitList* init_hitlist);

/** Check if array of initial HSPs is sorted by score. 
 * @param init_hitlist Initial hit list structure to check. [in]
 * @return TRUE if sorted, FALSE otherwise.
*/
NCBI_XBLAST_EXPORT
Boolean Blast_InitHitListIsSortedByScore(BlastInitHitList* init_hitlist);


#ifdef __cplusplus
}
#endif
#endif /* !ALGO_BLAST_CORE__BLAST_EXTEND__H */
