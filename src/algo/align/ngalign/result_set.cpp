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
 * Author:  Nathan Bouk
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiexpt.hpp>
#include <math.h>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/scope.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

#include <algo/blast/api/blast_results.hpp>

#include <algo/align/ngalign/result_set.hpp>
#include <algo/align/ngalign/ngalign_interface.hpp>


BEGIN_SCOPE(ncbi)
USING_SCOPE(objects);
USING_SCOPE(blast);


CQuerySet::CQuerySet(const CSearchResults& Results)
{
    m_QueryId.Reset(new CSeq_id);
    m_QueryId->Assign(*Results.GetSeqId());
    Insert(*Results.GetSeqAlign());
}


CQuerySet::CQuerySet(const objects::CSeq_align_set& Results)
{
    m_QueryId.Reset(new CSeq_id);
    m_QueryId->Assign(Results.Get().front()->GetSeq_id(0));
    Insert(Results);
}


CQuerySet::CQuerySet(const CRef<CSeq_align> Alignment)
{
    m_QueryId.Reset(new CSeq_id);
    m_QueryId->Assign(Alignment->GetSeq_id(0));
    Insert(Alignment);
}


CRef<CSeq_align_set> CQuerySet::ToSeqAlignSet() const
{
    CRef<CSeq_align_set> Out(new CSeq_align_set);
    ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, m_SubjectMap) {
        ITERATE(CSeq_align_set::Tdata, AlignIter, SubjectIter->second->Get()) {
            Out->Set().push_back( *AlignIter );
        }
    }
    return Out;
}


CRef<CSeq_align_set> CQuerySet::ToBestSeqAlignSet() const
{
    CRef<CSeq_align_set> Out(new CSeq_align_set);
    int BestRank = GetBestRank();
    ERR_POST(Info << "Best Rank: " << BestRank << " for " << m_QueryId->GetSeqIdString(true));
    if(BestRank == -1) {
        return CRef<CSeq_align_set>();
    }
    ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, m_SubjectMap) {
        ITERATE(CSeq_align_set::Tdata, AlignIter, SubjectIter->second->Get()) {
            int CurrRank;
            if((*AlignIter)->GetNamedScore(IAlignmentFilter::KFILTER_SCORE, CurrRank)) {
                if(CurrRank == BestRank) {
                    Out->Set().push_back(*AlignIter);
                }
            }
        }
    }

    x_FilterStrictSubAligns(*Out);

    return Out;
}


void CQuerySet::Insert(CRef<CQuerySet> QuerySet)
{
    ITERATE(TSubjectToAlignSet, SubjectIter, QuerySet->Get()) {
        Insert(*SubjectIter->second);
    }
}


void CQuerySet::Insert(const objects::CSeq_align_set& AlignSet)
{
    ITERATE(CSeq_align_set::Tdata, Iter, AlignSet.Get()) {
        Insert(*Iter);
    }
}


void CQuerySet::Insert(const CRef<CSeq_align> Alignment)
{
    if(!Alignment->GetSeq_id(0).Equals(*m_QueryId)) {
        // Error, Throw?
        ERR_POST(Error << "Id " << Alignment->GetSeq_id(0).AsFastaString()
                       << " tried to be inserted into set for "
                       << m_QueryId->AsFastaString());
        return;
    }

    string IdString = Alignment->GetSeq_id(1).AsFastaString();

    if(m_SubjectMap.find(IdString) == m_SubjectMap.end()) {
        CRef<CSeq_align_set> AlignSet(new CSeq_align_set);
        m_SubjectMap[IdString] = AlignSet;
    }
    if(!x_AlreadyContains(*m_SubjectMap[IdString], *Alignment)) {
        m_SubjectMap[IdString]->Set().push_back(Alignment);
    }
}


int CQuerySet::GetBestRank() const
{
    int BestRank = numeric_limits<int>::max();
    bool NeverRanked = true;

    ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, m_SubjectMap) {
        ITERATE(CSeq_align_set::Tdata, AlignIter, SubjectIter->second->Get()) {
            int CurrRank;
            if((*AlignIter)->GetNamedScore(IAlignmentFilter::KFILTER_SCORE, CurrRank)) {
                BestRank = min(BestRank, CurrRank);
                NeverRanked = false;
            }
        }
    }

    if(NeverRanked)
        return -1;

    return BestRank;
}


bool CQuerySet::x_AlreadyContains(const CSeq_align_set& Set, const CSeq_align& New) const
{
    ITERATE(CSeq_align_set::Tdata, AlignIter, Set.Get()) {
        if( (*AlignIter)->GetSeqStart(0) == New.GetSeqStart(0) &&
            (*AlignIter)->GetSeqStop(0) == New.GetSeqStop(0) &&
            (*AlignIter)->GetSeqStart(1) == New.GetSeqStart(1) &&
            (*AlignIter)->GetSeqStop(1) == New.GetSeqStop(1) &&
            (*AlignIter)->GetSegs().Which() == New.GetSegs().Which()) {
            return true;
        } else if( (*AlignIter)->GetSegs().Equals(New.GetSegs())) {
            return true;
        }

    }

    return false;
}


