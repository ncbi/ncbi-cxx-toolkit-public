/* 
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
 * Author:  Alex Kotliarov
 *
 * File Description: 
 *  Concrete implementation of scores for a collection of alignments.
 *      - seq_percent_coverage
 *      - uniq_seq_percent_coverage: unique subject sequence query coverage.
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/seqalign/Score.hpp>

#include <objmgr/bioseq_handle.hpp>
#include <objmgr/scope.hpp>

#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Dense_diag.hpp>

#include <objtools/alnmgr/alnmap.hpp>

#include <util/range_coll.hpp>

#include <algo/align/util/named_collection_score_impl.hpp>

#include <cassert>
BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

typedef vector<CSeq_align const*>::const_iterator TIterator;

static vector<CScoreValue> MakeSubjectScores(CScope& scope, CSeq_align_set const& coll, pair<double, bool> (* f)(CBioseq_Handle const&, TIterator, TIterator));
// Compare: order alignments by (query-id, subject-id)
static bool Compare(CSeq_align const* lhs, CSeq_align const* rhs);
static CRange<TSeqPos> & s_FixMinusStrandRange(CRange<TSeqPos> & rng);
static CRef<CSeq_align> CreateDensegFromDendiag(CSeq_align const& aln);

// Subject sequence query coverage.
const char* CScoreSeqCoverageName = "seq_percent_coverage";

string CScoreSeqCoverage::GetName() const
{
    return CScoreUniqSeqCoverage::Name;
}

vector<CScoreValue> CScoreSeqCoverage::Get(CScope& scope, CSeq_align_set const& coll) const
{
    return MakeSubjectScores(scope, coll, CScoreSeqCoverage::MakeScore);
}

pair<double, bool> CScoreSeqCoverage::MakeScore(CBioseq_Handle const& query_handle, vector<CSeq_align const*>::const_iterator begin, vector<CSeq_align const*>::const_iterator end) 
{
    CConstRef<CBioseq> bioseq = query_handle.GetCompleteBioseq();

    unsigned int qlen = 0;
    if ( !bioseq.Empty() && bioseq->IsSetLength()) {
        qlen = bioseq->GetLength();
    }

    if ( !qlen ) {
        return make_pair(0, false);
    }

    // Subject coverage score
    CRangeCollection<TSeqPos> range_coll;

    for ( ; begin != end; ++begin ) {
        CRange<TSeqPos> range = (*begin)->GetSeqRange(0);
        s_FixMinusStrandRange(range);
        range_coll += range;            
    } 
    double score = ( 100.0 * range_coll.GetCoveredLength() ) / qlen;
    if ( score < 99 ) score += 0.5;
    return make_pair(score, true);
}

CIRef<INamedAlignmentCollectionScore> CScoreSeqCoverage:: Create()
{
    return CIRef<INamedAlignmentCollectionScore>(new CScoreSeqCoverage);
}

// Unique subject sequence query coverage.
const char* CScoreUniqSeqCoverageName = "uniq_seq_percent_coverage";

string CScoreUniqSeqCoverage::GetName() const
{
    return CScoreUniqSeqCoverage::Name;
}

std::vector<objects::CScoreValue> CScoreUniqSeqCoverage::Get(CScope& scope, CSeq_align_set const& coll) const
{
    return MakeSubjectScores(scope, coll, CScoreUniqSeqCoverage::MakeScore);
}

pair<double, bool> CScoreUniqSeqCoverage::MakeScore(CBioseq_Handle const& query_handle, vector<CSeq_align const*>::const_iterator begin, vector<CSeq_align const*>::const_iterator end)
{
    CConstRef<CBioseq> bioseq = query_handle.GetCompleteBioseq();

    unsigned int qlen = 0;
    if ( !bioseq.Empty() && bioseq->IsSetLength()) {
        qlen = bioseq->GetLength();
    }

    if ( !qlen ) {
        return make_pair(0, false);
    }

    bool isDenDiag = ( (*begin)->GetSegs().Which() == CSeq_align::C_Segs::e_Dendiag) ?
                              true : false;

    CRangeCollection<TSeqPos> subj_rng_coll((*begin)->GetSeqRange(1));
    CRange<TSeqPos> q_rng((*begin)->GetSeqRange(0));
    
    CRangeCollection<TSeqPos> query_rng_coll(s_FixMinusStrandRange(q_rng));
    
    for( ++begin; begin != end; ++begin ) {
        const CRange<TSeqPos> align_subj_rng((*begin)->GetSeqRange(1));
        // subject range should always be on the positive strand
        assert(align_subj_rng.GetTo() > align_subj_rng.GetFrom());
        CRangeCollection<TSeqPos> coll(align_subj_rng);
        coll.Subtract(subj_rng_coll);

        if ( coll.empty() ) {
            continue;
        }

        if(coll[0] == align_subj_rng) {
            CRange<TSeqPos> query_rng ((*begin)->GetSeqRange(0));
            query_rng_coll += s_FixMinusStrandRange(query_rng);
            subj_rng_coll += align_subj_rng;
        }
        else {
            ITERATE (CRangeCollection<TSeqPos>, uItr, coll) {
                CRange<TSeqPos> query_rng;
                const CRange<TSeqPos> & subj_rng = (*uItr);
                CRef<CSeq_align> densegAln;
                if ( isDenDiag) {
                    densegAln = CreateDensegFromDendiag(**begin);
                }

                CAlnMap map( (isDenDiag) ? densegAln->GetSegs().GetDenseg() : (*begin)->GetSegs().GetDenseg());
                TSignedSeqPos subj_aln_start =  map.GetAlnPosFromSeqPos(1,subj_rng.GetFrom());
                TSignedSeqPos subj_aln_end =  map.GetAlnPosFromSeqPos(1,subj_rng.GetTo());
                query_rng.SetFrom(map.GetSeqPosFromAlnPos(0,subj_aln_start));
                query_rng.SetTo(map.GetSeqPosFromAlnPos(0,subj_aln_end));

                query_rng_coll += s_FixMinusStrandRange(query_rng);
                subj_rng_coll += subj_rng;
            }
        }
    }

    double score = ( 100. * query_rng_coll.GetCoveredLength() ) / qlen;
    if ( score < 99 ) score += 0.5;
    return make_pair(score, true);
}

CIRef<INamedAlignmentCollectionScore> CScoreUniqSeqCoverage::Create()
{
    return CIRef<INamedAlignmentCollectionScore>(new CScoreUniqSeqCoverage);
} 

// Order by (query-seq-id, subject-seq-id, subject-start-pos)
bool Compare(CSeq_align const* lhs, CSeq_align const* rhs)
{
    CSeq_id const& qid_lhs = lhs->GetSeq_id(0);
    CSeq_id const& qid_rhs = rhs->GetSeq_id(0);

    if (qid_lhs.CompareOrdered(qid_rhs) < 0) return true;
    if (qid_rhs.CompareOrdered(qid_lhs) < 0) return false;

    CSeq_id const& sid_lhs = lhs->GetSeq_id(1);
    CSeq_id const& sid_rhs = rhs->GetSeq_id(1);

    if (qid_lhs.CompareOrdered(sid_rhs) < 0) return true;
    if (qid_rhs.CompareOrdered(sid_lhs) < 0) return false;

    return lhs->GetSeqStart(1) < rhs->GetSeqStart(1);
}

vector<CScoreValue> MakeSubjectScores(CScope& scope, CSeq_align_set const& coll, pair<double, bool> (* f)(CBioseq_Handle const&, TIterator, TIterator))
{
    vector<CScoreValue> results;

    vector<CSeq_align const*> aligns;
     
    aligns.reserve(coll.Get().size());
    list<CRef<CSeq_align> > const& alignments = coll.Get();
    for ( list<CRef<CSeq_align> >::const_iterator i = alignments.cbegin(); i != alignments.cend(); ++i ) {
        aligns.push_back(i->GetPointer());
    }
    // Sort collection of alignments
    std::sort(aligns.begin(), aligns.end(), Compare);

    for ( vector<CSeq_align const*>::const_iterator i = aligns.begin(); i != aligns.end(); ) {

        CSeq_id const& qid = (*i)->GetSeq_id(0);
        CSeq_id const& sid = (*i)->GetSeq_id(1);
        CBioseq_Handle qh = scope.GetBioseqHandle(qid); 

        vector<CSeq_align const*>::const_iterator j = i + 1;
        for ( ; j != aligns.end(); ++j ) {
            if ( !qid.Match((*j)->GetSeq_id(0)) ) break;
            if ( !sid.Match((*j)->GetSeq_id(1)) ) break;
        }
       
        pair<double, bool> value = f(qh, i, j);
        if ( value.second ) {
            results.push_back(CScoreValue(CSeq_id_Handle::GetHandle(qid),
                                          CSeq_id_Handle::GetHandle(sid),
                                          value.first));
        }

        i = j;
    }
    return results;       
}

CRange<TSeqPos> & s_FixMinusStrandRange(CRange<TSeqPos> & rng)
{
    if(rng.GetFrom() > rng.GetTo()){
        rng.Set(rng.GetTo(), rng.GetFrom());
    }
    return rng;
}

// CreateDensegFromDendiag: 
// copied from 
//  - objtools/align_format/align_format_util.cpp
//      -- CRef<CSeq_align> CAlignFormatUtil::CreateDensegFromDendiag(const CSeq_align& aln) 
CRef<CSeq_align> CreateDensegFromDendiag(CSeq_align const & aln) 
{
    CRef<CSeq_align> sa(new CSeq_align);
    if ( !aln.GetSegs().IsDendiag()) {
        NCBI_THROW(CException, eUnknown, "Input Seq-align should be Dendiag!");
    }
    
    if(aln.IsSetType()){
        sa->SetType(aln.GetType());
    }
    if(aln.IsSetDim()){
        sa->SetDim(aln.GetDim());
    }
    if(aln.IsSetScore()){
        sa->SetScore() = aln.GetScore();
    }
    if(aln.IsSetBounds()){
        sa->SetBounds() = aln.GetBounds();
    }
    
    CDense_seg& ds = sa->SetSegs().SetDenseg();
    
    int counter = 0;
    ds.SetNumseg() = 0;
    ITERATE (CSeq_align::C_Segs::TDendiag, iter, aln.GetSegs().GetDendiag()){
        
        if(counter == 0){//assume all dendiag segments have same dim and ids
            if((*iter)->IsSetDim()){
                ds.SetDim((*iter)->GetDim());
            }
            if((*iter)->IsSetIds()){
                ds.SetIds() = (*iter)->GetIds();
            }
        }
        ds.SetNumseg() ++;
        if((*iter)->IsSetStarts()){
            ITERATE(CDense_diag::TStarts, iterStarts, (*iter)->GetStarts()){
                ds.SetStarts().push_back(*iterStarts);
            }
        }
        if((*iter)->IsSetLen()){
            ds.SetLens().push_back((*iter)->GetLen());
        }
        if((*iter)->IsSetStrands()){
            ITERATE(CDense_diag::TStrands, iterStrands, (*iter)->GetStrands()){
                ds.SetStrands().push_back(*iterStrands);
            }
        }
        if((*iter)->IsSetScores()){
            ITERATE(CDense_diag::TScores, iterScores, (*iter)->GetScores()){
                ds.SetScores().push_back(*iterScores); //this might not have
                                                       //right meaning
            }
        }
        counter ++;
    }
    
    return sa;
}

END_NCBI_SCOPE
