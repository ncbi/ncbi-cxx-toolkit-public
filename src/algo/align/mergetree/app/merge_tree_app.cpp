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
#include <corelib/ncbithr.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbi_system.hpp>

#include <connect/ncbi_util.h>
#include <connect/ncbi_core_cxx.hpp>

#include <serial/serialbase.hpp>
#include <serial/serial.hpp>
 
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seqalign/seqalign__.hpp>

#include <objmgr/util/sequence.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/asn_cache/asn_cache_loader.hpp>
#include <misc/data_loaders_util/data_loaders_util.hpp>

#include <util/sync_queue.hpp>

#include <algo/align/mergetree/merge_tree.hpp>

#define MARKLINE cerr << __LINE__ << endl;


USING_NCBI_SCOPE;
USING_SCOPE(objects);

////////////////////////////////////////////////////


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

typedef vector<CRef<objects::CSeq_align> > TAlignVec;
typedef CSyncQueue<TAlignVec, deque<TAlignVec>, CSyncQueue_Traits_ConcurrencyOn> TSyncQueue;
class CMergeTreeThread : public CThread 
{
public:
    
    CMergeTreeThread() : CThread() { ; }

    CRef<CScope> Scope;
    double SecondsLimit;
    CMergeTree::SScoring Scoring;
    TSyncQueue  *InQueue, *OutQueue;
    int TId;

    virtual void *  Main (void);
};


void *  CMergeTreeThread::Main (void)
{
    list< CRef<CSeq_align> > MergedAligns;
    
    cerr << "Merge Thread Starting : " << TId << endl;

    CTreeAlignMerger TreeMerger;
    TreeMerger.SetScope(Scope);
    TreeMerger.SetScoring(Scoring);

    STimerCallbackData CbData;
    CbData.StartTime = CTime(CTime::eCurrent);
    CbData.SecondsLimit = SecondsLimit; 
    TreeMerger.SetInterruptCallback(s_TimerCallback, &CbData);

    CTimeSpan PopSpan((long)2);

    // work with queue
    while(!InQueue->IsEmpty()) {
        TAlignVec InAligns, OutAligns;
        try {
            InAligns = InQueue->Pop(&PopSpan);
        } catch(CException& e) { cerr <<__LINE__<<"Thread Queue Exception: " << e.ReportAll() << endl; break; }
        
        list< CRef<CSeq_align> > In, Out;
        In.insert(In.end(), InAligns.begin(), InAligns.end());
        
        {{
            CRef<CSeq_align> Align = In.front();
            cerr << "T: " << TId << " : " 
                 << "Starting Merge: " << Align->GetSeq_id(0).AsFastaString() << (Align->GetSeqStrand(0)==eNa_strand_plus?"+":"-")
                              << " x " << Align->GetSeq_id(1).AsFastaString() << (Align->GetSeqStrand(1)==eNa_strand_plus?"+":"-")
                              << " : " << In.size() << " aligns" << endl;
        }}

        CbData.StartTime = CTime(CTime::eCurrent);
        CbData.SecondsLimit = SecondsLimit; 

        TreeMerger.Merge(In, Out);
        
        ITERATE(list<CRef<CSeq_align> >, OutIter, Out) {
            OutAligns.push_back(*OutIter);
        }
        OutQueue->Push(OutAligns);
    }

    cerr << "Merge Thread Finished : " << TId << endl;
    return NULL;
}

class CMergeOutputThread : public CThread 
{
public:
    
    CMergeOutputThread() : CThread() { ; }

    TSyncQueue*   InQueue;
    CNcbiOstream* OutStream;
    bool Finished;

    virtual void *  Main (void);
};

void *  CMergeOutputThread::Main (void)
{
    Finished = false;
    CTimeSpan PopSpan((long)10);
    
    size_t GroupCount = 0, AlignCount=0;

    // work with queue
    while(!Finished || !InQueue->IsEmpty()) {
        TAlignVec InAligns;
        try {
            InAligns = InQueue->Pop(&PopSpan);
        } catch(CException& e) { cerr <<"Thread Output Queue Timeout, repeat." << endl; continue; }
     
        GroupCount++;

        ITERATE(TAlignVec, InIter, InAligns) {
            (*OutStream) << MSerial_AsnText << **InIter;
            AlignCount++;
        }
        OutStream->flush();  
    }

    cerr << "Output Thread Finished, Wrote "<<GroupCount<< " sets for " << AlignCount<< " alignments"<<endl;
}
 

class CMergeyApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int Run();


};


void
CMergeyApp::Init(void)
{
    unique_ptr<CArgDescriptions> argList(new CArgDescriptions());

    //argList->SetUsageContext(GetArguments().GetProgramBasename(), "Mergey Test App");

    argList->AddKey("input", "infile", "seqalign file", CArgDescriptions::eInputFile);
    argList->AddDefaultKey("output", "outfile", "seqalign file", CArgDescriptions::eOutputFile, "-");
    argList->AddDefaultKey("timer", "double", "seconds", CArgDescriptions::eDouble, "120.0");
    argList->AddDefaultKey("ratio", "integer", "match value", CArgDescriptions::eInteger, "3");    
    argList->AddDefaultKey("threads", "integer", "thread count. core logic is sequential, but seperate seq-id-pairs can thread.", CArgDescriptions::eInteger, "1");    
    
    //argList->AddFlag("G", "no genbank");

    CDataLoadersUtil::AddArgumentDescriptions(*argList);

    SetupArgDescriptions(argList.release());
}

