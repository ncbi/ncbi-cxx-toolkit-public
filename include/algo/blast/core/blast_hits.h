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

File name: blast_hits.h

Author: Ilya Dondoshansky

Contents: Structures used for saving BLAST hits

******************************************************************************
 * $Revision$
 * */
#ifndef __BLAST_HITS__
#define __BLAST_HITS__

#ifdef __cplusplus
extern "C" {
#endif

#include <blast_seqsrc.h>
#include <blast_gapalign.h>

/** The structure to contain all BLAST results for one query sequence */
typedef struct BlastHitList {
   Int4 hsplist_count; /**< Filled size of the HSP lists array */
   Int4 hsplist_max; /**< Maximal allowed size of the HSP lists array */
   double worst_evalue; /**< Highest of the best e-values among the HSP 
                            lists */
   Boolean heapified; /**< Is this hit list already heapified? */
   BlastHSPList** hsplist_array; /**< Array of HSP lists for individual
                                          database hits */
} BlastHitList;

/** The structure to contain all BLAST results, for multiple queries */
typedef struct BlastResults {
   Int4 num_queries; /**< Number of query sequences */
   BlastHitList** hitlist_array; /**< Array of results for individual
                                          query sequences */
} BlastResults;

/** BLAST_SaveHitlist
 *  Save the current hit list to appropriate places in the results structure
 * @param program The type of BLAST search [in]
 * @param query The query sequence [in]
 * @param subject The subject sequence [in]
 * @param results The structure holding results for all queries [in] [out]
 * @param hsp_list The results for the current subject sequence; in case of 
 *                 multiple queries, offsets are still in the concatenated 
 *                 sequence coordinates [in]
 * @param hit_parameters The options/parameters related to saving hits [in]
 * @param query_info A whole BlastQueryInfo is needed for concatenation
 *                    information of nucleotide queries [in]
 * @param sbp The Karlin-Altschul statistical information block [in]
 * @param score_options Options related to scoring [in]
 * @param rdfp Needed to extract the whole subject sequence with
 *             ambiguities [in]
 * @param thr_info Information shared between multiple search threads [in]
 */
Int2 BLAST_SaveHitlist(Uint1 program, BLAST_SequenceBlk* query, 
        BLAST_SequenceBlk* subject, BlastResults* results, 
        BlastHSPList* hsp_list, BlastHitSavingParameters* hit_parameters, 
        BlastQueryInfo* query_info, BLAST_ScoreBlk* sbp, 
        const BlastScoringOptions* score_options, const BlastSeqSrc* bssp,
        BlastThrInfo* thr_info);

/** Initialize the results structure.
 * @param num_queries Number of query sequences to allocate results structure
 *                    for [in]
 * @param results_ptr The allocated structure [out]
 */
Int2 BLAST_ResultsInit(Int4 num_queries, BlastResults** results_ptr);

/** Sort each hit list in the BLAST results by best e-value */
Int2 BLAST_SortResults(BlastResults* results);

/** Calculate the expected values for all HSPs in a hit list. In case of 
 * multiple queries, the offsets are assumed to be already adjusted to 
 * individual query coordinates, and the contexts are set for each HSP.
 * @param program The integer BLAST program index [in]
 * @param query_info Auxiliary query information - needed only for effective
                     search space calculation if it is not provided [in]
 * @param hsp_list List of HSPs for one subject sequence [in] [out]
 * @param hit_options Options containing the e-value cut-off [in]
 * @param sbp Structure containing statistical information [in]
 */
Int2 BLAST_GetNonSumStatsEvalue(Uint1 program, BlastQueryInfo* query_info,
        BlastHSPList* hsp_list, BlastHitSavingOptions* hit_options, 
        BLAST_ScoreBlk* sbp);

/** Discard the HSPs above the e-value threshold from the HSP list 
 * @param hsp_list List of HSPs for one subject sequence [in] [out]
 * @param hit_options Options block containing the e-value cut-off [in]
*/
Int2 BLAST_ReapHitlistByEvalue(BlastHSPList* hsp_list, 
                               BlastHitSavingOptions* hit_options);

/** Cleans out the NULLed out HSP's from the HSP array,
 *	moving the BLAST_HSPPtr's up to fill in the gaps.
 * @param hsp_array Array of pointers to HSP structures [in]
 * @param hspcnt Size of the array [in]
 * @return Number of remaining HSPs.
*/
Int4
BlastHSPArrayPurge (BlastHSP** hsp_array, Int4 hspcnt);

/** Adjust the query offsets in a list of HSPs. The original offsets are in
 * the concatenated query coordinates. The adjusted offsets are in 
 * individual query coordinates; the context is saved for each HSP to 
 * indicate what query it comes from.
 * @param program_number Type of BLAST program [in]
 * @param hsp_list List of HSPs for one subject sequence [in] [out]
 * @param query_info Auxiliary query information [in]
 * @param is_ooframe Is out-of-frame gapping used? [in]
*/
void 
BLAST_AdjustQueryOffsets(Uint1 program_number, BlastHSPList* hsp_list, 
   BlastQueryInfo* query_info, Boolean is_ooframe);

/** Adjust subject offsets in an HSP list if only part of the subject sequence
 * was searched.
 * @param hsp_list List of HSPs from a chunk of a subject sequence [in]
 * @param offset Offset where the chunk starts [in]
 */
void AdjustOffsetsInHSPList(BlastHSPList* hsp_list, Int4 offset);

/** An auxiliary structure used for merging HSPs */
typedef struct BLASTHSPSegment {
   Int4 q_start, q_end;
   Int4 s_start, s_end;
   struct BLASTHSPSegment* next;
} BLASTHSPSegment;

/* By how much should the chunks of a subject sequence overlap if it is 
   too long and has to be split */
#define DBSEQ_CHUNK_OVERLAP 100

/** Merge an HSP list from a chunk of the subject sequence into a previously
 * computed HSP list.
 * @param hsp_list Contains HSPs from the new chunk [in]
 * @param combined_hsp_list_ptr Contains HSPs from previous chunks [in] [out]
 * @param start Offset where the current subject chunk starts [in]
 * @param merge_hsps Should the overlapping HSPs be merged into one? [in]
 * @param append Just append the new HSP list to the old - TRUE when 
 *               lists are from different frames [in]
 * @return 1 if HSP lists have been merged, 0 otherwise.
 */
Int2 MergeHSPLists(BlastHSPList* hsp_list, 
        BlastHSPList** combined_hsp_list_ptr, Int4 start,
        Boolean merge_hsps, Boolean append);

/** Deallocate memory for an HSP structure */
BlastHSP* BlastHSPFree(BlastHSP* hsp);

/** Deallocate memory for an HSP list structure */
BlastHSPList* BlastHSPListFree(BlastHSPList* hsp_list);

/** Deallocate memory for BLAST results */
BlastResults* BLAST_ResultsFree(BlastResults* results);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_HITS__ */

