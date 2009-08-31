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
#include <hash_map.h>
#include <algo/align/ngalign/banded_aligner.hpp>
//#include "align_instance.hpp"

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
#include <objmgr/seq_vector.hpp>

#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/blastinput/blastn_args.hpp>

#include <algo/align/contig_assembly/contig_assembly.hpp>
#include <algo/align/nw/nw_band_aligner.hpp>
#include <algo/align/nw/mm_aligner.hpp>

#include <algo/align/util/blast_tabular.hpp>
#include <algo/align/util/compartment_finder.hpp>
#include <algo/sequence/align_cleanup.hpp>

using namespace ncbi;
using namespace objects;
using namespace blast;
using namespace std;




CRef<CSeq_loc> s_CoverageSeqLoc(TAlignSetRef Alignments, int Row, CScope& Scope)
{
    CRef<CSeq_loc> Accum(new CSeq_loc);
    ITERATE(CSeq_align_set::Tdata, Iter, Alignments->Get()) {

        const CSeq_align& Curr = **Iter;
        const CDense_seg& Denseg = Curr.GetSegs().GetDenseg();

        size_t Dims = Denseg.GetDim();
        size_t SegCount = Denseg.GetNumseg();
        for(size_t CurrSeg = 0; CurrSeg < SegCount; ++CurrSeg) {
            int Index = (Dims*CurrSeg)+Row;
            int CurrStart = Denseg.GetStarts()[Index];
            if( CurrStart != -1) {
                CRef<CSeq_loc> CurrLoc(new CSeq_loc);
                CurrLoc->SetInt().SetId().Assign( *Denseg.GetIds()[Row] );
                CurrLoc->SetInt().SetFrom() = CurrStart;
                CurrLoc->SetInt().SetTo() = CurrStart + Denseg.GetLens()[CurrSeg];
                if(Denseg.CanGetStrands())
                    CurrLoc->SetInt().SetStrand() = Denseg.GetStrands()[Index];
                Accum->SetMix().Set().push_back(CurrLoc);
            }
        }
    }

    CRef<CSeq_loc> Merged;
    Merged = sequence::Seq_loc_Merge(*Accum, CSeq_loc::fSortAndMerge_All, &Scope);

    return Merged;
}


TSeqPos s_CalcCoverageCount(TAlignSetRef Alignments, int Row, CScope& Scope)
{

    CRef<CSeq_loc> Merged;

    Merged = s_CoverageSeqLoc(Alignments, Row, Scope);
    //cout << MSerial_AsnText << *Merged;

    Merged->ChangeToMix();
    TSeqPos PosCoveredBases = 0, NegCoveredBases = 0;
    ITERATE(CSeq_loc_mix::Tdata, LocIter, Merged->GetMix().Get()) {

//    ITERATE(CPacked_seqint_Base::Tdata, IntIter, Merged->GetPacked_int().Get())
        if( (*LocIter)->Which() == CSeq_loc::e_Int) {
            if( (*LocIter)->GetInt().GetStrand() == eNa_strand_plus)
                PosCoveredBases += (*LocIter)->GetInt().GetLength();
            else
                NegCoveredBases += (*LocIter)->GetInt().GetLength();
        }
    }
    //cerr << "Covered: " << PosCoveredBases << " or " << NegCoveredBases
    //     << "  len: " << m_Scope->GetBioseqHandle(*Merged->GetId()).GetBioseqLength() << endl;

    return max(PosCoveredBases, NegCoveredBases);
}




TAlignResultsRef CSimpleBandedAligner::GenerateAlignments(CRef<objects::CScope> Scope,
                                                    CRef<ISequenceSet> QuerySet,
                                                    CRef<ISequenceSet> SubjectSet,
                                                    TAlignResultsRef AccumResults)
{
    TAlignResultsRef NewResults(new CAlignResultsSet);

    ITERATE(CAlignResultsSet::TQueryToSubjectSet, QueryIter, AccumResults->Get()) {

        int BestRank = QueryIter->second->GetBestRank();
        if(BestRank > m_Threshold || BestRank == -1) {
                ERR_POST(Info << "Determined ID: "
                          << QueryIter->second->GetQueryId()->AsFastaString()
                          << " needs Instanced NW Aligner.");
            x_RunBanded(Scope, QueryIter->second, NewResults);
        }
/*        CAlignResultsSet::TQueryToSubjectSet::iterator ResultsFound;
        ResultsFound = GoodResults->Get().find(QueryIter->first);
        // No results for this query survived the filter, run banded for it
        if(ResultsFound == GoodResults->Get().end()) {
            ERR_POST(Info << "Determined ID: "
                          << QueryIter->second->GetQueryId()->AsFastaString()
                          << " needs Instanced NW Aligner.");
            x_RunBanded(Scope, QueryIter->second, NewResults);
        }*/
    }

    return NewResults;
}




