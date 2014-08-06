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
#include <objmgr/seq_vector.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

void CGapAnalysis::AddSeqEntryGaps(
    const CSeq_entry_Handle & entry_h,
    CSeq_inst::EMol filter,
    CBioseq_CI::EBioseqLevelFlag level,
    TAddFlag add_flags)
{
    CBioseq_CI bioseq_ci(entry_h, filter, level);
    for( ; bioseq_ci; ++bioseq_ci ) {
        AddBioseqGaps(*bioseq_ci, add_flags);
    }
}

void CGapAnalysis::AddBioseqGaps(
    const CBioseq_Handle & bioseq_h,
    TAddFlag add_flags)
{
    // get CSeq_id of CBioseq
    TSeqIdConstRef pSeqId = bioseq_h.GetSeqId();

    // flags control  what we look at
    CSeqMap::TFlags seq_map_flags = 0;
    if( add_flags & fAddFlag_IncludeSeqGaps ) {
        seq_map_flags |= CSeqMap::fFindGap;
    }
    if( add_flags & fAddFlag_IncludeUnknownBases ) {
        seq_map_flags |= CSeqMap::fFindData;
    }

    SSeqMapSelector selector;
    selector.SetFlags(seq_map_flags);
    CSeqMap_CI seqmap_ci(bioseq_h, selector);
    for( ; seqmap_ci; ++seqmap_ci ) {
        CSeqMap::ESegmentType seg_type = seqmap_ci.GetType();
        if(seg_type == CSeqMap::eSeqGap) {
            _ASSERT(add_flags & fAddFlag_IncludeSeqGaps);
            AddGap( pSeqId, seqmap_ci.GetLength() );
        } else if( seg_type == CSeqMap::eSeqData) {
            _ASSERT(add_flags & fAddFlag_IncludeUnknownBases);
            x_AddGapsFromBases(seqmap_ci, bioseq_h);
        } else {
            ERR_POST(
                Warning << 
                "This segment type is not supported at this time: " <<
                static_cast<int>(seg_type) );
        }
    }
}

void CGapAnalysis::AddGap( TSeqIdConstRef pSeqId, TGapLength iGapLength )
{
    m_mapGapLengthToSeqIds[iGapLength].insert(pSeqId);
    ++m_mapGapLengthToNumAppearances[iGapLength];
    m_histogramBinner.AddNumber(iGapLength);
}

void CGapAnalysis::clear(void)
{
    m_mapGapLengthToSeqIds.clear();
    m_mapGapLengthToNumAppearances.clear();
    m_histogramBinner.clear();
}

CGapAnalysis::TVectorGapLengthSummary *
CGapAnalysis::GetGapLengthSummary(
    ESortGapLength eSortGapLength,
    ESortDir eSortDir) const
{
    AutoPtr<TVectorGapLengthSummary> pAnswer( new TVectorGapLengthSummary );
    ITERATE( TMapGapLengthToSeqIds, gap_map_iter, m_mapGapLengthToSeqIds ) {
        const TGapLength iGapLength = gap_map_iter->first;
        const TSetSeqIdConstRef & setSeqIds = gap_map_iter->second;

        // find appearances of each gap length
        Uint8 num_gaps = 0;
        TMapGapLengthToNumAppearances::const_iterator find_iter =
            m_mapGapLengthToNumAppearances.find(iGapLength);
        _ASSERT( find_iter != m_mapGapLengthToNumAppearances.end() );
        num_gaps = find_iter->second;

        SOneGapLengthSummary gap_length_summary(
            iGapLength,
            setSeqIds.size(),
            num_gaps );
        pAnswer->push_back(gap_length_summary);
    }

    // sort if user uses non-default ordering
    if( eSortGapLength != eSortGapLength_Length ||
        eSortDir != eSortDir_Ascending )
    {
        SOneGapLengthSummarySorter sorter(eSortGapLength, eSortDir);
        stable_sort(pAnswer->begin(), pAnswer->end(), sorter );
    }

    return pAnswer.release();
}

CHistogramBinning::TListOfBins *
CGapAnalysis::GetGapHistogram(
    Uint8 num_bins,
    CHistogramBinning::EHistAlgo eHistAlgo )
{
    m_histogramBinner.SetNumBins(num_bins);
    return m_histogramBinner.CalcHistogram(eHistAlgo);
}

CGapAnalysis::SOneGapLengthSummarySorter::SOneGapLengthSummarySorter(
    ESortGapLength sort_gap_length_arg,
    ESortDir       sort_dir_arg )
    : sort_gap_length(sort_gap_length_arg), sort_dir(sort_dir_arg)
{
    // nothing to do
}

bool CGapAnalysis::SOneGapLengthSummarySorter::operator()(
    const SOneGapLengthSummary & lhs,
    const SOneGapLengthSummary & rhs ) const
{
    // handle if sorting reversed
    const SOneGapLengthSummary & real_lhs = (sort_dir == eSortDir_Ascending ? lhs : rhs);
    const SOneGapLengthSummary & real_rhs = (sort_dir == eSortDir_Ascending ? rhs : lhs);

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

void CGapAnalysis::x_AddGapsFromBases(
    const CSeqMap_CI & seqmap_ci, const CBioseq_Handle & bioseq_h)
{
    TSeqIdConstRef bioseq_seq_id = bioseq_h.GetSeqId();
    const TSeqPos begin_pos = seqmap_ci.GetPosition();

    // get location representing this segment's bases
    CRef<CSeq_loc> loc_of_bases(
        new CSeq_loc(
            *SerialClone(*bioseq_seq_id),
            begin_pos,
            (begin_pos + seqmap_ci.GetLength() - 1)));
    CSeqVector seq_vec(
        *loc_of_bases, *seqmap_ci.GetScope(), CBioseq_Handle::eCoding_Iupac);
    const char kGapChar = seq_vec.GetGapChar(
        CSeqVectorTypes::eCaseConversion_upper);

    // a simple "runs of unknown bases" algo
    size_t size_of_curr_gap = 0;
    CSeqVector_CI seq_vec_ci = seq_vec.begin();
    for( ; seq_vec_ci; ++seq_vec_ci ) {
        if( *seq_vec_ci == kGapChar ) {
            ++size_of_curr_gap;
        } else if( size_of_curr_gap > 0 ) {
            AddGap(bioseq_seq_id, size_of_curr_gap);
            size_of_curr_gap = 0;
        }
    }
    if( size_of_curr_gap > 0 ){
        AddGap(bioseq_seq_id, size_of_curr_gap);
        size_of_curr_gap = 0;
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE
