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
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Object_id.hpp>
#include <objmgr/scope.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objmgr/seq_vector.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seqalign/Score.hpp>

#include <algo/sequence/align_cleanup.hpp>

#include <algo/align/ngalign/unordered_spliter.hpp>

BEGIN_SCOPE(ncbi)
USING_SCOPE(objects);
USING_SCOPE(blast);


template<typename T>
T s_CalcMean(const list<T>& List) {
    typedef list<T> TypeList;
    size_t Count = List.size();
    T Mean = 0;

    ITERATE(typename TypeList, Iter, List) {
        Mean += *Iter/Count;
    }

    return Mean;
}

template<typename T>
double s_CalcStdDev(const list<T>& List, T Mean) {
    typedef list<T> TypeList;
    size_t Count = List.size();

    double RunningTotal = 0;
    ITERATE(typename TypeList, Iter, List) {
        RunningTotal += ((*Iter - Mean) * (*Iter - Mean))/double(Count);
    }

    return sqrt(RunningTotal);
}

template<typename T>
void s_CalcDevs(const list<T>& Values, T Mean, double StdDev, list<double>& Devs) {
    typedef list<T> TypeList;

    ITERATE(typename TypeList, Iter, Values) {
        double Dist = fabs( double(*Iter) - double(Mean));
        double Dev = (Dist/StdDev);
        Devs.push_back(Dev);
    }
}



void CUnorderedSplitter::SplitId(const CSeq_id& Id, TSeqIdList& SplitIds)
{
    CBioseq_Handle  OrigHandle;
    OrigHandle = m_Scope->GetBioseqHandle(Id);

    if(!OrigHandle.CanGetInst())
        return;

    const CBioseq::TInst& OrigInst = OrigHandle.GetInst();

    if(OrigInst.CanGetExt() && OrigInst.GetExt().IsDelta()) {
        x_SplitDeltaExt(Id, OrigHandle, SplitIds);
    } else if(OrigInst.CanGetSeq_data()) {
        x_SplitSeqData(Id, OrigHandle, SplitIds);
    }

    // Split Other Inst types?

}


void CUnorderedSplitter::CombineAlignments(const TSeqAlignList& SourceAligns, TSeqAlignList& MergedAligns)
{
    // Get alignments into their proper Seq-ids and coordinates
    ITERATE(TSeqAlignList, AlignIter, SourceAligns) {
        CRef<CSeq_align> Fixed;
        Fixed = x_FixAlignment(**AlignIter);
        if(!Fixed.IsNull())
            MergedAligns.push_back(Fixed);
    }

    // split them up by ID pairs
    typedef map<string, TSeqAlignList> TIdPairToAlignListMap;
    TIdPairToAlignListMap  PairsMap;
    ITERATE(TSeqAlignList, AlignIter, MergedAligns) {
        string PairIdStr;
        PairIdStr = (*AlignIter)->GetSeq_id(0).AsFastaString() + "_"
                  + (*AlignIter)->GetSeq_id(1).AsFastaString();
        PairsMap[PairIdStr].push_back(*AlignIter);
    }

    // Merge the ID pair sets into Disc alignments
    MergedAligns.clear();
    NON_CONST_ITERATE(TIdPairToAlignListMap, PairMapIter, PairsMap) {
        TSeqAlignList& AlignList = PairMapIter->second;
        if(AlignList.size() == 1) {
            MergedAligns.push_back(AlignList.front());
        } else {
            x_SortAlignSet(AlignList);
            x_MakeAlignmentsUnique(AlignList);
            x_StripDistantAlignments(AlignList);
            if(AlignList.size() == 1)
                MergedAligns.push_back(AlignList.front());
            else if(!AlignList.empty()) {
                CRef<CSeq_align> Disc(new CSeq_align);
                Disc->SetType() = CSeq_align::eType_disc;
                ITERATE(TSeqAlignList, AlignIter, AlignList) {
                    if(!x_IsAllGap((*AlignIter)->GetSegs().GetDenseg()))
                        Disc->SetSegs().SetDisc().Set().push_back(*AlignIter);
                }
                MergedAligns.push_back(Disc);
            }
        }
    }

}


void CUnorderedSplitter::GetSplitIdList(TSeqIdList& SplitIdList)
{
    ITERATE(TSplitIntervalsMap, PartIter, m_PartsMap) {
        const string& PartIdString = PartIter->first;
        CRef<CSeq_id> PartId(new CSeq_id(PartIdString));
        SplitIdList.push_back(PartId);
    }
}


void CUnorderedSplitter::x_SplitDeltaExt(const objects::CSeq_id& Id,
                                         CBioseq_Handle OrigHandle,
                                         TSeqIdList& SplitIds)
{
    const CBioseq::TInst& OrigInst = OrigHandle.GetInst();
    const CDelta_ext& OrigDelta = OrigInst.GetExt().GetDelta();

    string OrigIdStr = Id.GetSeqIdString(true);
    NStr::ReplaceInPlace(OrigIdStr, ".", "_");

    CRef<CBioseq> CurrBioseq(new CBioseq);
    CurrBioseq->SetInst().SetLength(0);
    CurrBioseq->SetInst().SetRepr() = OrigInst.GetRepr();
    CurrBioseq->SetInst().SetMol() = OrigInst.GetMol();
    CurrBioseq->SetInst().SetExt().SetDelta().Set();

    TSeqPos SegStart = 0;
    TSeqPos BigStart = 0;

    ITERATE(CDelta_ext::Tdata, SeqIter, OrigDelta.Get()) {

        const CDelta_seq& Seq = **SeqIter;

        bool IsGap;
        // A Gap is a Seq-literal without a Seq-data
        IsGap  = (Seq.IsLiteral() && !Seq.GetLiteral().CanGetSeq_data());
        // Or a Gap is a Seq-literal, with a Seq-data, that is a Seq-gap.
        IsGap |= (Seq.IsLiteral() && Seq.GetLiteral().CanGetSeq_data() &&
                  Seq.GetLiteral().GetSeq_data().IsGap());
        // Maybe future checks...?

        // Its a literal, but contains no data
        if(IsGap) {
            // Finished with this Bioseq. Put an ID on it and make the next one
            CRef<CSeq_interval> CurrInterval(new CSeq_interval);
            CurrInterval->SetId().Assign(Id);
            CurrInterval->SetFrom() = BigStart;
            CurrInterval->SetTo() = SegStart-1;
            CurrInterval->SetStrand(eNa_strand_plus);

            CRef<CSeq_id> CurrId(new CSeq_id);
            CurrId->SetLocal().SetStr() = OrigIdStr;
            CurrId->SetLocal().SetStr() += "__";
            CurrId->SetLocal().SetStr() += NStr::UInt8ToString(CurrInterval->GetFrom());
            CurrId->SetLocal().SetStr() += "_";
            CurrId->SetLocal().SetStr() += NStr::UInt8ToString(CurrInterval->GetTo());

            m_PartsMap[CurrId->AsFastaString()] = CurrInterval;

            SplitIds.push_back(CurrId);
            CurrBioseq->SetId().push_back(CurrId);
//cerr << MSerial_AsnText << *CurrBioseq;
            m_Scope->AddBioseq(*CurrBioseq);
            CurrBioseq.Reset(new CBioseq);
            CurrBioseq->SetInst().SetLength(0);
            CurrBioseq->SetInst().SetRepr() = OrigInst.GetRepr();
            CurrBioseq->SetInst().SetMol() = OrigInst.GetMol();
            CurrBioseq->SetInst().SetExt().SetDelta().Set();

            SegStart += Seq.GetLiteral().GetLength();
            BigStart = SegStart;
            continue;
        }

        CurrBioseq->SetInst().SetExt().SetDelta().Set().push_back(*SeqIter);
        if(Seq.IsLoc()) { // external IDs
            TSeqPos SeqLength = (Seq.GetLoc().GetStop(eExtreme_Positional)
                                -Seq.GetLoc().GetStart(eExtreme_Positional)+1);
            CurrBioseq->SetInst().SetLength() += SeqLength;
            SegStart += SeqLength;
        } else if(Seq.IsLiteral()) { // literals containing sequence
            CurrBioseq->SetInst().SetLength() += Seq.GetLiteral().GetLength();
            SegStart += Seq.GetLiteral().GetLength();
        }
    }

    if(!CurrBioseq->GetInst().GetExt().GetDelta().Get().empty()) {
        // Finished with this Bioseq. Put an ID on it and make the next one
        CRef<CSeq_interval> CurrInterval(new CSeq_interval);
        CurrInterval->SetId().Assign(Id);
        CurrInterval->SetFrom() = BigStart;
        CurrInterval->SetTo() = SegStart-1;
        CurrInterval->SetStrand(eNa_strand_plus);

        CRef<CSeq_id> CurrId(new CSeq_id);
        CurrId->SetLocal().SetStr() = OrigIdStr;
        CurrId->SetLocal().SetStr() += "__";
        CurrId->SetLocal().SetStr() += NStr::UInt8ToString(CurrInterval->GetFrom());
        CurrId->SetLocal().SetStr() += "_";
        CurrId->SetLocal().SetStr() += NStr::UInt8ToString(CurrInterval->GetTo());

        m_PartsMap[CurrId->AsFastaString()] = CurrInterval;

        SplitIds.push_back(CurrId);
        CurrBioseq->SetId().push_back(CurrId);
    //cerr << MSerial_AsnText << *CurrBioseq;
        m_Scope->AddBioseq(*CurrBioseq);
    }
}