void CSimpleBandedAligner::x_RunBanded(CRef<objects::CScope> Scope,
                                CRef<CQuerySet> QueryAligns,
                                TAlignResultsRef Results)
{

    unsigned int Diag = 0;
    unsigned int DiagFindingWindow = 200;
    ENa_strand Strand;

    TAlignSetRef HighestCoverageSet;
    TSeqPos HighestCoverage = 0;
    CSeq_id Query, Subject;
    Query.Assign(*QueryAligns->GetQueryId());

    NON_CONST_ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, QueryAligns->Get()) {
        TSeqPos CurrCoverage;
        CurrCoverage = s_CalcCoverageCount(SubjectIter->second, 0, *Scope);
        if(CurrCoverage > HighestCoverage) {
            HighestCoverage = CurrCoverage;
            HighestCoverageSet = SubjectIter->second;
            Subject.Assign(SubjectIter->second->Get().front()->GetSeq_id(1));
        }
    }

    if(HighestCoverageSet.IsNull()) {
        ERR_POST(Warning << "No decent coverage set found, to run Banded on " << Query.AsFastaString());
        return;
    } else {
        ERR_POST(Info << "Found Subject " << Subject.AsFastaString()
                      << " with HighestCoverage of " << HighestCoverage
                      << " from " << HighestCoverageSet->Get().size() << " alignments.");
    }

    CContigAssembly::FindDiagFromAlignSet(*HighestCoverageSet, *Scope,
                                        DiagFindingWindow,
                                        Strand, Diag);
    ERR_POST(Info << "Banded plans to use Strand: " << Strand << " and Diag: " << Diag);


    CRef<CDense_seg> GlobalDs;
    GlobalDs = x_RunBandedGlobal(//CContigAssembly::BandedGlobalAlignment
                Query, Subject, Strand, Diag, m_BandWidth/2, *Scope);

    CRef<CDense_seg> LocalDs;
    LocalDs = CContigAssembly::BestLocalSubAlignment(*GlobalDs, *Scope);

    CRef<CSeq_align> Result(new CSeq_align);
    Result->SetSegs().SetDenseg().Assign(*LocalDs);
    Result->SetType() = CSeq_align::eType_partial;

    CRef<CSeq_align_set> ResultSet(new CSeq_align_set);
    ResultSet->Set().push_back(Result);
    //ERR_POST(Info << "Banded Result: " << MSerial_AsnText << *ResultSet);
    //cerr << "Info: " << "Banded Result: " << MSerial_AsnText << *ResultSet;

    Results->Insert(CRef<CQuerySet>(new CQuerySet(*ResultSet)));

}


