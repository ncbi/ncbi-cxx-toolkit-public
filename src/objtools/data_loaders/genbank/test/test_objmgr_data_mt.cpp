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
* Authors:  Andrei Gourianov
*
* File Description:
*   Object Manager test: multiple threads working with annotations
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/test_mt.hpp>

#include <connect/ncbi_util.h>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>

#include <common/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE
using namespace objects;

DEFINE_STATIC_FAST_MUTEX(s_GlobalLock);

// GIs to process
#if 0
    const int g_gi_from = 156894;
    const int g_gi_to   = 156896;
#elif 0
    const int g_gi_from = 156201;
    const int g_gi_to   = 156203;
#else
    const int g_gi_from = 156000;
    const int g_gi_to   = 157000;
#endif

/////////////////////////////////////////////////////////////////////////////
//
//  Test application
//

class CTestOM : public CThreadedApp
{
protected:
    virtual bool Thread_Run(int idx);
    virtual bool TestApp_Args( CArgDescriptions& args);
    virtual bool TestApp_Init(void);
    virtual bool TestApp_Exit(void);

    typedef map<int, int> TValueMap;

    CRef<CScope> m_Scope;
    CRef<CObjectManager> m_ObjMgr;

    TValueMap m_mapGiToDesc, m_mapGiToFeat0, m_mapGiToFeat1;

    void SetValue(TValueMap& vm, int gi, int value);

    int m_gi_from;
    int m_gi_to;

    bool failed;
};


/////////////////////////////////////////////////////////////////////////////


void CTestOM::SetValue(TValueMap& vm, int gi, int value)
{
    int old_value;
    {{
        CFastMutexGuard guard(s_GlobalLock);
        old_value = vm.insert(TValueMap::value_type(gi, value)).first->second;
    }}
    if ( old_value != value ) {
        string name;
        if ( &vm == &m_mapGiToDesc ) name = "desc";
        if ( &vm == &m_mapGiToFeat0 ) name = "feat0";
        if ( &vm == &m_mapGiToFeat1 ) name = "feat1";
        ERR_POST("Inconsistent "<<name<<" on gi "<<gi<<
                 " was "<<old_value<<" now "<<value);
    }
    _ASSERT(old_value == value);
}


bool CTestOM::Thread_Run(int idx)
{
// initialize scope
    CScope scope(*m_ObjMgr);
    scope.AddDefaults();

    int from, to, delta;
    // make them go in opposite directions
    if (idx % 2 == 0) {
        from = m_gi_from;
        to = m_gi_to;
    } else {
        from = m_gi_to;
        to = m_gi_from;
    }
    delta = (to > from) ? 1 : -1;

    bool ok = true;
    const int kMaxErrorCount = 3;
    static int error_count = 0;
    for (int i = from;
        ((delta > 0) && (i <= to)) || ((delta < 0) && (i >= to)); i += delta) {
        try {
// load sequence
            CSeq_id sid;
            sid.SetGi(i);
            CBioseq_Handle handle = scope.GetBioseqHandle(sid);
            if (!handle) {
                LOG_POST("T" << idx << ": gi = " << i << ": INVALID HANDLE");
                SetValue(m_mapGiToDesc, i, -1);
                continue;
            }

            int count = 0;

// enumerate descriptions
            // Seqdesc iterator
            for (CSeqdesc_CI desc_it(handle); desc_it;  ++desc_it) {
                count++;
            }
// verify result
            SetValue(m_mapGiToDesc, i, count);

// enumerate features
            CSeq_loc loc;
            loc.SetWhole(sid);
            count = 0;
            if ( idx%2 == 0 ) {
                for (CFeat_CI feat_it(scope, loc,
                                      CSeqFeatData::e_not_set,
                                      SAnnotSelector::eOverlap_Intervals,
                                      CAnnotTypes_CI::eResolve_All);
                    feat_it;  ++feat_it) {
                    count++;
                }
// verify result
                SetValue(m_mapGiToFeat0, i, count);
            }
            else {
                for (CFeat_CI feat_it(handle, 0, 0, CSeqFeatData::e_not_set);
                    feat_it;  ++feat_it) {
                    count++;
                }
// verify result
                SetValue(m_mapGiToFeat1, i, count);
            }
        }
        catch (CLoaderException& e) {
            LOG_POST("T" << idx << ": gi = " << i 
                << ": EXCEPTION = " << e.what());
            ok = false;
            if ( e.GetErrCode() == CLoaderException::eNoConnection ) {
                break;
            }
            if ( ++error_count > kMaxErrorCount ) {
                break;
            }
        }
        catch (exception& e) {
            LOG_POST("T" << idx << ": gi = " << i 
                << ": EXCEPTION = " << e.what());
            ok = false;
            if ( ++error_count > kMaxErrorCount ) {
                break;
            }
        }
        scope.ResetHistory();
    }
    if ( !ok ) {
        failed = true;
    }
    return ok;
}

bool CTestOM::TestApp_Args( CArgDescriptions& args)
{
    args.AddDefaultKey
        ("fromgi", "FromGi",
         "Process sequences in the interval FROM this Gi",
         CArgDescriptions::eInteger, NStr::IntToString(g_gi_from));
    args.AddDefaultKey
        ("togi", "ToGi",
         "Process sequences in the interval TO this Gi",
         CArgDescriptions::eInteger, NStr::IntToString(g_gi_to));
    return true;
}

bool CTestOM::TestApp_Init(void)
{
    failed = false;
    CORE_SetLOCK(MT_LOCK_cxx2c());
    CORE_SetLOG(LOG_cxx2c());

    const CArgs& args = GetArgs();
    m_gi_from = args["fromgi"].AsInteger();
    m_gi_to   = args["togi"].AsInteger();

    NcbiCout << "Testing ObjectManager (" << s_NumThreads
        << " threads, gi from "
        << m_gi_from << " to " << m_gi_to << ")..." << NcbiEndl;

    m_ObjMgr = new CObjectManager;
    m_ObjMgr->RegisterDataLoader(*new CGBDataLoader("ID"),CObjectManager::eDefault);


    // Scope shared by all threads
/*
    m_Scope = new CScope(*m_ObjMgr);
    m_Scope->AddDefaults();
*/
    return true;
}

bool CTestOM::TestApp_Exit(void)
{
    if ( failed ) {
        NcbiCout << " Failed" << NcbiEndl << NcbiEndl;
    }
    else {
        NcbiCout << " Passed" << NcbiEndl << NcbiEndl;
    }

/*
    map<int, int>::iterator it;
    for (it = m_mapGiToDesc.begin(); it != m_mapGiToDesc.end(); ++it) {
        LOG_POST(
            "gi = "         << it->first
            << ": desc = "  << it->second
            << ", feat0 = " << m_mapGiToFeat0[it->first]
            << ", feat1 = " << m_mapGiToFeat1[it->first]
            );
    }
*/
    return true;
}
END_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  MAIN

USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    s_NumThreads = 12;
    return CTestOM().AppMain(argc, argv, 0, eDS_Default, 0);
}
