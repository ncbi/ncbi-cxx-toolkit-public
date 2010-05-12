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
 * Author: Ilya Dondoshansky
 *
 * Initial Version Creation Date:  November 13, 2003
 *
 *
 * File Description:
 *          C++ version of repeats filtering
 *
 * */

/// @file repeats_filter_cxx.cpp
/// C++ version of repeats filtering

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <serial/iterator.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objmgr/util/sequence.hpp>
#include <algo/blast/api/blast_types.hpp>

#include <algo/blast/api/seqsrc_seqdb.hpp>

#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include <algo/blast/api/repeats_filter.hpp>
#include "blast_setup.hpp"

#include <algo/blast/core/blast_seqsrc.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_filter.h>

#include <algo/blast/api/blast_aux.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

/** Convert a list of mask locations to a CSeq_loc object.
 * @param query Query sequence location [in]
 * @param scope Scope for use by object manager [in]
 * @param loc_list List of mask locations [in]
 * @return List of mask locations in a CSeq_loc form or NULL if loc_list is
 * NULL
 */
static CSeq_loc* 
s_BlastSeqLoc2CSeqloc(const CSeq_loc & query,
                      CScope         * scope,
                      BlastSeqLoc    * loc_list)
{
    if ( !loc_list ) {
        return NULL;
    }

    CSeq_loc* seqloc = new CSeq_loc();
    BlastSeqLoc* loc;

    seqloc->SetNull();
    for (loc = loc_list; loc; loc = loc->next) {
       seqloc->SetPacked_int().AddInterval(
           sequence::GetId(query, scope),
           loc->ssr->left, loc->ssr->right);
    }
    
    return seqloc;
}

/** Convert a list of mask locations to a CSeq_loc object.
 * @param query Query sequence location [in]
 * @param loc_list List of mask locations [in]
 * @return List of mask locations in a CSeq_loc form.
 */
static CSeq_loc* 
s_BlastSeqLoc2CSeqloc(SSeqLoc& query, BlastSeqLoc* loc_list)
{
    return s_BlastSeqLoc2CSeqloc(*query.seqloc, &*query.scope, loc_list);
}

/** Convert a list of mask locations to TMaskedQueryRegions.
 * @param query Query sequence location [in]
 * @param scope Scope for use by object manager [in]
 * @param loc_list List of mask locations [in]
 * @param program type of blast search [in]
 * @return List of mask locations in TMaskedQueryRegions form.
 */
TMaskedQueryRegions
s_BlastSeqLoc2MaskedRegions(const CSeq_loc    & query,
                            CScope            * scope,
                            BlastSeqLoc       * loc_list,
                            EBlastProgramType   program)
{
    CConstRef<CSeq_loc> sloc(s_BlastSeqLoc2CSeqloc(query, scope, loc_list));
    
    return PackedSeqLocToMaskedQueryRegions(sloc, program);
}


/// Build a list of BlastSeqLoc's from a set of Dense-seg contained in a
/// Seq-align-set.
///
/// This function processes Dense-segs, and adds the range of each hit to
/// a list of BlastSeqLoc structures.  Frame information is used to
/// translate hit coordinates hits to the plus strand.  All of the
/// HSPs should refer to the same query; both the query and subject in
/// the HSP are ignored.  This is used to construct a set of filtered
/// areas from hits against a repeats database.
///
/// @param alignment    Seq-align-set containing Dense-segs which specify the
///                     ranges of hits. [in]
/// @param locs         Filtered areas for this query are added here. [out]

static void
s_SeqAlignToBlastSeqLoc(const CSeq_align_set& alignment, 
                        BlastSeqLoc ** locs)
{
    ITERATE(CSeq_align_set::Tdata, itr, alignment.Get()) {
        _ASSERT((*itr)->GetSegs().IsDenseg());
        const CDense_seg& seg = (*itr)->GetSegs().GetDenseg();
        const int kNumSegments = seg.GetNumseg();
#if _DEBUG      /* to eliminate compiler warning in release mode */
        const int kNumDim = seg.GetDim();
#endif
        _ASSERT(kNumDim == 2);

        const CDense_seg::TStarts& starts = seg.GetStarts();
        const CDense_seg::TLens& lengths = seg.GetLens();
        const CDense_seg::TStrands& strands = seg.GetStrands();
        _ASSERT(kNumSegments*kNumDim == (int) starts.size());
        _ASSERT(kNumSegments == (int) lengths.size());
        _ASSERT(kNumSegments*kNumDim == (int) strands.size());

        int left(0), right(0);

        if (strands[0] == strands[1]) {
            left = starts.front();
            right = starts[(kNumSegments-1)*2] + lengths[kNumSegments-1] - 1;
        } else {
            left = starts[(kNumSegments-1)*2];
            right = starts.front() + lengths.front() - 1;
        }

        BlastSeqLocNew(locs, left, right);
    }
}

/** Fills the mask locations in the query SSeqLoc structures, as if it was a 
 * lower case mask, given the results of a BLAST search against a database of 
 * repeats.
 * @param query Vector of query sequence locations structures [in] [out]
 * @param results alignments returned from a BLAST search against a repeats 
 *                database [in]
 */
static void
s_FillMaskLocFromBlastResults(TSeqLocVector& query, 
                              const CSearchResultSet& results)
{
    _ASSERT(results.GetNumResults() == query.size());
    
    for (size_t query_index = 0; query_index < query.size(); ++query_index) {
        const CSearchResults& result = results[query_index];

        if (result.GetSeqAlign().Empty() || result.GetSeqAlign()->IsEmpty()) {
            continue;
        }

        // Get the previous mask locations
        BlastSeqLoc* loc_list = CSeqLoc2BlastSeqLoc(query[query_index].mask);
        
        // Find all HSP intervals in query
/* DELME
        ITERATE(CSeq_align_set::Tdata, alignment, result.GetSeqAlign()->Get()) {
            _ASSERT((*alignment)->GetSegs().IsDisc());
            s_SeqAlignToBlastSeqLoc((*alignment)->GetSegs().GetDisc(), 
                                    &loc_list);
        }
*/
        s_SeqAlignToBlastSeqLoc(*(result.GetSeqAlign()), &loc_list);
        
        
        // Make the intervals unique
        BlastSeqLocCombine(&loc_list, REPEAT_MASK_LINK_VALUE);
        BlastSeqLoc* ordered_loc_list = loc_list;
        loc_list = NULL;

        /* Create a CSeq_loc with these locations and fill it for the 
           respective query */
        CRef<CSeq_loc> filter_seqloc(s_BlastSeqLoc2CSeqloc(query[query_index],
                                                           ordered_loc_list));

        // Free the combined mask list in the BlastSeqLoc form.
        ordered_loc_list = BlastSeqLocFree(ordered_loc_list);

        query[query_index].mask.Reset(filter_seqloc);
    }
}

/** Fills the mask locations in the BlastSearchQuery structures, as if it was a
 * lower case mask, given the results of a BLAST search against a database of 
 * repeats.
 * @param query Vector of queries [in] [out]
 * @param results alignments returned from a BLAST search against a repeats 
 *                database [in]
 * @param program type of blast search [in]
 */
static void
s_FillMaskLocFromBlastResults(CBlastQueryVector& query,
                              const CSearchResultSet& results,
                              EBlastProgramType program)
{
    _ASSERT(results.GetNumResults() == query.Size());
    
    for (size_t qindex = 0; qindex < query.Size(); ++qindex) {
        const CSearchResults& result = results[qindex];
        
        if (result.GetSeqAlign().Empty() || result.GetSeqAlign()->IsEmpty()) {
            continue;
        }
        
        // Get the previous mask locations
        TMaskedQueryRegions mqr = query.GetMaskedRegions(qindex);
        
        CRef<CBlastQueryFilteredFrames> frames
            (new CBlastQueryFilteredFrames(program, mqr));
        
        typedef set<CSeqLocInfo::ETranslationFrame> TFrameSet;
        const TFrameSet& used = frames->ListFrames();
        
        BlastSeqLoc* loc_list = 0;
        
        ITERATE(TFrameSet, itr, used) {
            // Pick frame +1 for nucleotide, or 0 (the only one) for protein.
            int pframe = *itr;
            
            BlastSeqLoc* locs1 = *(*frames)[pframe];
            frames->Release(pframe);
            
            BlastSeqLoc ** pplast = & loc_list;
            
            while(*pplast) {
                pplast = & (*pplast)->next;
            }
            
            *pplast = locs1;
        }
        
        // Find all HSP intervals in query
/* DELME
        ITERATE(CSeq_align_set::Tdata, alignment, result.GetSeqAlign()->Get()) {
            _ASSERT((*alignment)->GetSegs().IsDisc());
            s_SeqAlignToBlastSeqLoc((*alignment)->GetSegs().GetDisc(), 
                                    &loc_list);
        }
*/
        s_SeqAlignToBlastSeqLoc(*(result.GetSeqAlign()), &loc_list);
        
        // Make the intervals unique
        BlastSeqLocCombine(&loc_list, REPEAT_MASK_LINK_VALUE);
        BlastSeqLoc* ordered_loc_list = loc_list;
        loc_list = NULL;
        
        /* Create a CSeq_loc with these locations and fill it for the 
           respective query */
        
        TMaskedQueryRegions filter_seqloc =
            s_BlastSeqLoc2MaskedRegions(*query.GetQuerySeqLoc(qindex),
                                        query.GetScope(qindex),
                                        ordered_loc_list,
                                        program);
        
        // Free the combined mask list in the BlastSeqLoc form.
        ordered_loc_list = BlastSeqLocFree(ordered_loc_list);
        
        query.SetMaskedRegions(qindex, filter_seqloc);
    }
}