CRef<CDense_seg> CSimpleBandedAligner::x_RunBandedGlobal(CSeq_id& QueryId,
                                               CSeq_id& SubjectId,
                                               ENa_strand Strand,
                                               TSeqPos Diagonal,
                                               int BandHalfWidth,
                                               CScope& Scope)
{

    // ASSUMPTION: Query is short, Subject is long.
    //     We should be able to align all of Query against a subsection of Subject
    // Banded Memory useage is Longer Sequence * Band Width * unit size (1)
    // The common case is that Subject will be a very sizeable Scaffold.
    //  a bandwidth of anything more than 10-500 can pop RAM on a 16G machine.
    //  So we're gonna snip Subject to a subregion

    CBioseq_Handle::EVectorStrand VecStrand;
    VecStrand = (Strand == eNa_strand_plus ? CBioseq_Handle::eStrand_Plus
                                           : CBioseq_Handle::eStrand_Minus);

    CBioseq_Handle QueryHandle, SubjectHandle;
    QueryHandle = Scope.GetBioseqHandle(QueryId);
    SubjectHandle = Scope.GetBioseqHandle(SubjectId);

    CSeqVector QueryVec, SubjectVec;
    QueryVec = QueryHandle.GetSeqVector(CBioseq_Handle::eCoding_Iupac, VecStrand);
    SubjectVec = SubjectHandle.GetSeqVector(CBioseq_Handle::eCoding_Iupac);

    unsigned int SubjectLengthConst = 3;
    unsigned int SubjectHalfLength = SubjectLengthConst * QueryVec.size();
    TSeqPos SubjectStart = ((Diagonal > SubjectHalfLength)
                         ?  (Diagonal - SubjectHalfLength) : 0);
    TSeqPos SubjectStop  = (((Diagonal + SubjectHalfLength) > SubjectVec.size())
                         ?  SubjectVec.size() : (Diagonal + SubjectHalfLength));

    string QuerySeq, SubjectSeq;
    QueryVec.GetSeqData(0, QueryVec.size(), QuerySeq);
    SubjectVec.GetSeqData(SubjectStart, SubjectStop, SubjectSeq);

    ERR_POST(Info << "Banded Aligning: ");
    ERR_POST(Info << "Query 0 to " << QueryVec.size() << " against");
    ERR_POST(Info << "Subject " << SubjectStart << " to " << SubjectStop);
    // We have the sequence strings, now we try and set up CBandAligner
    Uint1 Direction;
    size_t Shift;

    Diagonal -= SubjectStart;

    if(Diagonal <= QuerySeq.size()) {
        Direction = 0;
        Shift = QuerySeq.size() - 1 - Diagonal;
    } else {
        Direction = 1;
        Shift = Diagonal - (QuerySeq.size() - 1);
    }


    CBandAligner Banded(QuerySeq, SubjectSeq, 0, BandHalfWidth);
    Banded.SetEndSpaceFree(true, true, true, true);
    Banded.SetWms(-3);
    Banded.SetWg(-1);
    Banded.SetWs(-1);
    Banded.SetScoreMatrix(NULL);
    Banded.SetShift(Direction, Shift);
    while(true) {
        Banded.SetBand(BandHalfWidth);
        try {
            //cerr << "Trying BandHalfWidth: " << BandHalfWidth << endl;
            Banded.Run();
            break;
        } catch(CException& e) {
            BandHalfWidth = (int)(BandHalfWidth * 0.8);
            if(BandHalfWidth < 100) {
                cerr << e.ReportAll() << endl;
                throw e;
            }
        }
    }


    // Extract Results
    TSeqPos QueryStart = ((Strand == eNa_strand_plus) ? 0 : (QuerySeq.size() - 1));
    CRef<CDense_seg> ResultDenseg;
    ResultDenseg = Banded.GetDense_seg(QueryStart, Strand, QueryId,
                                       0, eNa_strand_plus, SubjectId);


    ResultDenseg->TrimEndGaps();

    if(ResultDenseg->GetNumseg() > 0)
        ResultDenseg->OffsetRow(1, SubjectStart);


    return ResultDenseg;
}


///////



TAlignResultsRef CInstancedAligner::GenerateAlignments(CRef<objects::CScope> Scope,
                                                    CRef<ISequenceSet> QuerySet,
                                                    CRef<ISequenceSet> SubjectSet,
                                                    TAlignResultsRef AccumResults)
{
    TAlignResultsRef NewResults(new CAlignResultsSet);

    ITERATE(CAlignResultsSet::TQueryToSubjectSet, QueryIter, AccumResults->Get()) {
        int BestRank = QueryIter->second->GetBestRank();
        if(BestRank > m_Threshold || BestRank == -1) {
            ERR_POST(Info << "Determined ID: "
                          << QueryIter->second->GetQueryId()->AsFastaString()
                          << " needs Instanced MM Aligner.");
            x_RunAligner(Scope, QueryIter->second, NewResults);
        }
    }

    return NewResults;
}




