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
 * Authors:  Josh Cherry
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <objmgr/object_manager.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/scope.hpp>
//#include <algo/seqqa/seqtest.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>

#include <util/line_reader.hpp>

#include <algo/align/ngalign/ngalign.hpp>
#include <algo/align/ngalign/alignment_filterer.hpp>
#include <algo/align/ngalign/sequence_set.hpp>
#include <algo/align/ngalign/blast_aligner.hpp>
#include <algo/align/ngalign/banded_aligner.hpp>
#include <algo/align/ngalign/merge_aligner.hpp>
#include <algo/align/ngalign/overlap_aligner.hpp>
#include <algo/align/ngalign/inversion_merge_aligner.hpp>
#include <algo/align/ngalign/alignment_scorer.hpp>

#include <algo/align/ngalign/unordered_spliter.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


class CNgAlignTest : public CNcbiApplication
{
private:
    void Init();
    int Run();

    void x_OneToOneCase(IRegistry* TestCases, const string& Case);
    void x_OneToBlastDbCase(IRegistry* TestCases, const string& Case);
    void x_ListToBlastDbCase(IRegistry* TestCases, const string& Case);
    void x_SplitOneToBlastDbCase(IRegistry* TestCases, const string& Case);
    void x_SplitListToBlastDbCase(IRegistry* TestCases, const string& Case);
    void x_OverlapOneToBlastDbCase(IRegistry* TestCases, const string& Case);
    void x_OverlapListToBlastDbCase(IRegistry* TestCases, const string& Case);


    CRef<CScope> m_Scope;

    bool x_CompareAlignSets(CRef<CSeq_align_set> New, CRef<CSeq_align_set> Ref);
};


void CNgAlignTest::Init()
{
    auto_ptr<CArgDescriptions> arg_descr(new CArgDescriptions);
    arg_descr->SetUsageContext(GetArguments().GetProgramName(),
                               "Demo application testing "
                               "xuberalign library");

    arg_descr->AddDefaultKey("blast_params", "parameter_string",
                             "Similar to what would be given to "
                             "command-line blast (single quotes respected)",
                             CArgDescriptions::eString, " -evalue 1e-5");

/*    arg_descr->AddDefaultKey("bandwidth", "bandwidth",
                             "list of bandwidths to try in banded alignment",
                             CArgDescriptions::eString, "401");
    arg_descr->AddDefaultKey("min_frac_ident", "fraction",
                             "minumum acceptable fraction identity",
                             CArgDescriptions::eDouble, "0.985");
    arg_descr->AddDefaultKey("max_end_slop", "base_pairs",
                             "maximum allowable unaligned bases at ends",
                             CArgDescriptions::eInteger, "10");
*/
    SetupArgDescriptions(arg_descr.release());
}


int CNgAlignTest::Run()
{
    //const CArgs& args = GetArgs();

    CRef<CObjectManager> obj_mgr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*obj_mgr);
    m_Scope.Reset(new CScope(*obj_mgr));
    m_Scope->AddDefaults();


    //string TestCaseIni = "/home/boukn/code/toolkit/c++/src/algo/align/uberalign/test/testcases.ini";
    string TestCaseIni = "/panfs/pan1.be-md.ncbi.nlm.nih.gov/genome_maint/work/uberalign/test/testcases.ini";

    CNcbiIfstream TestCaseFile(TestCaseIni.c_str());
    CNcbiRegistry TestCases(TestCaseFile);

    string CasesStr = TestCases.Get("caselist", "cases");
    list<string> Cases;
    NStr::Split(CasesStr, " \t\r\n,", Cases);

    ITERATE(list<string>, CaseIter, Cases) {

        string Type = TestCases.Get(*CaseIter, "type");
        if(Type == "one_to_one") {
            x_OneToOneCase(&TestCases, *CaseIter);
        } else if(Type == "one_to_blastdb") {
            x_OneToBlastDbCase(&TestCases, *CaseIter);
        } else if(Type == "list_to_blastdb") {
            x_ListToBlastDbCase(&TestCases, *CaseIter);
        } else if(Type == "split_one_to_blastdb") {
            x_SplitOneToBlastDbCase(&TestCases, *CaseIter);
        } else if(Type == "split_list_to_blastdb") {
            x_SplitListToBlastDbCase(&TestCases, *CaseIter);
        } else if(Type == "overlap_one_to_blastdb") {
            x_OverlapOneToBlastDbCase(&TestCases, *CaseIter);
        } else if(Type == "overlap_list_to_blastdb") {
            x_OverlapListToBlastDbCase(&TestCases, *CaseIter);
        }

    }


    return 0;
}