/// Create an options handle with the defaults set for a search for repeats.
static 
CRef<CBlastOptionsHandle> s_CreateRepeatsSearchOptions()
{
    CBlastNucleotideOptionsHandle* opts(new CBlastNucleotideOptionsHandle);
    opts->SetTraditionalBlastnDefaults();
    opts->SetMismatchPenalty(REPEATS_SEARCH_PENALTY);
    opts->SetMatchReward(REPEATS_SEARCH_REWARD);
    opts->SetCutoffScore(REPEATS_SEARCH_MINSCORE);
    opts->SetGapXDropoffFinal(REPEATS_SEARCH_XDROP_FINAL);
    opts->SetXDropoff(REPEATS_SEARCH_XDROP_UNGAPPED);
    opts->SetGapOpeningCost(REPEATS_SEARCH_GAP_OPEN);
    opts->SetGapExtensionCost(REPEATS_SEARCH_GAP_EXTEND);
    opts->SetDustFiltering(false);  // FIXME, is this correct?
    opts->SetWordSize(REPEATS_SEARCH_WORD_SIZE);
    return CRef<CBlastOptionsHandle>(opts);
}

void
Blast_FindRepeatFilterLoc(TSeqLocVector& query, 
                          const CBlastOptionsHandle* opts_handle)
{
    const CBlastNucleotideOptionsHandle* nucl_handle = 
        dynamic_cast<const CBlastNucleotideOptionsHandle*>(opts_handle);

    // Either non-blastn search or repeat filtering not desired.
    if (nucl_handle == NULL || nucl_handle->GetRepeatFiltering() == false)
       return;

    Blast_FindRepeatFilterLoc(query, nucl_handle->GetRepeatFilteringDB());
}

void
Blast_FindRepeatFilterLoc(TSeqLocVector& query, const char* filter_db)
{
    const CSearchDatabase target_db(filter_db,
                                    CSearchDatabase::eBlastDbIsNucleotide);

    CRef<CBlastOptionsHandle> repeat_opts = s_CreateRepeatsSearchOptions();

    // Remove any lower case masks, because they should not be used for the 
    // repeat locations search.
    vector< CRef<CSeq_loc> > lcase_mask_v;
    lcase_mask_v.reserve(query.size());
    
    for (unsigned int index = 0; index < query.size(); ++index) {
        lcase_mask_v.push_back(query[index].mask);
        query[index].mask.Reset(NULL);
    }

    CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(query));
    CLocalBlast blaster(query_factory, repeat_opts, target_db);
    CRef<CSearchResultSet> results = blaster.Run();

    // Restore the lower case masks
    for (unsigned int index = 0; index < query.size(); ++index) {
        query[index].mask.Reset(lcase_mask_v[index]);
    }

    // Extract the repeat locations and combine them with the previously 
    // existing mask in queries.
    s_FillMaskLocFromBlastResults(query, *results);
}

void
Blast_FindRepeatFilterLoc(CBlastQueryVector& queries, const char* filter_db)
{
    const CSearchDatabase target_db(filter_db,
                                    CSearchDatabase::eBlastDbIsNucleotide);

    CRef<CBlastOptionsHandle> repeat_opts = s_CreateRepeatsSearchOptions();

    // Remove any lower case masks, because they should not be used for the 
    // repeat locations search.
    CBlastQueryVector temp_queries;
    for (size_t i = 0; i < queries.Size(); ++i) {
        TMaskedQueryRegions no_masks;
        CRef<CBlastSearchQuery> query
            (new CBlastSearchQuery(*queries.GetQuerySeqLoc(i),
                                   *queries.GetScope(i), no_masks));
        temp_queries.AddQuery(query);
    }

    CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(temp_queries));
    CLocalBlast blaster(query_factory, repeat_opts, target_db);
    CRef<CSearchResultSet> results = blaster.Run();

    // Extract the repeat locations and combine them with the previously 
    // existing mask in queries.
    s_FillMaskLocFromBlastResults(queries, *results, eBlastTypeBlastn);
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
