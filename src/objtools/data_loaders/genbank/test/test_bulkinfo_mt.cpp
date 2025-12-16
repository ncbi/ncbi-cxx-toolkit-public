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
* Author:
*           Andrei Gourianov, Michael Kimelman
*
* File Description:
*           Basic test of GenBank data loader
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/test_mt.hpp>
#include <corelib/ncbi_test.hpp>
#include <util/random_gen.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/genbank/readers.hpp>
#include <dbapi/driver/drivers.hpp>
#include <common/ncbi_sanitizers.h>

#include "bulkinfo_tester.hpp"

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;
USING_SCOPE(objects);


const int g_gi_from = 156000;
const int g_gi_to   = 156500;
const int g_acc_from = 1;


/////////////////////////////////////////////////////////////////////////////
//
//  CTestApplication::
//


class CTestApplication : public CThreadedApp
{
public:
    virtual bool TestApp_Args(CArgDescriptions& args);
    virtual bool TestApp_Init(void);
    virtual bool TestApp_Exit(void);

    virtual bool Thread_Run(int idx);
    bool RunPass(void);
    CRef<CScope> MakeScope(void) const;

    typedef vector<CSeq_id_Handle> TIds;
    bool ProcessBlock(const TIds& ids,
                      const vector<string>& reference,
                      CRef<CScope> scope) const;

    CRandom::TValue m_Seed;
    size_t m_RunCount;
    enum EReuseScope {
        eReuseScope_none, // separate scope for each Seq-id scan
        eReuseScope_iteration, // separate scope for each processing thread
        eReuseScope_all, // global scope for all processing threads
        eReuseScope_split // distribute above variants over processing threads
    };
    EReuseScope m_ReuseScope;
    CRef<CScope> m_GlobalScope;
    size_t m_RunSize;
    TIds m_Ids;
    vector<string> m_Reference;
    IBulkTester::EBulkType m_Type;
    bool m_Verbose;
    bool m_Sort;
    bool m_Single;
    bool m_Verify;
    CScope::TGetFlags m_GetFlags;
    vector<string> other_loaders;
    mutable CAtomicCounter m_ErrorCount;
};


bool CTestApplication::TestApp_Args(CArgDescriptions& args)
{
    args.AddOptionalKey
        ("fromgi", "FromGi",
         "Process sequences in the interval FROM this Gi",
         CArgDescriptions::eInteger);
    args.AddOptionalKey
        ("togi", "ToGi",
         "Process sequences in the interval TO this Gi",
         CArgDescriptions::eInteger);
    args.AddOptionalKey
        ("idlist", "IdList",
         "File with list of Seq-ids to test",
         CArgDescriptions::eInputFile);
    IBulkTester::AddArgs(args);
    args.AddFlag("verbose", "Verbose results");
    args.AddFlag("sort", "Sort requests");
    args.AddFlag("single", "Use single id queries (non-bulk)");
    args.AddFlag("verify", "Run extra test to verify returned values");
    args.AddOptionalKey
        ("reference", "Reference",
         "Load reference results from a file",
         CArgDescriptions::eInputFile);
    args.AddOptionalKey
        ("save_reference", "SaveReference",
         "Save results into a file",
         CArgDescriptions::eOutputFile);
    args.AddDefaultKey
        ("count", "Count",
         "Number of iterations to run",
         CArgDescriptions::eInteger, "10");
    args.AddDefaultKey
        ("reuse_scope", "ReuseScope",
         "Re-use OM scope",
         CArgDescriptions::eString, "split");
    args.SetConstraint("reuse_scope",
                       &(*new CArgAllow_Strings,
                         "none", "iteration", "all", "split"));
    string size_limit = "200";
    // ThreadSanitizer has a limit on number of mutexes held by a thread
#ifdef NCBI_USE_TSAN
    size_limit = "20";
#endif
    args.AddDefaultKey
        ("size", "Size",
         "Limit number of Seq-ids to process in a run",
         CArgDescriptions::eInteger, size_limit);
    args.AddOptionalKey("other_loaders", "OtherLoaders",
                        "Extra data loaders as plugins (comma separated)",
                        CArgDescriptions::eString);

    args.SetUsageContext(GetArguments().GetProgramBasename(),
                         "test_bulkinfo_mt", false);
    return true;
}


