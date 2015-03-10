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

CTempString CGapAnalysis::s_GapTypeToStr(
    EGapType eGapType)
{
    switch (eGapType) 
    {
    case eGapType_All:
        return "All Gaps";
    case eGapType_SeqGap:
        return "Seq Gaps";
    case eGapType_UnknownBases:
        return "Unknown Bases";
    default:
        return "UNKNOWN GAP TYPE";
    }
}

void CGapAnalysis::AddSeqEntryGaps(
    const CSeq_entry_Handle & entry_h,
    CSeq_inst::EMol filter,
    CBioseq_CI::EBioseqLevelFlag level,
    TAddFlag add_flags,
    TFlag fFlags)
{
    CBioseq_CI bioseq_ci(entry_h, filter, level);
    for( ; bioseq_ci; ++bioseq_ci ) {
        AddBioseqGaps(*bioseq_ci, add_flags, fFlags);
    }
}

void CGapAnalysis::AddBioseqGaps(
    const CBioseq_Handle & bioseq_h,
    TAddFlag add_flags,
    TFlag fFlags)
{
    // get CSeq_id of CBioseq
    TSeqIdConstRef pSeqId = bioseq_h.GetSeqId();
    const TSeqPos bioseq_len = bioseq_h.GetBioseqLength();

    // fFlags control  what we look at
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
            AddGap(
                eGapType_SeqGap, pSeqId,
                seqmap_ci.GetLength(),
                bioseq_len,
                seqmap_ci.GetPosition(), seqmap_ci.GetEndPosition(),
                fFlags);
        } else if( seg_type == CSeqMap::eSeqData) {
            _ASSERT(add_flags & fAddFlag_IncludeUnknownBases);
            x_AddGapsFromBases(
                seqmap_ci, pSeqId,
                bioseq_len, fFlags);
        } else {
            ERR_POST(
                Warning << 
                "This segment type is not supported at this time: " <<
                static_cast<int>(seg_type) );
        }
    }
}

void CGapAnalysis::AddGap(
    EGapType eGapType,
    TSeqIdConstRef pSeqId,
    TGapLength iGapLength,
    TSeqPos iBioseqLength,
    TSeqPos iGapStartPos,
    TSeqPos iGapEndPosExclusive,
    TFlag fFlags)
{
    // filter out edge gaps if requested
    _ASSERT(iGapStartPos < iGapEndPosExclusive);
    _ASSERT((iGapEndPosExclusive - iGapStartPos) == iGapLength);

    if( ! (fFlags & fFlag_IncludeEndGaps) ) {
        if( iGapStartPos == 0 ||
            iGapEndPosExclusive == iBioseqLength )
        {
            // skip since it's an end gap
            return;
        }
    }

    m_gapTypeAndLengthToSeqIds[eGapType][iGapLength].insert(pSeqId);
    m_gapTypeAndLengthToSeqIds[eGapType_All][iGapLength].insert(pSeqId);

    ++m_gapTypeAndLengthToNumAppearances[eGapType][iGapLength];
    ++m_gapTypeAndLengthToNumAppearances[eGapType_All][iGapLength];

    x_GetOrCreateHistogramBinner(eGapType).AddNumber(iGapLength);
    x_GetOrCreateHistogramBinner(eGapType_All).AddNumber(iGapLength);
}

void CGapAnalysis::clear(void)
{
    m_gapTypeAndLengthToSeqIds.clear();
    m_gapTypeAndLengthToNumAppearances.clear();
    m_gapTypeToHistogramBinner.clear();
}

ostream& operator<<(
    ostream& s,
    const CGapAnalysis::SOneGapLengthSummary & one_gap_len_summary )
{
    s << "SOneGapLengthSummary("
      << "gap_length: " << one_gap_len_summary.gap_length
      << ", num_seqs: " << one_gap_len_summary.num_seqs
      << ", num_gaps: " << one_gap_len_summary.num_gaps
      << ")";
    return s;
}

ostream& operator<<(
    ostream& s,
    const CGapAnalysis::TVectorGapLengthSummary& gap_len_summary)
{
    s << "TVectorGapLengthSummary(" << endl;
    ITERATE(CGapAnalysis::TVectorGapLengthSummary, summ_it, gap_len_summary )
    {
        s << **summ_it << endl;
    }

    s << ")";
    return s;
}

AutoPtr<CGapAnalysis::TVectorGapLengthSummary>
CGapAnalysis::GetGapLengthSummary(
    EGapType eGapType,
    ESortGapLength eSortGapLength,
    ESortDir eSortDir) const
{
    AutoPtr<TVectorGapLengthSummary> pAnswer( new TVectorGapLengthSummary );
    const TMapGapLengthToSeqIds & mapGapLengthToSeqIds =
        GetGapLengthSeqIds(eGapType);
    const TMapGapLengthToNumAppearances & m_mapGapLengthToNumAppearances =
        GetGapLengthToNumAppearances(eGapType);
    ITERATE( TMapGapLengthToSeqIds, gap_map_iter, mapGapLengthToSeqIds ) {
        const TGapLength iGapLength = gap_map_iter->first;
        const TSetSeqIdConstRef & setSeqIds = gap_map_iter->second;

        // find appearances of each gap length
        Uint8 num_gaps = 0;
        TMapGapLengthToNumAppearances::const_iterator find_iter =
            m_mapGapLengthToNumAppearances.find(iGapLength);
        _ASSERT( find_iter != m_mapGapLengthToNumAppearances.end() );
        num_gaps = find_iter->second;

        pAnswer->push_back(
            Ref(new SOneGapLengthSummary(
                    iGapLength,
                    setSeqIds.size(),
                    num_gaps )));
    }

    // sort if user uses non-default ordering
    if( eSortGapLength != eSortGapLength_Length ||
        eSortDir != eSortDir_Ascending )
    {
        SOneGapLengthSummarySorter sorter(eSortGapLength, eSortDir);
        stable_sort(pAnswer->begin(), pAnswer->end(), sorter );
    }

    return pAnswer;
}

const CGapAnalysis::TMapGapLengthToSeqIds & 
CGapAnalysis::GetGapLengthSeqIds(EGapType eGapType) const
{
    static const TMapGapLengthToSeqIds empty_map;
    TGapTypeAndLengthToSeqIds::const_iterator find_it =
        m_gapTypeAndLengthToSeqIds.find(eGapType);
    if( find_it != m_gapTypeAndLengthToSeqIds.end() ) {
        return find_it->second;
    } else {
        return empty_map;
    }
}

const CGapAnalysis::TMapGapLengthToNumAppearances &
CGapAnalysis::GetGapLengthToNumAppearances(EGapType eGapType) const
{
    static TMapGapLengthToNumAppearances empty_map;
    TGapTypeAndLengthToNumAppearances::const_iterator find_it =
        m_gapTypeAndLengthToNumAppearances.find(eGapType);
    if( find_it != m_gapTypeAndLengthToNumAppearances.end() ) {
        return find_it->second;
    } else {
        return empty_map;
    }
}

AutoPtr<CHistogramBinning::TListOfBins>
CGapAnalysis::GetGapHistogram(
    EGapType eGapType,
    Uint8 num_bins,
    CHistogramBinning::EHistAlgo eHistAlgo)
{
    CHistogramBinning & histogramBinner =
        x_GetOrCreateHistogramBinner(eGapType);
    histogramBinner.SetNumBins(num_bins);
    return histogramBinner.CalcHistogram(eHistAlgo);
}

CGapAnalysis::SOneGapLengthSummarySorter::SOneGapLengthSummarySorter(
    ESortGapLength sort_gap_length_arg,
    ESortDir       sort_dir_arg )
    : sort_gap_length(sort_gap_length_arg), sort_dir(sort_dir_arg)
{
    // nothing to do
}

bool CGapAnalysis::SOneGapLengthSummarySorter::operator()(
    const CConstRef<SOneGapLengthSummary> & lhs,
    const CConstRef<SOneGapLengthSummary> & rhs ) const
{
    // handle if sorting reversed
    const SOneGapLengthSummary & real_lhs = (sort_dir == eSortDir_Ascending ? *lhs : *rhs);
    const SOneGapLengthSummary & real_rhs = (sort_dir == eSortDir_Ascending ? *rhs : *lhs);

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

CHistogramBinning &
CGapAnalysis::x_GetOrCreateHistogramBinner(EGapType eGapType)
{
    TGapTypeToHistogramBinner::iterator find_iter =
        m_gapTypeToHistogramBinner.find(eGapType);
    if( find_iter != m_gapTypeToHistogramBinner.end() ) {
        return find_iter->second->GetData();
    }

    // not found, so create
    TRefHistogramBinning new_value(
        Ref(new CObjectFor<CHistogramBinning>()));
    typedef pair<TGapTypeToHistogramBinner::iterator, bool> TInsertResult;
    TInsertResult insert_result = m_gapTypeToHistogramBinner.insert(
        make_pair(eGapType, new_value));
    _ASSERT(insert_result.second);

    return insert_result.first->second->GetData();
}

void CGapAnalysis::x_AddGapsFromBases(
    const CSeqMap_CI & seqmap_ci,
    TSeqIdConstRef bioseq_seq_id,
    TSeqPos iBioseqLength,
    TFlag fFlags)
{
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
    size_t start_pos_of_curr_gap = kInvalidSeqPos;

    CSeqVector_CI seq_vec_ci = seq_vec.begin();
    for( ; seq_vec_ci; ++seq_vec_ci ) {
        if( *seq_vec_ci == kGapChar ) {
            ++size_of_curr_gap;
            if( start_pos_of_curr_gap == kInvalidSeqPos ) {
                start_pos_of_curr_gap = (begin_pos + seq_vec_ci.GetPos());
            }
        } else if( size_of_curr_gap > 0 ) {
            _ASSERT(start_pos_of_curr_gap != kInvalidSeqPos);
            AddGap(
                eGapType_UnknownBases, bioseq_seq_id, size_of_curr_gap,
                iBioseqLength,
                start_pos_of_curr_gap, (begin_pos + seq_vec_ci.GetPos()),
                fFlags);
            size_of_curr_gap = 0;
            start_pos_of_curr_gap = kInvalidSeqPos;
        }
    }
    if( size_of_curr_gap > 0 ) {
        _ASSERT(start_pos_of_curr_gap != kInvalidSeqPos);
        AddGap(
            eGapType_UnknownBases, bioseq_seq_id, size_of_curr_gap,
            iBioseqLength,
            start_pos_of_curr_gap, (begin_pos + seq_vec_ci.GetPos()),
            fFlags);
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE
