/*  $Id$
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
* Author:  Christiam Camacho
*
*/

/// @file blast_seqalign.hpp
/// Utility function to convert internal BLAST result structures into
/// objects::CSeq_align_set objects.

#ifndef ALGO_BLAST_API___BLAST_SEQALIGN__HPP
#define ALGO_BLAST_API___BLAST_SEQALIGN__HPP

#include <corelib/ncbistd.hpp>

#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/api/blast_seqinfosrc_aux.hpp>
#include <objects/seqalign/Seq_align.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

/// Forward declaration
class ILocalQueryData; 

/// Remaps Seq-align offsets relative to the query Seq-loc. 
/// Since the query strands were already taken into account when CSeq_align 
/// was created, only start position shifts in the CSeq_loc's are relevant in 
/// this function. 
/// @param sar Seq-align for a given query [in] [out]
/// @param query The query Seq-loc [in]
void
RemapToQueryLoc(CRef<CSeq_align> sar, const CSeq_loc & query);

/// Constructs an empty Seq-align-set containing an empty discontinuous
/// seq-align, and appends it to a previously constructed Seq-align-set.
/// @param sas Pointer to a Seq-align-set, to which new object should be 
///            appended (if not NULL).
/// @return Resulting Seq-align-set. 
CSeq_align_set*
CreateEmptySeq_align_set(CSeq_align_set* sas);

void
BLASTHspListToSeqAlign(EBlastProgramType program, 
                       BlastHSPList* hsp_list, 
                       CRef<CSeq_id> query_id, 
                       CRef<CSeq_id> subject_id,
                       Int4 query_length, 
                       Int4 subject_length,
                       bool is_ooframe,
                       const vector<int> & gi_list,
                       vector<CRef<CSeq_align > > & sa_vector);

void
BLASTUngappedHspListToSeqAlign(EBlastProgramType program, 
                               BlastHSPList* hsp_list, 
                               CRef<CSeq_id> query_id, 
                               CRef<CSeq_id> subject_id, 
                               Int4 query_length, 
                               Int4 subject_length,
                               const vector<int> & gi_list,
                               vector<CRef<CSeq_align > > & sa_vector);

/// Convert traceback output into Seq-align format.
/// 
/// This converts the traceback stage output into a standard
/// Seq-align.  The result_type argument indicates whether the
/// seqalign is for a database search or a sequence comparison
/// (eDatabaseSearch or eSequenceComparison).  The seqinfo_src
/// argument is used to translate oids into Seq-ids.
/// 
/// @param hsp_results
///   Results of a traceback search. [in]
/// @param local_data
///   The queries used to perform the search. [in]
/// @param seqinfo_src
///   Provides sequence identifiers and meta-data. [in]
/// @param program
///   The type of search done. [in]
/// @param gapped
///   True if this was a gapped search. [in]
/// @param oof_mode
///   True if out-of-frame matches are allowed. [in]
/// @param subj_masks
///   If applicable, it'll be populated with subject masks that intersect the
///   HSPs (each element corresponds to a query) [in|out]
/// @param result_type
///   Specify how to arrange the results in the return value. [in]

TSeqAlignVector
LocalBlastResults2SeqAlign(BlastHSPResults   * hsp_results,
                           ILocalQueryData   & local_data,
                           const IBlastSeqInfoSrc& seqinfo_src,
                           EBlastProgramType   program,
                           bool                gapped,
                           bool                oof_mode,
                           vector<TSeqLocInfoVector>& subj_masks,
                           EResultType         result_type = eDatabaseSearch);

// Convert PrelminSearch Output to CStdseg
//
// This converts the BlatsHitsLists for a query into a list of CStd_seg
// @param program
//		Blast Program type [in]
// @param   hit_list,
// 		ptr to BlastHitList, results from prelimiary search [in]
// @param  query_loc
//		seq-loc for the query [in]
// @param query_length,
//		query length [in]
// @param subject_seqiinfo
//		sbject seqinfosrc ptr [in]
// @param seg_list
// 		List of CStd_seg convetred from blast hsp [out]
void
BLASTPrelminSearchHitListToStdSeg(EBlastProgramType 	   program,
                     	 	 	  BlastHitList*			   hit_list,
                     	 	 	  const CSeq_loc & 		   query_loc,
                     	 	 	  TSeqPos				   query_length,
                     	 	 	  const IBlastSeqInfoSrc * subject_seqinfo,
                     	 	 	  list<CRef<CStd_seg > > & seg_list);

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___BLAST_SEQALIGN__HPP */
