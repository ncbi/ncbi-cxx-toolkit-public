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

File name: blast_gapalign.h

Author: Ilya Dondoshansky

Contents: Structures and functions prototypes used for BLAST gapped extension

******************************************************************************
 * $Revision$
 * */

#ifndef __BLAST_GAPALIGN__
#define __BLAST_GAPALIGN__

#include <blast_def.h>
#include <blast_options.h>
#include <blast_extend.h>
#include <gapxdrop.h>
#include <mbalign.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EXTEND_DYN_PROG            1
#define EXTEND_GREEDY              2
#define EXTEND_GREEDY_NO_TRACEBACK 3

#define MB_DIAG_NEAR 30
#define MB_DIAG_CLOSE 6
#define MIN_NEIGHBOR_HSP_LENGTH 100
#define MIN_NEIGHBOR_PERC_IDENTITY 96

#define MAX_DBSEQ_LEN 5000000

/** Auxiliary structure for dynamic programming gapped extension */
typedef struct BlastGapDP {
  Int4 CC, DD, FF;      /**< Values for gap opening and extensions. */
} BlastGapDP, PNTR BlastGapDPPtr;

/** Structure supporting the gapped alignment */
typedef struct BlastGapAlignStruct {
   Uint1 program;   /**< BLAST program type */
   Boolean positionBased; /**< Is this PSI-BLAST? */
   GapXDropStateArrayStructPtr state_struct; /**< Structure to keep extension 
                                                state information */
   GapXEditBlockPtr edit_block; /**< The traceback (gap) information */
   BlastGapDPPtr dyn_prog; /**< Preallocated memory for the dynamic 
                              programming extension */
   GreedyAlignMemPtr greedy_align_mem;/**< Preallocated memory for the greedy 
                                         gapped extension */
   BLAST_ScoreBlkPtr sbp; /**< Pointer to the scoring information block */
   Int4 gap_x_dropoff; /**< X-dropoff parameter to use */
   Int4 query_start, query_stop;/**< Return values: query offsets */
   Int4 subject_start, subject_stop;/**< Return values: subject offsets */
   Int4 score;   /**< Return value: alignment score */
   FloatHi percent_identity;/**< Return value: percent identity - filled only 
                               by the greedy non-affine alignment algorithm */
} BlastGapAlignStruct, PNTR BlastGapAlignStructPtr;

/** Initializes the BlastGapAlignStruct structure 
 * @param score_options Options related to scoring alignments [in]
 * @param ext_params Options and parameters related to gapped extension [in]
 * @param total_num_contexts Number of contexts of the query sequence [in]
 * @param max_dbseq_length The length of the longest database sequence [in]
 * @param query_length The length of the query sequence [in]
 * @param program The name of the BLAST program [in]
 * @param sbp The scoring information block [in]
 * @return The BlastGapAlignStruct structure
*/
BlastGapAlignStructPtr
BLAST_GapAlignStructNew(BlastScoringOptionsPtr score_options, 
			BlastExtensionParametersPtr ext_params,
			Int4 total_num_contexts, Int4 max_dbseq_length,
                        Int4 query_length, const CharPtr program,
                        BLAST_ScoreBlkPtr sbp);

/** Deallocates memory in the BlastGapAlignStruct structure */
BlastGapAlignStructPtr 
BLAST_GapAlignStructFree(BlastGapAlignStructPtr gap_align);

/** The structure to hold all HSPs for a given sequence after the gapped 
 *  alignment.
 */
typedef struct BlastHSPList {
   Int4 oid;/**< The ordinal id of the subject sequence this HSP list is for */
   BlastHSPPtr PNTR hsp_array; /**< Array of pointers to individual HSPs */
   Int4 hspcnt; /**< Number of HSPs saved */
   Int4 allocated; /**< The allocated size of the hsp_array */
   Int4 hsp_max; /**< The maximal number of HSPs saved in the hsp_array so 
                    far */
   Boolean do_not_reallocate; /**< Is reallocation of the hsp_array allowed? */
   Boolean traceback_done; /**< Has the traceback already been done on HSPs in
                              this list? */
} BlastHSPList, *BlastHSPListPtr;

/** Creates HSP list structure with a default size HSP array */
BlastHSPListPtr BlastHSPListNew(void);

/** Deallocate memory for the HSP list */
BlastHSPListPtr BlastHSPListDestruct(BlastHSPListPtr hsp_list);

Int2 BLAST_MbGetGappedScore(BLAST_SequenceBlkPtr query, 
			    BLAST_SequenceBlkPtr subject,
			    BlastGapAlignStructPtr gap_align,
			    BlastScoringOptionsPtr score_options, 
			    BlastExtensionParametersPtr ext_params,
			    BlastHitSavingOptionsPtr hit_options,
			    BlastInitHitListPtr init_hitlist,
			    BlastHSPListPtr PNTR hsp_list_ptr);

/** Performs gapped extension for all non-Mega BLAST programs, given
 * that ungapped extension has been done earlier.
 * Sorts initial HSPs by score (from ungapped extension);
 * Deletes HSPs that are included in already extended HSPs;
 * Performs gapped extension;
 * Saves HSPs into an HSP list.
 * @param query The query sequence block [in]
 * @param subject The subject sequence block [in]
 * @param gap_align The auxiliary structure for gapped alignment [in]
 * @param score_options Options related to scoring [in]
 * @param ext_params Options and parameters related to extensions [in]
 * @param hit_options Options related to saving hits [in]
 * @param init_hitlist List of initial HSPs (offset pairs with additional 
 *        information from the ungapped alignment performed earlier) [in]
 * @param hsp_list_ptr Structure containing all saved HSPs [out]
 */
Int2 LIBCALL
BLAST_GetGappedScore (BLAST_SequenceBlkPtr query, 
		      BLAST_SequenceBlkPtr subject,
		      BlastGapAlignStructPtr gap_align,
		      BlastScoringOptionsPtr score_options, 
		      BlastExtensionParametersPtr ext_params,
		      BlastHitSavingOptionsPtr hit_options,
		      BlastInitHitListPtr init_hitlist,
		      BlastHSPListPtr PNTR hsp_list_ptr);

/** Perform a gapped alignment with traceback
 * @param query The query sequence [in]
 * @param subject The subject sequence [in]
 * @param gap_align The gapped alignment structure [in] [out]
 * @param score_options Scoring parameters [in]
 * @param q_start Offset in query where to start alignment [in]
 * @param s_start Offset in subject where to start alignment [in]
 * @param subject_length Maximal allowed extension in subject [in]
 */
Int2 BLAST_GappedAlignmentWithTraceback(Uint1Ptr query, Uint1Ptr subject, 
        BlastGapAlignStructPtr gap_align, BlastScoringOptionsPtr score_options,
        Int4 q_start, Int4 s_start, Int4 query_length, Int4 subject_length);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_GAPALIGN__ */
