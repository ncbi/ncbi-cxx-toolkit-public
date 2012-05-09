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
#include <objmgr/annot_selector.hpp>
#include <objmgr/annot_ci.hpp>
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
#include <objects/seq/Seq_literal.hpp>
#include <objmgr/seq_vector.hpp>
#include <objects/seqalign/Score.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objmgr/util/sequence.hpp>

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
        NON_CONST_ITERATE(CQuerySet::TAssemblyToSubjectSet, AssemIter, QueryIter->second->Get()) {
            NON_CONST_ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, AssemIter->second) {  
            //NON_CONST_ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, QueryIter->second->Get()) {

                ITERATE(CSeq_align_set::Tdata, Iter, SubjectIter->second->Get()) {

                    CRef<CSeq_align> Curr(*Iter);

                    double DummyScore;
                    if(!Curr->GetNamedScore(CSeq_align::eScore_Score, DummyScore)) {
                        Scorer.AddScore(Scope, *Curr, CSeq_align::eScore_Blast);
                    }

                    if(!Curr->GetNamedScore(CSeq_align::eScore_BitScore, DummyScore)) {
                        Scorer.AddScore(Scope, *Curr, CSeq_align::eScore_BitScore);
                    }

                    if(!Curr->GetNamedScore(CSeq_align::eScore_IdentityCount, DummyScore)) {
                        Scorer.AddScore(Scope, *Curr, CSeq_align::eScore_IdentityCount);
                    }
                }
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
        NON_CONST_ITERATE(CQuerySet::TAssemblyToSubjectSet, AssemIter, QueryIter->second->Get()) {
            NON_CONST_ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, AssemIter->second) {  
            //NON_CONST_ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, QueryIter->second->Get()) {

                ITERATE(CSeq_align_set::Tdata, Iter, SubjectIter->second->Get()) {

                    CRef<CSeq_align> Curr(*Iter);

                    //Scorer.AddScore(Scope, *Curr, CScoreBuilder::eScore_PercentIdentity);
                    Scorer.AddScore(Scope, *Curr, CSeq_align::eScore_PercentIdentity_Gapped);
                    Scorer.AddScore(Scope, *Curr, CSeq_align::eScore_PercentIdentity_Ungapped);
                    Scorer.AddScore(Scope, *Curr, CSeq_align::eScore_PercentIdentity_GapOpeningOnly);
                }
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
        NON_CONST_ITERATE(CQuerySet::TAssemblyToSubjectSet, AssemIter, QueryIter->second->Get()) {
            NON_CONST_ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, AssemIter->second) {  
            //NON_CONST_ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, QueryIter->second->Get()) {

                ITERATE(CSeq_align_set::Tdata, Iter, SubjectIter->second->Get()) {

                    CRef<CSeq_align> Curr(*Iter);

                    Scorer.AddScore(Scope, *Curr, CSeq_align::eScore_PercentCoverage);
                }
            }
        }
    }
}



void CExpansionScorer::ScoreAlignments(TAlignResultsRef AlignSet, CScope& Scope)
{
    CScoreBuilder Scorer;

    NON_CONST_ITERATE(CAlignResultsSet::TQueryToSubjectSet, QueryIter, AlignSet->Get()) {
        NON_CONST_ITERATE(CQuerySet::TAssemblyToSubjectSet, AssemIter, QueryIter->second->Get()) {
            NON_CONST_ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, AssemIter->second) {  
            //NON_CONST_ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, QueryIter->second->Get()) {

                ITERATE(CSeq_align_set::Tdata, Iter, SubjectIter->second->Get()) {

                    CRef<CSeq_align> Curr(*Iter);

                    TSeqPos AlignLen, AlignedLen;
                    AlignedLen = Scorer.GetAlignLength(*Curr, true);
                    AlignLen = Curr->GetSeqRange(1).GetLength();

                    Curr->SetNamedScore("expansion", double(AlignLen)/double(AlignedLen) );
                }
            }
        }
    }
}




void COverlapScorer::ScoreAlignments(TAlignResultsRef AlignSet, CScope& Scope)
{
    NON_CONST_ITERATE(CAlignResultsSet::TQueryToSubjectSet, QueryIter, AlignSet->Get()) {
        NON_CONST_ITERATE(CQuerySet::TAssemblyToSubjectSet, AssemIter, QueryIter->second->Get()) {
            NON_CONST_ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, AssemIter->second) {  
            //NON_CONST_ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, QueryIter->second->Get()) {

                ITERATE(CSeq_align_set::Tdata, Iter, SubjectIter->second->Get()) {

                    CRef<CSeq_align> Curr(*Iter);

                    if(!Curr->GetSegs().IsDenseg())
                        continue;

                    if(Curr->GetSegs().GetDenseg().GetDim() != 2)
                        continue;

                    vector<pair<TSeqPos, TSeqPos> > Tails;

                    for(int Row = 0;  Row < Curr->GetSegs().GetDenseg().GetDim(); ++Row) {
                        TSeqPos Start = Curr->GetSeqStart(Row);
                        TSeqPos Stop  = Curr->GetSeqStop(Row);

                        TSeqPos SeqLen = Scope.GetBioseqHandle(Curr->GetSeq_id(Row)).GetInst_Length();

                        pair<TSeqPos, TSeqPos> Pair;
                        if(Curr->GetSeqStrand(Row) == eNa_strand_plus) {
                            Pair.first = Start;
                            Pair.second = SeqLen - Stop - 1;
                        } else {
                            Pair.second = Start;
                            Pair.first = SeqLen - Stop - 1;
                        }
                        Tails.push_back(Pair);
                    }

                    bool Full, Half;
                    int Contained;
                    TSeqPos TailLen;

                    Full = (Tails[0].second <= m_Slop && Tails[1].first <= m_Slop)
                        || (Tails[0].first <= m_Slop && Tails[1].second <= m_Slop);

                    if(Tails[0].first <= m_Slop && Tails[0].second <= m_Slop)
                        Contained = 0;
                    else if(Tails[1].first <= m_Slop && Tails[1].second <= m_Slop)
                        Contained = 1;
                    else
                        Contained = -1;

                    bool Forewards;
                    Forewards = (Tails[0].first > Tails[0].second)
                             && (Tails[1].first < Tails[1].second);

                    if(Forewards) {
                        Half = Tails[0].second <= m_Slop || Tails[1].first <= m_Slop;
                        TailLen = max(Tails[0].second, Tails[1].first);
                    }
                    else {
                        Half = Tails[0].first <= m_Slop || Tails[1].second <= m_Slop;
                        TailLen = max(Tails[0].first, Tails[1].second);
                    }

                    Curr->SetNamedScore("full_dovetail", (int)Full);
                    Curr->SetNamedScore("half_dovetail", (int)Half);
                    Curr->SetNamedScore("contained", Contained);
                    Curr->SetNamedScore("tail_length", (int)TailLen);
                    //Curr->SetNamedScore("align_length_ungap", (int)Curr->GetAlignLength(false));
                }
            }
        }
    }
}