void CQuerySet::x_FilterStrictSubAligns(CSeq_align_set& Source) const
{
//  Later alignments are likely to contain earlier alignments. But the iterators
//  and erase() function only want to work one way. So we reverse the list
    Source.Set().reverse();

    CSeq_align_set::Tdata::iterator Outer, Inner;
    for(Outer = Source.Set().begin(); Outer != Source.Set().end(); ++Outer) {

        const CSeq_id& OuterSubjId = (*Outer)->GetSeq_id(1);

        for(Inner = Outer, ++Inner; Inner != Source.Set().end(); ) {

            const CSeq_id& InnerSubjId = (*Inner)->GetSeq_id(1);
            if(!OuterSubjId.Equals(InnerSubjId)) {
                ++Inner;
                continue;
            }

            bool IsInnerContained = x_ContainsAlignment(**Outer, **Inner);
            if(IsInnerContained) {
                Inner = Source.Set().erase(Inner);
            }
            else
                ++Inner;
        }
    }
}


bool CQuerySet::x_ContainsAlignment(const CSeq_align& Outer,
                                    const CSeq_align& Inner) const
{
    // Recurse over any Disc alignments
    if(Outer.GetSegs().IsDisc()) {
        bool AccumResults = false;
        ITERATE(CSeq_align_set::Tdata, AlignIter, Outer.GetSegs().GetDisc().Get()) {
            AccumResults |= x_ContainsAlignment(**AlignIter, Inner);
        }
        return AccumResults;
    } else if(Inner.GetSegs().IsDisc()) {
        bool AccumResults = false;
        ITERATE(CSeq_align_set::Tdata, AlignIter, Inner.GetSegs().GetDisc().Get()) {
            AccumResults |= x_ContainsAlignment(Outer, **AlignIter);
        }
        return AccumResults;
    }

    CRange<TSeqPos> InQueryRange, InSubjRange;
    CRange<TSeqPos> OutQueryRange, OutSubjRange;

    InQueryRange = Inner.GetSeqRange(0);
    InSubjRange = Inner.GetSeqRange(1);
    OutQueryRange = Outer.GetSeqRange(0);
    OutSubjRange = Outer.GetSeqRange(1);

    // Overly simple check, of just the alignments edges, without care for the segments inside
    /*if(OutQueryRange.IntersectionWith(InQueryRange).GetLength() == InQueryRange.GetLength() &&
       OutSubjRange.IntersectionWith(InSubjRange).GetLength() == InSubjRange.GetLength()) {
        return true;
    } else {
        return false;
    }*/

    // if they dont intersect at all, we bail early.
    if(!OutQueryRange.IntersectingWith(InQueryRange) ||
       !OutSubjRange.IntersectingWith(InSubjRange)) {
        return false;
    }

    const CDense_seg& OuterSeg = Outer.GetSegs().GetDenseg();
    const CDense_seg& InnerSeg = Inner.GetSegs().GetDenseg();

    int OuterSegIdx, InnerSegIdx;

    bool AllMatch = true;

    for(InnerSegIdx = 0; InnerSegIdx < InnerSeg.GetNumseg(); InnerSegIdx++) {

        bool InnerMatched = false;

        InQueryRange.SetFrom(InnerSeg.GetStarts()[InnerSegIdx*2]);
        InQueryRange.SetTo(InnerSeg.GetLens()[InnerSegIdx]);
        InSubjRange.SetFrom(InnerSeg.GetStarts()[(InnerSegIdx*2)+1]);
        InSubjRange.SetTo(InnerSeg.GetLens()[InnerSegIdx]);

        for(OuterSegIdx = 0; OuterSegIdx < OuterSeg.GetNumseg(); OuterSegIdx++) {

            OutQueryRange.SetFrom(OuterSeg.GetStarts()[OuterSegIdx*2]);
            OutQueryRange.SetTo(OuterSeg.GetLens()[OuterSegIdx]);
            OutSubjRange.SetFrom(OuterSeg.GetStarts()[(OuterSegIdx*2)+1]);
            OutSubjRange.SetTo(OuterSeg.GetLens()[OuterSegIdx]);

            // If the Outer segments are >= the Inner segments
            if(OutQueryRange.IntersectionWith(InQueryRange).GetLength() == InQueryRange.GetLength() &&
               OutSubjRange.IntersectionWith(InSubjRange).GetLength() == InSubjRange.GetLength() ) {
                InnerMatched = true;
                break;
            }
        }

        if(!InnerMatched) {
            AllMatch = false;
            break;
        }
    }

    return AllMatch;
}