void CInstancedAligner::x_RunAligner(CRef<objects::CScope> Scope,
                                     CRef<CQuerySet> QueryAligns,
                                     TAlignResultsRef Results)
{
    CRef<CSeq_align_set> ResultSet(new CSeq_align_set);

    CRef<CSeq_align_set> Instances = x_GetInstances(QueryAligns, *Scope);
    if(Instances->Get().empty()) {
        ERR_POST(Info << " No instances of " << QueryAligns->GetQueryId()->AsFastaString() << " found.");
    }


    TSeqPos LongestInstance = 0;
    ITERATE(CSeq_align_set::Tdata, InstIter, Instances->Get()) {
        const CSeq_align& Inst = **InstIter;
        TSeqPos CurrLength = (Inst.GetSeqStop(0) - Inst.GetSeqStart(0));
        LongestInstance = max(CurrLength, LongestInstance);
    }

    CRef<CSeq_align> Result(new CSeq_align);
    ITERATE(CSeq_align_set::Tdata, InstIter, Instances->Get()) {

        const CSeq_align& Inst = **InstIter;

        if( (Inst.GetSeqStop(0) - Inst.GetSeqStart(0)) <= (LongestInstance*0.5)) {
            continue;
        }

        ERR_POST(Info << " Aligning " << Inst.GetSeq_id(0).AsFastaString() << " to "
                                     << Inst.GetSeq_id(1).AsFastaString());
        ERR_POST(Info << "  q:" << Inst.GetSeqStart(0) << ":"
                                << Inst.GetSeqStop(0) << ":"
                                << (Inst.GetSeqStrand(0) == eNa_strand_plus ? "+" : "-")
                      << " and s: " << Inst.GetSeqStart(1) << ":"
                                    << Inst.GetSeqStop(1) << ":"
                                    << (Inst.GetSeqStrand(1) == eNa_strand_plus ? "+" : "-"));

        CRef<CDense_seg> GlobalDs;
        try {
            GlobalDs = x_RunMMGlobal(
                            Inst.GetSeq_id(0), Inst.GetSeq_id(1),
                            Inst.GetSeqStrand(0),
                            Inst.GetSeqStart(0), Inst.GetSeqStop(0),
                            Inst.GetSeqStart(1), Inst.GetSeqStop(1),
                            *Scope);
        } catch(CException& e) {
            ERR_POST(Error << e.ReportAll());
            ERR_POST(Error << "CInstancedBandedAligner failed.");
            continue;
        }

        if(GlobalDs.IsNull())
            continue;

        Result->SetSegs().SetDenseg().Assign(*GlobalDs);
        Result->SetType() = CSeq_align::eType_partial;
        ResultSet->Set().push_back(Result);

/*
        // CContigAssembly::BestLocalSubAlignmentrescores, and finds better-scoring
        // sub-regions of the Denseg, and keeps only the subregion.
        // The practical result is, if the alignment has a large gap near the edges
        //  that gap might be trimmed. If there is more matching region past the
        //  too-big gap, that matching region is lost. Sometimes we don't want that
        //  especially in the Instance case, because we trust the region so much
        // So, (see below) if this function changes the Denseg, we return both, the
        //  trimmed and the untrimmed.
        CRef<CDense_seg> LocalDs;
        LocalDs = CContigAssembly::BestLocalSubAlignment(*GlobalDs, *Scope);

        Result->SetSegs().SetDenseg().Assign(*LocalDs);
        Result->SetType() = CSeq_align::eType_partial;
        ResultSet->Set().push_back(Result);

        if(!LocalDs->Equals(*GlobalDs)) {
            CRef<CSeq_align> Result(new CSeq_align);
            Result->SetSegs().SetDenseg().Assign(*GlobalDs);
            Result->SetType() = CSeq_align::eType_partial;
            ResultSet->Set().push_back(Result);
        }
*/
    }

    if(!ResultSet->Get().empty()) {
        Results->Insert(CRef<CQuerySet>(new CQuerySet(*ResultSet)));
    }
}




