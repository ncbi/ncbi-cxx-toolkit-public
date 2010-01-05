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
        string IdString = (*Iter)->GetSeq_id(1).AsFastaString();

        if(m_SubjectMap.find(IdString) == m_SubjectMap.end()) {
            CRef<CSeq_align_set> AlignSet(new CSeq_align_set);
            m_SubjectMap[IdString] = AlignSet;
        }
        if(!x_AlreadyContains(*m_SubjectMap[IdString], **Iter)) {
            m_SubjectMap[IdString]->Set().push_back(*Iter);
        }
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

END_SCOPE(ncbi)