void CClippedScorer::ScoreAlignments(TAlignResultsRef AlignSet, CScope& Scope)
{
    CScoreBuilder Scorer;

    NON_CONST_ITERATE(CAlignResultsSet::TQueryToSubjectSet, QueryIter, AlignSet->Get()) {
        NON_CONST_ITERATE(CQuerySet::TAssemblyToSubjectSet, AssemIter, QueryIter->second->Get()) {
            NON_CONST_ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, AssemIter->second) {  
            //NON_CONST_ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, QueryIter->second->Get()) {

                ITERATE(CSeq_align_set::Tdata, Iter, SubjectIter->second->Get()) {

                    CRef<CSeq_align> Curr(*Iter);

                    if(!Curr->GetSegs().IsDenseg())
                        continue;

                    CSeq_id EndId;
                    EndId.Assign(Curr->GetSeq_id(0));
                    
                    CBioseq_Handle EndHandle = Scope.GetBioseqHandle(EndId);

                    // Extract the Seq-annot.locs, for the clip region.
                    CSeq_loc ClipLoc;
                    ClipLoc.SetEmpty();

                    SAnnotSelector Sel(CSeqFeatData::e_Region);
                    CAnnot_CI AnnotIter(EndHandle, Sel);

                    while(AnnotIter) {
                        if (AnnotIter->IsFtable() && 
                            AnnotIter->GetName() == "NCBI_GPIPE") {
                            CConstRef<CSeq_annot> Annot = AnnotIter->GetCompleteSeq_annot();
                            
                            ITERATE(CSeq_annot::C_Data::TFtable, FeatIter,
                                    Annot->GetData().GetFtable()) {
                                CConstRef<CSeq_feat> Feat = *FeatIter;
                                if (Feat->CanGetLocation() &&
                                    Feat->CanGetData() && 
                                    Feat->GetData().IsRegion() &&
                                    (Feat->GetData().GetRegion() == "high_quality" ||
                                     Feat->GetData().GetRegion() == "hight_quality") ) {
                                    
                                    ClipLoc.Assign(Feat->GetLocation());
                                }
                            }
                        }
                        ++AnnotIter;
                    }

        
                    if(ClipLoc.IsEmpty() && EndHandle.HasAnnots() ) {
                        CConstRef<CBioseq> EndBioseq = EndHandle.GetCompleteBioseq();
                        ITERATE(CBioseq::TAnnot, AnnotIter, EndBioseq->GetAnnot()) {
                            if( (*AnnotIter)->GetData().IsLocs() ) {
                                ITERATE(CSeq_annot::C_Data::TLocs, 
                                        LocIter, (*AnnotIter)->GetData().GetLocs()) {
                                    if( (*LocIter)->IsInt() &&
                                        (*LocIter)->GetInt().GetId().Equals(EndId))
                                        ClipLoc.Assign(**LocIter);
                                }
                            }
                        }
                    }
                    
                    if(ClipLoc.IsEmpty())
                        continue;

                    
                    CRef<CSeq_id> ClipId(new CSeq_id);
                    ClipId->SetLocal().SetStr("CLIPPED_SCORER_ID_"+EndId.GetSeqIdString(true) );
                    
                    CBioseq_Handle ClipHandle;  
                    
                    try {
                        ClipHandle = Scope.GetBioseqHandle(*ClipId);
                    } catch(...) {
                        ;
                    }

                    if(!ClipHandle) {
                        /*CRef<CSeq_entry> ClipEntry(new CSeq_entry);
                        ClipEntry->SetSeq().SetId().push_back(ClipId);
                        CRef<CDelta_seq> ClipDelta(new CDelta_seq);
                        ClipDelta->SetLoc().Assign(ClipLoc);
                        ClipEntry->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(ClipDelta);
                        ClipEntry->SetSeq().SetInst().SetRepr() = CSeq_inst::eRepr_virtual;
                        ClipEntry->SetSeq().SetInst().SetMol() = CSeq_inst::eMol_dna;
                        ClipEntry->SetSeq().SetInst().SetLength() = ClipLoc.GetInt().GetLength();
                        //ClipHandle = Scope.AddTopLevelSeqEntry(*ClipEntry);
                        */
                        
                        CRef<CBioseq> ClipBioseq(new CBioseq);
                        ClipBioseq->SetId().push_back(ClipId);
                        CRef<CDelta_seq> ClipDelta(new CDelta_seq);
                        ClipDelta->SetLoc().Assign(ClipLoc);
                        ClipBioseq->SetInst().SetExt().SetDelta().Set().push_back(ClipDelta);
                        ClipBioseq->SetInst().SetRepr() = CSeq_inst::eRepr_virtual;
                        ClipBioseq->SetInst().SetMol() = CSeq_inst::eMol_dna;
                        ClipBioseq->SetInst().SetLength() = ClipLoc.GetInt().GetLength();
                            
                        ClipHandle = Scope.AddBioseq(*ClipBioseq);
                    }


                    // Fudge the New alignment.
                    CRef<CSeq_align> ClipAlign(new CSeq_align);
                    try {
                        ClipAlign->Assign(*Curr);
                        CRef<CDense_seg> ClipDenseg;
                        CRange<TSeqPos> ClipRange(ClipLoc.GetInt().GetFrom(), ClipLoc.GetInt().GetTo());
                        CRange<TSeqPos> AlignRange = ClipAlign->GetSeqRange(0);
                        CRange<TSeqPos> Intersect = ClipRange.IntersectionWith(AlignRange);
                        if(Intersect.Empty())
                            continue;

                        ClipDenseg = ClipAlign->GetSegs().GetDenseg().ExtractSlice(0, 
                                        Intersect.GetFrom(), Intersect.GetTo() );
                        ClipDenseg->SetIds().front()->Assign(*ClipId);
                        ClipDenseg->OffsetRow(0, -ClipLoc.GetInt().GetFrom());
                        ClipAlign->SetSegs().SetDenseg().Assign(*ClipDenseg);
                        ClipAlign->SetScore().clear();
                    } catch(CException& e) {
                        cerr << "Make Clip Align exception: " << e.ReportAll() << endl;
                        continue;
                    }
                    
            
                    double Temp;
                    
                    Scorer.AddScore(Scope, *ClipAlign, CSeq_align::eScore_IdentityCount);
                    ClipAlign->GetNamedScore(CSeq_align::eScore_IdentityCount, Temp);
                    Curr->SetNamedScore("num_ident_clip", Temp);

                    Scorer.AddScore(Scope, *ClipAlign, CSeq_align::eScore_PercentIdentity_Gapped);
                    Scorer.AddScore(Scope, *ClipAlign, CSeq_align::eScore_PercentIdentity_Ungapped);
                    Scorer.AddScore(Scope, *ClipAlign, CSeq_align::eScore_PercentIdentity_GapOpeningOnly);
                    ClipAlign->GetNamedScore(CSeq_align::eScore_PercentIdentity_GapOpeningOnly, Temp);
                    Curr->SetNamedScore("pct_identity_gapopen_only_clip", Temp);
                    
                    Scorer.AddScore(Scope, *ClipAlign, CSeq_align::eScore_PercentCoverage);
                    ClipAlign->GetNamedScore(CSeq_align::eScore_PercentCoverage, Temp);
                    Curr->SetNamedScore("pct_coverage_clip", Temp);
            
                    TSeqPos AlignLen = 0;
                    TSeqPos AlignedLen = 0;
                    AlignedLen = Scorer.GetAlignLength(*ClipAlign, true);
                    try {
                        // Sometimes the clipped region ends up being entirely gap, on the subject side.
                        AlignLen = ClipAlign->GetSeqRange(1).GetLength();
                    } catch(CException& e) {
                        //cerr << "Expansion exception: " << e.ReportAll() << endl;
                        //cerr << MSerial_AsnText << *ClipAlign << endl;
                    }
                    ClipAlign->SetNamedScore("expansion", double(AlignLen)/double(AlignedLen) );
                    ClipAlign->GetNamedScore("expansion", Temp);
                    Curr->SetNamedScore("expansion_clip", Temp);

                
                    /*
                    double WeightedIdent;
                    double NumIdent, Matches, QueryLen;
                    double k = 0.04;

                    Curr->GetNamedScore(CSeq_align::eScore_IdentityCount, NumIdent);
                    Matches = Curr->GetAlignLength(false);
                    QueryLen = EndHandle.GetInst_Length();
                                        
                    WeightedIdent = (NumIdent + (Matches * k)) / (Matches + (QueryLen * k ));
                    Curr->SetNamedScore("weighted_identity", WeightedIdent); 

                    ClipAlign->GetNamedScore(CSeq_align::eScore_IdentityCount, NumIdent);
                    Matches = ClipAlign->GetAlignLength(false);
                    QueryLen = ClipLoc.GetInt().GetLength();
                    
                    WeightedIdent = (NumIdent + (Matches * k)) / (Matches + (QueryLen * k ));
                    Curr->SetNamedScore("weighted_identity_clip", WeightedIdent); 
                    */

                    //Scope.RemoveFromHistory(Scope.GetBioseqHandle(*ClipId));
                    //Scope.RemoveTopLevelSeqEntry(ClipHandle);
                }
            }
        }
    }
}





