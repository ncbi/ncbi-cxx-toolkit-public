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

#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/gapinfo.h>
#include <algo/blast/core/blast_seqsrc.h>

/* One sequence segment within an HSP */
typedef struct BlastSeg {
   Int2 frame;  /**< Translation frame */
   Int4 offset; /**< Start of hsp */
   Int4 length; /**< Length of hsp */
   Int4 end;    /**< End of HSP */
   Int4 gapped_start;/**< Where the gapped extension started. */
} BlastSeg;

/** Structure holding all information about an HSP */
typedef struct BlastHSP {
   Int4 score;         /**< This HSP's raw score */
   Int4 num_ident;         /**< Number of identical base pairs in this HSP */
   double evalue;        /**< This HSP's e-value */
   BlastSeg query;            /**< Query sequence info. */
   BlastSeg subject;          /**< Subject sequence info. */
   Int4     context;          /**< Context number of query */
   GapEditBlock* gap_info; /**< ALL gapped alignment is here */
   Int2 splice_junction; /**< Splice junction count in a linked set of
                            HSPs. Each present splice junction counts as +1, 
                            absent as -1. */
   Int4 num;            /**< How many HSP's make up this (sum) segment? */
   Uint4 pattern_length; /**< Length of pattern occurrence in this HSP, in 
                            PHI BLAST */
} BlastHSP;

/** The structure to hold all HSPs for a given sequence after the gapped 
 *  alignment.
 */
typedef struct BlastHSPList {
   Int4 oid;/**< The ordinal id of the subject sequence this HSP list is for */
   BlastHSP** hsp_array; /**< Array of pointers to individual HSPs */
   Int4 hspcnt; /**< Number of HSPs saved */
   Int4 allocated; /**< The allocated size of the hsp_array */
   Int4 hsp_max; /**< The maximal number of HSPs allowed to be saved */
   Boolean do_not_reallocate; /**< Is reallocation of the hsp_array allowed? */
   Boolean traceback_done; /**< Has the traceback already been done on HSPs in
                              this list? */
} BlastHSPList;

/** Creates HSP list structure with a default size HSP array */
BlastHSPList* BlastHSPListNew(void);

/** The structure to contain all BLAST results for one query sequence */
typedef struct BlastHitList {
   Int4 hsplist_count; /**< Filled size of the HSP lists array */
   Int4 hsplist_max; /**< Maximal allowed size of the HSP lists array */
   double worst_evalue; /**< Highest of the best e-values among the HSP 
                           lists */
   Int4 low_score; /**< The lowest of the best scores among the HSP lists */
   Boolean heapified; /**< Is this hit list already heapified? */
   BlastHSPList** hsplist_array; /**< Array of HSP lists for individual
                                          database hits */
} BlastHitList;

/** The structure to contain all BLAST results, for multiple queries */
typedef struct BlastHSPResults {
   Int4 num_queries; /**< Number of query sequences */
   BlastHitList** hitlist_array; /**< Array of results for individual
                                          query sequences */
} BlastHSPResults;

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
 * @param bssp Needed to extract the whole subject sequence with
 *             ambiguities [in]
 */
Int2 BLAST_SaveHitlist(Uint1 program, BLAST_SequenceBlk* query, 
        BLAST_SequenceBlk* subject, BlastHSPResults* results, 
        BlastHSPList* hsp_list, BlastHitSavingParameters* hit_parameters, 
        BlastQueryInfo* query_info, BlastScoreBlk* sbp, 
        const BlastScoringOptions* score_options, const BlastSeqSrc* bssp);

/** Initialize the results structure.
 * @param num_queries Number of query sequences to allocate results structure
 *                    for [in]
 * @param results_ptr The allocated structure [out]
 */
Int2 BLAST_ResultsInit(Int4 num_queries, BlastHSPResults** results_ptr);

/** Sort each hit list in the BLAST results by best e-value */
Int2 BLAST_SortResults(BlastHSPResults* results);

/** Calculate the expected values for all HSPs in a hit list. In case of 
 * multiple queries, the offsets are assumed to be already adjusted to 
 * individual query coordinates, and the contexts are set for each HSP.
 * @param program The integer BLAST program index [in]
 * @param query_info Auxiliary query information - needed only for effective
                     search space calculation if it is not provided [in]
 * @param hsp_list List of HSPs for one subject sequence [in] [out]
 * @param gapped_calculation Is this for a gapped or ungapped search? [in]
 * @param sbp Structure containing statistical information [in]
 */
Int2 BLAST_GetNonSumStatsEvalue(Uint1 program, BlastQueryInfo* query_info,
        BlastHSPList* hsp_list, Boolean gapped_calculation, 
        BlastScoreBlk* sbp);

/** Calculate e-value for an HSP found by PHI BLAST.
 * @param score Raw score of the HSP [in]
 * @param sbp Scoring block with statistical parameters [in]
 * @return The e-value corresponding to this score.
 */
double PHIScoreToEvalue(Int4 score, BlastScoreBlk* sbp);

/** Calculate e-values for a PHI BLAST HSP list.
 * @param hsp_list HSP list found by PHI BLAST [in] [out]
 * @param sbp Scoring block with statistical parameters [in]
 */
void PHIGetEvalue(BlastHSPList* hsp_list, BlastScoreBlk* sbp);


/** Discard the HSPs above the e-value threshold from the HSP list 
 * @param hsp_list List of HSPs for one subject sequence [in] [out]
 * @param hit_options Options block containing the e-value cut-off [in]
*/
Int2 BLAST_ReapHitlistByEvalue(BlastHSPList* hsp_list, 
                               BlastHitSavingOptions* hit_options);

/** Reevaluate the HSP's score, e-value and percent identity after taking
 * into account the ambiguity information. Needed for blastn only, either
 * after a greedy gapped extension, or for ungapped search.
 * @param hsp The HSP structure [in] [out]
 * @param query_start Pointer to the start of the query sequence [in]
 * @param subject_start Pointer to the start of the subject sequence [in]
 * @param hit_options Hit saving options with e-value cut-off [in]
 * @param score_options Scoring options [in]
 * @param query_info Query information structure, containing effective search
 *                   space(s) [in]
 * @param sbp Score block with Karlin-Altschul parameters [in]
 * @return Should this HSP be deleted after the score reevaluation?
 */
Boolean ReevaluateHSPWithAmbiguities(BlastHSP* hsp, 
           Uint1* query_start, Uint1* subject_start, 
           const BlastHitSavingOptions* hit_options, 
           const BlastScoringOptions* score_options, 
           BlastQueryInfo* query_info, BlastScoreBlk* sbp);
/** Calculate number of identities in an HSP.
 * @param query The query sequence [in]
 * @param subject The uncompressed subject sequence [in]
 * @param hsp All information about the HSP [in]
 * @param is_gapped Is this a gapped search? [in]
 * @param num_ident_ptr Number of identities [out]
 * @param align_length_ptr The alignment length, including gaps [out]
 */
Int2
BlastHSPGetNumIdentities(Uint1* query, Uint1* subject, 
   BlastHSP* hsp, Boolean is_gapped, Int4* num_ident_ptr, 
   Int4* align_length_ptr);

/** Calculate number of identities in an HSP for an out-of-frame alignment.
 * @param query The query sequence [in]
 * @param subject The uncompressed subject sequence [in]
 * @param hsp All information about the HSP [in]
 * @param program BLAST program (blastx or tblastn) [in]
 * @param num_ident_ptr Number of identities [out]
 * @param align_length_ptr The alignment length, including gaps [out]
 */
Int2
BlastOOFGetNumIdentities(Uint1* query, Uint1* subject, 
   BlastHSP* hsp, Uint1 program, 
   Int4* num_ident_ptr, Int4* align_length_ptr);

/** Cleans out the NULLed out HSP's from the HSP array,
 *	moving the BLAST_HSPPtr's up to fill in the gaps.
 * @param hsp_array Array of pointers to HSP structures [in]
 * @param hspcnt Size of the array [in]
 * @return Number of remaining HSPs.
*/
Int4
BlastHSPArrayPurge (BlastHSP** hsp_array, Int4 hspcnt);

/** Assign frames in all HSPs in the HSP list.
 * @param program_number Type of BLAST program [in]
 * @param hsp_list List of HSPs for one subject sequence [in] [out]
 * @param is_ooframe Is out-of-frame gapping used? [in]
*/
void 
HSPListSetFrames(Uint1 program_number, BlastHSPList* hsp_list, 
                 Boolean is_ooframe);

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
 * @param hsp_num_max Maximal allowed number of HSPs to save (unlimited if 0) [in]
 * @param start Offset where the current subject chunk starts [in]
 * @param merge_hsps Should the overlapping HSPs be merged into one? [in]
 * @return 0 if HSP lists have been merged successfully, -1 otherwise.
 */
Int2 MergeHSPLists(BlastHSPList* hsp_list, 
                   BlastHSPList** combined_hsp_list_ptr, 
                   Int4 hsp_num_max, Int4 start, Boolean merge_hsps);

/** Append one HSP list to the other. Discard lower scoring HSPs if there is
 * not enough space to keep all.
 * @param hsp_list New list of HSPs [in]
 * @param combined_hsp_list_ptr Pointer to the combined list of HSPs, possibly
 *                              containing previously saved HSPs [in] [out]
 * @param hsp_num_max Maximal allowed number of HSPs to save (unlimited if 0) [in]
 * @return Status: 0 on success, -1 on failure.
 */ 
Int2 AppendHSPList(BlastHSPList* hsp_list, BlastHSPList** combined_hsp_list_ptr, 
                   Int4 hsp_num_max);

/** Deallocate memory for an HSP structure */
BlastHSP* BlastHSPFree(BlastHSP* hsp);

/** Deallocate memory for an HSP list structure */
BlastHSPList* BlastHSPListFree(BlastHSPList* hsp_list);

/** Deallocate memory for BLAST results */
BlastHSPResults* BLAST_ResultsFree(BlastHSPResults* results);

/** Convert a prelimiary list of HSPs, that are the result of
  * an RPS blast search, to a format compatible with the rest
  * of the blast engine. RPS blast takes a single query and the
  * concatenation of all DB sequences; the DB sequences are the
  * query input to the engine, and the single query is treated as
  * a single subject sequence.
  *
  *   BEFORE THIS CALL      AFTER THIS CALL
  *  - many HSP lists       - one HSP list
  *  - each list has the    - each HSP has its own
  *    same subject OID       subject OID
  *  - each list has a      - the list has one context
  *    different context
  *
  * @param init_results The input result list [in]
  * @param results The modified results (in new format) [out]
  */

void RPSUpdateResults(BlastHSPResults *results,
                      BlastHSPResults *init_results);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_HITS__ */