bool CTestApplication::TestApp_Init(void)
{
    //CONNECT_Init(&GetConfig());
    //CORE_SetLOCK(MT_LOCK_cxx2c());
    //CORE_SetLOG(LOG_cxx2c());
    SetDiagPostLevel(eDiag_Info);

    m_ErrorCount.Set(0);

    const CArgs& args = GetArgs();
    if ( args["idlist"] ) {
        CNcbiIstream& file = args["idlist"].AsInputFile();
        string line;
        while ( getline(file, line) ) {
            size_t comment = line.find('#');
            if ( comment != NPOS ) {
                line.erase(comment);
            }
            line = NStr::TruncateSpaces(line);
            if ( line.empty() ) {
                continue;
            }
            
            CSeq_id sid(line);
            CSeq_id_Handle sih = CSeq_id_Handle::GetHandle(sid);
            if ( !sih ) {
                continue;
            }
            m_Ids.push_back(sih);
        }
        
        LOG_POST("Testing bulk info load ("<<m_Ids.size()<<" Seq-ids from file)...");
    }
    if ( m_Ids.empty() && (args["fromgi"] || args["togi"]) ) {
        TIntId gi_from  = args["fromgi"]? args["fromgi"].AsInteger(): g_gi_from;
        TIntId gi_to    = args["togi"]? args["togi"].AsInteger(): g_gi_to;
        TIntId delta = gi_to > gi_from? 1: -1;
        for ( TIntId gi = gi_from; gi != gi_to+delta; gi += delta ) {
            m_Ids.push_back(CSeq_id_Handle::GetGiHandle(GI_FROM(TIntId, gi)));
        }
        LOG_POST("Testing bulk info load (gi from "<<gi_from<<" to "<<gi_to<<")...");
    }
    if ( m_Ids.empty() ) {
        TIntId count = g_gi_to-g_gi_from+1;
        for ( TIntId i = 0; i < count; ++i ) {
            if ( i % 3 != 0 ) {
                m_Ids.push_back(CSeq_id_Handle::GetGiHandle(GI_FROM(TIntId, i+g_gi_from)));
            }
            else {
                CNcbiOstrstream str;
                if ( i & 1 )
                    str << "A";
                else
                    str << "a";
                if ( i & 2 )
                    str << "A";
                else
                    str << "a";
                str << setfill('0') << setw(6) << (i/3+g_acc_from);
                string acc = CNcbiOstrstreamToString(str);
                CSeq_id seq_id(acc);
                m_Ids.push_back(CSeq_id_Handle::GetHandle(seq_id));
            }
        }
        LOG_POST("Testing bulk info load (accessions and gi from "<<g_gi_from<<" to "<<g_gi_to<<")...");
    }
    m_Type = IBulkTester::ParseType(args);
    m_GetFlags = IBulkTester::ParseGetFlags(args);
    m_Verbose = args["verbose"];
    if ( args["seed"] ) {
        m_Seed = args["seed"].AsInteger();
    }
    else {
        m_Seed = CNcbiTest::GetRandomSeed();
    }
    m_Sort = args["sort"];
    m_Single = args["single"];
    m_Verify = args["verify"];
    m_RunCount = args["count"].AsInteger();
    m_ReuseScope = eReuseScope_split;
    if ( args["reuse_scope"].AsString() == "none" ) {
        m_ReuseScope = eReuseScope_none;
    }
    else if ( args["reuse_scope"].AsString() == "iteration" ) {
        m_ReuseScope = eReuseScope_iteration;
    }
    else if ( args["reuse_scope"].AsString() == "all" ) {
        m_ReuseScope = eReuseScope_all;
    }
    m_RunSize = args["size"].AsInteger();
    if ( args["reference"] ) {
        CNcbiIstream& in = args["reference"].AsInputFile();
        m_Reference.resize(m_Ids.size());
        NON_CONST_ITERATE ( vector<string>, it, m_Reference ) {
            getline(in, *it);
        }
        if ( !in ) {
            ERR_POST("Failed to read reference data from "<<args["reference"].AsString());
            return false;
        }
    }
    if ( args["other_loaders"] ) {
        vector<string> names;
        NStr::Split(args["other_loaders"].AsString(), ",", names);
        CRef<CObjectManager> pOm = CObjectManager::GetInstance();
        ITERATE ( vector<string>, i, names ) {
            other_loaders.push_back(pOm->RegisterDataLoader(0, *i)->GetName());
        }
    }
    if ( m_ReuseScope == eReuseScope_all || m_ReuseScope == eReuseScope_split ) {
        m_GlobalScope = MakeScope(); // global scope is needed
    }
    return true;
}


bool CTestApplication::TestApp_Exit(void)
{
    if ( m_ErrorCount.Get() == 0 ) {
        LOG_POST("Passed");
        return true;
    }
    else {
        ERR_POST("Failed: " << m_ErrorCount.Get());
        return false;
    }
}