void CUnorderedSplitter::x_SplitSeqData(const objects::CSeq_id& Id,
                                        CBioseq_Handle OrigHandle,
                                        TSeqIdList& SplitIds)
{

    CSeqVector Vec = OrigHandle.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
    string IupacStr;
    Vec.GetSeqData(0, Vec.size(), IupacStr);
    string Ns(100, 'n');

    string OrigIdStr = Id.GetSeqIdString(true);
    NStr::ReplaceInPlace(OrigIdStr, ".", "_");

    const CBioseq::TInst& OrigInst = OrigHandle.GetInst();
       
    size_t Start = 0, Found = 0;
    do {
        Found = IupacStr.find(Ns, Start);

        if(Found != string::npos) {
        
            CRef<CSeq_interval> CurrInterval(new CSeq_interval);
            CurrInterval->SetId().Assign(Id);
            CurrInterval->SetFrom() = Start;
            CurrInterval->SetTo() = Found-1;
            CurrInterval->SetStrand(eNa_strand_plus);

            CRef<CSeq_id> CurrId(new CSeq_id);
            CurrId->SetLocal().SetStr() = OrigIdStr;
            CurrId->SetLocal().SetStr() += "__";
            CurrId->SetLocal().SetStr() += NStr::UInt8ToString(CurrInterval->GetFrom());
            CurrId->SetLocal().SetStr() += "_";
            CurrId->SetLocal().SetStr() += NStr::UInt8ToString(CurrInterval->GetTo());

            m_PartsMap[CurrId->AsFastaString()] = CurrInterval;

            SplitIds.push_back(CurrId);
            CRef<CDelta_seq> DeltaSeq(new CDelta_seq);
            DeltaSeq->SetLoc().SetInt().Assign(*CurrInterval);
            
            CRef<CBioseq> CurrBioseq(new CBioseq);
            CurrBioseq->SetInst().SetLength() = CurrInterval->GetLength();
            CurrBioseq->SetInst().SetRepr() = OrigInst.GetRepr();
            CurrBioseq->SetInst().SetMol() = OrigInst.GetMol();
            CurrBioseq->SetInst().SetExt().SetDelta().Set();
    
            CurrBioseq->SetInst().SetExt().SetDelta().Set().push_back(DeltaSeq);
            CurrBioseq->SetId().push_back(CurrId);
//cerr << MSerial_AsnText << *CurrBioseq;
            m_Scope->AddBioseq(*CurrBioseq);
            
            Start = Found+Ns.size();
        }
    
    } while(Found != string::npos);
        
    if(Start <= Vec.size()) {

        CRef<CSeq_interval> CurrInterval(new CSeq_interval);
        CurrInterval->SetId().Assign(Id);
        CurrInterval->SetFrom() = Start;
        CurrInterval->SetTo() = Vec.size()-1;
        CurrInterval->SetStrand(eNa_strand_plus);

        CRef<CSeq_id> CurrId(new CSeq_id);
        CurrId->SetLocal().SetStr() = OrigIdStr;
        CurrId->SetLocal().SetStr() += "__";
        CurrId->SetLocal().SetStr() += NStr::UInt8ToString(CurrInterval->GetFrom());
        CurrId->SetLocal().SetStr() += "_";
        CurrId->SetLocal().SetStr() += NStr::UInt8ToString(CurrInterval->GetTo());

        m_PartsMap[CurrId->AsFastaString()] = CurrInterval;

        SplitIds.push_back(CurrId);
        CRef<CDelta_seq> DeltaSeq(new CDelta_seq);
        DeltaSeq->SetLoc().SetInt().Assign(*CurrInterval);
        
        CRef<CBioseq> CurrBioseq(new CBioseq);
        CurrBioseq->SetInst().SetLength() = CurrInterval->GetLength();
        CurrBioseq->SetInst().SetRepr() = OrigInst.GetRepr();
        CurrBioseq->SetInst().SetMol() = OrigInst.GetMol();
        CurrBioseq->SetInst().SetExt().SetDelta().Set();

        CurrBioseq->SetInst().SetExt().SetDelta().Set().push_back(DeltaSeq);
        CurrBioseq->SetId().push_back(CurrId);
//cerr << MSerial_AsnText << *CurrBioseq;
        m_Scope->AddBioseq(*CurrBioseq);
    }

}


