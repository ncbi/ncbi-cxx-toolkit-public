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
#include <util/random_gen.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/genbank/readers.hpp>
#include <dbapi/driver/drivers.hpp>

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

    typedef vector<CSeq_id_Handle> TIds;
    bool ProcessBlock(const TIds& ids) const;

    CRandom::TValue m_Seed;
    size_t m_RunCount;
    size_t m_RunSize;
    TIds m_Ids;
    IBulkTester::EBulkType m_Type;
    bool m_Verbose;
    bool m_Sort;
    bool m_Single;
    bool m_Verify;
    CScope::EForceLoad m_ForceLoad;
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
    args.AddDefaultKey
        ("type", "Type",
         "Type of bulk request",
         CArgDescriptions::eString, "gi");
    args.SetConstraint("type",
                       &(*new CArgAllow_Strings,
                         "gi", "acc", "label", "taxid", "hash",
                         "length", "type", "state"));
    args.AddFlag("no-force", "Do not force info loading");
    args.AddFlag("verbose", "Verbose results");
    args.AddFlag("sort", "Sort requests");
    args.AddFlag("single", "Use single id queries (non-bulk)");
    args.AddFlag("verify", "Run extra test to verify returned values");
    args.AddDefaultKey
        ("count", "Count",
         "Number of iterations to run",
         CArgDescriptions::eInteger, "10");
    args.AddDefaultKey
        ("size", "Size",
         "Number of Seq-ids to process in a run",
         CArgDescriptions::eInteger, "200");

    args.SetUsageContext(GetArguments().GetProgramBasename(),
                         "test_bulkinfo", false);
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
        
        NcbiCout << "Testing bulk info load (" <<
            m_Ids.size() << " Seq-ids from file)..." << NcbiEndl;
    }
    if ( m_Ids.empty() && (args["fromgi"] || args["togi"]) ) {
        TIntId gi_from  = args["fromgi"]? args["fromgi"].AsInteger(): g_gi_from;
        TIntId gi_to    = args["togi"]? args["togi"].AsInteger(): g_gi_to;
        TIntId delta = gi_to > gi_from? 1: -1;
        for ( TIntId gi = gi_from; gi != gi_to+delta; gi += delta ) {
            m_Ids.push_back(CSeq_id_Handle::GetGiHandle(gi));
        }
        NcbiCout << "Testing bulk info load ("
            "gi from " << gi_from << " to " << gi_to << ")..." << NcbiEndl;
    }
    if ( m_Ids.empty() ) {
        TIntId count = g_gi_to-g_gi_from+1;
        for ( TIntId i = 0; i < count; ++i ) {
            if ( i % 3 != 0 ) {
                m_Ids.push_back(CSeq_id_Handle::GetGiHandle(i+g_gi_from));
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
        NcbiCout << "Testing bulk info load ("
            "accessions and gi from " <<
            g_gi_from << " to " << g_gi_to << ")..." << NcbiEndl;
    }
    if ( args["type"].AsString() == "gi" ) {
        m_Type = IBulkTester::eBulk_gi;
    }
    else if ( args["type"].AsString() == "acc" ) {
        m_Type = IBulkTester::eBulk_acc;
    }
    else if ( args["type"].AsString() == "label" ) {
        m_Type = IBulkTester::eBulk_label;
    }
    else if ( args["type"].AsString() == "taxid" ) {
        m_Type = IBulkTester::eBulk_taxid;
    }
    else if ( args["type"].AsString() == "hash" ) {
        m_Type = IBulkTester::eBulk_hash;
    }
    else if ( args["type"].AsString() == "length" ) {
        m_Type = IBulkTester::eBulk_length;
    }
    else if ( args["type"].AsString() == "type" ) {
        m_Type = IBulkTester::eBulk_type;
    }
    else if ( args["type"].AsString() == "state" ) {
        m_Type = IBulkTester::eBulk_state;
    }
    m_ForceLoad = args["no-force"]? CScope::eNoForceLoad: CScope::eForceLoad;
    m_Verbose = args["verbose"];
    m_Sort = args["sort"];
    m_Single = args["single"];
    m_Verify = args["verify"];
    m_Seed = 0;
    m_RunCount = args["count"].AsInteger();
    m_RunSize = args["size"].AsInteger();
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


static
CRef<CScope> s_MakeScope(void)
{
    CRef<CScope> ret;
    CRef<CObjectManager> pOm = CObjectManager::GetInstance();
#ifdef HAVE_PUBSEQ_OS
    DBAPI_RegisterDriver_FTDS();
    GenBankReaders_Register_Pubseq();
#endif
    CGBDataLoader::RegisterInObjectManager(*pOm);
    
    ret = new CScope(*pOm);
    ret->AddDefaults();
    return ret;
}


bool CTestApplication::Thread_Run(int thread_id)
{
    vector<CSeq_id_Handle> ids;
    CRandom random(m_Seed+thread_id);
    for ( size_t run_i = 0; run_i < m_RunCount; ++run_i ) {
        size_t size = min(m_RunSize, m_Ids.size());
        ids = m_Ids;
        for ( size_t i = 0; i < size; ++i ) {
            swap(ids[i], ids[random.GetRandSize_t(i, m_Ids.size()-1)]);
        }
        ids.resize(size);
        if ( m_Sort ) {
            sort(ids.begin(), ids.end());
        }
        if ( !ProcessBlock(ids) ) {
            m_ErrorCount.Add(1);
        }
    }
    return true;
}


bool CTestApplication::ProcessBlock(const vector<CSeq_id_Handle>& ids) const
{
    AutoPtr<IBulkTester> data(IBulkTester::CreateTester(m_Type));
    data->SetParams(ids, m_ForceLoad);
    {{
        CRef<CScope> scope = s_MakeScope();
        if ( m_Single ) {
            data->LoadSingle(*scope);
        }
        else {
            data->LoadBulk(*scope);
        }
        ITERATE ( TIds, it, ids ) {
            _ASSERT(!scope->GetBioseqHandle(*it, CScope::eGetBioseq_Loaded));
        }
    }}
    vector<bool> errors;
    if ( m_Verify ) {
        CRef<CScope> scope = s_MakeScope();
        data->LoadVerify(*scope);
        errors = data->GetErrors();
    }
    if ( m_Verbose ) {
        data->Display(cout, m_Verify);
    }
    bool ok = find(errors.begin(), errors.end(), true) == errors.end();
    if ( !ok ) {
        CMutexGuard guard(IBulkTester::sm_DisplayMutex);
        cerr << "Errors in "<<data->GetType()<<":\n";
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