CRef<CScope> CTestApplication::MakeScope(void) const
{
    CRef<CScope> ret;
    CRef<CObjectManager> pOm = CObjectManager::GetInstance();
#ifdef HAVE_PUBSEQ_OS
    DBAPI_RegisterDriver_FTDS();
    GenBankReaders_Register_Pubseq();
    GenBankReaders_Register_Pubseq2();
#endif
    CGBDataLoader::RegisterInObjectManager(*pOm);
    
    ret = new CScope(*pOm);
    ret->AddDefaults();
    ITERATE ( vector<string>, it, other_loaders ) {
        ret->AddDataLoader(*it, 88);
    }
    return ret;
}


bool CTestApplication::Thread_Run(int thread_id)
{
    CRandom random(m_Seed+thread_id);
    EReuseScope reuse_scope = m_ReuseScope;
    if ( reuse_scope == eReuseScope_split ) {
        // assign each thread specific behavior
        switch ( random.GetRand(0, 2) ) {
        default:
            reuse_scope = eReuseScope_none;
            LOG_POST("Test thread "<<thread_id<<" selected separate scope for each iteration");
            break;
        case 1:
            reuse_scope = eReuseScope_iteration;
            LOG_POST("Test thread "<<thread_id<<" selected single scope for all iterations");
            break;
        case 2:
            reuse_scope = eReuseScope_all;
            LOG_POST("Test thread "<<thread_id<<" selected global scope");
            break;
        }
    }
    CRef<CScope> scope;
    if ( reuse_scope == eReuseScope_all ) {
        scope = m_GlobalScope;
    }
    try {
        vector<CSeq_id_Handle> ids;
        vector<string> reference;
        vector<pair<CSeq_id_Handle, string> > data;
        for ( size_t run_i = 0; run_i < m_RunCount; ++run_i ) {
            LOG_POST("Testing pass "<<run_i);
            size_t size = min(m_RunSize, m_Ids.size());
            data.clear();
            data.resize(m_Ids.size());
            for ( size_t i = 0; i < m_Ids.size(); ++i ) {
                data[i].first = m_Ids[i];
                if ( !m_Reference.empty() ) {
                    data[i].second = m_Reference[i];
                }
            }
            for ( size_t i = 0; i < size; ++i ) {
                swap(data[i], data[random.GetRandSize_t(i, data.size()-1)]);
            }
            data.resize(size);
            if ( m_Sort ) {
                sort(data.begin(), data.end());
            }
            ids.clear();
            reference.clear();
            for ( size_t i = 0; i < data.size(); ++i ) {
                ids.push_back(data[i].first);
                if ( !m_Reference.empty() ) {
                    reference.push_back(data[i].second);
                }
            }
            if ( reuse_scope == eReuseScope_none || !scope ) {
                scope = MakeScope();
            }
            if ( !ProcessBlock(ids, reference, scope) ) {
                m_ErrorCount.Add(1);
            }
        }
        return true;
    }
    catch (CException& e) {
        LOG_POST(SetPostFlags(eDPF_DateTime|eDPF_TID)<<
                 "EXCEPTION = "<<e<<" at "<<e.GetStackTrace());
        return false;
    }
    catch (exception& e) {
        LOG_POST(SetPostFlags(eDPF_DateTime|eDPF_TID)<<
                 "EXCEPTION = "<<e.what());
        return false;
    }
    catch (...) {
        LOG_POST(SetPostFlags(eDPF_DateTime|eDPF_TID)<<
                 "EXCEPTION unknown");
        return false;
    }
}


bool CTestApplication::ProcessBlock(const vector<CSeq_id_Handle>& ids,
                                    const vector<string>& reference,
                                    CRef<CScope> scope) const
{
    AutoPtr<IBulkTester> data(IBulkTester::CreateTester(m_Type));
    data->SetParams(ids, m_GetFlags);
    {{
        if ( m_Single ) {
            data->LoadSingle(*scope);
        }
        else {
            data->LoadBulk(*scope);
        }
        data->VerifyWhatShouldBeNotLoaded(*scope);
    }}
    vector<bool> errors;
    if ( !reference.empty() ) {
        data->LoadVerify(reference);
        errors = data->GetErrors();
    }
    else if ( m_Verify ) {
        CRef<CScope> verify_scope = MakeScope();
        data->LoadVerify(*verify_scope);
        errors = data->GetErrors();
    }
    if ( m_Verbose ) {
        data->Display(cout, m_Verify);
    }
    bool ok = find(errors.begin(), errors.end(), true) == errors.end();
    if ( !ok ) {
        CMutexGuard guard(IBulkTester::sm_DisplayMutex);
        ERR_POST("Errors in "<<data->GetType()<<":");
        for ( size_t i = 0; i < ids.size(); ++i ) {
            if ( errors[i] ) {
                data->Display(cerr, i, true);
            }
        }
    }
    return ok;
}


/////////////////////////////////////////////////////////////////////////////
//
//  MAIN
//


int main(int argc, const char* argv[])
{
    return CTestApplication().AppMain(argc, argv);
}