void CNgAlignTest::x_OneToOneCase(IRegistry* TestCases, const string& Case)
{
    string QueryIdStr, SubjectIdStr;
    QueryIdStr = TestCases->Get(Case, "query");
    SubjectIdStr = TestCases->Get(Case, "subject");
    CRef<CSeq_id> QueryId(new CSeq_id(QueryIdStr));
    CRef<CSeq_id> SubjectId(new CSeq_id(SubjectIdStr));

    int FilterCount = TestCases->GetInt("consts", "filter_count", 0);
    list<string> Filters;
    for(int Loop = 0; Loop < FilterCount; Loop++) {
        string Curr = TestCases->Get("consts", "filter"+NStr::IntToString(Loop));
        Filters.push_back(Curr);
    }
    string NMerFile = TestCases->Get("consts", "nmer");
    string Operation = TestCases->Get(Case, "operation");
    list<string> BlastParams;
    BlastParams.push_back(TestCases->Get("consts", "blast"));
    BlastParams.push_back(TestCases->Get("consts", "blast2"));
 //   BlastParams.push_back(TestCases->Get("consts", "blast3"));

    if(Operation == "skip")
        return;

    auto_ptr<CSeqMasker> SeqMasker;
    if(!NMerFile.empty()) {
        SeqMasker.reset(new CSeqMasker(NMerFile,
            0, 1, 1, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, 0, "mean", 0, false, 0, false));
    }

    CRef<CSeqIdListSet> Query(new CSeqIdListSet);
    CRef<CSeqIdListSet> Subject(new CSeqIdListSet);
    list<CRef<CQueryFilter> > QueryFilters;
    int Rank = 0;
    ITERATE(list<string>, StrIter, Filters) {
        CRef<CQueryFilter> CurrFilter(new CQueryFilter(Rank, *StrIter));
        Rank++;
        QueryFilters.push_back(CurrFilter);
    }
    list<CRef<CBlastAligner> > BlastAligners;
    BlastAligners = CBlastAligner::CreateBlastAligners(BlastParams, 1);
    CRef<CInstancedAligner> InstancedAligner(new CInstancedAligner(300, 3.0, 1.0, 1));
    CRef<CMergeAligner> MergeAligner(new CMergeAligner(1));
    CRef<CInversionMergeAligner> InversionMergeAligner(new CInversionMergeAligner(1));

    CRef<CBlastScorer> BlastScorer(
        new CBlastScorer(CBlastScorer::eSkipUnsupportedAlignments));
    CRef<CPctIdentScorer> PctIdentScorer(new CPctIdentScorer);
    CRef<CPctCoverageScorer> PctCoverageScorer(new CPctCoverageScorer);
    CRef<CCommonComponentScorer> CommonComponentScorer(new CCommonComponentScorer);
   // CRef<CTailsScorer> TailsScorer(new CTailsScorer);


    Query->SetIdList().push_back(QueryId);
    Subject->SetIdList().push_back(SubjectId);

    if(SeqMasker.get() != NULL) {
        Query->SetSeqMasker(SeqMasker.get());
        Subject->SetSeqMasker(SeqMasker.get());
    }

    CNgAligner Aligner(*m_Scope);
    Aligner.SetQuery(CRef<ISequenceSet>(Query.GetPointer()));
    Aligner.SetSubject(CRef<ISequenceSet>(Subject.GetPointer()));

    NON_CONST_ITERATE(list<CRef<CQueryFilter> >, FilterIter, QueryFilters) {
        Aligner.AddFilter(&**FilterIter);
    }

//    Aligner.AddAligner(CRef<IAlignmentFactory>(BlastListAligner.GetPointer()));
    NON_CONST_ITERATE(list<CRef<CBlastAligner> >, BlastIter, BlastAligners) {
        (*BlastIter)->SetSoftFiltering(30);
        Aligner.AddAligner(&**BlastIter);
    }
//    Aligner.AddAligner(InstancedAligner.GetPointer());
    Aligner.AddAligner(MergeAligner.GetPointer());
    Aligner.AddAligner(InversionMergeAligner.GetPointer());

    Aligner.AddScorer(CRef<IAlignmentScorer>(BlastScorer.GetPointer()));
    Aligner.AddScorer(CRef<IAlignmentScorer>(PctIdentScorer.GetPointer()));
    Aligner.AddScorer(CRef<IAlignmentScorer>(PctCoverageScorer.GetPointer()));
    Aligner.AddScorer(CRef<IAlignmentScorer>(CommonComponentScorer.GetPointer()));
  //  Aligner.AddScorer(TailsScorer.GetPointer());

    CRef<CSeq_align_set> AlignSet;
    AlignSet = Aligner.Align();

    if(Operation == "save") {
        string SaveFileName = TestCases->Get(Case, "savefile");
        if(!SaveFileName.empty()) {
            SaveFileName = TestCases->Get("consts", "casedir") + "/" + SaveFileName;
            CNcbiOfstream Out(SaveFileName.c_str());
            Out << MSerial_AsnText << *AlignSet;
        }
    } else if(Operation == "comp") {
        string CompFileName = TestCases->Get(Case, "compfile");
        if(!CompFileName.empty()) {
            CompFileName = TestCases->Get("consts", "casedir") + "/" + CompFileName;
            CNcbiIfstream In(CompFileName.c_str());
            CRef<CSeq_align_set> CompSet(new CSeq_align_set);
            In >> MSerial_AsnText >> *CompSet;
            if(CompSet->Get().empty() || AlignSet->Get().empty()) {
                cout << "Case: " << Case << " Empty Set, can not compare." << endl;
            }
            bool Match = x_CompareAlignSets(AlignSet, CompSet);
            cout << "Case: " << Case << " Comps: " << (Match ? "True" : "False") << endl;
        }
    } else if(Operation == "print") {
        cout << MSerial_AsnText << *AlignSet;
    }
}




void CNgAlignTest::x_OneToBlastDbCase(IRegistry* TestCases, const string& Case)
{
    string QueryIdStr, BlastDbStr;
    QueryIdStr = TestCases->Get(Case, "query");
    BlastDbStr = TestCases->Get(Case, "blastdb");
    CRef<CSeq_id> QueryId(new CSeq_id(QueryIdStr));

    int FilterCount = TestCases->GetInt("consts", "filter_count", 0);
    list<string> Filters;
    for(int Loop = 0; Loop < FilterCount; Loop++) {
        string Curr = TestCases->Get("consts", "filter"+NStr::IntToString(Loop));
        Filters.push_back(Curr);
    }
    string NMerFile = TestCases->Get("consts", "nmer");
    string Operation = TestCases->Get(Case, "operation");
    list<string> BlastParams;
    BlastParams.push_back(TestCases->Get("consts", "blast"));
    BlastParams.push_back(TestCases->Get("consts", "blast2"));
 //   BlastParams.push_back(TestCases->Get("consts", "blast3"));

    if(Operation == "skip")
        return;

    auto_ptr<CSeqMasker> SeqMasker;
    if(!NMerFile.empty()) {
        SeqMasker.reset(new CSeqMasker(NMerFile,
            0, 1, 1, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, 0, "mean", 0, false, 0, false));
    }

    CRef<CSeqIdListSet> Query(new CSeqIdListSet);
    CRef<CBlastDbSet> Subject(new CBlastDbSet(BlastDbStr));
    list<CRef<CQueryFilter> > QueryFilters;
    int Rank = 0;
    ITERATE(list<string>, StrIter, Filters) {
        CRef<CQueryFilter> CurrFilter(new CQueryFilter(Rank, *StrIter));
        Rank++;
        QueryFilters.push_back(CurrFilter);
    }
    list<CRef<CBlastAligner> > BlastAligners;
    BlastAligners = CBlastAligner::CreateBlastAligners(BlastParams, 1);
    CRef<CInstancedAligner> InstancedAligner(new CInstancedAligner(300, 3.0, 1.0, 1));
    CRef<CMergeAligner> MergeAligner(new CMergeAligner(1));
    CRef<CInversionMergeAligner> InversionMergeAligner(new CInversionMergeAligner(1));

    CRef<CBlastScorer> BlastScorer(
        new CBlastScorer(CBlastScorer::eSkipUnsupportedAlignments));
    CRef<CPctIdentScorer> PctIdentScorer(new CPctIdentScorer);
    CRef<CPctCoverageScorer> PctCoverageScorer(new CPctCoverageScorer);
    CRef<CCommonComponentScorer> CommonComponentScorer(new CCommonComponentScorer);


    Query->SetIdList().push_back(QueryId);

    if(SeqMasker.get() != NULL) {
    //    Query->SetSeqMasker(SeqMasker.get());
    //    Subject->SetSoftFiltering().push_back(1);
    }

    Subject->SetSoftFiltering(30);


    CNgAligner Aligner(*m_Scope);
    Aligner.SetQuery(CRef<ISequenceSet>(Query.GetPointer()));
    Aligner.SetSubject(CRef<ISequenceSet>(Subject.GetPointer()));

    NON_CONST_ITERATE(list<CRef<CQueryFilter> >, FilterIter, QueryFilters) {
        Aligner.AddFilter(&**FilterIter);
    }

//    Aligner.AddAligner(CRef<IAlignmentFactory>(BlastListAligner.GetPointer()));
    NON_CONST_ITERATE(list<CRef<CBlastAligner> >, BlastIter, BlastAligners) {
        (*BlastIter)->SetSoftFiltering(30);
        Aligner.AddAligner(&**BlastIter);
    }
    Aligner.AddAligner(MergeAligner.GetPointer());
    Aligner.AddAligner(InversionMergeAligner.GetPointer());
   // Aligner.AddAligner(InstancedAligner.GetPointer());

    Aligner.AddScorer(CRef<IAlignmentScorer>(BlastScorer.GetPointer()));
    Aligner.AddScorer(CRef<IAlignmentScorer>(PctIdentScorer.GetPointer()));
    Aligner.AddScorer(CRef<IAlignmentScorer>(PctCoverageScorer.GetPointer()));
    Aligner.AddScorer(CRef<IAlignmentScorer>(CommonComponentScorer.GetPointer()));

    CRef<CSeq_align_set> AlignSet;
    AlignSet = Aligner.Align();

    if(Operation == "save") {
        string SaveFileName = TestCases->Get(Case, "savefile");
        if(!SaveFileName.empty()) {
            SaveFileName = TestCases->Get("consts", "casedir") + "/" + SaveFileName;
            CNcbiOfstream Out(SaveFileName.c_str());
            Out << MSerial_AsnText << *AlignSet;
        }
    } else if(Operation == "comp") {
        string CompFileName = TestCases->Get(Case, "compfile");
        if(!CompFileName.empty()) {
            CompFileName = TestCases->Get("consts", "casedir") + "/" + CompFileName;
            CNcbiIfstream In(CompFileName.c_str());
            CRef<CSeq_align_set> CompSet(new CSeq_align_set);
            In >> MSerial_AsnText >> *CompSet;
            if(CompSet->Get().empty() || AlignSet->Get().empty()) {
                cout << "Case: " << Case << " Empty Set, can not compare." << endl;
            }
            bool Match = x_CompareAlignSets(AlignSet, CompSet);
            cout << "Case: " << Case << " Comps: " << (Match ? "True" : "False") << endl;
        }
    } else if(Operation == "print") {
        cout << MSerial_AsnText << *AlignSet;
    }
}




void CNgAlignTest::x_ListToBlastDbCase(IRegistry* TestCases, const string& Case)
{
    string QueryIdFile, BlastDbStr;
    QueryIdFile = TestCases->Get(Case, "queryfile");
    BlastDbStr = TestCases->Get(Case, "blastdb");

    int FilterCount = TestCases->GetInt("consts", "filter_count", 0);
    list<string> Filters;
    for(int Loop = 0; Loop < FilterCount; Loop++) {
        string Curr = TestCases->Get("consts", "filter"+NStr::IntToString(Loop));
        Filters.push_back(Curr);
    }
    string NMerFile = TestCases->Get("consts", "nmer");
    string Operation = TestCases->Get(Case, "operation");
    list<string> BlastParams;
    BlastParams.push_back(TestCases->Get("consts", "blast"));
//    BlastParams.push_back(TestCases->Get("consts", "blast2"));
//    BlastParams.push_back(TestCases->Get("consts", "blast3"));

    if(Operation == "skip")
        return;

    auto_ptr<CSeqMasker> SeqMasker;
    if(!NMerFile.empty()) {
        SeqMasker.reset(new CSeqMasker(NMerFile,
            0, 1, 1, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, 0, "mean", 0, false, 0, false));
    }

    CRef<CSeqIdListSet> Query(new CSeqIdListSet);
    CRef<CBlastDbSet> Subject(new CBlastDbSet(BlastDbStr));
    list<CRef<CQueryFilter> > QueryFilters;
    int Rank = 0;
    ITERATE(list<string>, StrIter, Filters) {
        CRef<CQueryFilter> CurrFilter(new CQueryFilter(Rank, *StrIter));
        Rank++;
        QueryFilters.push_back(CurrFilter);
    }
    list<CRef<CBlastAligner> > BlastAligners;
    BlastAligners = CBlastAligner::CreateBlastAligners(BlastParams, 1);
    CRef<CInstancedAligner> InstancedAligner(new CInstancedAligner(180, 3.0, 1.0, 0));
    CRef<CMergeAligner> MergeAligner(new CMergeAligner(0));
    CRef<CInversionMergeAligner> InversionMergeAligner(new CInversionMergeAligner(1));

    CRef<CBlastScorer> BlastScorer(
        new CBlastScorer(CBlastScorer::eSkipUnsupportedAlignments));
    CRef<CPctIdentScorer> PctIdentScorer(new CPctIdentScorer);
    CRef<CPctCoverageScorer> PctCoverageScorer(new CPctCoverageScorer);
    CRef<CCommonComponentScorer> CommonComponentScorer(new CCommonComponentScorer);
    CRef<CExpansionScorer> ExpansionScorer(new CExpansionScorer);

    set<string> IdSet;
    {{
        QueryIdFile = TestCases->Get("consts", "casedir") + "/" + QueryIdFile;
        CNcbiIfstream In(QueryIdFile.c_str());
        CStreamLineReader Reader(In);
        while(!Reader.AtEOF()) {
            ++Reader;
            string Line = *Reader;
            if(!Line.empty() && Line[0] != '#') {
                //cerr << "Line: " << Line << endl;
                Line = Line.substr(0, Line.find(" "));
                CRef<CSeq_id> QueryId(new CSeq_id(Line));
                Query->SetIdList().push_back(QueryId);
                IdSet.insert(Line);
                cout << Line << " : " << m_Scope->GetBioseqHandle(*QueryId).GetInst_Length() << endl;
            }
        }
    }}

    if(SeqMasker.get() != NULL) {
        Query->SetSeqMasker(SeqMasker.get());
    }

    Subject->SetSoftFiltering(30);

    CNgAligner Aligner(*m_Scope);
    Aligner.SetQuery(CRef<ISequenceSet>(Query.GetPointer()));
    Aligner.SetSubject(CRef<ISequenceSet>(Subject.GetPointer()));

    NON_CONST_ITERATE(list<CRef<CQueryFilter> >, FilterIter, QueryFilters) {
        Aligner.AddFilter(&**FilterIter);
    }

//    Aligner.AddAligner(CRef<IAlignmentFactory>(BlastListAligner.GetPointer()));
    bool First = true;
    NON_CONST_ITERATE(list<CRef<CBlastAligner> >, BlastIter, BlastAligners) {
        if(First) {
            (*BlastIter)->SetSoftFiltering(30);
            First = false;
        } else {
            (*BlastIter)->SetSoftFiltering(CBlastDbSet::eNoSoftFiltering);
        }
        Aligner.AddAligner(&**BlastIter);
    }
    Aligner.AddAligner(MergeAligner.GetPointer());
    Aligner.AddAligner(InstancedAligner.GetPointer());
    Aligner.AddAligner(InversionMergeAligner.GetPointer());


    Aligner.AddScorer(BlastScorer.GetPointer());
    Aligner.AddScorer(PctIdentScorer.GetPointer());
    Aligner.AddScorer(PctCoverageScorer.GetPointer());
    Aligner.AddScorer(CommonComponentScorer.GetPointer());
    Aligner.AddScorer(ExpansionScorer.GetPointer());

    CRef<CSeq_align_set> AlignSet;
    AlignSet = Aligner.Align();

    // Completion Report
    ITERATE(CSeq_align_set::Tdata, AlignIter, AlignSet->Get()) {
        string CurrIdStr = (*AlignIter)->GetSeq_id(0).AsFastaString();
        set<string>::iterator Found = IdSet.find(CurrIdStr);
        if(Found != IdSet.end()) {
            IdSet.erase(Found);
        }
    }
    ITERATE(set<string> , IdIter, IdSet) {
        ERR_POST(Info << "Query " << *IdIter << " found no alignments.");
    }

    if(Operation == "save") {
        string SaveFileName = TestCases->Get(Case, "savefile");
        if(!SaveFileName.empty()) {
            SaveFileName = TestCases->Get("consts", "casedir") + "/" + SaveFileName;
            CNcbiOfstream Out(SaveFileName.c_str());
            Out << MSerial_AsnText << *AlignSet;
        }
    } else if(Operation == "comp") {
        string CompFileName = TestCases->Get(Case, "compfile");
        if(!CompFileName.empty()) {
            CompFileName = TestCases->Get("consts", "casedir") + "/" + CompFileName;
            CNcbiIfstream In(CompFileName.c_str());
            CRef<CSeq_align_set> CompSet(new CSeq_align_set);
            In >> MSerial_AsnText >> *CompSet;
            if(CompSet->Get().empty() || AlignSet->Get().empty()) {
                cout << "Case: " << Case << " Empty Set, can not compare." << endl;
            }
            bool Match = x_CompareAlignSets(AlignSet, CompSet);
            cout << "Case: " << Case << " Comps: " << (Match ? "True" : "False") << endl;
        }
    } else if(Operation == "print") {
        cout << MSerial_AsnText << *AlignSet;
    }
}



void CNgAlignTest::x_SplitOneToBlastDbCase(IRegistry* TestCases, const string& Case)
{
    string QueryIdStr, BlastDbStr;
    QueryIdStr = TestCases->Get(Case, "query");
    BlastDbStr = TestCases->Get(Case, "blastdb");
    CRef<CSeq_id> QueryId(new CSeq_id(QueryIdStr));

    int FilterCount = TestCases->GetInt("consts", "filter_count", 0);
    list<string> Filters;
    for(int Loop = 0; Loop < FilterCount; Loop++) {
        string Curr = TestCases->Get("consts", "filter"+NStr::IntToString(Loop));
        Filters.push_back(Curr);
    }
    string NMerFile = TestCases->Get("consts", "nmer");
    string Operation = TestCases->Get(Case, "operation");
    list<string> BlastParams;
    BlastParams.push_back(TestCases->Get("consts", "blast"));
    BlastParams.push_back(TestCases->Get("consts", "blast2"));
 //   BlastParams.push_back(TestCases->Get("consts", "blast3"));

    if(Operation == "skip")
        return;

    auto_ptr<CSeqMasker> SeqMasker;
    if(!NMerFile.empty()) {
        SeqMasker.reset(new CSeqMasker(NMerFile,
            0, 1, 1, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, 0, "mean", 0, false, 0, false));
    }

    CUnorderedSplitter Splitter(*m_Scope);

    CRef<CSplitSeqIdListSet> Query(new CSplitSeqIdListSet(&Splitter));
    CRef<CBlastDbSet> Subject(new CBlastDbSet(BlastDbStr));
    list<CRef<CQueryFilter> > QueryFilters;
    int Rank = 0;
    ITERATE(list<string>, StrIter, Filters) {
        CRef<CQueryFilter> CurrFilter(new CQueryFilter(Rank, *StrIter));
        Rank++;
        QueryFilters.push_back(CurrFilter);
    }
    list<CRef<CBlastAligner> > BlastAligners;
    BlastAligners = CBlastAligner::CreateBlastAligners(BlastParams, 1);
    CRef<CInstancedAligner> InstancedAligner(new CInstancedAligner(300, 3.0, 1.0, 1));
    CRef<CMergeAligner> MergeAligner(new CMergeAligner(1));
    CRef<CInversionMergeAligner> InversionMergeAligner(new CInversionMergeAligner(1));
    CRef<CSplitSeqAlignMerger> SplitSeqAlignMerger(new CSplitSeqAlignMerger(&Splitter));

    CRef<CBlastScorer> BlastScorer(
        new CBlastScorer(CBlastScorer::eSkipUnsupportedAlignments));
    CRef<CPctIdentScorer> PctIdentScorer(new CPctIdentScorer);
    CRef<CPctCoverageScorer> PctCoverageScorer(new CPctCoverageScorer);
    CRef<CCommonComponentScorer> CommonComponentScorer(new CCommonComponentScorer);

    Query->AddSeqId(QueryId);

    if(SeqMasker.get() != NULL) {
    //    Query->SetSeqMasker(SeqMasker.get());
    //    Subject->SetSoftFiltering().push_back(1);
    }

    Subject->SetSoftFiltering(30);


    CNgAligner Aligner(*m_Scope);
    Aligner.SetQuery(CRef<ISequenceSet>(Query.GetPointer()));
    Aligner.SetSubject(CRef<ISequenceSet>(Subject.GetPointer()));

    NON_CONST_ITERATE(list<CRef<CQueryFilter> >, FilterIter, QueryFilters) {
        Aligner.AddFilter(&**FilterIter);
    }

//    Aligner.AddAligner(CRef<IAlignmentFactory>(BlastListAligner.GetPointer()));
    NON_CONST_ITERATE(list<CRef<CBlastAligner> >, BlastIter, BlastAligners) {
        (*BlastIter)->SetSoftFiltering(30);
        Aligner.AddAligner(&**BlastIter);
    }
    Aligner.AddAligner(MergeAligner.GetPointer());
   // Aligner.AddAligner(InstancedAligner.GetPointer());
    Aligner.AddAligner(SplitSeqAlignMerger.GetPointer());
    //Aligner.AddAligner(MergeAligner.GetPointer());

//    Aligner.AddAligner(InversionMergeAligner.GetPointer());


    Aligner.AddScorer(CRef<IAlignmentScorer>(BlastScorer.GetPointer()));
    Aligner.AddScorer(CRef<IAlignmentScorer>(PctIdentScorer.GetPointer()));
    Aligner.AddScorer(CRef<IAlignmentScorer>(PctCoverageScorer.GetPointer()));
    Aligner.AddScorer(CRef<IAlignmentScorer>(CommonComponentScorer.GetPointer()));

    CRef<CSeq_align_set> AlignSet;
    AlignSet = Aligner.Align();


    if(Operation == "save") {
        string SaveFileName = TestCases->Get(Case, "savefile");
        if(!SaveFileName.empty()) {
            SaveFileName = TestCases->Get("consts", "casedir") + "/" + SaveFileName;
            CNcbiOfstream Out(SaveFileName.c_str());
            Out << MSerial_AsnText << *AlignSet;
        }
    } else if(Operation == "comp") {
        string CompFileName = TestCases->Get(Case, "compfile");
        if(!CompFileName.empty()) {
            CompFileName = TestCases->Get("consts", "casedir") + "/" + CompFileName;
            CNcbiIfstream In(CompFileName.c_str());
            CRef<CSeq_align_set> CompSet(new CSeq_align_set);
            In >> MSerial_AsnText >> *CompSet;
            if(CompSet->Get().empty() || AlignSet->Get().empty()) {
                cout << "Case: " << Case << " Empty Set, can not compare." << endl;
            }
            bool Match = x_CompareAlignSets(AlignSet, CompSet);
            cout << "Case: " << Case << " Comps: " << (Match ? "True" : "False") << endl;
        }
    } else if(Operation == "print") {
        cout << MSerial_AsnText << *AlignSet;
    }
}




void CNgAlignTest::x_SplitListToBlastDbCase(IRegistry* TestCases, const string& Case)
{
    string QueryIdFile, BlastDbStr;
    QueryIdFile = TestCases->Get(Case, "queryfile");
    BlastDbStr = TestCases->Get(Case, "blastdb");

    int FilterCount = TestCases->GetInt("consts", "filter_count", 0);
    list<string> Filters;
    for(int Loop = 0; Loop < FilterCount; Loop++) {
        string Curr = TestCases->Get("consts", "filter"+NStr::IntToString(Loop));
        Filters.push_back(Curr);
    }
    string NMerFile = TestCases->Get("consts", "nmer");
    string Operation = TestCases->Get(Case, "operation");
    list<string> BlastParams;
    BlastParams.push_back(TestCases->Get("consts", "blast"));
    BlastParams.push_back(TestCases->Get("consts", "blast2"));
 //   BlastParams.push_back(TestCases->Get("consts", "blast3"));

    if(Operation == "skip")
        return;

    auto_ptr<CSeqMasker> SeqMasker;
    if(!NMerFile.empty()) {
        SeqMasker.reset(new CSeqMasker(NMerFile,
            0, 1, 1, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, 0, "mean", 0, false, 0, false));
    }

    CUnorderedSplitter Splitter(*m_Scope);

    CRef<CSplitSeqIdListSet> Query(new CSplitSeqIdListSet(&Splitter));
    CRef<CBlastDbSet> Subject(new CBlastDbSet(BlastDbStr));
    list<CRef<CQueryFilter> > QueryFilters;
    int Rank = 0;
    ITERATE(list<string>, StrIter, Filters) {
        CRef<CQueryFilter> CurrFilter(new CQueryFilter(Rank, *StrIter));
        Rank++;
        QueryFilters.push_back(CurrFilter);
    }
    list<CRef<CBlastAligner> > BlastAligners;
    BlastAligners = CBlastAligner::CreateBlastAligners(BlastParams, 1);
    CRef<CInstancedAligner> InstancedAligner(new CInstancedAligner(300, 3.0, 1.0, 1));
    CRef<CMergeAligner> MergeAligner(new CMergeAligner(1));
    CRef<CInversionMergeAligner> InversionMergeAligner(new CInversionMergeAligner(1));
    CRef<CSplitSeqAlignMerger> SplitSeqAlignMerger(new CSplitSeqAlignMerger(&Splitter));

    CRef<CBlastScorer> BlastScorer(
        new CBlastScorer(CBlastScorer::eSkipUnsupportedAlignments));
    CRef<CPctIdentScorer> PctIdentScorer(new CPctIdentScorer);
    CRef<CPctCoverageScorer> PctCoverageScorer(new CPctCoverageScorer);
    CRef<CCommonComponentScorer> CommonComponentScorer(new CCommonComponentScorer);


    set<string> IdSet;
    {{
        QueryIdFile = TestCases->Get("consts", "casedir") + "/" + QueryIdFile;
        CNcbiIfstream In(QueryIdFile.c_str());
        CStreamLineReader Reader(In);
        while(!Reader.AtEOF()) {
            ++Reader;
            string Line = *Reader;
            if(!Line.empty() && Line[0] != '#') {
                //cerr << "Line: " << Line << endl;
                Line = Line.substr(0, Line.find(" "));
                CRef<CSeq_id> QueryId(new CSeq_id(Line));
                Query->AddSeqId(QueryId);
                IdSet.insert(Line);
                cout << Line << " : " << m_Scope->GetBioseqHandle(*QueryId).GetInst_Length() << endl;
            }
        }
    }}

    if(SeqMasker.get() != NULL) {
    //    Query->SetSeqMasker(SeqMasker.get());
    //    Subject->SetSoftFiltering().push_back(1);
    }

    Subject->SetSoftFiltering(30);


    CNgAligner Aligner(*m_Scope);
    Aligner.SetQuery(CRef<ISequenceSet>(Query.GetPointer()));
    Aligner.SetSubject(CRef<ISequenceSet>(Subject.GetPointer()));

    NON_CONST_ITERATE(list<CRef<CQueryFilter> >, FilterIter, QueryFilters) {
        Aligner.AddFilter(&**FilterIter);
    }

//    Aligner.AddAligner(CRef<IAlignmentFactory>(BlastListAligner.GetPointer()));
    NON_CONST_ITERATE(list<CRef<CBlastAligner> >, BlastIter, BlastAligners) {
        (*BlastIter)->SetSoftFiltering(30);
        Aligner.AddAligner(&**BlastIter);
    }
    Aligner.AddAligner(MergeAligner.GetPointer());
   // Aligner.AddAligner(InstancedAligner.GetPointer());
    Aligner.AddAligner(SplitSeqAlignMerger.GetPointer());
    //Aligner.AddAligner(MergeAligner.GetPointer());

//    Aligner.AddAligner(InversionMergeAligner.GetPointer());


    Aligner.AddScorer(CRef<IAlignmentScorer>(BlastScorer.GetPointer()));
    Aligner.AddScorer(CRef<IAlignmentScorer>(PctIdentScorer.GetPointer()));
    Aligner.AddScorer(CRef<IAlignmentScorer>(PctCoverageScorer.GetPointer()));
    Aligner.AddScorer(CRef<IAlignmentScorer>(CommonComponentScorer.GetPointer()));

    CRef<CSeq_align_set> AlignSet;
    AlignSet = Aligner.Align();


    if(Operation == "save") {
        string SaveFileName = TestCases->Get(Case, "savefile");
        if(!SaveFileName.empty()) {
            SaveFileName = TestCases->Get("consts", "casedir") + "/" + SaveFileName;
            CNcbiOfstream Out(SaveFileName.c_str());
            Out << MSerial_AsnText << *AlignSet;
        }
    } else if(Operation == "comp") {
        string CompFileName = TestCases->Get(Case, "compfile");
        if(!CompFileName.empty()) {
            CompFileName = TestCases->Get("consts", "casedir") + "/" + CompFileName;
            CNcbiIfstream In(CompFileName.c_str());
            CRef<CSeq_align_set> CompSet(new CSeq_align_set);
            In >> MSerial_AsnText >> *CompSet;
            if(CompSet->Get().empty() || AlignSet->Get().empty()) {
                cout << "Case: " << Case << " Empty Set, can not compare." << endl;
            }
            bool Match = x_CompareAlignSets(AlignSet, CompSet);
            cout << "Case: " << Case << " Comps: " << (Match ? "True" : "False") << endl;
        }
    } else if(Operation == "print") {
        cout << MSerial_AsnText << *AlignSet;
    }
}



void CNgAlignTest::x_OverlapOneToBlastDbCase(IRegistry* TestCases, const string& Case)
{
    string QueryIdStr, BlastDbStr;
    QueryIdStr = TestCases->Get(Case, "query");
    BlastDbStr = TestCases->Get(Case, "blastdb");
    CRef<CSeq_id> QueryId(new CSeq_id(QueryIdStr));

    int FilterCount = TestCases->GetInt("consts", "filter_count", 0);
    list<string> Filters;
    for(int Loop = 0; Loop < FilterCount; Loop++) {
        string Curr = TestCases->Get("consts", "filter"+NStr::IntToString(Loop));
        Filters.push_back(Curr);
    }
    string NMerFile = TestCases->Get("consts", "nmer");
    string Operation = TestCases->Get(Case, "operation");
    list<string> BlastParams;
    BlastParams.push_back(TestCases->Get("consts", "blast"));
 //   BlastParams.push_back(TestCases->Get("consts", "blast2"));
 //   BlastParams.push_back(TestCases->Get("consts", "blast3"));

    if(Operation == "skip")
        return;

    auto_ptr<CSeqMasker> SeqMasker;
    if(!NMerFile.empty()) {
        SeqMasker.reset(new CSeqMasker(NMerFile,
            0, 1, 1, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, 0, "mean", 0, false, 0, false));
    }

    CRef<CSeqIdListSet> Query(new CSeqIdListSet);
    CRef<CBlastDbSet> Subject(new CBlastDbSet(BlastDbStr));
    list<CRef<CQueryFilter> > QueryFilters;
    int Rank = 0;
    ITERATE(list<string>, StrIter, Filters) {
        CRef<CQueryFilter> CurrFilter(new CQueryFilter(Rank, *StrIter));
        Rank++;
        QueryFilters.push_back(CurrFilter);
    }
    list<CRef<CBlastAligner> > BlastAligners;
    BlastAligners = CBlastAligner::CreateBlastAligners(BlastParams, 0);
    CRef<CInstancedAligner> InstancedAligner(new CInstancedAligner(300, 3.0, 1.0, 0));
    CRef<CMergeAligner> MergeAligner(new CMergeAligner(-1));
    CRef<COverlapAligner> OverlapAligner(new COverlapAligner(TestCases->Get("consts", "blast2"), 2000, -1));
    CRef<COverlapAligner> ShortOverlapAligner(new COverlapAligner(TestCases->Get("consts", "blast2"), 100, -1));


    CRef<CBlastScorer> BlastScorer(new CBlastScorer);
    CRef<CPctIdentScorer> PctIdentScorer(new CPctIdentScorer);
    CRef<CPctCoverageScorer> PctCoverageScorer(new CPctCoverageScorer);
    CRef<COverlapScorer> OverlapScorer(new COverlapScorer);
    CRef<CExpansionScorer> ExpansionScorer(new CExpansionScorer);


    Query->SetIdList().push_back(QueryId);

    if(SeqMasker.get() != NULL) {
    //    Query->SetSeqMasker(SeqMasker.get());
    //    Subject->SetSoftFiltering().push_back(1);
    }

    Subject->SetSoftFiltering(30);

    //BlastAligners.front()->SetSoftFiltering(30);
    //BlastAligners.back()->SetSoftFiltering(11);

    CNgAligner Aligner(*m_Scope);
    Aligner.SetQuery(CRef<ISequenceSet>(Query.GetPointer()));
    Aligner.SetSubject(CRef<ISequenceSet>(Subject.GetPointer()));

    NON_CONST_ITERATE(list<CRef<CQueryFilter> >, FilterIter, QueryFilters) {
        Aligner.AddFilter(&**FilterIter);
    }

//    Aligner.AddAligner(CRef<IAlignmentFactory>(BlastListAligner.GetPointer()));
    int Count = 0;
    NON_CONST_ITERATE(list<CRef<CBlastAligner> >, BlastIter, BlastAligners) {
        if(Count == 0)
            (*BlastIter)->SetSoftFiltering(30);
        Aligner.AddAligner(&**BlastIter);
        Count++;
    }
    Aligner.AddAligner(MergeAligner.GetPointer());
    Aligner.AddAligner(OverlapAligner.GetPointer());
    Aligner.AddAligner(ShortOverlapAligner.GetPointer());
    Aligner.AddAligner(MergeAligner.GetPointer());
   // Aligner.AddAligner(InstancedAligner.GetPointer());

    Aligner.AddScorer(CRef<IAlignmentScorer>(BlastScorer.GetPointer()));
    Aligner.AddScorer(CRef<IAlignmentScorer>(PctIdentScorer.GetPointer()));
    Aligner.AddScorer(CRef<IAlignmentScorer>(PctCoverageScorer.GetPointer()));
    Aligner.AddScorer(CRef<IAlignmentScorer>(OverlapScorer.GetPointer()));
    Aligner.AddScorer(CRef<IAlignmentScorer>(ExpansionScorer.GetPointer()));

    CRef<CSeq_align_set> AlignSet;
    AlignSet = Aligner.Align();

    if(Operation == "save") {
        string SaveFileName = TestCases->Get(Case, "savefile");
        if(!SaveFileName.empty()) {
            SaveFileName = TestCases->Get("consts", "casedir") + "/" + SaveFileName;
            CNcbiOfstream Out(SaveFileName.c_str());
            Out << MSerial_AsnText << *AlignSet;
        }
    } else if(Operation == "comp") {
        string CompFileName = TestCases->Get(Case, "compfile");
        if(!CompFileName.empty()) {
            CompFileName = TestCases->Get("consts", "casedir") + "/" + CompFileName;
            CNcbiIfstream In(CompFileName.c_str());
            CRef<CSeq_align_set> CompSet(new CSeq_align_set);
            In >> MSerial_AsnText >> *CompSet;
            if(CompSet->Get().empty() || AlignSet->Get().empty()) {
                cout << "Case: " << Case << " Empty Set, can not compare." << endl;
            }
            bool Match = x_CompareAlignSets(AlignSet, CompSet);
            cout << "Case: " << Case << " Comps: " << (Match ? "True" : "False") << endl;
        }
    } else if(Operation == "print") {
        cout << MSerial_AsnText << *AlignSet;
    }
}



void CNgAlignTest::x_OverlapListToBlastDbCase(IRegistry* TestCases, const string& Case)
{
    string QueryIdFile, BlastDbStr;
    QueryIdFile = TestCases->Get(Case, "queryfile");
    BlastDbStr = TestCases->Get(Case, "blastdb");


    int FilterCount = TestCases->GetInt("consts", "filter_count", 0);
    list<string> Filters;
    for(int Loop = 0; Loop < FilterCount; Loop++) {
        string Curr = TestCases->Get("consts", "filter"+NStr::IntToString(Loop));
        Filters.push_back(Curr);
    }
    string NMerFile = TestCases->Get("consts", "nmer");
    string Operation = TestCases->Get(Case, "operation");
    list<string> BlastParams;
    BlastParams.push_back(TestCases->Get("consts", "blast"));

    string OverlapParams = TestCases->Get("consts", "blast2");

    set<string> IdSet;
    {{
        QueryIdFile = TestCases->Get("consts", "casedir") + "/" + QueryIdFile;
        CNcbiIfstream In(QueryIdFile.c_str());
        CStreamLineReader Reader(In);
        while(!Reader.AtEOF()) {
            ++Reader;
            string Line = *Reader;
            if(!Line.empty() && Line[0] != '#') {
                //cerr << "Line: " << Line << endl;
                Line = Line.substr(0, Line.find(" "));
                CRef<CSeq_id> QueryId(new CSeq_id(Line));
                //Query->SetIdList().push_back(QueryId);
                IdSet.insert(Line);
                cout << Line << " : " << m_Scope->GetBioseqHandle(*QueryId).GetInst_Length() << endl;
            }
        }
    }}

    if(Operation == "skip")
        return;

    CRef<CSeq_align_set> Accums(new CSeq_align_set);

	bool Found = false;
    ITERATE(set<string>, QueryIter, IdSet) {

		//if(!Found && *QueryIter != "1930930")
		//	continue;

		//Found = true;

      	CStopWatch Timer;
        Timer.Start();

        CRef<CSeqIdListSet> Query(new CSeqIdListSet);
        CRef<CBlastDbSet> Subject(new CBlastDbSet(BlastDbStr));
        list<CRef<CQueryFilter> > QueryFilters;
        int Rank = 0;
        ITERATE(list<string>, StrIter, Filters) {
            CRef<CQueryFilter> CurrFilter(new CQueryFilter(Rank, *StrIter));
            Rank++;
            QueryFilters.push_back(CurrFilter);
        }

        CRef<CSeq_id> QueryId(new CSeq_id(*QueryIter));
        Query->SetIdList().push_back(QueryId);
        Subject->SetSoftFiltering(30);

        list<CRef<CBlastAligner> > BlastAligners;
        BlastAligners = CBlastAligner::CreateBlastAligners(BlastParams, 0);
        CRef<CInstancedAligner> InstancedAligner(new CInstancedAligner(300, 3.0, 1.0, 0));
        CRef<CMergeAligner> MergeAligner(new CMergeAligner(-1));
        CRef<COverlapAligner> OverlapAligner(new COverlapAligner(OverlapParams, 2000, -1));
        CRef<COverlapAligner> ShortOverlapAligner(new COverlapAligner(OverlapParams, 100, -1));
        CRef<CMergeAligner> Merge2Aligner(new CMergeAligner(-1));



        CRef<CBlastScorer> BlastScorer(new CBlastScorer);
        CRef<CPctIdentScorer> PctIdentScorer(new CPctIdentScorer);
        CRef<CPctCoverageScorer> PctCoverageScorer(new CPctCoverageScorer);
        CRef<COverlapScorer> OverlapScorer(new COverlapScorer);
        CRef<CExpansionScorer> ExpansionScorer(new CExpansionScorer);


        CNgAligner Aligner(*m_Scope);
        Aligner.SetQuery(CRef<ISequenceSet>(Query.GetPointer()));
        Aligner.SetSubject(CRef<ISequenceSet>(Subject.GetPointer()));

        NON_CONST_ITERATE(list<CRef<CQueryFilter> >, FilterIter, QueryFilters) {
            Aligner.AddFilter(&**FilterIter);
        }

    //    Aligner.AddAligner(CRef<IAlignmentFactory>(BlastListAligner.GetPointer()));
        int Count = 0;
        NON_CONST_ITERATE(list<CRef<CBlastAligner> >, BlastIter, BlastAligners) {
            if(Count == 0)
                (*BlastIter)->SetSoftFiltering(30);
            Aligner.AddAligner(&**BlastIter);
            Count++;
        }
        Aligner.AddAligner(MergeAligner.GetPointer());
        Aligner.AddAligner(OverlapAligner.GetPointer());
        Aligner.AddAligner(ShortOverlapAligner.GetPointer());
        Aligner.AddAligner(Merge2Aligner.GetPointer());

        Aligner.AddScorer(CRef<IAlignmentScorer>(BlastScorer.GetPointer()));
        Aligner.AddScorer(CRef<IAlignmentScorer>(PctIdentScorer.GetPointer()));
        Aligner.AddScorer(CRef<IAlignmentScorer>(PctCoverageScorer.GetPointer()));
        Aligner.AddScorer(CRef<IAlignmentScorer>(OverlapScorer.GetPointer()));
        Aligner.AddScorer(CRef<IAlignmentScorer>(ExpansionScorer.GetPointer()));

        CRef<CSeq_align_set> AlignSet;
        AlignSet = Aligner.Align();

        Timer.Stop();

        bool OverlapFound;
        if(!AlignSet.IsNull()) {
            int Overlap = 0;
            ITERATE(CSeq_align_set::Tdata, AlignIter, AlignSet->Get()) {
                (*AlignIter)->GetNamedScore("overlap_aligner", Overlap);
                OverlapFound |= Overlap;
            }
        }

        cout << *QueryIter << " timed at " << Timer.AsString() << "s, Overlap "
            << (OverlapFound ? "found" : "NOT found") << endl;

        if(AlignSet.IsNull())
            continue;

        ITERATE(CSeq_align_set::Tdata, AlignIter, AlignSet->Get()) {
            Accums->Set().push_back(*AlignIter);
        }

    }

    CRef<CSeq_align_set> AlignSet = Accums;

    if(Operation == "save") {
        string SaveFileName = TestCases->Get(Case, "savefile");
        if(!SaveFileName.empty()) {
            SaveFileName = TestCases->Get("consts", "casedir") + "/" + SaveFileName;
            CNcbiOfstream Out(SaveFileName.c_str());
            Out << MSerial_AsnBinary << *AlignSet;
        }
    } else if(Operation == "comp") {
        string CompFileName = TestCases->Get(Case, "compfile");
        if(!CompFileName.empty()) {
            CompFileName = TestCases->Get("consts", "casedir") + "/" + CompFileName;
            CNcbiIfstream In(CompFileName.c_str());
            CRef<CSeq_align_set> CompSet(new CSeq_align_set);
            In >> MSerial_AsnText >> *CompSet;
            if(CompSet->Get().empty() || AlignSet->Get().empty()) {
                cout << "Case: " << Case << " Empty Set, can not compare." << endl;
            }
            bool Match = x_CompareAlignSets(AlignSet, CompSet);
            cout << "Case: " << Case << " Comps: " << (Match ? "True" : "False") << endl;
        }
    } else if(Operation == "print") {
        cout << MSerial_AsnText << *AlignSet;
    }
}





bool CNgAlignTest::x_CompareAlignSets(CRef<CSeq_align_set> New,
                                        CRef<CSeq_align_set> Ref)
{
    CRef<CSeq_align_set> LocalNew(new CSeq_align_set),
                         LocalRef(new CSeq_align_set);
    //LocalNew->Assign(*New);
    //LocalRef->Assign(*Ref);

    NON_CONST_ITERATE(CSeq_align_set::Tdata, AlignIter, New->Set()) {
        CRef<CSeq_align> Copy(new CSeq_align);
        Copy->SetType() = (*AlignIter)->GetType();
        Copy->SetSegs().Assign( (*AlignIter)->GetSegs() );
        LocalNew->Set().push_back(Copy);
    }
    NON_CONST_ITERATE(CSeq_align_set::Tdata, AlignIter, Ref->Set()) {
        CRef<CSeq_align> Copy(new CSeq_align);
        Copy->SetType() = (*AlignIter)->GetType();
        Copy->SetSegs().Assign( (*AlignIter)->GetSegs() );
        LocalRef->Set().push_back(Copy);
    }

    //cout << "**************************" << endl;
    //cout << MSerial_AsnText << *LocalRef << MSerial_AsnText << *LocalNew;
    //cout << "**************************" << endl;

    return LocalRef->Equals(*LocalNew);
}


END_NCBI_SCOPE
USING_SCOPE(ncbi);


int main(int argc, char** argv)
{
    SetSplitLogFile(false);
    GetDiagContext().SetOldPostFormat(true);
    //SetDiagPostLevel(eDiag_Info);
    return CNgAlignTest().AppMain(argc, argv, 0, eDS_Default, 0);
}