////////////////////////////////////////////////////////////////////////////////

CAlignResultsSet::CAlignResultsSet(const blast::CSearchResultSet& BlastResults)
{
    Insert(BlastResults);
}


bool CAlignResultsSet::QueryExists(const CSeq_id& Id) const
{
    string IdString = Id.AsFastaString();
    TQueryToSubjectSet::const_iterator Found;
    Found = m_QueryMap.find(IdString);
    if(Found == m_QueryMap.end()) {
        return false;
    }
    return true;
}


CRef<CQuerySet> CAlignResultsSet::GetQuerySet(const CSeq_id& Id)
{
    string IdString = Id.AsFastaString();
    TQueryToSubjectSet::iterator Found;
    Found = m_QueryMap.find(IdString);
    if(Found == m_QueryMap.end()) {
        return CRef<CQuerySet>();
    }
    return Found->second;
}



CConstRef<CQuerySet> CAlignResultsSet::GetQuerySet(const CSeq_id& Id) const
{
    string IdString = Id.AsFastaString();
    TQueryToSubjectSet::const_iterator Found;
    Found = m_QueryMap.find(IdString);
    if(Found == m_QueryMap.end()) {
        return CRef<CQuerySet>();
    }
    return Found->second;
}


CRef<CSeq_align_set> CAlignResultsSet::ToSeqAlignSet() const
{
    CRef<CSeq_align_set> Out(new CSeq_align_set);
    ITERATE(TQueryToSubjectSet, QueryIter, m_QueryMap) {
        ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, QueryIter->second->Get()) {
            ITERATE(CSeq_align_set::Tdata, AlignIter, SubjectIter->second->Get()) {
                Out->Set().push_back( *AlignIter );
            }
        }
    }
    return Out;
}


CRef<CSeq_align_set> CAlignResultsSet::ToBestSeqAlignSet() const
{
    CRef<CSeq_align_set> Out(new CSeq_align_set);
    ITERATE(TQueryToSubjectSet, QueryIter, m_QueryMap) {

        CRef<CSeq_align_set> CurrSet;
        CurrSet = QueryIter->second->ToBestSeqAlignSet();

        if(CurrSet.IsNull())
            continue;

        ITERATE(CSeq_align_set::Tdata, AlignIter, CurrSet->Get()) {
            Out->Set().push_back(*AlignIter);
        }
    }
    return Out;
}


void CAlignResultsSet::Insert(CRef<CQuerySet> QuerySet)
{
    string IdString = QuerySet->GetQueryId()->AsFastaString();
    if(m_QueryMap.find(IdString) != m_QueryMap.end()) {
        m_QueryMap[IdString]->Insert(QuerySet);
    } else {
        m_QueryMap[IdString] = QuerySet;
    }
}


void CAlignResultsSet::Insert(CRef<CAlignResultsSet> AlignSet)
{
    ITERATE(TQueryToSubjectSet, QueryIter, AlignSet->Get()) {
        Insert(QueryIter->second);
    }
}


void CAlignResultsSet::Insert(const blast::CSearchResultSet& BlastResults)
{
    ITERATE(CSearchResultSet, Iter, BlastResults) {
        CRef<CQuerySet> Set(new CQuerySet(**Iter));
        string IdString = (*Iter)->GetSeqId()->AsFastaString();
        if(m_QueryMap.find(IdString) != m_QueryMap.end()) {
            m_QueryMap[IdString]->Insert(Set);
        } else {
            m_QueryMap[IdString] = Set;
        }
    }
}


void CAlignResultsSet::Insert(CRef<CSeq_align> Alignment)
{
    string IdString = Alignment->GetSeq_id(0).AsFastaString();
    if(m_QueryMap.find(IdString) != m_QueryMap.end()) {
        m_QueryMap[IdString]->Insert(Alignment);
    } else {
        CRef<CQuerySet> Set(new CQuerySet(Alignment));
        m_QueryMap[IdString] = Set;
    }
}


void CAlignResultsSet::Insert(const CSeq_align_set& AlignSet)
{
    if(!AlignSet.CanGet() || AlignSet.Get().empty())
        return;

    ITERATE(CSeq_align_set::Tdata, AlignIter, AlignSet.Get()) {
        Insert(*AlignIter);
    }
}


void CAlignResultsSet::DropQuery(const CSeq_id& Id)
{
    string IdString = Id.AsFastaString();
    TQueryToSubjectSet::iterator Found = m_QueryMap.find(IdString);
    if(Found != m_QueryMap.end()) {
        m_QueryMap.erase(Found);
    }
}



END_SCOPE(ncbi)