int
CMergeyApp::Run()
{
    CORE_SetLOCK(MT_LOCK_cxx2c());
    const CArgs& args = GetArgs();
    
  
	CRef<CObjectManager> OM = CObjectManager::GetInstance();
    CRef<CScope> Scope;
    Scope = CDataLoadersUtil::GetDefaultScope(args);
    //if(!args["G"].HasValue())
    //    CGBDataLoader::RegisterInObjectManager(*OM, NULL, CObjectManager::eDefault, 16000);
	//Scope.Reset(new CScope(*OM));
	//Scope->AddDefaults();


    CNcbiIstream& In = args["input"].AsInputFile();
    list<CRef<CSeq_align> > OrigAligns;
    while(In) {
        CRef<CSeq_align> Align(new CSeq_align);
        try {
            In >> MSerial_AsnText >> *Align;
        } catch(...) {  break; }
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
            } catch(...) { break; }
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
            } catch(...) { break; }
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
    if(OrigAligns.empty()) {
        cerr << "No alignments read in." << endl;
        return 0;
    }

    /*{{
        list<CRef<CSeq_align> > ta;
        ITERATE(list<CRef<CSeq_align> >, AlignIter, OrigAligns) {
            if((*AlignIter)->GetSegs().IsDisc()) {
                ITERATE(CSeq_align_set::Tdata, DiscAlignIter, 
                    (*AlignIter)->GetSegs().GetDisc().Get()) {
                    ta.push_back(*DiscAlignIter);
                }
            } 
            else {
                ta.push_back(*AlignIter);
            }
        }
        OrigAligns = ta;
        cerr << "After Disc flatten, OrigAligns.size: " << OrigAligns.size() << endl;
    }}*/

    list< CRef<CSeq_align> > MergedAligns;
    
    int ThreadCount = args["threads"].AsInteger();
    double Timer = args["timer"].AsDouble();
    int Ratio = args["ratio"].AsInteger();
    
    CMergeTree::SScoring Scoring;
    Scoring.Match = Ratio;
    Scoring.MisMatch = -1;
    Scoring.GapOpen = -1;
    Scoring.GapExtend = -1;

    // Uses Threaded Mode with even 1 Thread
    // Uses 'old no-threads at all' on 0 Threads
    if(ThreadCount > 0) {
        typedef pair<objects::CSeq_id_Handle, objects::ENa_strand> TSeqIdPair;
        typedef pair<TSeqIdPair, TSeqIdPair> TMapKey;
        typedef vector<CRef<objects::CSeq_align> > TAlignVec;
        typedef map<TMapKey, TAlignVec> TAlignGroupMap;
        typedef pair<TMapKey, TAlignVec> TAlignGroupPair;
        TAlignGroupMap AlignGroupMap;
        ITERATE(list<CRef<CSeq_align> >, AlignIter, OrigAligns) {
            const CSeq_align& Align = **AlignIter;
            TMapKey Key;
            Key.first.first   = CSeq_id_Handle::GetHandle(Align.GetSeq_id(0));
            Key.first.second  = Align.GetSeqStrand(0);
            Key.second.first   = CSeq_id_Handle::GetHandle(Align.GetSeq_id(1));
            Key.second.second  = Align.GetSeqStrand(1);
            AlignGroupMap[Key].push_back(*AlignIter);
        }
        cerr << "Thread Queue Length: " << AlignGroupMap.size() << endl;
        CSyncQueue<TAlignVec, deque<TAlignVec>, CSyncQueue_Traits_ConcurrencyOn> ThreadQueue(AlignGroupMap.size());
        CSyncQueue<TAlignVec, deque<TAlignVec>, CSyncQueue_Traits_ConcurrencyOn> ThreadResults(AlignGroupMap.size());
        ITERATE(TAlignGroupMap, AlignGroupIter, AlignGroupMap) {
            ThreadQueue.Push(AlignGroupIter->second);
        }

        // Make threads
        list< CMergeTreeThread* > Threads;
        for(int i = 0; i < ThreadCount; i++) {
            CMergeTreeThread* New = new CMergeTreeThread();
            //New->Scope = Scope;
            //New->Scope.Reset(new CScope(*OM));
            //New->Scope->AddDefaults();
            New->Scope = CDataLoadersUtil::GetDefaultScope(args);
            New->SecondsLimit = Timer;
            New->Scoring = Scoring;
            New->InQueue = &ThreadQueue;
            New->OutQueue = &ThreadResults;
            New->TId = i+1;
            Threads.push_back(New);
        }

        CNcbiOstream& Out = args["output"].AsOutputFile();
        CMergeOutputThread* FileOutThread = new CMergeOutputThread();
        FileOutThread->InQueue = &ThreadResults;
        FileOutThread->OutStream = &Out; 

        // dispatch queue to threads
        ITERATE(list< CMergeTreeThread* >, ThreadIter, Threads) {
            (*ThreadIter)->Run();
        }
        FileOutThread->Run();
        cerr<<"Threads Launched, Joining" << endl;
        ITERATE(list< CMergeTreeThread* >, ThreadIter, Threads) {
            (*ThreadIter)->Join();
        }
        FileOutThread->Finished = true;
        FileOutThread->Join();
        cerr<< "Threads Joined, Exiting." << endl;
       
        /*
        while(!ThreadResults.IsEmpty()) {
            TAlignVec OutBatch = ThreadResults.Pop();
            ITERATE(TAlignVec, AlignIter, OutBatch) {
                MergedAligns.push_back(*AlignIter);
            }
        }*/
    } // End Non-Zero ThreadCount
    else if(ThreadCount == 0) {
        CTreeAlignMerger TreeMerger;
        TreeMerger.SetScope(Scope);
        TreeMerger.SetScoring(Scoring);

        STimerCallbackData CbData;
        CbData.StartTime = CTime(CTime::eCurrent);
        CbData.SecondsLimit = Timer;
        TreeMerger.SetInterruptCallback(s_TimerCallback, &CbData);

        TreeMerger.Merge(OrigAligns, MergedAligns);
    
        cerr << "MergedAligns.size: " << MergedAligns.size() << endl;
    
        CNcbiOstream& Out = args["output"].AsOutputFile();
        ITERATE(list<CRef<CSeq_align> >, MergeIter, MergedAligns) {
            Out << MSerial_AsnText << **MergeIter;
        }

    } // end Zero ThreadCount

    return 0;
}




int
main(int argc, const char* argv[])
{
    return CMergeyApp().AppMain(argc, argv, NULL,
                              eDS_Default, NULL );
}