void CCommonComponentScorer::ScoreAlignments(TAlignResultsRef AlignSet,
                                             CScope& Scope)
{

    NON_CONST_ITERATE(CAlignResultsSet::TQueryToSubjectSet, QueryIter,
                      AlignSet->Get()) {
        NON_CONST_ITERATE(CQuerySet::TAssemblyToSubjectSet, AssemIter, QueryIter->second->Get()) {
            NON_CONST_ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, AssemIter->second) {  
            //NON_CONST_ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, QueryIter->second->Get()) {

                ITERATE(CSeq_align_set::Tdata, Iter, SubjectIter->second->Get()) {

                    CRef<CSeq_align> Curr(*Iter);

                    list<CRef<CSeq_id> > QueryIds, SubjectIds;

                    x_GetCompList(Curr->GetSeq_id(0),
                                  Curr->GetSeqStart(0),
                                  Curr->GetSeqStop(0),
                                  QueryIds, Scope);
                    x_GetCompList(Curr->GetSeq_id(1),
                                  Curr->GetSeqStart(1),
                                  Curr->GetSeqStop(1),
                                  SubjectIds, Scope);

                    bool IsCommon = x_CompareCompLists(QueryIds, SubjectIds);

                    Curr->SetNamedScore("common_component", (int)IsCommon);
                }
            }
        }
    }
}