CRef<CSeq_align> CUnorderedSplitter::x_FixAlignment(const objects::CSeq_align& SourceAlignment)
{
    CRef<CSeq_align> Fixed(new CSeq_align);
    Fixed->Assign(SourceAlignment);

    if(SourceAlignment.GetSegs().IsDisc()) {
        Fixed->SetSegs().SetDisc().Set().clear();
        ITERATE(CSeq_align_set::Tdata, AlignIter, SourceAlignment.GetSegs().GetDisc().Get()) {
            CRef<CSeq_align> FixedPart;
            FixedPart = x_FixAlignment(**AlignIter);
            if(!FixedPart.IsNull())
                Fixed->SetSegs().SetDisc().Set().push_back(FixedPart);
        }
        return Fixed;
    }

    Fixed->SetScore().clear();

    CDense_seg& Denseg = Fixed->SetSegs().SetDenseg();

    CRef<CSeq_interval> SourceInterval;
    SourceInterval = m_PartsMap[Fixed->GetSeq_id(0).AsFastaString()];
    if(SourceInterval.IsNull())
        return CRef<CSeq_align>();

    Denseg.OffsetRow(0, SourceInterval->GetFrom());
    Denseg.SetIds()[0]->Assign(SourceInterval->GetId());
    return Fixed;
}


bool CUnorderedSplitter::s_SortByQueryStart(const CRef<CSeq_align>& A,
                                            const CRef<CSeq_align>& B)
{
    TSeqPos QueryStarts[2];

    QueryStarts[0] = A->GetSeqStart(0);
    QueryStarts[1] = B->GetSeqStart(0);

    // if both are with pct_coverage order them by the coverage value
    return ( QueryStarts[0] > QueryStarts[1] );
}


void CUnorderedSplitter::x_SortAlignSet(TSeqAlignList& AlignSet)
{
    vector<CRef<CSeq_align> > TempVec;
    TempVec.reserve(AlignSet.size());
    TempVec.resize(AlignSet.size());
    copy(AlignSet.begin(), AlignSet.end(), TempVec.begin());
    sort(TempVec.begin(), TempVec.end(), s_SortByQueryStart);
    AlignSet.clear();
    copy(TempVec.begin(), TempVec.end(),
        insert_iterator<TSeqAlignList>(AlignSet, AlignSet.end()));
}


void CUnorderedSplitter::x_MakeAlignmentsUnique(TSeqAlignList& Alignments)
{
    TSeqAlignList::iterator Outer, Inner;
    for(Outer = Alignments.begin(); Outer != Alignments.end(); ) {
        for(Inner = Outer, ++Inner; Inner != Alignments.end(); ) {
            x_MakeAlignmentPairUnique(*Outer, *Inner);
            if(x_IsAllGap((*Inner)->GetSegs().GetDenseg()))
                Inner = Alignments.erase(Inner);
            else
                ++Inner;
        }
        if(x_IsAllGap((*Outer)->GetSegs().GetDenseg()))
            Outer = Alignments.erase(Outer);
        else
            ++Outer;
    }
}


void CUnorderedSplitter::x_MakeAlignmentPairUnique(CRef<CSeq_align> First, CRef<CSeq_align> Second)
{
    if(x_IsAllGap(First->GetSegs().GetDenseg()) ||
       x_IsAllGap(Second->GetSegs().GetDenseg())) {
        return;
    }

    CRef<CSeq_align> DomRef, NonRef;
    if(First->GetSeqRange(0).GetLength() >= Second->GetSeqRange(0).GetLength()) {
        DomRef = First;
        NonRef = Second;
    } else {
        DomRef = Second;
        NonRef = First;
    }

    CDense_seg& DomSeg = DomRef->SetSegs().SetDenseg();
    CDense_seg& NonSeg = NonRef->SetSegs().SetDenseg();

    x_TrimRows(DomSeg, NonSeg, 0);
    x_TrimRows(DomSeg, NonSeg, 1);

    if(x_IsAllGap(NonSeg)) {
        return;
    }

    NonSeg.RemovePureGapSegs();
    NonSeg.Compact();
    if(x_IsAllGap(NonSeg)) {
        return;
    }
    NonSeg.TrimEndGaps();

    CRef<CDense_seg> FillUnaligned = NonSeg.FillUnaligned();
    if(!FillUnaligned.IsNull()) {
        NonSeg.Assign(*FillUnaligned);
    }
}


void CUnorderedSplitter::x_TrimRows(const CDense_seg& DomSeg, CDense_seg& NonSeg, int Row)
{
    if(x_IsAllGap(NonSeg))
        return;

    CRange<TSeqPos> DomRange, NonRange;
    DomRange = DomSeg.GetSeqRange(Row);
    NonRange = NonSeg.GetSeqRange(Row);

    //cerr << "NonRange Row " << Row << "  " << NonRange.GetFrom() << " to " << NonRange.GetTo() << endl;
    CRange<TSeqPos> Intersection = DomRange.IntersectionWith(NonRange);
    if(NonRange == Intersection) {
        NonSeg.SetStarts().clear();
        NonSeg.SetStarts().push_back(-1);
        NonSeg.SetStarts().push_back(-1);
        NonSeg.SetLens().clear();
        NonSeg.SetLens().push_back(1);
        NonSeg.SetNumseg(1);
        NonSeg.SetStrands().clear();
        return;
    } else if(Intersection.NotEmpty()) {
        if(Intersection.GetFrom() > NonRange.GetFrom())
            NonRange.SetTo(Intersection.GetFrom()-1);
        if(Intersection.GetTo() < NonRange.GetTo())
            NonRange.SetFrom(Intersection.GetTo()+1);
    } else {
        return;
    }

    //cerr << "NonRange Row " << Row << "  " << NonRange.GetFrom() << " to " << NonRange.GetTo() << endl;

    CRef<CDense_seg> Slice;
    Slice = NonSeg.ExtractSlice(Row, NonRange.GetFrom(), NonRange.GetTo());
    NonSeg.Assign(*Slice);
}


bool CUnorderedSplitter::x_IsAllGap(const CDense_seg& Denseg)
{
    for(int Index = 0; Index < Denseg.GetNumseg(); Index++) {
        if( Denseg.GetStarts()[Index*Denseg.GetDim()] != -1 &&
            Denseg.GetStarts()[(Index*Denseg.GetDim())+1] != -1) {
            return false;
        }
    }
    return true;
}


