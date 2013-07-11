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
 * Authors:
 *      Michael Kornbluh, NCBI
 *
 * File Description:
 *      Given a Bioseq, etc. it returns analysis of the gap data.

 *
 */

#include <ncbi_pch.hpp>

#include <algo/sequence/gap_analysis.hpp>

#include <objmgr/seq_map_ci.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

void CGapAnalysis::AddSeqEntryGaps(
    const CSeq_entry_Handle & entry_h,  
    CSeq_inst::EMol filter, 
    CBioseq_CI::EBioseqLevelFlag level)
{
    CBioseq_CI bioseq_ci(entry_h, filter, level);
    for( ; bioseq_ci; ++bioseq_ci ) {
        AddBioseqGaps(*bioseq_ci);
    }
}

void CGapAnalysis::AddBioseqGaps(const CBioseq_Handle & bioseq_h)
{
    // get CSeq_id of CBioseq
    TSeqIdConstRef pSeqId = bioseq_h.GetSeqId();

    SSeqMapSelector selector;
    selector.SetFlags(CSeqMap::fFindGap);
    CSeqMap_CI seqmap_ci(bioseq_h, selector);
    for( ; seqmap_ci; ++seqmap_ci ) {
        _ASSERT( seqmap_ci.GetType() == CSeqMap::eSeqGap );
        AddGap( pSeqId, seqmap_ci.GetLength() );
    }
}

void CGapAnalysis::AddGap( TSeqIdConstRef pSeqId, TGapLength iGapLength )
{
    m_mapGapLengthToSeqIds[iGapLength].insert(pSeqId);
    ++m_mapGapLengthToNumAppearances[iGapLength];
    m_histogramBinner.AddNumber(iGapLength);
}

void CGapAnalysis::Clear(void)
{
    m_mapGapLengthToSeqIds.clear();
    m_mapGapLengthToNumAppearances.clear();
    m_histogramBinner.clear();
}

AutoPtr<CGapAnalysis::TVectorGapLengthSummary> 
CGapAnalysis::GetGapLengthSummary(
    ESortGapLength eSortGapLength,
    ESortDir eSortDir) const
{
    AutoPtr<TVectorGapLengthSummary> pAnswer( new TVectorGapLengthSummary );
    ITERATE( TMapGapLengthToSeqIds, gap_map_iter, m_mapGapLengthToSeqIds ) {
        const TGapLength iGapLength = gap_map_iter->first;
        const TSetSeqIdConstRef & setSeqIds = gap_map_iter->second;

        // find appearances of each gap length
        size_t num_gaps = 0;
        TMapGapLengthToNumAppearances::const_iterator find_iter =
            m_mapGapLengthToNumAppearances.find(iGapLength);
        _ASSERT( find_iter != m_mapGapLengthToNumAppearances.end() );
        num_gaps = find_iter->second;

        SGapLengthSummary gap_length_summary(
            iGapLength,
            setSeqIds.size(),
            num_gaps );
        pAnswer->push_back(gap_length_summary);
    }

    // sort if user uses non-default ordering
    if( eSortGapLength != eSortGapLength_Length ||
        eSortDir != eSortDir_Ascending )
    {
        SGapLengthSummarySorter sorter(eSortGapLength, eSortDir);
        stable_sort(pAnswer->begin(), pAnswer->end(), sorter );
    }

    return pAnswer;
}

AutoPtr<CHistogramBinning::TListOfBins>
CGapAnalysis::GetGapHistogram(
    size_t num_bins,
    CHistogramBinning::EHistAlgo eHistAlgo )
{
    m_histogramBinner.SetNumBins(num_bins);
    return m_histogramBinner.CalcHistogram(eHistAlgo);
}

CGapAnalysis::SGapLengthSummarySorter::SGapLengthSummarySorter(
    ESortGapLength sort_gap_length_arg,
    ESortDir       sort_dir_arg )
    : sort_gap_length(sort_gap_length_arg), sort_dir(sort_dir_arg)
{
    // nothing to do
}

bool CGapAnalysis::SGapLengthSummarySorter::operator()(
    const SGapLengthSummary & lhs,
    const SGapLengthSummary & rhs ) const
{
    // handle if sorting reversed
    const SGapLengthSummary & real_lhs = (sort_dir == eSortDir_Ascending ? lhs : rhs);
    const SGapLengthSummary & real_rhs = (sort_dir == eSortDir_Ascending ? rhs : lhs);

    switch(sort_gap_length) {
    case eSortGapLength_Length:
        return real_lhs.gap_length < real_rhs.gap_length;
    case eSortGapLength_NumSeqs:
        return real_lhs.num_seqs < real_rhs.num_seqs;
    case eSortGapLength_NumGaps:
        return real_lhs.num_gaps < real_rhs.num_gaps;
    default:
        NCBI_USER_THROW_FMT("Unknown sort_gap_length: " <<
            static_cast<int>(sort_gap_length) );
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE
