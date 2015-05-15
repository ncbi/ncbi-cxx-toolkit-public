/* $Id$
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
* Authors: Nathan Bouk
*
* File Description:
*   App for CMergeTree
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbi_system.hpp>

#include <serial/serialbase.hpp>
#include <serial/serial.hpp>
 
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seqalign/seqalign__.hpp>

#include <objmgr/util/sequence.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <algo/align/mergetree/merge_tree.hpp>

#define MARKLINE cerr << __LINE__ << endl;


USING_NCBI_SCOPE;
USING_SCOPE(objects);

////////////////////////////////////////////////////

class CMergeyApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int Run();


};


void
CMergeyApp::Init(void)
{
    auto_ptr<CArgDescriptions> argList(new CArgDescriptions());

    //argList->SetUsageContext(GetArguments().GetProgramBasename(), "Mergey Test App");

    argList->AddKey("input", "infile", "seqalign file", CArgDescriptions::eInputFile);
    argList->AddDefaultKey("output", "outfile", "seqalign file", CArgDescriptions::eOutputFile, "-");
    argList->AddDefaultKey("timer", "double", "seconds", CArgDescriptions::eDouble, "120.0");
    
    argList->AddFlag("G", "no genbank");

    SetupArgDescriptions(argList.release());
}

string s_Strand(ENa_strand strand) {
    if(strand == eNa_strand_plus)
        return "+";
    else if(strand == eNa_strand_minus)
        return "-";
    else if(strand == eNa_strand_both)
        return "b";
    else
        return "?";
}

struct STimerCallbackData {
    CTime StartTime;
    double SecondsLimit;
};
bool s_TimerCallback(void* Data) {
   
    //cerr << "s_TimerCallback" << endl;
    STimerCallbackData* CbData = (STimerCallbackData*)Data;
    CTime Current(CTime::eCurrent);

    CTimeSpan Diff = Current - CbData->StartTime;

    if(Diff.GetAsDouble() > CbData->SecondsLimit) {
        cerr << "s_TimerCallback " << Diff.GetAsDouble() << endl;
        return true;
    }
    
    return false;    
}

int
CMergeyApp::Run()
{
    const CArgs& args = GetArgs();
    
  
	CRef<CObjectManager> OM = CObjectManager::GetInstance();
    CRef<CScope> Scope;
    if(!args["G"].HasValue())
        CGBDataLoader::RegisterInObjectManager(*OM, NULL, CObjectManager::eDefault, 16000);
	Scope.Reset(new CScope(*OM));
	Scope->AddDefaults();


    CNcbiIstream& In = args["input"].AsInputFile();
    list<CRef<CSeq_align> > OrigAligns;
    while(In) {
        CRef<CSeq_align> Align(new CSeq_align);
        try {
            In >> MSerial_AsnText >> *Align;
        } catch(...) { cerr << __LINE__ << endl; break; }
        OrigAligns.push_back(Align);        
    }
    if(OrigAligns.empty()) {
        In.clear();
        In.seekg(0);
        In.clear();
        while(In) {
            CRef<CSeq_align> Align(new CSeq_align);
            try {
                In >> MSerial_AsnBinary >> *Align;
            } catch(...) { cerr << __LINE__ << endl; break; }
            OrigAligns.push_back(Align);        
        }
    }
    if(OrigAligns.empty()) {
        In.clear();
        In.seekg(0);
        In.clear();
        while(In) {
            CRef<CSeq_align_set> AlignSet(new CSeq_align_set);
            try {
                In >> MSerial_AsnBinary >> *AlignSet;
            } catch(...) { cerr << __LINE__ << endl; break; }
            OrigAligns.insert(OrigAligns.end(), AlignSet->Get().begin(), AlignSet->Get().end());       
        }
    }

    /*{{
        //  9              /
        // *8            6/
        // *7    4/     5/3
        // *6    /2       
        // *5  3/          
        // *4               
        // *3    /         
        // *2  2/1         
        // *1 1/           
        // *0123456789ABCDEF                
        // 
        
        CRef<CSeq_id> ID1(new CSeq_id("lcl|FIRST")), ID2(new CSeq_id("lcl|SECOND"));

        CRef<CSeq_align> One(new CSeq_align);
        One->SetSegs().SetDenseg().SetDim(2);
        One->SetSegs().SetDenseg().SetNumseg(1);
        One->SetSegs().SetDenseg().SetIds().push_back(ID1);
        One->SetSegs().SetDenseg().SetIds().push_back(ID2);
        One->SetSegs().SetDenseg().SetStarts().push_back(3);
        One->SetSegs().SetDenseg().SetStarts().push_back(1);
        One->SetSegs().SetDenseg().SetLens().push_back(3);
        One->SetSegs().SetDenseg().SetStrands().push_back(eNa_strand_plus);
        One->SetSegs().SetDenseg().SetStrands().push_back(eNa_strand_plus);
        OrigAligns.push_back(One);

        CRef<CSeq_align> Two(new CSeq_align);
        Two->SetSegs().SetDenseg().SetDim(2);
        Two->SetSegs().SetDenseg().SetNumseg(1);
        Two->SetSegs().SetDenseg().SetIds().push_back(ID1);
        Two->SetSegs().SetDenseg().SetIds().push_back(ID2);
        Two->SetSegs().SetDenseg().SetStarts().push_back(4);
        Two->SetSegs().SetDenseg().SetStarts().push_back(5);
        Two->SetSegs().SetDenseg().SetLens().push_back(3);
        Two->SetSegs().SetDenseg().SetStrands().push_back(eNa_strand_plus);
        Two->SetSegs().SetDenseg().SetStrands().push_back(eNa_strand_plus);
        OrigAligns.push_back(Two);

        CRef<CSeq_align> Tre(new CSeq_align);
        Tre->SetSegs().SetDenseg().SetDim(2);
        Tre->SetSegs().SetDenseg().SetNumseg(1);
        Tre->SetSegs().SetDenseg().SetIds().push_back(ID1);
        Tre->SetSegs().SetDenseg().SetIds().push_back(ID2);
        Tre->SetSegs().SetDenseg().SetStarts().push_back(13);
        Tre->SetSegs().SetDenseg().SetStarts().push_back(7);
        Tre->SetSegs().SetDenseg().SetLens().push_back(3);
        Tre->SetSegs().SetDenseg().SetStrands().push_back(eNa_strand_plus);
        Tre->SetSegs().SetDenseg().SetStrands().push_back(eNa_strand_plus);
        OrigAligns.push_back(Tre);
    }}*/
    
    /*{{
        OrigAligns.clear(); 
        
        CRef<CSeq_id> ID1(new CSeq_id("gi|224589800")), ID2(new CSeq_id("gi|224589811"));

        CRef<CSeq_align> One(new CSeq_align);
        One->SetSegs().SetDenseg().SetDim(2);
        One->SetSegs().SetDenseg().SetNumseg(1);
        One->SetSegs().SetDenseg().SetIds().push_back(ID1);
        One->SetSegs().SetDenseg().SetIds().push_back(ID2);
        One->SetSegs().SetDenseg().SetStarts().push_back(10);
        One->SetSegs().SetDenseg().SetStarts().push_back(10);
        One->SetSegs().SetDenseg().SetLens().push_back(30);
        One->SetSegs().SetDenseg().SetStrands().push_back(eNa_strand_plus);
        One->SetSegs().SetDenseg().SetStrands().push_back(eNa_strand_plus);
        OrigAligns.push_back(One);

        CRef<CSeq_align> Two(new CSeq_align);
        Two->SetSegs().SetDenseg().SetDim(2);
        Two->SetSegs().SetDenseg().SetNumseg(1);
        Two->SetSegs().SetDenseg().SetIds().push_back(ID1);
        Two->SetSegs().SetDenseg().SetIds().push_back(ID2);
        Two->SetSegs().SetDenseg().SetStarts().push_back(20);
        Two->SetSegs().SetDenseg().SetStarts().push_back(40);
        Two->SetSegs().SetDenseg().SetLens().push_back(40);
        Two->SetSegs().SetDenseg().SetStrands().push_back(eNa_strand_plus);
        Two->SetSegs().SetDenseg().SetStrands().push_back(eNa_strand_plus);
        OrigAligns.push_back(Two);

        CRef<CSeq_align> Tre(new CSeq_align);
        Tre->SetSegs().SetDenseg().SetDim(2);
        Tre->SetSegs().SetDenseg().SetNumseg(1);
        Tre->SetSegs().SetDenseg().SetIds().push_back(ID1);
        Tre->SetSegs().SetDenseg().SetIds().push_back(ID2);
        Tre->SetSegs().SetDenseg().SetStarts().push_back(50);
        Tre->SetSegs().SetDenseg().SetStarts().push_back(20);
        Tre->SetSegs().SetDenseg().SetLens().push_back(60);
        Tre->SetSegs().SetDenseg().SetStrands().push_back(eNa_strand_plus);
        Tre->SetSegs().SetDenseg().SetStrands().push_back(eNa_strand_plus);
        OrigAligns.push_back(Tre);
    }}*/


    cerr << "OrigAligns.size: " << OrigAligns.size() << endl;
    
    list< CRef<CSeq_align> > MergedAligns;
    
    CTreeAlignMerger TreeMerger;
    TreeMerger.SetScope(Scope);
    CMergeTree::SScoring Scoring;
    Scoring.Match = 4;
    Scoring.MisMatch = -1;
    Scoring.GapOpen = -1;
    Scoring.GapExtend = -1;
    TreeMerger.SetScoring(Scoring);

    STimerCallbackData CbData;
    CbData.StartTime = CTime(CTime::eCurrent);
    CbData.SecondsLimit = args["timer"].AsDouble();
    TreeMerger.SetInterruptCallback(s_TimerCallback, &CbData);

    TreeMerger.Merge(OrigAligns, MergedAligns);
    //TreeMerger.Merge_Pairwise(OrigAligns, MergedAligns);
    //TreeMerger.Merge_Dist(OrigAligns, MergedAligns);
    
    cerr << "MergedAligns.size: " << MergedAligns.size() << endl;
    
    CNcbiOstream& Out = args["output"].AsOutputFile();
    ITERATE(list<CRef<CSeq_align> >, MergeIter, MergedAligns) {
        Out << MSerial_AsnText << **MergeIter;
    }


    return 0;
}




int
main(int argc, const char* argv[])
{
    return CMergeyApp().AppMain(argc, argv, NULL,
                              eDS_Default, NULL );
}