void CCommonComponentScorer::x_GetUserCompList(CBioseq_Handle Handle,
                                                list<CRef<CSeq_id> >& CompIds)
{
    if(!Handle.CanGetDescr())
        return;

/*
user {
      type
        str "RefGeneTracking" ,
      data {
        {
          label
            str "Status" ,
          data
            str "Inferred" } ,
        {
          label
            str "Assembly" ,
          data
            fields {
              {
                label
                  id 0 ,
                data
                  fields {
                    {
                      label
                        str "accession" ,
                      data
                        str "AC092447.5" } } } ,
              {
                label
                  id 0 ,
                data
                  fields {
                    {
                      label
                        str "gi" ,
                      data
                        int 123456 } } } } } } } ,
*/

    const CSeq_descr& Descr = Handle.GetDescr();
    ITERATE(CSeq_descr::Tdata, DescIter, Descr.Get()) {
        const CSeqdesc& Desc = **DescIter;
        if(!Desc.IsUser())
            continue;

        const CUser_object& User = Desc.GetUser();
        string TypeString;
        if(User.CanGetType()  &&  User.GetType().IsStr())
            TypeString = User.GetType().GetStr();
        if(TypeString == "RefGeneTracking" && User.HasField("Assembly")) {

            const CUser_field& TopField = User.GetField("Assembly");
            ITERATE(CUser_field::C_Data::TFields, FieldIter, TopField.GetData().GetFields()) {
                const CUser_field& MiddleField = **FieldIter;
                ITERATE(CUser_field::C_Data::TFields, FieldIter, MiddleField.GetData().GetFields()) {

                    const CUser_field& LastField = **FieldIter;
                    string IdStr;
                    if(LastField.GetData().IsStr())
                        IdStr = LastField.GetData().GetStr();
                    else if(LastField.GetData().IsInt())
                        IdStr = NStr::IntToString(LastField.GetData().GetInt());
                    else
                        continue;

                    CRef<CSeq_id> Id;
                    try {
                        Id.Reset(new CSeq_id(IdStr));
                    } catch(...) {
                        continue; // Non-Seq-id entry, just skip it.
                    }

                    CSeq_id_Handle CanonicalId;
                    CanonicalId = sequence::GetId(*Id, Handle.GetScope(),
                                                sequence::eGetId_Canonical);
                    Id->Assign(*CanonicalId.GetSeqId());
                    CompIds.push_back(Id);
                }
            }
        }
    }

}