CRef<CDense_seg> CInstancedAligner::x_RunMMGlobal(const CSeq_id& QueryId,
                                                    const CSeq_id& SubjectId,
                                                    ENa_strand Strand,
                                                    TSeqPos QueryStart,
                                                    TSeqPos QueryStop,
                                                    TSeqPos SubjectStart,
                                                    TSeqPos SubjectStop,
                                                    CScope& Scope)
{

    // ASSUMPTION: Query is short, Subject is long.
    //     We should be able to align all of Query against a subsection of Subject
    // Banded Memory useage is Longer Sequence * Band Width * unit size (1)
    // The common case is that Subject will be a very sizeable Scaffold.
    //  a bandwidth of anything more than 10-500 can pop RAM on a 16G machine.
    //  So we're gonna snip Subject to a subregion
    // CMMAligner uses linear RAM, but we still use the restricted region because
    //  we know where the regions are.

    CBioseq_Handle::EVectorStrand VecStrand;
    VecStrand = (Strand == eNa_strand_plus ? CBioseq_Handle::eStrand_Plus
                                           : CBioseq_Handle::eStrand_Minus);

    CBioseq_Handle QueryHandle, SubjectHandle;
    QueryHandle = Scope.GetBioseqHandle(QueryId);
    SubjectHandle = Scope.GetBioseqHandle(SubjectId);

    CSeqVector QueryVec, SubjectVec;
    QueryVec = QueryHandle.GetSeqVector(CBioseq_Handle::eCoding_Iupac, VecStrand);
    SubjectVec = SubjectHandle.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
/*
    int Wiggle = 0;

    QueryStart = ( QueryStart > Wiggle ? QueryStart-Wiggle : 0);
    QueryStop = ( QueryStop + Wiggle < QueryVec.size() ? QueryStop+Wiggle : QueryVec.size()-1);

    SubjectStart = ( SubjectStart > Wiggle ? SubjectStart-Wiggle : 0);
    SubjectStop = ( SubjectStop + Wiggle < SubjectVec.size() ? SubjectStop+Wiggle : SubjectVec.size()-1);
*/

    // CSeqVector, when making a minus vector, not only compliments the bases,
    // flips the order to. But the start and stops provided are in Plus space.
    // So we need to swap them to Minus space
    TSeqPos QueryStrandedStart, QueryStrandedStop;
    if(Strand == eNa_strand_plus) {
        QueryStrandedStart = QueryStart;
        QueryStrandedStop = QueryStop;
    } else {
        QueryStrandedStart = ( (QueryVec.size()-1) - QueryStop);
        QueryStrandedStop = ( (QueryVec.size()-1) - QueryStart);
    }

    string QuerySeq, SubjectSeq;
    QueryVec.GetSeqData(QueryStrandedStart, QueryStrandedStop+1, QuerySeq);
    SubjectVec.GetSeqData(SubjectStart, SubjectStop+1, SubjectSeq);

/*  ERR_POST(Info << " Query " << QueryStart << " to " << QueryStop << " against");
    ERR_POST(Info << " QStranded " << QueryStrandedStart << " to " << QueryStrandedStop << " against");
    ERR_POST(Info << " Subject " << SubjectStart << " to " << SubjectStop);
*/

    TSeqPos ExtractQueryStart = ((Strand == eNa_strand_plus) ? 0 : QuerySeq.size()-1);
    CRef<CDense_seg> ResultDenseg;

    CMMAligner Aligner(QuerySeq, SubjectSeq);
    Aligner.SetWm(2);
    Aligner.SetWms(-3);
    Aligner.SetWg(-100);
    Aligner.SetWs(-1);
    Aligner.SetScoreMatrix(NULL);
    try {
        int Score;
        Score = Aligner.Run();
        if(Score == numeric_limits<int>::min()) {
            return CRef<CDense_seg>();
        }
        ResultDenseg = Aligner.GetDense_seg(ExtractQueryStart, Strand, QueryId,
                                            0, eNa_strand_plus, SubjectId);
    } catch(CException& e) {
        ERR_POST(Error << "MMAligner: " << e.ReportAll());
        return CRef<CDense_seg>();
    }


    if(ResultDenseg->GetNumseg() > 0) {
        ResultDenseg->OffsetRow(0, QueryStart);
        ResultDenseg->OffsetRow(1, SubjectStart);

        // TrimEndGaps Segfaults on the only one segment which is a gap case
        if(ResultDenseg->GetNumseg() == 1 &&
           (ResultDenseg->GetStarts()[0] == -1 || ResultDenseg->GetStarts()[1] == -1)) {
            return CRef<CDense_seg>();
        } else {
            ResultDenseg->TrimEndGaps();
        }

    } else {
        return CRef<CDense_seg>();
    }

    return ResultDenseg;
}



CRef<CSeq_align_set> CInstancedAligner::x_GetInstances(CRef<CQuerySet> QueryAligns, CScope& Scope)
{
    CRef<CSeq_align_set> Instances(new CSeq_align_set);
    CAlignCleanup Cleaner(Scope);
    //Cleaner.FillUnaligned(true);

    NON_CONST_ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, QueryAligns->Get()) {

        CRef<CSeq_align_set> Set = SubjectIter->second;
        list<CConstRef<CSeq_align> > In;
        ITERATE(CSeq_align_set::Tdata, AlignIter, Set->Get()) {
            CConstRef<CSeq_align> Align(*AlignIter);
            In.push_back(Align);
        }

        CRef<CSeq_align_set> Out(new CSeq_align_set);

        try {
            Cleaner.Cleanup(In, Out->Set());
        } catch(CException& e) {
            ERR_POST(Info << "Cleanup Error: " << e.ReportAll());
            break;
        }

        NON_CONST_ITERATE(CSeq_align_set::Tdata, AlignIter, Out->Set()) {
            CDense_seg& Denseg = (*AlignIter)->SetSegs().SetDenseg();

            if(!Denseg.CanGetStrands() || Denseg.GetStrands().empty()) {
                Denseg.SetStrands().resize(Denseg.GetDim()*Denseg.GetNumseg(), eNa_strand_plus);
            }

            if(Denseg.GetSeqStrand(1) != eNa_strand_plus) {
                Denseg.Reverse();
            }
            //CRef<CDense_seg> Filled = Denseg.FillUnaligned();
            //Denseg.Assign(*Filled);
        }

        ITERATE(CSeq_align_set::Tdata, AlignIter, Out->Set()) {
            Instances->Set().push_back(*AlignIter);
        }
    }

    return Instances;
}


