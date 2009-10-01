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
#include <corelib/ncbi_system.hpp>
#include <math.h>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/scope.hpp>
#include <algo/blast/api/blast_types.hpp>
#include <algo/blast/api/bl2seq.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include <algo/blast/api/disc_nucl_options.hpp>
#include <algo/align/util/score_builder.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objmgr/seq_vector.hpp>

#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/blastinput/blastn_args.hpp>



#include <algo/align/ngalign/alignment_scorer.hpp>

BEGIN_SCOPE(ncbi)
USING_SCOPE(objects);
USING_SCOPE(blast);




void CBlastScorer::ScoreAlignments(TAlignResultsRef AlignSet, CScope& Scope)
{
    //CScoreBuilder Scorer(*Options);
    //Scorer.SetEffectiveSearchSpace(Options->GetEffectiveSearchSpace());
    CScoreBuilder Scorer(blast::eMegablast);

    NON_CONST_ITERATE(CAlignResultsSet::TQueryToSubjectSet, QueryIter, AlignSet->Get()) {
        NON_CONST_ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, QueryIter->second->Get()) {

            ITERATE(CSeq_align_set::Tdata, Iter, SubjectIter->second->Get()) {

                CRef<CSeq_align> Curr(*Iter);

                double DummyScore;
                if(!Curr->GetNamedScore(CSeq_align::eScore_Score, DummyScore))
                    Scorer.AddScore(Scope, *Curr, CScoreBuilder::eScore_Blast);
                if(!Curr->GetNamedScore(CSeq_align::eScore_BitScore, DummyScore))
                    Scorer.AddScore(Scope, *Curr, CScoreBuilder::eScore_Blast_BitScore);
                if(!Curr->GetNamedScore(CSeq_align::eScore_IdentityCount, DummyScore))
                    Scorer.AddScore(Scope, *Curr, CScoreBuilder::eScore_IdentityCount);
            }
        }
    }

}




void CPctIdentScorer::ScoreAlignments(TAlignResultsRef AlignSet, CScope& Scope)
{
    //CScoreBuilder Scorer(*Options);
    //Scorer.SetEffectiveSearchSpace(Options->GetEffectiveSearchSpace());
    CScoreBuilder Scorer(blast::eMegablast);

    NON_CONST_ITERATE(CAlignResultsSet::TQueryToSubjectSet, QueryIter, AlignSet->Get()) {
        NON_CONST_ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, QueryIter->second->Get()) {

            ITERATE(CSeq_align_set::Tdata, Iter, SubjectIter->second->Get()) {

                CRef<CSeq_align> Curr(*Iter);

                Scorer.AddScore(Scope, *Curr, CScoreBuilder::eScore_PercentIdentity);
            }
        }
    }
}





void CPctCoverageScorer::ScoreAlignments(TAlignResultsRef AlignSet, CScope& Scope)
{
    //CScoreBuilder Scorer(*Options);
    //Scorer.SetEffectiveSearchSpace(Options->GetEffectiveSearchSpace());
    CScoreBuilder Scorer(blast::eMegablast);

    NON_CONST_ITERATE(CAlignResultsSet::TQueryToSubjectSet, QueryIter, AlignSet->Get()) {
        NON_CONST_ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, QueryIter->second->Get()) {

            ITERATE(CSeq_align_set::Tdata, Iter, SubjectIter->second->Get()) {

                CRef<CSeq_align> Curr(*Iter);

                Scorer.AddScore(Scope, *Curr, CScoreBuilder::eScore_PercentCoverage);
            }
        }
    }
}




void CCommonComponentScorer::ScoreAlignments(TAlignResultsRef AlignSet, CScope& Scope)
{

    NON_CONST_ITERATE(CAlignResultsSet::TQueryToSubjectSet, QueryIter, AlignSet->Get()) {
        NON_CONST_ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, QueryIter->second->Get()) {

            ITERATE(CSeq_align_set::Tdata, Iter, SubjectIter->second->Get()) {

                CRef<CSeq_align> Curr(*Iter);

                list<CRef<CSeq_id> > QueryIds, SubjectIds;

                x_GetCompList(Curr->GetSeq_id(0), QueryIds, Scope);
                x_GetCompList(Curr->GetSeq_id(1), SubjectIds, Scope);

                bool IsCommon = x_CompareCompLists(QueryIds, SubjectIds);

                Curr->SetNamedScore("common_component", (int)IsCommon);
            }
        }
    }
}


void CCommonComponentScorer::x_GetCompList(const CSeq_id& Id,
                                           list<CRef<CSeq_id> >& CompIds,
                                           CScope& Scope)
{
    CBioseq_Handle Handle = Scope.GetBioseqHandle(Id);

    if(!Handle.CanGetInst_Ext())
        return;

    const CSeq_ext& Ext = Handle.GetInst_Ext();

    if(!Ext.IsDelta())
        return;

    const CDelta_ext& DeltaExt = Ext.GetDelta();

    ITERATE(CDelta_ext::Tdata, SegIter, DeltaExt.Get()) {
        CRef<CDelta_seq> Seg(*SegIter);

        if(!Seg->IsLoc())
            continue;

        const CSeq_loc& Loc = Seg->GetLoc();

        if(!Loc.IsInt())
            continue;

        const CSeq_interval& Int = Loc.GetInt();

        CRef<CSeq_id> Id(new CSeq_id);
        Id->Assign(Int.GetId());
        CompIds.push_back(Id);
    }
}


bool CCommonComponentScorer::x_CompareCompLists(list<CRef<CSeq_id> >& QueryIds,
                                                list<CRef<CSeq_id> >& SubjectIds)
{
    list<CRef<CSeq_id> >::iterator QueryIter, SubjectIter;
    for(QueryIter = QueryIds.begin(); QueryIter != QueryIds.end(); ++QueryIter) {
        for(SubjectIter = SubjectIds.begin(); SubjectIter != SubjectIds.end(); ++SubjectIter) {
            if( (*QueryIter)->Equals(**SubjectIter) )
                return true;
        }
    }

    return false;
}


END_SCOPE(ncbi)