void CUnorderedSplitter::x_StripDistantAlignments(TSeqAlignList& Alignments)
{
    list<Int8> Centers;
    ITERATE(TSeqAlignList, AlignIter, Alignments) {
        CRange<TSeqPos> SubjRange;
        SubjRange = (*AlignIter)->GetSeqRange(1);
        TSeqPos SubjCenter;
        SubjCenter = SubjRange.GetFrom()+((SubjRange.GetTo()-SubjRange.GetFrom())/2);
        Centers.push_back(SubjCenter);
    }

    Int8 MeanCenter = 0;
    double StdDev = 0.0;
    list<double> Devs;
    MeanCenter = s_CalcMean(Centers);
    StdDev = s_CalcStdDev(Centers, MeanCenter);
    s_CalcDevs(Centers, MeanCenter, StdDev, Devs);

//    double MeanDev, StdDev2;
//    list<double> Devs2;
//    MeanDev = s_CalcMean(Devs);
//    StdDev2 = s_CalcStdDev(Devs, MeanDev);
//    s_CalcDevs(Devs, MeanDev, StdDev2, Devs2);

    TSeqAlignList::iterator AlignIter;
    list<Int8>::iterator CenterIter;
    list<double>::iterator DevsIter, Devs2Iter;
    for(AlignIter = Alignments.begin(), CenterIter = Centers.begin(),
        DevsIter = Devs.begin();
        AlignIter != Alignments.end(); ) {

        if(*DevsIter <= 3.0) {
            ++AlignIter;
            ++CenterIter;
            ++DevsIter;
        } else {
            ++AlignIter;
            ++CenterIter;
            ++DevsIter;
        }
    }

}



////////////////////////////////////////////////////////////////////////////////

CSplitSeqIdListSet::CSplitSeqIdListSet(CUnorderedSplitter* Splitter)
                    : m_Splitter(Splitter)
{
    ;
}


void CSplitSeqIdListSet::AddSeqId(const CRef<CSeq_id> Id)
{
    m_OrigSeqIdList.push_back(Id);
    m_Splitter->SplitId(*Id, m_SeqIdListSet.SetIdList());
}


void CSplitSeqIdListSet::SetSeqMasker(CSeqMasker* SeqMasker)
{
    m_SeqIdListSet.SetSeqMasker(SeqMasker);
}


CRef<IQueryFactory>
CSplitSeqIdListSet::CreateQueryFactory(CScope& Scope,
                                  const CBlastOptionsHandle& BlastOpts)
{
    if(m_OrigSeqIdList.empty()) {
        NCBI_THROW(CException, eInvalid,
                   "CSplitSeqIdListSet::CreateQueryFactory: Id List is empty.");
    }

    return m_SeqIdListSet.CreateQueryFactory(Scope, BlastOpts);
}


CRef<IQueryFactory>
CSplitSeqIdListSet::CreateQueryFactory(CScope& Scope,
                                  const CBlastOptionsHandle& BlastOpts,
                                  const CAlignResultsSet& Alignments, int Threshold)
{
    if(m_OrigSeqIdList.empty()) {
        NCBI_THROW(CException, eInvalid,
                   "CSplitSeqIdListSet::CreateQueryFactory: Id List is empty.");
    }


    return m_SeqIdListSet.CreateQueryFactory(Scope, BlastOpts, Alignments, Threshold);
}



CRef<CLocalDbAdapter>
CSplitSeqIdListSet::CreateLocalDbAdapter(CScope& Scope,
                                    const CBlastOptionsHandle& BlastOpts)
{
    if(m_OrigSeqIdList.empty()) {
        NCBI_THROW(CException, eInvalid,
                   "CSplitSeqIdListSet::CreateLocalDbAdapter: Id List is empty.");
    }


    return m_SeqIdListSet.CreateLocalDbAdapter(Scope, BlastOpts);
}


////////////////////////////////////////////////////////////////////////////////


TAlignResultsRef
CSplitSeqAlignMerger::GenerateAlignments(objects::CScope& Scope,
                                    ISequenceSet* QuerySet,
                                    ISequenceSet* SubjectSet,
                                    TAlignResultsRef AccumResults)
{
    TAlignResultsRef NewResults(new CAlignResultsSet);


    CRef<CSeq_align_set> MergedAligns(new CSeq_align_set);
    m_Splitter->CombineAlignments(AccumResults->ToSeqAlignSet()->Get(), MergedAligns->Set());
    NON_CONST_ITERATE(CSeq_align_set::Tdata, AlignIter, MergedAligns->Set()) {
        (*AlignIter)->SetNamedScore(GetName(), 1);
    }
    NewResults->Insert(*MergedAligns);

    CUnorderedSplitter::TSeqIdList PartsList;
    m_Splitter->GetSplitIdList(PartsList);
    ITERATE(CUnorderedSplitter::TSeqIdList, IdIter, PartsList) {
        AccumResults->DropQuery(**IdIter);
    }

    return NewResults;
}





END_SCOPE(ncbi)