void CCommonComponentScorer::x_GetDeltaExtCompList(CBioseq_Handle Handle,
                                           TSeqPos Start, TSeqPos Stop,
                                           list<CRef<CSeq_id> >& CompIds)
{
    if(!Handle.CanGetInst_Ext())
        return;

    const CSeq_ext& Ext = Handle.GetInst_Ext();

    if(!Ext.IsDelta())
        return;

    const CDelta_ext& DeltaExt = Ext.GetDelta();

    TSeqPos SegStart = 0;
    ITERATE(CDelta_ext::Tdata, SegIter, DeltaExt.Get()) {
        CRef<CDelta_seq> Seg(*SegIter);

        if(Seg->IsLiteral()) {
            SegStart += Seg->GetLiteral().GetLength();
            continue;
        }

        const CSeq_loc& Loc = Seg->GetLoc();

        if(!Loc.IsInt()) {
            // ??
            continue;
        }

        const CSeq_interval& Int = Loc.GetInt();

        if(Start <= (SegStart+Int.GetLength()) && Stop >= SegStart) {
            CSeq_id_Handle CanonicalId;
            CanonicalId = sequence::GetId(Int.GetId(), Handle.GetScope(),
                                        sequence::eGetId_Canonical);

            CRef<CSeq_id> Id(new CSeq_id);
            Id->Assign(*CanonicalId.GetSeqId());
            CompIds.push_back(Id);
        }

        SegStart += Int.GetLength();
    }
}


void CCommonComponentScorer::x_GetSeqHistCompList(CBioseq_Handle Handle,
                                           TSeqPos Start, TSeqPos Stop,
                                           list<CRef<CSeq_id> >& CompIds)
{
    if(!Handle.CanGetInst_Hist())
        return;

    const CSeq_hist& Hist = Handle.GetInst_Hist();

    if(!Hist.CanGetAssembly())
        return;

    const CSeq_hist::TAssembly& Assembly = Hist.GetAssembly();

    CSeq_id_Handle HandleId = sequence::GetId(Handle.GetSeq_id_Handle(),
                                Handle.GetScope(), sequence::eGetId_Canonical);

    ITERATE(CSeq_hist::TAssembly, AlignIter, Assembly) {
        const CSeq_align& Align = **AlignIter;

        // the Seq-aligns are expected to be in this row order
        // but just in case, there is a check and swap later if needed
        int query_row     = 0;
        int component_row = 1;

        CSeq_id_Handle AlignIdHandle =
            CSeq_id_Handle::GetHandle(Align.GetSeq_id(query_row));
        AlignIdHandle = sequence::GetId(AlignIdHandle, Handle.GetScope(),
                                        sequence::eGetId_Canonical);
        if (AlignIdHandle != HandleId) {
            std::swap(query_row, component_row);
        }

        list< CConstRef<CSeq_align> > SplitAligns;
        if (Align.GetSegs().IsDisc()) {
            ITERATE (CSeq_align_set::Tdata, it,
                     Align.GetSegs().GetDisc().Get()) {
                SplitAligns.push_back(*it);
            }
        } else {
            SplitAligns.push_back(CConstRef<CSeq_align>(&Align));
        }

        ITERATE(list<CConstRef<CSeq_align> >, SplitIter, SplitAligns) {
            CSeq_id_Handle CompId =
                CSeq_id_Handle::GetHandle
                ((*SplitIter)->GetSeq_id(component_row));
            CompId = sequence::GetId(CompId, Handle.GetScope(),
                                   sequence::eGetId_Canonical);

            if(Start <= (*SplitIter)->GetSeqStop(query_row) &&
               Stop >= (*SplitIter)->GetSeqStart(query_row)) {
                CRef<CSeq_id> Id(new CSeq_id);
                Id->Assign(*CompId.GetSeqId());
                CompIds.push_back(Id);
            }
        }
    }
}


void CCommonComponentScorer::x_GetCompList(const CSeq_id& Id,
                                           TSeqPos Start, TSeqPos Stop,
                                           list<CRef<CSeq_id> >& CompIds,
                                           CScope& Scope)
{
    CBioseq_Handle Handle = Scope.GetBioseqHandle(Id);

    x_GetDeltaExtCompList(Handle, Start, Stop, CompIds);
    if(CompIds.empty())
        x_GetUserCompList(Handle, CompIds);
    if(CompIds.empty())
        x_GetSeqHistCompList(Handle, Start, Stop, CompIds);
    if(CompIds.empty()) {
        CRef<CSeq_id> SelfId(new CSeq_id);
        SelfId->Assign(Id);
        CompIds.push_back(SelfId);
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

